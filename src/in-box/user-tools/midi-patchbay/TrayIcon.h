// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midipatchbay
{
    struct TrayPatchItem
    {
        std::wstring PatchId{};
        std::wstring Name{};
        bool IsRouting{ false };
        bool HasWarning{ false };
        std::wstring Detail{};
    };

    // The notification area presence. Deliberately never created unless the customer turns the
    // setting on: a tool that quietly lives in the tray without being asked is not welcome.
    //
    // Owns its own hidden window rather than subclassing the XAML window, so nothing here can
    // disturb WinUI's own message handling.
    class TrayIcon
    {
    public:
        TrayIcon() noexcept = default;
        ~TrayIcon();

        TrayIcon(TrayIcon const&) = delete;
        TrayIcon& operator=(TrayIcon const&) = delete;

        // All callbacks are raised on the thread that created the icon, which is the UI thread.
        void SetOpenHandler(_In_ std::function<void()> handler) noexcept { m_onOpen = std::move(handler); }
        void SetExitHandler(_In_ std::function<void()> handler) noexcept { m_onExit = std::move(handler); }
        void SetStopAllHandler(_In_ std::function<void()> handler) noexcept { m_onStopAll = std::move(handler); }
        void SetTogglePatchHandler(_In_ std::function<void(std::wstring const&)> handler) noexcept
        {
            m_onTogglePatch = std::move(handler);
        }

        bool Show() noexcept;
        void Hide() noexcept;

        bool IsVisible() const noexcept { return m_visible; }

        // Tooltip and menu content. Safe to call often; the icon is only updated when the text
        // actually changed, because Shell_NotifyIcon is not free.
        void Update(_In_ std::wstring const& tooltip, _In_ std::vector<TrayPatchItem> items) noexcept;

    private:
        static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) noexcept;

        // Takes the handle rather than reading m_window, which is still null while the window
        // is being created and would make the default handling of WM_NCCREATE fail the create.
        LRESULT HandleMessage(_In_ HWND window, _In_ UINT message, _In_ WPARAM wParam, _In_ LPARAM lParam) noexcept;

        void ShowContextMenu() noexcept;

        bool EnsureWindow() noexcept;

        HWND m_window{ nullptr };
        bool m_visible{ false };

        std::wstring m_tooltip{};
        std::vector<TrayPatchItem> m_items{};

        std::function<void()> m_onOpen{};
        std::function<void()> m_onExit{};
        std::function<void()> m_onStopAll{};
        std::function<void(std::wstring const&)> m_onTogglePatch{};
    };
}
