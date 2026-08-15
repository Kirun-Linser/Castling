/* MemoryCleaner - 原生内存清理工具
 * 机制与 PCL2 一致：遍历进程 SetProcessWorkingSetSize(-1,-1) + EmptyWorkingSet
 * 管理员运行时额外：启用特权清理系统进程 + 清空 Standby List（备用内存）
 * 深色自绘提示窗口，零运行时依赖
 */
#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <objbase.h>
#include <propidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

/* ---- Standby list 清理 ---- */
typedef LONG (WINAPI *NtSetSystemInformation_t)(ULONG, PVOID, ULONG);
#define SystemMemoryListInformation 80
typedef struct { ULONG Version; ULONG Flags; ULONG Count; } MEMORY_LIST_COMMAND;

static HWND g_hwnd;
static BOOL g_btnHover;
static BOOL g_isAdmin;
static int g_ok;
static long long g_freed;
static ULONGLONG g_before, g_after;
static int g_scale; /* dpi 缩放，百分比 */

static BOOL EnablePrivilege(LPCWSTR name, BOOL *granted) {
    HANDLE tok; TOKEN_PRIVILEGES tp; LUID luid;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok)) return FALSE;
    if (!LookupPrivilegeValueW(NULL, name, &luid)) { CloseHandle(tok); return FALSE; }
    tp.PrivilegeCount = 1; tp.Privileges[0].Luid = luid; tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    BOOL ok = AdjustTokenPrivileges(tok, FALSE, &tp, sizeof(tp), NULL, NULL);
    if (granted) *granted = (GetLastError() == ERROR_SUCCESS);
    CloseHandle(tok);
    return ok;
}

static void PurgeStandbyList(void) {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return;
    NtSetSystemInformation_t fn = (NtSetSystemInformation_t)(void*)GetProcAddress(ntdll, "NtSetSystemInformation");
    if (!fn) return;
    MEMORY_LIST_COMMAND cmd; cmd.Version = 1; cmd.Flags = 2; cmd.Count = 0;
    fn(SystemMemoryListInformation, &cmd, sizeof(cmd));
}

static ULONGLONG AvailMB(void) {
    MEMORYSTATUSEX m; m.dwLength = sizeof(m);
    GlobalMemoryStatusEx(&m);
    return m.ullAvailPhys / (1024uLL * 1024uLL);
}

/* ---------- 深色提示窗口 ---------- */

static HFONT MakeFont(int pt, int weight) {
    return CreateFontW(-MulDiv(pt, g_scale, 72), 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
}

static void DrawRoundRect(HDC dc, int x, int y, int w, int h, int r, COLORREF c) {
    HBRUSH br = CreateSolidBrush(c);
    HPEN pen = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ ob = SelectObject(dc, br), op = SelectObject(dc, pen);
    RoundRect(dc, x, y, x + w, y + h, r, r);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(br); DeleteObject(pen);
}

static void DrawTextLine(HDC dc, const wchar_t *txt, int x, int y, int w, int h, HFONT f, COLORREF c) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, c);
    HGDIOBJ of = SelectObject(dc, f);
    RECT rc = { x, y, x + w, y + h };
    DrawTextW(dc, txt, -1, &rc, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOCLIP);
    SelectObject(dc, of);
}

static void DrawTextCenter(HDC dc, const wchar_t *txt, int y, int wndW, int w, int h, HFONT f, COLORREF c) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, c);
    HGDIOBJ of = SelectObject(dc, f);
    RECT rc = { 0, y, wndW, y + h };
    DrawTextW(dc, txt, -1, &rc, DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOCLIP);
    SelectObject(dc, of);
}

