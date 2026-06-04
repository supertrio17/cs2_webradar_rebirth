#include "pch.hpp"
#include <format>
#include <thread>

// Global config
config_data_t config;

HWND g_hMainWnd = NULL;
HWND g_hLogEdit = NULL;
constexpr UINT WM_APP_LOG = WM_APP + 1;

void LogMessage(const std::string& msg, int type = 1) {
    if (!g_hMainWnd) return;

    static const char* levels[] = { "[OTHER] ", "[INFO] ", "[WARNING] ", "[ERROR] " };
    const char* prefix = (type >= 0 && type <= 3) ? levels[type] : levels[0];

    std::string fullMsg = prefix + msg;

    char* pText = new char[fullMsg.size() + 1];
    std::copy(fullMsg.begin(), fullMsg.end(), pText);
    pText[fullMsg.size()] = '\0';

    PostMessage(g_hMainWnd, WM_APP_LOG, 0, (LPARAM)pText);
}

DWORD WINAPI AppLogic(LPVOID) {
    LOG_CLEAR();

    if (!utils::is_updated()) {
        LogMessage("Radar is not updated! Check LOG for more info.", 3);
        return 0;
    }
    LogMessage("Radar is up to date.");

    int cfgResult = cfg::setup(config);
    if (cfgResult != 0) {
        const char* errs[] = { "", "Couldn't open config.json.", "Failed to parse config.json.", "Failed to deserialize config.json." };
        LogMessage(errs[cfgResult], 3);
        return 0;
    }

    if (config.m_debug) LOG_DEBUG("config loaded");

    if (!exc::setup()) { LogMessage("Exception setup failed!", 3); return 0; }
    if (config.m_debug) LOG_DEBUG("exception handlers initialized");

    int memStatus;
    bool waitingLog = true;
    do {
        memStatus = m_memory->setup();
        if (memStatus == 2) {
            if (waitingLog) {
                LogMessage("Waiting for CS2.exe...");
                waitingLog = false;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } while (memStatus == 2);

    LogMessage("Found CS2.exe, initializing...");

    uint32_t interface_attempts = 0;
    while (!i::setup()) {
        interface_attempts++;
        if (waitingLog) {
            LogMessage("Waiting for game load...");
            waitingLog = false;
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    LogMessage("Game loaded.");

    if (!schema::setup()) { LogMessage("Schema setup failed!", 3); return 0; }
    LogMessage("Schema initialized.");

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 0;

    auto ipv4 = utils::get_ipv4_address(config);
    if (ipv4.empty()) ipv4 = config.m_local_ip;

    std::string url = std::format("ws://{}:22006/cs2_webradar", ipv4);

    auto ws = easywsclient::WebSocket::from_url(url);
    uint32_t websocket_attempts = 1;

    while (!ws) {
        LogMessage("Connection failed, retrying...", 2);
        ws = easywsclient::WebSocket::from_url(url);
        websocket_attempts++;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    LogMessage("Connected to websocket.");

    auto last_update = std::chrono::steady_clock::now();

    while (true) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update).count() >= 50) {
            last_update = now;

            sdk::update();
            bool in_match = f::run();

            if (!in_match)
                f::m_data["m_map"] = "invalid";

            try {
                ws->send(f::m_data.dump());
            }
            catch (...) {}
        }

        ws->poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_hLogEdit = CreateWindow("EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            10, 0, 360, 210, hWnd, (HMENU)1, NULL, NULL);
        SendMessage(g_hLogEdit, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        return 0;

    case WM_APP_LOG: {
        char* pText = (char*)lParam;
        int len = GetWindowTextLength(g_hLogEdit);
        SendMessage(g_hLogEdit, EM_SETSEL, len, len);
        SendMessage(g_hLogEdit, EM_REPLACESEL, 0, (LPARAM)pText);
        SendMessage(g_hLogEdit, EM_REPLACESEL, 0, (LPARAM)"\r\n");
        delete[] pText;
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    const char* CNAME = "WBFCS";
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CNAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc);

    g_hMainWnd = CreateWindowEx(0, CNAME, CNAME,
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 260, NULL, NULL, hInst, NULL);

    if (!g_hMainWnd) return 0;

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    CloseHandle(CreateThread(NULL, 0, AppLogic, NULL, 0, NULL));

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}