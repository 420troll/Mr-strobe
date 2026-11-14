#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <array>
#include <filesystem>
#include <string>

namespace {

constexpr int kArm = 100;
constexpr int kSave = 101;
constexpr int kStatus = 102;
constexpr int kFirstOutputControl = 200;

struct OutputControls {
    HWND enabled{};
    HWND blackout{};
    HWND flip_x{};
    HWND flip_y{};
    HWND scale_x{};
    HWND scale_y{};
    HWND offset_x{};
    HWND offset_y{};
};

HWND g_arm{};
HWND g_status{};
std::array<OutputControls, 2> g_outputs{};
std::filesystem::path g_ini;

std::filesystem::path executable_directory() {
    std::wstring buffer(32768, L'\0');
    buffer.resize(GetModuleFileNameW(nullptr, buffer.data(),
                                     static_cast<DWORD>(buffer.size())));
    return std::filesystem::path(buffer).parent_path();
}

HWND add(HWND parent, const wchar_t* kind, const wchar_t* text, DWORD style,
         int x, int y, int w, int h, int id) {
    return CreateWindowExW(0, kind, text, WS_CHILD | WS_VISIBLE | style,
                           x, y, w, h, parent,
                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                           GetModuleHandleW(nullptr), nullptr);
}

bool read_bool(const wchar_t* section, const wchar_t* key, bool fallback) {
    return GetPrivateProfileIntW(section, key, fallback ? 1 : 0,
                                 g_ini.c_str()) != 0;
}

std::wstring read_text(const wchar_t* section, const wchar_t* key,
                       const wchar_t* fallback) {
    wchar_t value[64]{};
    GetPrivateProfileStringW(section, key, fallback, value,
                             static_cast<DWORD>(std::size(value)), g_ini.c_str());
    return value;
}

void set_check(HWND control, bool checked) {
    SendMessageW(control, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

bool get_check(HWND control) {
    return SendMessageW(control, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

std::wstring get_text(HWND control) {
    const int size = GetWindowTextLengthW(control) + 1;
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    GetWindowTextW(control, result.data(), size);
    result.resize(wcslen(result.c_str()));
    return result;
}

void load() {
    set_check(g_arm, read_bool(L"mirror", L"armed", false));
    for (int i = 0; i < 2; ++i) {
        const std::wstring section = L"output" + std::to_wstring(i);
        auto& c = g_outputs[i];
        set_check(c.enabled, read_bool(section.c_str(), L"enabled", true));
        set_check(c.blackout, read_bool(section.c_str(), L"blackout", true));
        set_check(c.flip_x, read_bool(section.c_str(), L"flip_x", false));
        set_check(c.flip_y, read_bool(section.c_str(), L"flip_y", false));
        SetWindowTextW(c.scale_x, read_text(section.c_str(), L"scale_x", L"1.0").c_str());
        SetWindowTextW(c.scale_y, read_text(section.c_str(), L"scale_y", L"1.0").c_str());
        SetWindowTextW(c.offset_x, read_text(section.c_str(), L"offset_x", L"0").c_str());
        SetWindowTextW(c.offset_y, read_text(section.c_str(), L"offset_y", L"0").c_str());
    }
}

void write_value(const wchar_t* section, const wchar_t* key,
                 const std::wstring& value) {
    WritePrivateProfileStringW(section, key, value.c_str(), g_ini.c_str());
}

void save() {
    write_value(L"mirror", L"armed", get_check(g_arm) ? L"1" : L"0");
    for (int i = 0; i < 2; ++i) {
        const std::wstring section = L"output" + std::to_wstring(i);
        const auto& c = g_outputs[i];
        write_value(section.c_str(), L"enabled", get_check(c.enabled) ? L"1" : L"0");
        write_value(section.c_str(), L"blackout", get_check(c.blackout) ? L"1" : L"0");
        write_value(section.c_str(), L"flip_x", get_check(c.flip_x) ? L"1" : L"0");
        write_value(section.c_str(), L"flip_y", get_check(c.flip_y) ? L"1" : L"0");
        write_value(section.c_str(), L"scale_x", get_text(c.scale_x));
        write_value(section.c_str(), L"scale_y", get_text(c.scale_y));
        write_value(section.c_str(), L"offset_x", get_text(c.offset_x));
        write_value(section.c_str(), L"offset_y", get_text(c.offset_y));
    }
    SetWindowTextW(g_status, L"Saved. The bridge reloads within 500 ms.");
}

void make_output_panel(HWND window, int index, int x) {
    const std::wstring title = L"Helios output " + std::to_wstring(index);
    add(window, L"STATIC", title.c_str(), SS_LEFT, x, 72, 210, 22, 0);
    auto& c = g_outputs[index];
    int id = kFirstOutputControl + index * 20;
    c.enabled = add(window, L"BUTTON", L"Enabled", BS_AUTOCHECKBOX, x, 98, 90, 22, id++);
    c.blackout = add(window, L"BUTTON", L"Blackout", BS_AUTOCHECKBOX, x + 100, 98, 90, 22, id++);
    c.flip_x = add(window, L"BUTTON", L"Flip X", BS_AUTOCHECKBOX, x, 126, 90, 22, id++);
    c.flip_y = add(window, L"BUTTON", L"Flip Y", BS_AUTOCHECKBOX, x + 100, 126, 90, 22, id++);
    add(window, L"STATIC", L"Scale X", SS_LEFT, x, 158, 68, 20, 0);
    c.scale_x = add(window, L"EDIT", L"1.0", WS_BORDER | ES_AUTOHSCROLL, x + 72, 154, 116, 24, id++);
    add(window, L"STATIC", L"Scale Y", SS_LEFT, x, 188, 68, 20, 0);
    c.scale_y = add(window, L"EDIT", L"1.0", WS_BORDER | ES_AUTOHSCROLL, x + 72, 184, 116, 24, id++);
    add(window, L"STATIC", L"Offset X", SS_LEFT, x, 218, 68, 20, 0);
    c.offset_x = add(window, L"EDIT", L"0", WS_BORDER | ES_AUTOHSCROLL, x + 72, 214, 116, 24, id++);
    add(window, L"STATIC", L"Offset Y", SS_LEFT, x, 248, 68, 20, 0);
    c.offset_y = add(window, L"EDIT", L"0", WS_BORDER | ES_AUTOHSCROLL, x + 72, 244, 116, 24, id++);
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        add(window, L"STATIC", L"TROLLfx Helios Mirror V2", SS_LEFT, 20, 16, 360, 24, 0);
        g_arm = add(window, L"BUTTON", L"ARM OUTPUT (laser safety interlocks still required)",
                    BS_AUTOCHECKBOX, 20, 44, 390, 24, kArm);
        make_output_panel(window, 0, 20);
        make_output_panel(window, 1, 250);
        add(window, L"BUTTON", L"Save configuration", BS_DEFPUSHBUTTON,
            20, 292, 180, 30, kSave);
        g_status = add(window, L"STATIC", L"Disarmed and blacked out by default.",
                       SS_LEFT, 20, 334, 430, 36, kStatus);
        load();
        return 0;
    case WM_COMMAND:
        if (LOWORD(wparam) == kSave) {
            save();
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

}  // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show) {
    g_ini = executable_directory() / L"mirror.ini";
    WNDCLASSW wc{};
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"TrollfxHeliosMirrorV2Controller";
    RegisterClassW(&wc);
    HWND window = CreateWindowExW(
        0, wc.lpszClassName, L"TROLLfx Helios Mirror V2 Controller",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 420, nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    ShowWindow(window, show);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