/* 从资源(ID 2, PNG)用 GDI+ 高质量绘制图标 */
static void DrawPngIcon(HINSTANCE hInst, HDC dc, int x, int y, int w, int h) {
    HRSRC hRes = FindResourceW(hInst, MAKEINTRESOURCEW(2), MAKEINTRESOURCEW(10));
    if (!hRes) return;
    HGLOBAL hg = LoadResource(hInst, hRes);
    if (!hg) return;
    void* data = LockResource(hg);
    DWORD size = SizeofResource(hInst, hRes);
    IStream* stream = NULL;
    if (CreateStreamOnHGlobal(NULL, TRUE, &stream) != S_OK) return;
    ULONG written = 0;
    stream->lpVtbl->Write(stream, data, size, &written);
    LARGE_INTEGER li; li.QuadPart = 0;
    stream->lpVtbl->Seek(stream, li, STREAM_SEEK_SET, NULL);
    GpImage* img = NULL;
    if (GdipLoadImageFromStream(stream, &img) == 0 && img) {
        GpGraphics* gfx = NULL;
        if (GdipCreateFromHDC(dc, &gfx) == 0 && gfx) {
            GdipSetInterpolationMode(gfx, 7 /*InterpolationModeHighQualityBicubic*/);
            GdipSetPixelOffsetMode(gfx, 2 /*PixelOffsetModeHighQuality*/);
            GdipDrawImageRectI(gfx, img, x, y, w, h);
            GdipDeleteGraphics(gfx);
        }
        GdipDisposeImage(img);
    }
    stream->lpVtbl->Release(stream);
}

