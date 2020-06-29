// VTPlayer.cpp : 定义应用程序的入口点。
//

#include "VTPlayer.h"

HWND hWnd;
HINSTANCE hInst;
CKContext* context;
CKRenderContext* renderContext;
UINT menuIds[26];

CKInputManager* inputManager;
CKRenderManager* renderManager;

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK Keys(HWND, UINT, WPARAM, LPARAM);
BOOL InitCKEnvironment();
BOOL ReleaseCKEnvironment();
void ProcessCamera();
BOOL OpenMapFile(const char* filename);
void UpdateCamera();
void UpdateBgMenu();
void SetBackground(int id);

void Assert(bool cond, const char* desc) {
    if (!cond) {
        MessageBox(hWnd, desc, "Error", MB_OK | MB_ICONERROR);
        ExitProcess(0);
    }
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

#ifdef _DEBUG
    freopen("stdout.txt", "w", stdout);
    freopen("stderr.txt", "w", stderr);
#endif

    MyRegisterClass(hInstance);
    Assert(InitInstance(hInstance, nCmdShow), "Init Instance Error");
    UpdateBgMenu();
    InitCKEnvironment();

    MSG msg;
    while (true) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                break;
            }
            else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        if (context && context->IsPlaying()) {
            float beforeRender = 0;
            float beforeProcess = 0;
            context->GetTimeManager()->GetTimeToWaitForLimits(beforeRender, beforeProcess);

            if (beforeProcess <= 0) {
#ifdef _DEBUG
                std::cout << "******** Tick ********" << std::endl;

                std::function<void(CKBehavior*, int)> active = [&active](CKBehavior* script, int depth) {
                    int num = script->GetSubBehaviorCount();
                    for (int j = 0; j < num; j++) {
                        CKBehavior* beh = script->GetSubBehavior(j);
                        if (beh->IsActive()) {
                            for (int i = 0; i < depth; i++)
                                std::cout << '\t';
                            std::cout << beh->GetName() << std::endl;
                            if (!beh->IsUsingFunction())
                                active(beh, depth + 1);
                        }
                    }
                };

                std::function<void(CKBehavior*)> delay = [&delay](CKBehavior* script) {
                    int num = script->GetSubBehaviorCount();
                    for (int j = 0; j < num; j++) {
                        CKBehavior* beh = script->GetSubBehavior(j);
                        if (!beh->IsUsingFunction())
                            delay(beh);
                    }
                    num = script->GetSubBehaviorLinkCount();
                    for (int j = 0; j < num; j++) {
                        script->GetSubBehaviorLink(j)->SetInitialActivationDelay(1);
                    }
                };

                int cnt = context->GetObjectsCountByClassID(CKCID_BEHAVIOR);
                CK_ID* scripts = context->GetObjectsListByClassID(CKCID_BEHAVIOR);
                bool act = false;
                for (int i = 0; i < cnt; i++) {
                    CKBehavior* script = (CKBehavior*)context->GetObject(scripts[i]);
                    if (script->GetType() == CKBEHAVIORTYPE_SCRIPT) {
                        // delay(script);
                        if (script->IsActive()) {
                            std::cout << script->GetName() << std::endl;
                            act = true;
                            active(script, 1);
                        }
                    }
                }
#endif

                context->GetTimeManager()->ResetChronos(FALSE, TRUE);
                context->Process();

                ProcessCamera();
            }

            if (beforeRender <= 0) {
                context->GetTimeManager()->ResetChronos(TRUE, FALSE);
                renderContext->Render();
            }
        }
    }

    ReleaseCKEnvironment();
    return 0;
}

