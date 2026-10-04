// language: C++17, file: main.cpp, app: gasgauge — ethereum gas fee monitor
// Windows 11, MSVC, WinAPI + WinHTTP. No external dependencies.
// Polls an EIP-1559 oracle endpoint every 15 s and shows slow/standard/fast
// tiers plus tx cost estimates. Demo mode until ORACLE_URL is set (edit below).
#include <windows.h>
#include <winhttp.h>
#include <string>

#pragma comment(lib, "winhttp.lib")

// Point this at any oracle returning { "baseFeeGwei": 14.2, "priorityGwei": 1.1 }
static const wchar_t* kOracleHost = L"your-gas-oracle.example";
static const wchar_t* kOraclePath = L"/latest";

static HWND g_base, g_tiers, g_status;
static double g_ethUsd = 3400.0;

static double ExtractNumber(const std::string& json, const std::string& field) {
    size_t p = json.find("\"" + field + "\":");
    if (p == std::string::npos) return -1.0;
    p += field.size() + 3;
    return std::stod(json.substr(p, json.find_first_of(",}", p) - p));
}

static std::string HttpsGet(const std::wstring& host, const std::wstring& path) {
    std::string out;
    HINTERNET sess = WinHttpOpen(L"gasgauge/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!sess) return out;
    HINTERNET conn = WinHttpConnect(sess, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (conn) {
        HINTERNET req = WinHttpOpenRequest(conn, L"GET", path.c_str(), nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           WINHTTP_FLAG_SECURE);
        if (req && WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                      WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
                && WinHttpReceiveResponse(req, nullptr)) {
            DWORD bytes = 0; char buf[4096];
            for (;;) {
                if (!WinHttpReadData(req, buf, sizeof(buf), &bytes) || bytes == 0) break;
                out.append(buf, bytes);
            }
        }
        if (req) WinHttpCloseHandle(req);
        WinHttpCloseHandle(conn);
    }
    WinHttpCloseHandle(sess);
    return out;
}

// Cost in USD: gwei * 1e-9 * gasLimit * ethUsd
static double TxCost(double gwei, int gasLimit) {
    return gwei * 1e-9 * gasLimit * g_ethUsd;
}

static DWORD WINAPI PollOracle(LPVOID) {
    for (;;) {
        double base = 14.0 + (GetTickCount64() % 800) / 100.0;  // demo until oracle configured
        double prio = 1.0;
        std::string body = HttpsGet(kOracleHost, kOraclePath);
        if (!body.empty()) {
            double b = ExtractNumber(body, "baseFeeGwei");
            double p = ExtractNumber(body, "priorityGwei");
            if (b > 0) base = b;
            if (p >= 0) prio = p;
        }
        wchar_t buf[512];
        swprintf(buf, 512, L"base fee: %.1f gwei   priority: %.1f gwei", base, prio);
        SetWindowTextW(g_base, buf);
        swprintf(buf, 512,
                 L"slow: %.1f gwei ($%.2f send / $%.2f swap)\r\n"
                 L"standard: %.1f gwei ($%.2f send / $%.2f swap)\r\n"
                 L"fast: %.1f gwei ($%.2f send / $%.2f swap)",
                 base * 0.9 + prio, TxCost(base * 0.9 + prio, 21000), TxCost(base * 0.9 + prio, 150000),
                 base * 1.0 + prio, TxCost(base * 1.0 + prio, 21000), TxCost(base * 1.0 + prio, 150000),
                 base * 1.25 + prio, TxCost(base * 1.25 + prio, 21000), TxCost(base * 1.25 + prio, 150000));
        SetWindowTextW(g_tiers, buf);
        SetWindowTextW(g_status, base < 15 ? L"status: cheap — good time to transact"
                                           : L"status: congested — consider waiting");
        Sleep(15000);
    }
    return 0;
}

static LRESULT CALLBACK WndProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        HFONT big = CreateFontW(28, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, 0, 0, L"Segoe UI");
        HFONT mid = CreateFontW(18, 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, 0, 0, L"Segoe UI");
        g_base = CreateWindowW(L"STATIC", L"loading…", WS_CHILD | WS_VISIBLE,
                               24, 24, 480, 32, w, nullptr, nullptr, nullptr);
        g_tiers = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                24, 72, 480, 120, w, nullptr, nullptr, nullptr);
        g_status = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                 24, 208, 480, 28, w, nullptr, nullptr, nullptr);
        SendMessageW(g_base, WM_SETFONT, (WPARAM)big, TRUE);
        SendMessageW(g_tiers, WM_SETFONT, (WPARAM)mid, TRUE);
        SendMessageW(g_status, WM_SETFONT, (WPARAM)mid, TRUE);
        CreateThread(nullptr, 0, PollOracle, nullptr, 0, nullptr);
        return 0;
    }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int show) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(13, 17, 23));
    wc.lpszClassName = L"GasGaugeWnd";
    RegisterClassW(&wc);
    HWND w = CreateWindowExW(0, L"GasGaugeWnd", L"GasGauge — ethereum gas monitor",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 540, 300,
                             nullptr, nullptr, inst, nullptr);
    ShowWindow(w, show);
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) { TranslateMessage(&m); DispatchMessageW(&m); }
    return 0;
}
