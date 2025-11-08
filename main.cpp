#include <SDL3/SDL.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#include <windows.h>
#include <dwmapi.h>
#include <vector>
#include <string>
#include <iostream>

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define HOTKEY_ID 1

/* Application State */

struct HiddenWindow {
    HWND hwnd;
    std::string title;
};

class AppState {
public:
    // Window management
    std::vector<HiddenWindow> hiddenWindows;
    bool selectingWindow = false;
    bool guiVisible = true;
    bool shouldExit = false;
    bool openGUIbyHotKey = false;
    int current_window_index = 0;

    // SDL
    SDL_Window* sdlWindow = nullptr;
    SDL_Renderer* renderer = nullptr;

    // Windows handles
    HWND hotkeyHwnd = nullptr;
    HWND trayHwnd = nullptr;
    NOTIFYICONDATAA trayIcon = {};

    // Hooks and cursors
    HHOOK mouseHook = nullptr;
    HHOOK keyboardHook = nullptr;
    HCURSOR crosshairCursor = nullptr;
    HCURSOR originalCursor = nullptr;

    void CleanupHooks()
    {
        if (mouseHook) { UnhookWindowsHookEx(mouseHook); mouseHook = nullptr; }
        if (keyboardHook) { UnhookWindowsHookEx(keyboardHook); keyboardHook = nullptr; }
    }

    void CleanupMouseHooks()
    {
        if (mouseHook) { UnhookWindowsHookEx(mouseHook); mouseHook = nullptr; }
    }

    void CleanupKeyboardHooks()
    {
        if (keyboardHook) { UnhookWindowsHookEx(keyboardHook); keyboardHook = nullptr; }
    }

    void RestoreAllWindows()
    {
        for (const auto& hw : hiddenWindows) if (IsWindow(hw.hwnd)) RestoreToAltTab(hw.hwnd);
        hiddenWindows.clear();
    }

private:
    void RestoreToAltTab(HWND hwnd)
    {
        LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_TOOLWINDOW); // remove WS_EX_TOOLWINDOW only
    }
};

AppState g_app;

/* Window Utility Functions */

// show window
void ShowGUI(bool show)
{
    if (!g_app.sdlWindow) return;
    
    if (show) {
        SDL_ShowWindow(g_app.sdlWindow);
        SDL_RaiseWindow(g_app.sdlWindow);
    } else {
        SDL_HideWindow(g_app.sdlWindow);
        g_app.CleanupHooks();
        g_app.openGUIbyHotKey = false;
    }
    g_app.guiVisible = show;
}

void OpenWindow(int window_index)
{
    std::cout << "open window:" << window_index << std::endl;
    if (!g_app.hiddenWindows.empty() 
        && window_index >=0 && window_index < g_app.hiddenWindows.size()) {
        HWND targetHwnd = g_app.hiddenWindows[window_index].hwnd;
        if (IsWindow(targetHwnd)) {
            ShowWindow(targetHwnd, SW_SHOWMAXIMIZED);
            SetForegroundWindow(targetHwnd);
        }
    }
}

bool IsAltTabWindow(HWND hwnd)
{
    if (!IsWindowVisible(hwnd)) return false;
    
    HWND hwndWalk = GetAncestor(hwnd, GA_ROOTOWNER);
    HWND hwndTry;
    while ((hwndTry = GetLastActivePopup(hwndWalk)) != hwndWalk) {
        if (IsWindowVisible(hwndTry)) break;
        hwndWalk = hwndTry;
    }
    if (hwndWalk != hwnd) return false;

    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (exStyle & (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE)) return false;

    BOOL isCloaked = FALSE;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &isCloaked, sizeof(isCloaked)))) {
        if (isCloaked) return false;
    }

    return true;
}

void HideFromAltTab(HWND hwnd)
{
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_TOOLWINDOW; // add WS_EX_TOOLWINDOW
    exStyle &= ~WS_EX_APPWINDOW;  // remove WS_EX_APPWINDOW
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
}

std::string GetWindowTitle(HWND hwnd)
{
    char title[256];
    int len = GetWindowTextA(hwnd, title, sizeof(title));
    return len > 0 ? std::string(title) : std::string();
}

/* Selection Mode */

void CancelSelectionMode()
{
    if (!g_app.selectingWindow) return;
    
    g_app.selectingWindow = false;
    SetCursor(g_app.originalCursor);
    g_app.CleanupHooks();
}

void StartSelectionMode()
{
    if (g_app.selectingWindow) return;
    
    g_app.selectingWindow = true;
    g_app.originalCursor = GetCursor();
    // SetCursor(g_app.crosshairCursor);
}

void SelectWindowAtPoint(POINT pt)
{
    HWND hwnd = WindowFromPoint(pt);
    if (!hwnd) return;
    
    // get root window
    hwnd = GetAncestor(hwnd, GA_ROOT);
    
    if (!IsAltTabWindow(hwnd)) {
        std::cout << "Window is not in Alt+Tab list" << std::endl;
        return;
    }
    
    std::string title = GetWindowTitle(hwnd);
    if (title.empty()) return;
    g_app.hiddenWindows.push_back({hwnd, title});

    HideFromAltTab(hwnd);
    std::cout << "Hidden window: " << title << std::endl;
}