BOOL InitCKEnvironment() {
    Assert(LoadLibrary("CK2.dll"), "Error loading CK2.dll");

    Assert(!CKStartUp(), "CKStartUp Error");
    CKPluginManager* pluginManager = CKGetPluginManager();
    Assert(pluginManager, "PluginManager = null");
    Assert(pluginManager->ParsePlugins("RenderEngines") > 0, "Error loading RenderEngines");
    Assert(pluginManager->ParsePlugins("Managers") > 0, "Error loading Managers");
    Assert(pluginManager->ParsePlugins("BuildingBlocks") > 0, "Error loading BuildingBlocks");
    Assert(pluginManager->ParsePlugins("Plugins") > 0, "Error loading Plugins");

    Assert(!CKCreateContext(&context, hWnd), "CKCreateContext Error");

    inputManager = static_cast<CKInputManager*>(context->GetManagerByGuid(INPUT_MANAGER_GUID));
    if (inputManager == nullptr) {
        int pluginCnt = pluginManager->GetPluginCount(CKPLUGIN_MANAGER_DLL);

        for (int i = 0; i < pluginCnt; i++) {
            CKPluginEntry* entry = pluginManager->GetPluginInfo(CKPLUGIN_MANAGER_DLL, i);
            if (entry->m_PluginInfo.m_GUID == INPUT_MANAGER_GUID)
                entry->m_PluginInfo.m_InitInstanceFct(context);
        }
    }

    inputManager = static_cast<CKInputManager*>(context->GetManagerByGuid(INPUT_MANAGER_GUID));
    Assert(inputManager, "InputManager = null");

    renderManager = context->GetRenderManager();
    Assert(renderManager, "RenderManager = null");
    Assert(renderManager->GetRenderDriverCount(), "No render driver found");

    renderContext = renderManager->CreateRenderContext(hWnd);
    Assert(renderContext, "RenderContext = null");
    RECT rect; GetClientRect(hWnd, &rect);
    renderContext->Resize(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
    renderContext->Clear();
    renderContext->BackToFront();
    renderContext->Clear();

    CKPathManager* pathManager = context->GetPathManager();
    Assert(pathManager, "PathManager = null");
    std::string path; path.resize(MAX_PATH);
    path.resize(GetCurrentDirectory(MAX_PATH, &path[0]));

    pathManager->AddPath(DATA_PATH_IDX, (path + "\\").c_str());
    pathManager->AddPath(SOUND_PATH_IDX, (path + "\\Sounds").c_str());
    pathManager->AddPath(BITMAP_PATH_IDX, (path + "\\").c_str());
    pathManager->AddPath(BITMAP_PATH_IDX, (path + "\\Textures").c_str());
    pathManager->AddPath(DATA_PATH_IDX, (path + "\\Textures").c_str());

    XString resolvedfile = "Levelinit.cmo";
    Assert(!pathManager->ResolveFileName(resolvedfile, 0), "Resolve path failed");
    CKObjectArray* array = CreateCKObjectArray();
    Assert(!context->Load(resolvedfile.CStr(), array), "Load CMO failed");
    DeleteCKObjectArray(array);

    UpdateCamera();

    CKLevel* level = context->GetCurrentLevel();
    Assert(level, "Level = null");
    level->AddRenderContext(renderContext, true);
    Assert(!level->LaunchScene(nullptr), "Launch scene failed");
    Assert(!renderContext->Render(), "Render failed");

    return TRUE;
}

BOOL ReleaseCKEnvironment() {
    Assert(!context->Reset(), "Reset failed");
    Assert(!context->ClearAll(), "ClearAll failed");

    if (renderManager && renderContext) {
        Assert(!renderManager->DestroyRenderContext(renderContext), "DestroyRenderContext failed");
    }

    Assert(!CKCloseContext(context), "CKCloseContext failed");
    Assert(!CKShutdown(), "CKShutdown failed");

    return TRUE;
}

void ProcessCamera() {
    CKCamera* camera = static_cast<CKCamera*>(context->GetObjectByNameAndClass("IngameCam", CKCID_TARGETCAMERA));
    CK3dEntity* camref = camera->GetTarget();
    CK3dEntity* camPos = static_cast<CK3dEntity*>(context->GetObjectByNameAndClass("CamPos", CKCID_3DENTITY));
    CK3dEntity* camTar = static_cast<CK3dEntity*>(context->GetObjectByNameAndClass("CamTarget", CKCID_3DENTITY));
    float delta = context->GetTimeManager()->GetLastDeltaTime();

    static unsigned char oldState[256] = { 0 };
    unsigned char* newState = inputManager->GetKeyboardState();
    if (newState[CKKEY_Q])
        camref->Rotate(&VxVector(0, 1, 0), -0.002f * delta);
    if (newState[CKKEY_E])
        camref->Rotate(&VxVector(0, 1, 0), 0.002f * delta);
    if (newState[CKKEY_W] && !oldState[CKKEY_W])
        camref->Rotate(&VxVector(0, 1, 0), PI / 4);
    if (newState[CKKEY_D] && !oldState[CKKEY_D])
        camref->SetQuaternion(&VxQuaternion());

    if (newState[CKKEY_UP])
        camTar->Translate(&VxVector(0, 0, 0.1f * delta), camref);
    if (newState[CKKEY_DOWN])
        camTar->Translate(&VxVector(0, 0, -0.1f * delta), camref);
    if (newState[CKKEY_LEFT])
        camTar->Translate(&VxVector(-0.1f * delta, 0, 0), camref);
    if (newState[CKKEY_RIGHT])
        camTar->Translate(&VxVector(0.1f * delta, 0, 0), camref);
    if (newState[CKKEY_SPACE])
        camTar->Translate(&VxVector(0, 0.1f * delta, 0), camref);
    if (newState[CKKEY_LSHIFT])
        camTar->Translate(&VxVector(0, -0.1f * delta, 0), camref);

    if (newState[CKKEY_A]) {
        VxVector distance;
        camPos->GetPosition(&distance, camref);
        if (distance.z < -5)
            camPos->Translate(&VxVector(0, 0, 0.02f * delta), camref);
    }
    if (newState[CKKEY_Z])
        camPos->Translate(&VxVector(0, 0, -0.02f * delta), camref);
    if (newState[CKKEY_S])
        camPos->Translate(&VxVector(0, 0.02f * delta, 0), camref);
    if (newState[CKKEY_X])
        camPos->Translate(&VxVector(0, -0.02f * delta, 0), camref);

    VxVector curpos, tarpos;
    camera->GetPosition(&curpos);
    camPos->GetPosition(&tarpos);
    curpos += (tarpos - curpos) * 0.005f * delta;
    camera->SetPosition(&curpos);

    camref->GetPosition(&curpos);
    camTar->GetPosition(&tarpos);
    VxVector deltaPos = (tarpos - curpos) * 0.005f * delta;
    camref->SetPosition(&(curpos + deltaPos));
    camera->GetPosition(&curpos);
    camera->SetPosition(&(curpos + deltaPos));

    memcpy(oldState, newState, sizeof(oldState));
}

CKBehavior* FindFirstBB(CKBehavior* script, CKSTRING name) {
    int cnt = script->GetSubBehaviorCount();
    for (int i = 0; i < cnt; i++) {
        CKBehavior* beh = script->GetSubBehavior(i);
        if (!strcmp(beh->GetName(), name))
            return beh;
    }

    return nullptr;
}

BOOL OpenMapFile(const char* filename) {
    Assert(!context->Reset(), "Reset failed");
    CKBehavior* build = static_cast<CKBehavior*>(context->GetObjectByNameAndClass("Levelinit_build", CKCID_BEHAVIOR));
    CKBehavior* loadLevel = FindFirstBB(build, "Load LevelXX");
    CKBehavior* objLoad = FindFirstBB(loadLevel, "Object Load");
    objLoad->GetInputParameter(0)->GetDirectSource()->SetStringValue(filename);
    Assert(!context->Play(), "Play failed");
    UpdateCamera();
    static_cast<CKBehavior*>(context->GetObjectByNameAndClass("Levelinit_build", CKCID_BEHAVIOR))->Activate(true, true);
    return TRUE;
}

void UpdateCamera() {
    RECT rect; GetClientRect(hWnd, &rect);
    renderContext->Resize(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
    CKCamera* camera = static_cast<CKCamera*>(context->GetObjectByNameAndClass("IngameCam", CKCID_TARGETCAMERA));
    camera->SetAspectRatio(renderContext->GetWidth(), renderContext->GetHeight());
}

ATOM MyRegisterClass(HINSTANCE hInstance) {
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_VTPLAYER));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCE(IDC_VTPLAYER);
    wcex.lpszClassName = "vtplayer";
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDC_VTPLAYER));

    return RegisterClassEx(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
    hInst = hInstance;

    hWnd = CreateWindow("vtplayer", "Ballance Map Previewer V1.2", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
        return FALSE;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

void UpdateBgMenu() {
    HMENU bgMenu = CreateMenu();
    std::string path = "Textures\\Sky\\";
    for (int i = 0; i < 26; i++) {
        menuIds[i] = 32800 + i;
        char szMenu[16];
        sprintf(szMenu, "Sky_%c", 'A' + i);
        bool exist = true;
        for (auto suffix : { "_Back", "_Right", "_Front", "_Left", "_Down" }) {
            std::string file = path + szMenu + suffix + ".bmp";
            if (!std::filesystem::exists(file)) {
                exist = false;
                break;
            }
        }

        if (!exist) break;
        MENUITEMINFO info = { 0 };
        info.cbSize = sizeof(info);
        info.fMask = MIIM_ID | MIIM_TYPE;
        info.wID = menuIds[i];
        info.dwTypeData = szMenu;
        info.cch = strlen(szMenu) + 1;
        InsertMenuItem(bgMenu, menuIds[i], false, &info);
    }

    InsertMenu(GetMenu(hWnd), 1, MF_BYPOSITION | MF_POPUP | MF_STRING, reinterpret_cast<UINT_PTR>(bgMenu), "背景(&B)");
    DrawMenuBar(hWnd);
}

void SetBackground(int id) {
    if (context) {
        char skyName[16];
        sprintf(skyName, "Sky_%c", 'A' + id);
        CKBehavior* sky = static_cast<CKBehavior*>(context->GetObjectByNameAndClass("Gameplay_Sky", CKCID_BEHAVIOR));
        CKBehavior* loadTex = FindFirstBB(sky, "Load Sky-Textures");
        loadTex->GetInputParameter(0)->GetDirectSource()->SetStringValue(skyName);
        auto str = static_cast<char*>(loadTex->GetInputParameter(0)->GetDirectSource()->GetReadDataPtr());
        loadTex->ActivateInput(0);
        loadTex->Activate(true);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        switch (wmId) {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_KEYS:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_KEYSBOX), hWnd, About);
            break;
        case IDM_OPEN: {
            char szBuffer[MAX_PATH] = { 0 };
            OPENFILENAME ofn = { 0 };
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFilter = "Ballance 地图文件 (*.nmo)\0*.nmo\0";
            ofn.lpstrInitialDir = "";
            ofn.lpstrFile = szBuffer;
            ofn.nMaxFile = sizeof(szBuffer) / sizeof(*szBuffer);
            ofn.nFilterIndex = 0;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;
            BOOL bSel = GetOpenFileName(&ofn);

            OpenMapFile(szBuffer);
        }
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            for (int i = 0; i < 26; i++) {
                if (wmId == menuIds[i]) {
                    SetBackground(i);
                    break;
                }
            }
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_SIZE: {
        if (renderContext) {
            UpdateCamera();
        }
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }

    return (INT_PTR)FALSE;
}