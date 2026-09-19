// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

namespace
{
    constexpr UINT WM_MIDI_TRAY_ICON        { WM_APP + 1 };
    constexpr UINT WM_MIDI_SIGNAL_CHANGED   { WM_APP + 2 };
    constexpr UINT WM_MIDI_SETTINGS_CHANGED { WM_APP + 3 };

    constexpr UINT_PTR TimerIdEvaluate      { 1 };

    // A host coming up reports every waiting remote in quick succession. Waiting a moment before
    // asking the service turns that into one question and one banner.
    constexpr UINT DebounceMilliseconds     { 1500 };

    constexpr UINT MenuCommandOpenNetworkSetup { 1001 };
    constexpr UINT MenuCommandExit             { 1002 };

    // Per session, not machine wide. Every logged on customer gets their own notifications, so a
    // Global name here would silently leave all but the first user without any.
    constexpr wchar_t SingleInstanceKey[]{ MIDI_NOTIFICATIONS_INSTANCE_KEY };

    constexpr wchar_t WindowClassName[]{ L"MidiNotificationsMessageWindow" };

    NetworkApprovalNotifier g_networkNotifier{ };
    RegistryChangeWatcher g_signalWatcher{ };
    RegistryChangeWatcher g_settingsWatcher{ };

    NOTIFYICONDATAW g_trayIcon{ };
    bool g_trayIconAdded{ false };
    bool g_trayIconOwnsIcon{ false };

    std::wstring LoadAppString(_In_ UINT const id) noexcept
    {
        wchar_t buffer[256]{ };

        auto const length = ::LoadStringW(::GetModuleHandleW(nullptr), id, buffer, ARRAYSIZE(buffer));

        return length > 0 ? std::wstring{ buffer, static_cast<size_t>(length) } : std::wstring{ };
    }

    void AddTrayIcon(_In_ HWND const window) noexcept
    {
        g_trayIcon = { };
        g_trayIcon.cbSize = sizeof(g_trayIcon);
        g_trayIcon.hWnd = window;
        g_trayIcon.uID = 1;
        g_trayIcon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        g_trayIcon.uCallbackMessage = WM_MIDI_TRAY_ICON;

        // The notification area draws at the small icon metric. LoadIcon would return the 32
        // pixel entry for the shell to shrink, which is visibly soft; this picks the entry which
        // matches, so the 16, 20 and 24 pixel art in the .ico is what gets used.
        g_trayIcon.hIcon = static_cast<HICON>(::LoadImageW(
            ::GetModuleHandleW(nullptr),
            MAKEINTRESOURCEW(IDI_APPICON),
            IMAGE_ICON,
            ::GetSystemMetrics(SM_CXSMICON),
            ::GetSystemMetrics(SM_CYSMICON),
            LR_DEFAULTCOLOR));

        if (g_trayIcon.hIcon == nullptr)
        {
            g_trayIcon.hIcon = ::LoadIconW(nullptr, IDI_APPLICATION);
            g_trayIconOwnsIcon = false;
        }
        else
        {
            g_trayIconOwnsIcon = true;
        }

        auto const tooltip = LoadAppString(IDS_TRAY_TOOLTIP);
        ::wcsncpy_s(g_trayIcon.szTip, tooltip.c_str(), _TRUNCATE);

        g_trayIconAdded = ::Shell_NotifyIconW(NIM_ADD, &g_trayIcon) != FALSE;
    }

    void RemoveTrayIcon() noexcept
    {
        if (g_trayIconAdded)
        {
            ::Shell_NotifyIconW(NIM_DELETE, &g_trayIcon);
            g_trayIconAdded = false;
        }

        // LoadImage hands back a handle we own, unlike the shared one LoadIcon returns for the
        // IDI_APPLICATION fallback.
        if (g_trayIconOwnsIcon && g_trayIcon.hIcon != nullptr)
        {
            ::DestroyIcon(g_trayIcon.hIcon);
            g_trayIcon.hIcon = nullptr;
            g_trayIconOwnsIcon = false;
        }
    }

    // False when the Network MIDI package is not installed, which is a supported state: this app
    // ships with the tools rather than with the transport. Nothing here fails in that case, there
    // is simply nothing to tell the customer about and nowhere to send them.
    bool IsNetworkTransportPresent() noexcept
    {
        try
        {
            return ::winrt::Windows::Devices::Midi2::Transports::Network::
                MidiNetworkTransportManager::IsTransportAvailable();
        }
        catch (...)
        {
            return false;
        }
    }

    void OpenNetworkSetup() noexcept
    {
        // The same entry point the toast button uses. It only navigates: nothing is approved
        // without the customer doing it in the app.
        ::ShellExecuteW(nullptr, L"open", MIDI_NETWORK_SETUP_PROTOCOL_URI_PENDING, nullptr, nullptr, SW_SHOWNORMAL);
    }