/* Hook Callbacks */

LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && g_app.selectingWindow) {
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            KBDLLHOOKSTRUCT* pkb = (KBDLLHOOKSTRUCT*)lParam;
            if (pkb->vkCode == VK_ESCAPE) {
                CancelSelectionMode();
                return 1;
            }
        }
    } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
        KBDLLHOOKSTRUCT* pkb = (KBDLLHOOKSTRUCT*)lParam;
        std::cout<< "hotket" << std::endl;

        if (pkb->vkCode == VK_MENU && g_app.openGUIbyHotKey) {
            OpenWindow(g_app.current_window_index);
            return 1;
        }
    }
    return CallNextHookEx(g_app.keyboardHook, nCode, wParam, lParam);
}

LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && g_app.selectingWindow) {
        if (wParam == WM_LBUTTONDOWN) {
            MSLLHOOKSTRUCT* pMouseStruct = (MSLLHOOKSTRUCT*)lParam;
            SelectWindowAtPoint(pMouseStruct->pt);
            CancelSelectionMode();
            return 1;
        }
    }
    return CallNextHookEx(g_app.mouseHook, nCode, wParam, lParam);
}

// keyboard used esc to exit selectmode, mouse used select window, cursor used change style 
void InstallHooks()
{
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    g_app.mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, hInstance, 0);
    g_app.keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookProc, hInstance, 0);
}

/* Hotkey System */

// use a window to handle hotkey make structure clear
LRESULT CALLBACK HotkeyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_HOTKEY && wParam == HOTKEY_ID) {
        if (!g_app.hiddenWindows.empty()) {
            if(g_app.guiVisible == true) {
                g_app.current_window_index += 1;
                if(g_app.current_window_index == g_app.hiddenWindows.size()) {
                    g_app.current_window_index = 0;
                }
            }
            ShowGUI(true);
            g_app.openGUIbyHotKey = true;
            std::cout << g_app.current_window_index << std::endl;
        }
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void InitHotkeyWindow()
{
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = HotkeyWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "HotkeyWindow";
    RegisterClassA(&wc);
    
    g_app.hotkeyHwnd = CreateWindowA("HotkeyWindow", "Hotkey", 0, 0, 0, 0, 0, 
                HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), nullptr);
    if (g_app.hotkeyHwnd) RegisterHotKey(g_app.hotkeyHwnd, HOTKEY_ID, MOD_ALT | MOD_NOREPEAT, VK_OEM_6);
}

void CleanupHotkeyWindow()
{
    if (g_app.hotkeyHwnd) {
        UnregisterHotKey(g_app.hotkeyHwnd, HOTKEY_ID);
        DestroyWindow(g_app.hotkeyHwnd);
        g_app.hotkeyHwnd = nullptr;
    }
}

/* System Tray */

// Tray Message Proc
LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_TRAYICON:
            if (lParam == WM_LBUTTONDBLCLK) {
                ShowGUI(!g_app.guiVisible);
            } else if (lParam == WM_RBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                HMENU hMenu = CreatePopupMenu();
                AppendMenuA(hMenu, MF_STRING, ID_TRAY_EXIT, "Exit");
                SetForegroundWindow(hwnd);
                TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, nullptr);
                DestroyMenu(hMenu);
            }
            break;
            
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_TRAY_EXIT) g_app.shouldExit = true;
            break;
            
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void InitTrayIcon()
{
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = TrayWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "TrayWindow";
    RegisterClassA(&wc);
    
    g_app.trayHwnd = CreateWindowA("TrayWindow", "Tray", 0, 0, 0, 0, 0, 
                                    HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), nullptr);
    
    g_app.trayIcon.cbSize = sizeof(NOTIFYICONDATAA);
    g_app.trayIcon.hWnd = g_app.trayHwnd;
    g_app.trayIcon.uID = 1;
    g_app.trayIcon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_app.trayIcon.uCallbackMessage = WM_TRAYICON;
    g_app.trayIcon.hIcon = LoadIcon(nullptr, IDI_APPLICATION);

    strcpy_s(g_app.trayIcon.szTip, "Window Manager");
    
    Shell_NotifyIconA(NIM_ADD, &g_app.trayIcon);
}

void CleanupTrayIcon()
{
    Shell_NotifyIconA(NIM_DELETE, &g_app.trayIcon);
    if (g_app.trayHwnd) {
        DestroyWindow(g_app.trayHwnd);
        g_app.trayHwnd = nullptr;
    }
}

/* GUI Rendering */