static void OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    int W = rc.right, H = rc.bottom;

    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, W, H);
    HGDIOBJ ob = SelectObject(mem, bmp);

    /* 背景（更深的黑蓝） */
    HBRUSH bg = CreateSolidBrush(RGB(0x0B, 0x0D, 0x12));
    FillRect(mem, &rc, bg);
    DeleteObject(bg);

    int pad = W / 20;
    int iconSize = 52 * g_scale / 100;

    /* 图标（GDI+ 高质量绘制，资源 PNG） */
    DrawPngIcon(GetModuleHandleW(NULL), mem, pad, H * 9 / 100, iconSize, iconSize);

    /* 标题 */
    HFONT fTitle = MakeFont(19, FW_SEMIBOLD);
    DrawTextLine(mem, L"内存清理完成", pad + iconSize + W * 3 / 100, H * 9 / 100 + (iconSize - 30 * g_scale / 100) / 2, W - 2 * pad, 40 * g_scale / 100, fTitle, RGB(0xF5, 0xF6, 0xFA));
    DeleteObject(fTitle);

    /* 大字释放量（淡天蓝色） */
    wchar_t big[128];
    wsprintfW(big, L"约 %I64d MB", g_freed);
    HFONT fBig = MakeFont(27, FW_BOLD);
    DrawTextCenter(mem, big, H * 26 / 100, W, W, 56 * g_scale / 100, fBig, RGB(0xA9, 0xD3, 0xF5));
    DeleteObject(fBig);

    /* 副行（管理员模式拆两行） */
    if (g_isAdmin) {
        wchar_t sub1[128], sub2[128];
        wsprintfW(sub1, L"已清理 %d 个进程的工作集", g_ok);
        wsprintfW(sub2, L"（含系统进程与备用内存）", g_ok);
        HFONT fSub = MakeFont(11, FW_NORMAL);
        DrawTextCenter(mem, sub1, H * 45 / 100, W, W, 26 * g_scale / 100, fSub, RGB(0xA8, 0xB0, 0xC2));
        DrawTextCenter(mem, sub2, H * 45 / 100 + 18 * g_scale / 100, W, W, 22 * g_scale / 100, fSub, RGB(0xA8, 0xB0, 0xC2));
        DeleteObject(fSub);
    } else {
        wchar_t sub[128];
        wsprintfW(sub, L"已清理 %d 个进程的工作集", g_ok);
        HFONT fSub = MakeFont(11, FW_NORMAL);
        DrawTextCenter(mem, sub, H * 49 / 100, W, W, 34 * g_scale / 100, fSub, RGB(0xA8, 0xB0, 0xC2));
        DeleteObject(fSub);
    }

    /* 对比区：单行文本（箭头自然紧凑）+ GB 小字精确对准两个数字段 */
    HFONT fSub2 = MakeFont(11, FW_NORMAL);
    HGDIOBJ of = SelectObject(mem, fSub2);
    int row1 = H * 62 / 100;
    int rh1 = 30 * g_scale / 100, rh2 = 18 * g_scale / 100;

    wchar_t cmp[128], gbBefore[64], gbAfter[64];
    wsprintfW(cmp, L"清理前可用 %I64u MB  →  清理后可用 %I64u MB", g_before, g_after);
    wsprintfW(gbBefore, L"约 %I64d.%I64d GB", g_before * 10 / 1024 / 10, g_before * 10 / 1024 % 10);
    wsprintfW(gbAfter, L"约 %I64d.%I64d GB", g_after * 10 / 1024 / 10, g_after * 10 / 1024 % 10);

    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, RGB(0x75, 0x7D, 0x8F));
    RECT all = { 0, row1, W, row1 + rh1 };
    DrawTextW(mem, cmp, -1, &all, DT_CENTER | DT_TOP | DT_SINGLELINE);

    /* 测量定位 */
    RECT mc = { 0, 0, W, 0 };
    DrawTextW(mem, cmp, -1, &mc, DT_CALCRECT | DT_SINGLELINE);
    int cx0 = (W - mc.right) / 2;
    /* 左段中心 */
    wchar_t pre1[64];
    wsprintfW(pre1, L"清理前可用 %I64u MB", g_before);
    RECT q1 = { 0, 0, 0, 0 };
    DrawTextW(mem, pre1, -1, &q1, DT_CALCRECT | DT_SINGLELINE);
    RECT pp1 = { 0, 0, 0, 0 };
    DrawTextW(mem, L"清理前可用 ", -1, &pp1, DT_CALCRECT | DT_SINGLELINE);
    int c1 = cx0 + pp1.right + (q1.right - pp1.right) / 2;
    /* 右段中心 */
    wchar_t pre2[128];
    wsprintfW(pre2, L"清理前可用 %I64u MB  →  ", g_before);
    RECT q2 = { 0, 0, 0, 0 };
    DrawTextW(mem, pre2, -1, &q2, DT_CALCRECT | DT_SINGLELINE);
    wchar_t full2[64];
    wsprintfW(full2, L"清理后可用 %I64u MB", g_after);
    RECT f2 = { 0, 0, 0, 0 };
    DrawTextW(mem, full2, -1, &f2, DT_CALCRECT | DT_SINGLELINE);
    RECT pp2 = { 0, 0, 0, 0 };
    DrawTextW(mem, L"清理后可用 ", -1, &pp2, DT_CALCRECT | DT_SINGLELINE);
    int c2 = cx0 + q2.right + pp2.right + (f2.right - pp2.right) / 2;

    /* GB 行（更贴近 MB 文本底部） */
    int row2 = row1 + rh1 - 18;
    HFONT fGb = MakeFont(9, FW_NORMAL);
    HGDIOBJ og = SelectObject(mem, fGb);
    SetTextColor(mem, RGB(0xB4, 0xB9, 0xC2));
    RECT lr2 = { c1 - 120, row2, c1 + 120, row2 + rh2 };
    DrawTextW(mem, gbBefore, -1, &lr2, DT_CENTER | DT_TOP | DT_SINGLELINE);
    RECT rr2 = { c2 - 120, row2, c2 + 120, row2 + rh2 };
    DrawTextW(mem, gbAfter, -1, &rr2, DT_CENTER | DT_TOP | DT_SINGLELINE);
    SelectObject(mem, og);
    DeleteObject(fGb);
    SelectObject(mem, of);
    DeleteObject(fSub2);

    /* 确定按钮（水平居中） */
    int bw = 100 * g_scale / 100, bh = 36 * g_scale / 100;
    int bx = (W - bw) / 2, by = H - pad - bh;
    COLORREF btnC = g_btnHover ? RGB(0x5B, 0x90, 0xFF) : RGB(0x3D, 0x7B, 0xFF);
    DrawRoundRect(mem, bx, by, bw, bh, 15 * g_scale / 100, btnC);
    HFONT fBtn = MakeFont(9, FW_SEMIBOLD);
    SetBkMode(mem, TRANSPARENT); SetTextColor(mem, RGB(0xFF, 0xFF, 0xFF));
    HGDIOBJ ob2 = SelectObject(mem, fBtn);
    RECT brc = { bx, by, bx + bw, by + bh };
    DrawTextW(mem, L"确定", -1, &brc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(mem, ob2);
    DeleteObject(fBtn);

    /* 悬停提示：按钮下方灰色小字 */
    if (g_btnHover) {
        HFONT fHint = MakeFont(8, FW_NORMAL);
        DrawTextCenter(mem, L"或者按 ESC 关闭弹窗", by + bh + 4 * g_scale / 100, W, W, 18 * g_scale / 100, fHint, RGB(0xB4, 0xB9, 0xC2));
        DeleteObject(fHint);
    }

    /* 深蓝色描边（细） */
    int ew = g_scale / 100; if (ew < 1) ew = 1;
    HPEN edgePen = CreatePen(PS_SOLID, ew, RGB(0x11, 0x28, 0x5C));
    HGDIOBJ oldPen = SelectObject(mem, edgePen);
    HGDIOBJ oldBrush = SelectObject(mem, GetStockObject(NULL_BRUSH));
    RoundRect(mem, 2, 2, W - 2, H - 2, 40 * g_scale / 100, 40 * g_scale / 100);
    SelectObject(mem, oldPen); SelectObject(mem, oldBrush);
    DeleteObject(edgePen);

    BitBlt(dc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
    SelectObject(mem, ob);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

static void InBtn(LPARAM lp, BOOL *in) {
    RECT rc; GetClientRect(g_hwnd, &rc);
    int pad = rc.right / 20;
    int bw = 100 * g_scale / 100, bh = 36 * g_scale / 100;
    int bx = (rc.right - bw) / 2, by = rc.bottom - pad - bh;
    POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
    *in = (p.x >= bx && p.x <= bx + bw && p.y >= by && p.y <= by + bh);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: OnPaint(hwnd); return 0;
    case WM_MOUSEMOVE: {
        BOOL in = FALSE; InBtn(lp, &in);
        if (in != g_btnHover) {
            g_btnHover = in;
            InvalidateRect(hwnd, NULL, FALSE);
            TRACKMOUSEEVENT tme; tme.cbSize = sizeof(tme); tme.dwFlags = TME_LEAVE; tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
        }
        return 0;
    }
    case WM_MOUSELEAVE: if (g_btnHover) { g_btnHover = FALSE; InvalidateRect(hwnd, NULL, FALSE); } return 0;
    case WM_LBUTTONDOWN: {
        BOOL in = FALSE; InBtn(lp, &in);
        if (!in) { ReleaseCapture(); SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0); }
        return 0;
    }
    case WM_LBUTTONUP: {
        BOOL in = FALSE; InBtn(lp, &in);
        if (in) DestroyWindow(hwnd);
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE || wp == VK_RETURN) DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void ShowResult(HINSTANCE hInst) {
    g_scale = GetDpiForSystem() * 100 / 96;
    WNDCLASSW wc; ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512));
    wc.lpszClassName = L"MemoryCleanerDarkWnd";
    wc.hbrBackground = NULL;
    RegisterClassW(&wc);

    int W = GetSystemMetrics(SM_CXSCREEN) / 3, H = GetSystemMetrics(SM_CYSCREEN) / 3;
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST, L"MemoryCleanerDarkWnd", L"内存清理完成",
        WS_POPUP, (sw - W) / 2, (sh - H) / 2, W, H, NULL, NULL, hInst, NULL);
    if (!hwnd) return;
    HRGN rgn = CreateRoundRectRgn(0, 0, W + 1, H + 1, 44 * g_scale / 100, 44 * g_scale / 100);
    SetWindowRgn(hwnd, rgn, TRUE);
    g_hwnd = hwnd;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0)) { TranslateMessage(&m); DispatchMessageW(&m); }
}

