// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "TrayIcon.h"
#include "StringResources.h"
#include "resource.h"

namespace midipatchbay
{
    namespace
    {
        constexpr wchar_t WindowClassName[] = L"WindowsMidiPatchbayTrayWindow";

        constexpr UINT TrayCallbackMessage = WM_APP + 1;

        constexpr UINT CommandOpen = 1;
        constexpr UINT CommandExit = 2;
        constexpr UINT CommandStopAll = 3;
        constexpr UINT CommandFirstPatch = 100;

        constexpr UINT TrayIconId = 1;

        constexpr size_t MaximumMenuPatches = 40;
    }

    TrayIcon::~TrayIcon()
    {
        Hide();

        if (m_window != nullptr)
        {
            ::DestroyWindow(m_window);
            m_window = nullptr;
        }
    }

    _Use_decl_annotations_
    LRESULT CALLBACK TrayIcon::WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) noexcept
    {
        if (message == WM_NCCREATE)
        {
            auto const* create = reinterpret_cast<CREATESTRUCTW const*>(lParam);

            ::SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        }

        auto* self = reinterpret_cast<TrayIcon*>(::GetWindowLongPtrW(window, GWLP_USERDATA));

        if (self != nullptr)
        {
            return self->HandleMessage(message, wParam, lParam);
        }

        return ::DefWindowProcW(window, message, wParam, lParam);
    }

    _Use_decl_annotations_
    LRESULT TrayIcon::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) noexcept
    {
        try
        {
            switch (message)
            {
            case TrayCallbackMessage:
                switch (LOWORD(lParam))
                {
                case WM_LBUTTONUP:
                    if (m_onOpen)
                    {
                        m_onOpen();
                    }
                    return 0;

                case WM_RBUTTONUP:
                case WM_CONTEXTMENU:
                    ShowContextMenu();
                    return 0;

                default:
                    break;
                }
                break;

            case WM_COMMAND:
            {
                auto const command = static_cast<UINT>(LOWORD(wParam));

                if (command == CommandOpen && m_onOpen)
                {
                    m_onOpen();
                }
                else if (command == CommandExit && m_onExit)
                {
                    m_onExit();
                }
                else if (command == CommandStopAll && m_onStopAll)
                {
                    m_onStopAll();
                }
                else if (command >= CommandFirstPatch)
                {
                    auto const index = static_cast<size_t>(command - CommandFirstPatch);

                    if (index < m_items.size() && m_onTogglePatch)
                    {
                        m_onTogglePatch(m_items[index].PatchId);
                    }
                }

                return 0;
            }

            default:
                break;
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"A notification area message handler failed.")

        return ::DefWindowProcW(m_window, message, wParam, lParam);
    }

    bool TrayIcon::EnsureWindow() noexcept
    {
        if (m_window != nullptr)
        {
            return true;
        }

        WNDCLASSEXW windowClass{};

        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = &TrayIcon::WindowProcedure;
        windowClass.hInstance = ::GetModuleHandleW(nullptr);
        windowClass.lpszClassName = WindowClassName;

        // a second registration of the same class is not an error here, it just means the class
        // survived a previous instance of this object
        ::RegisterClassExW(&windowClass);

        m_window = ::CreateWindowExW(
            0, WindowClassName, WindowClassName, 0,
            0, 0, 0, 0,
            HWND_MESSAGE, nullptr, ::GetModuleHandleW(nullptr), this);

        return m_window != nullptr;
    }

    bool TrayIcon::Show() noexcept
    {
        try
        {
            if (m_visible)
            {
                return true;
            }

            if (!EnsureWindow())
            {
                return false;
            }

            NOTIFYICONDATAW data{};

            data.cbSize = sizeof(data);
            data.hWnd = m_window;
            data.uID = TrayIconId;
            data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
            data.uCallbackMessage = TrayCallbackMessage;
            data.hIcon = static_cast<HICON>(::LoadImageW(
                ::GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON,
                ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), 0));

            auto const tooltip = m_tooltip.empty()
                ? std::wstring{ resources::GetString(L"AppDisplayName") }
                : m_tooltip;

            ::wcsncpy_s(data.szTip, ARRAYSIZE(data.szTip), tooltip.c_str(), _TRUNCATE);

            if (!::Shell_NotifyIconW(NIM_ADD, &data))
            {
                return false;
            }

            data.uVersion = NOTIFYICON_VERSION_4;
            ::Shell_NotifyIconW(NIM_SETVERSION, &data);

            m_visible = true;

            return true;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the notification area icon.")

        return false;
    }

    void TrayIcon::Hide() noexcept
    {
        try
        {
            if (!m_visible || m_window == nullptr)
            {
                return;
            }

            NOTIFYICONDATAW data{};

            data.cbSize = sizeof(data);
            data.hWnd = m_window;
            data.uID = TrayIconId;

            ::Shell_NotifyIconW(NIM_DELETE, &data);

            m_visible = false;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to remove the notification area icon.")
    }

    _Use_decl_annotations_
    void TrayIcon::Update(std::wstring const& tooltip, std::vector<TrayPatchItem> items) noexcept
    {
        try
        {
            m_items = std::move(items);

            if (tooltip == m_tooltip)
            {
                return;
            }

            m_tooltip = tooltip;

            if (!m_visible || m_window == nullptr)
            {
                return;
            }

            NOTIFYICONDATAW data{};

            data.cbSize = sizeof(data);
            data.hWnd = m_window;
            data.uID = TrayIconId;
            data.uFlags = NIF_TIP | NIF_SHOWTIP;

            ::wcsncpy_s(data.szTip, ARRAYSIZE(data.szTip), m_tooltip.c_str(), _TRUNCATE);

            ::Shell_NotifyIconW(NIM_MODIFY, &data);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to update the notification area icon.")
    }

    void TrayIcon::ShowContextMenu() noexcept
    {
        try
        {
            if (m_window == nullptr)
            {
                return;
            }

            wil::unique_hmenu menu{ ::CreatePopupMenu() };

            if (!menu)
            {
                return;
            }

            auto const header = m_items.empty()
                ? std::wstring{ resources::GetString(L"TrayNoPatches") }
                : std::wstring{ resources::FormatString(L"TrayHeaderFormat", m_items.size()) };

            ::AppendMenuW(menu.get(), MF_STRING | MF_DISABLED | MF_GRAYED, 0, header.c_str());
            ::AppendMenuW(menu.get(), MF_SEPARATOR, 0, nullptr);

            auto const count = std::min(m_items.size(), MaximumMenuPatches);

            for (size_t i = 0; i < count; i++)
            {
                auto const& item = m_items[i];

                auto text = item.Name;

                if (!item.Detail.empty())
                {
                    text += L"\t" + item.Detail;
                }

                UINT flags = MF_STRING;

                if (item.IsRouting)
                {
                    flags |= MF_CHECKED;
                }

                ::AppendMenuW(menu.get(), flags, CommandFirstPatch + i, text.c_str());
            }

            if (count > 0)
            {
                ::AppendMenuW(menu.get(), MF_SEPARATOR, 0, nullptr);
                ::AppendMenuW(menu.get(), MF_STRING, CommandStopAll,
                    std::wstring{ resources::GetString(L"TrayStopAll") }.c_str());
            }

            ::AppendMenuW(menu.get(), MF_STRING, CommandOpen,
                std::wstring{ resources::GetString(L"TrayOpen") }.c_str());
            ::AppendMenuW(menu.get(), MF_STRING, CommandExit,
                std::wstring{ resources::GetString(L"TrayExit") }.c_str());

            POINT cursor{};
            ::GetCursorPos(&cursor);

            // without this the menu does not dismiss when the customer clicks elsewhere
            ::SetForegroundWindow(m_window);

            ::TrackPopupMenuEx(
                menu.get(), TPM_RIGHTBUTTON | TPM_RIGHTALIGN | TPM_BOTTOMALIGN,
                cursor.x, cursor.y, m_window, nullptr);

            ::PostMessageW(m_window, WM_NULL, 0, 0);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the notification area menu.")
    }
}