// restore last hidden window
void RestoreLastWindow()
{
    if (g_app.hiddenWindows.empty()) return;
    
    HiddenWindow hw = g_app.hiddenWindows.back();
    if (IsWindow(hw.hwnd)) {
        LONG exStyle = GetWindowLong(hw.hwnd, GWL_EXSTYLE);
        SetWindowLong(hw.hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_TOOLWINDOW);
    }
    g_app.hiddenWindows.pop_back();
    
    if (g_app.hiddenWindows.empty()) {
        g_app.current_window_index = 0;
    } else if (g_app.current_window_index >= (int)g_app.hiddenWindows.size()) {
        g_app.current_window_index = (int)g_app.hiddenWindows.size() - 1;
    }
}

// render a frame
void RenderGUI()
{
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("Window Manager", nullptr, ImGuiWindowFlags_NoCollapse);

    int windowW = ImGui::GetIO().DisplaySize.x;
    int windowH = ImGui::GetIO().DisplaySize.y;
    int buttonW = windowW * 0.9 / 3;
    int buttonH = windowH * 0.1;

    if (ImGui::Button("Select Window to Hide", ImVec2(buttonW, buttonH))) {
        StartSelectionMode();
        InstallHooks();
    }

    ImGui::SameLine();
    if (ImGui::Button("Restore Last Window", ImVec2(buttonW, buttonH))) {
        RestoreLastWindow();
    }

    ImGui::Separator();
    ImGui::Text("Hidden Windows (Press Alt+] to maximize first):");
    ImGui::Separator();

    if (g_app.hiddenWindows.empty()) {
        ImGui::TextDisabled("No hidden windows");
    } else {
        for (size_t i = 0; i < g_app.hiddenWindows.size(); i++) {
            if (i == g_app.current_window_index) {
                ImVec2 p0 = ImGui::GetCursorScreenPos();
                ImVec2 p1 = ImVec2(p0.x + ImGui::GetContentRegionAvail().x, p0.y + ImGui::GetTextLineHeightWithSpacing());
                ImGui::GetWindowDrawList()->AddRectFilled(p0, p1, IM_COL32(60, 100, 160, 120)); // 半透明蓝色背景
            }
            ImGui::Text("%zu. %s", i + 1, g_app.hiddenWindows[i].title.c_str());
        }
    }

    ImGui::Separator();
    ImGui::End();

    ImGui::Render();
    SDL_SetRenderDrawColor(g_app.renderer, 20, 20, 20, 255);
    SDL_RenderClear(g_app.renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), g_app.renderer);
    SDL_RenderPresent(g_app.renderer);
}

/* Main App */

// reate window
bool InitSDL()
{
    if (!SDL_Init(SDL_INIT_VIDEO)) return false;

    g_app.sdlWindow = SDL_CreateWindow("Window Manager", 1200, 600, SDL_WINDOW_RESIZABLE);
    if (!g_app.sdlWindow) { SDL_Quit(); return false; }

    g_app.renderer = SDL_CreateRenderer(g_app.sdlWindow, nullptr);
    if (!g_app.renderer) { 
        SDL_DestroyWindow(g_app.sdlWindow); 
        SDL_Quit(); 
        return false; 
    }

    return true;
}

// set font and style
void InitImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = 1.5f;
    io.Fonts->AddFontFromFileTTF("./SarasaTermSCNerd-Regular.ttf", 15, nullptr, io.Fonts->GetGlyphRangesChineseFull());
    
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(g_app.sdlWindow, g_app.renderer);
    ImGui_ImplSDLRenderer3_Init(g_app.renderer);
}

// used when window shown
void ProcessEvents()
{
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        
        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            ShowGUI(false);
        } else if (event.type == SDL_EVENT_QUIT) {
            if (g_app.shouldExit) return;
            ShowGUI(false);
        }
    }

    if (g_app.selectingWindow) {
        SetCursor(g_app.crosshairCursor);
    }

    if (g_app.openGUIbyHotKey && (GetAsyncKeyState(VK_MENU) & 0x8000) == 0) {
        // Alt release
        OpenWindow(g_app.current_window_index);
        ShowGUI(false);
    }

}

// clean Tray SDL ImGui Hotkey Hook
void Cleanup()
{
    g_app.RestoreAllWindows();
    g_app.CleanupHooks();
    CleanupHotkeyWindow();
    CleanupTrayIcon();
    
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    
    SDL_DestroyRenderer(g_app.renderer);
    SDL_DestroyWindow(g_app.sdlWindow);
    SDL_Quit();
}

int main(int argc, char* argv[])
{
    InitSDL();
    InitImGui();
    InitHotkeyWindow();
    
    g_app.originalCursor = LoadCursor(nullptr, IDC_ARROW);
    g_app.crosshairCursor = LoadCursor(nullptr, IDC_CROSS);
    
    InitTrayIcon();

    while (!g_app.shouldExit) {
        ProcessEvents();
        
        if (g_app.guiVisible) {
            RenderGUI();
        } else {
            MSG msg;
            BOOL ret = GetMessage(&msg, nullptr, 0, 0);
            if (ret > 0) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            } else if (ret == 0) {
                break;
            }
        }
    }

    Cleanup();
    return 0;
}