/* ---------- 主逻辑 ---------- */

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    (void)hPrev; (void)lpCmd; (void)nShow;
    GdiplusStartupInput gsi; ZeroMemory(&gsi, sizeof(gsi)); gsi.GdiplusVersion = 1;
    ULONG_PTR gdipToken = 0;
    GdiplusStartup(&gdipToken, &gsi, NULL);
    g_before = AvailMB();

    g_isAdmin = FALSE;
    if (EnablePrivilege(L"SeDebugPrivilege", &g_isAdmin) && !g_isAdmin) g_isAdmin = FALSE;
    if (g_isAdmin) {
        BOOL dummy;
        EnablePrivilege(L"SeProfileSingleProcessPrivilege", &dummy);
        PurgeStandbyList();
    }

    g_ok = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe; pe.dwSize = sizeof(pe);
        if (Process32FirstW(snap, &pe)) {
            do {
                if (pe.th32ProcessID == GetCurrentProcessId()) continue;
                HANDLE h = OpenProcess(PROCESS_SET_QUOTA | PROCESS_QUERY_INFORMATION, FALSE, pe.th32ProcessID);
                if (h) {
                    SetProcessWorkingSetSize(h, (SIZE_T)-1, (SIZE_T)-1);
                    EmptyWorkingSet(h);
                    CloseHandle(h);
                    g_ok++;
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
    }

    g_after = AvailMB();
    g_freed = (long long)g_after - (long long)g_before;
    ShowResult(hInst);
    GdiplusShutdown(gdipToken);
    return 0;
}