    void ShowTrayMenu(_In_ HWND const window) noexcept
    {
        auto menu = ::CreatePopupMenu();

        if (menu == nullptr)
        {
            return;
        }

        ::AppendMenuW(
            menu,
            IsNetworkTransportPresent() ? MF_STRING : (MF_STRING | MF_GRAYED),
            MenuCommandOpenNetworkSetup,
            LoadAppString(IDS_MENU_OPEN_NETWORK_SETUP).c_str());
        ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        ::AppendMenuW(menu, MF_STRING, MenuCommandExit, LoadAppString(IDS_MENU_EXIT).c_str());

        POINT cursor{ };
        ::GetCursorPos(&cursor);

        // Required for a tray menu to dismiss when the customer clicks elsewhere.
        ::SetForegroundWindow(window);

        ::TrackPopupMenu(menu, TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, window, nullptr);

        ::DestroyMenu(menu);
    }

    LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept
    {
        switch (message)
        {
        case WM_MIDI_SIGNAL_CHANGED:
        case WM_MIDI_SETTINGS_CHANGED:
            // Restarted on every hint, so a burst collapses into one evaluation.
            ::SetTimer(window, TimerIdEvaluate, DebounceMilliseconds, nullptr);
            return 0;

        case WM_TIMER:
            if (wparam == TimerIdEvaluate)
            {
                ::KillTimer(window, TimerIdEvaluate);

                if (!AppSettings::NotificationsEnabled())
                {
                    // The customer turned the app off. Leaving it resident would mean the switch
                    // did not do what it says.
                    ::DestroyWindow(window);
                    return 0;
                }

                g_networkNotifier.Evaluate();
            }
            return 0;

        case WM_MIDI_TRAY_ICON:
            if (LOWORD(lparam) == WM_RBUTTONUP || LOWORD(lparam) == WM_CONTEXTMENU)
            {
                ShowTrayMenu(window);
            }
            else if (LOWORD(lparam) == WM_LBUTTONUP)
            {
                if (IsNetworkTransportPresent())
                {
                    OpenNetworkSetup();
                }
            }
            return 0;

        case WM_COMMAND:
            switch (LOWORD(wparam))
            {
            case MenuCommandOpenNetworkSetup:
                OpenNetworkSetup();
                return 0;

            case MenuCommandExit:
                ::DestroyWindow(window);
                return 0;

            default:
                break;
            }
            break;

        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;

        default:
            break;
        }

        return ::DefWindowProcW(window, message, wparam, lparam);
    }
}

int APIENTRY wWinMain(
    _In_ HINSTANCE instance,
    _In_opt_ HINSTANCE,
    _In_ LPWSTR,
    _In_ int)
{
    // There is no window to bring forward, so a second copy just leaves. The shared helper is
    // used anyway so every MIDI tool answers "am I already running" the same way.
    if (!::midiapp::SingleInstance::AcquireOrActivateExisting(SingleInstanceKey))
    {
        return 0;
    }

    // Checked before anything is created. The Run entry is machine wide, so this is how a
    // customer who wants nothing from us ends up with nothing running.
    if (!AppSettings::NotificationsEnabled())
    {
        ::midiapp::SingleInstance::Release();
        return 0;
    }
    winrt::init_apartment();

    WNDCLASSEXW windowClass{ };
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = WindowClassName;

    if (::RegisterClassExW(&windowClass) == 0)
    {
        return 1;
    }

    // Never shown. It exists to own the tray icon and to give the watchers somewhere to post, so
    // that all the deciding happens on one thread.
    auto const window = ::CreateWindowExW(
        0, WindowClassName, LoadAppString(IDS_APP_TITLE).c_str(),
        0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, instance, nullptr);

    if (window == nullptr)
    {
        return 1;
    }

    AddTrayIcon(window);

    // A failure here is not fatal. The tray icon still works and the customer can still open the
    // setup app; they just will not be told when something starts waiting.
    LOG_IF_FAILED(g_signalWatcher.StartVolatile(
        HKEY_LOCAL_MACHINE,
        MIDI_NOTIFICATION_SIGNAL_ROOT_REG_KEY,
        MIDI_NOTIFICATION_SIGNAL_SUBKEY_NAME,
        [window]() { ::PostMessageW(window, WM_MIDI_SIGNAL_CHANGED, 0, 0); }));

    LOG_IF_FAILED(g_settingsWatcher.StartPersistent(
        HKEY_CURRENT_USER,
        MIDI_NOTIFICATIONS_SETTINGS_REG_KEY,
        [window]() { ::PostMessageW(window, WM_MIDI_SETTINGS_CHANGED, 0, 0); }));

    // Something may already have been waiting since before this session signed in, so the first
    // look is taken without waiting for a change. It goes through the same debounce, which keeps
    // logon from racing the service still starting up.
    ::SetTimer(window, TimerIdEvaluate, DebounceMilliseconds, nullptr);

    MSG message{ };

    while (::GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }

    g_signalWatcher.Stop();
    g_settingsWatcher.Stop();

    RemoveTrayIcon();

    ::midiapp::SingleInstance::Release();

    return 0;
}
