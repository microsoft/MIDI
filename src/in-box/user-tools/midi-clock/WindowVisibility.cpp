// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "WindowVisibility.h"

#include <dwmapi.h>
#include <powrprof.h>

namespace midiclock
{
    namespace
    {
        // Written from a system thread. Only static storage is touched there, so a notification
        // that arrives late cannot reach a window that has already gone.
        std::atomic<bool> s_displayOn{ true };
        HPOWERNOTIFY s_displayPowerNotification{ nullptr };

        ULONG CALLBACK OnDisplayPowerChanged(_In_opt_ PVOID, _In_ ULONG type, _In_ PVOID setting) noexcept
        {
            auto const change = static_cast<POWERBROADCAST_SETTING const*>(setting);

            if (type == PBT_POWERSETTINGCHANGE &&
                change != nullptr &&
                change->PowerSetting == GUID_SESSION_DISPLAY_STATUS &&
                change->DataLength >= sizeof(DWORD))
            {
                DWORD state{ 0 };
                memcpy(&state, change->Data, sizeof(state));

                // 0 is off, 1 on and 2 dimmed, and a dimmed display can still be read
                s_displayOn.store(state != 0);
            }

            return ERROR_SUCCESS;
        }

        DEVICE_NOTIFY_SUBSCRIBE_PARAMETERS s_displayPowerSubscription{ &OnDisplayPowerChanged, nullptr };

        bool IsCloaked(_In_ HWND window) noexcept
        {
            DWORD cloaked{ 0 };

            return SUCCEEDED(::DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked != 0;
        }

        // What a person sees of the window, without the invisible resize border and the shadow.
        bool TryGetVisibleBounds(_In_ HWND window, _Out_ RECT& bounds) noexcept
        {
            bounds = {};

            if (FAILED(::DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &bounds, sizeof(bounds))) &&
                !::GetWindowRect(window, &bounds))
            {
                return false;
            }

            return !::IsRectEmpty(&bounds);
        }

        // Only a window that certainly hides what is behind it. Anything that may be see-through
        // or is only there for a moment is left out, so any doubt comes down on "still visible".
        bool TryGetCover(_In_ HWND window, _Out_ RECT& cover) noexcept
        {
            cover = {};

            if (!::IsWindowVisible(window) || ::IsIconic(window))
            {
                return false;
            }

            auto const style = ::GetWindowLongW(window, GWL_STYLE);
            auto const extendedStyle = ::GetWindowLongW(window, GWL_EXSTYLE);

            // menus, tooltips, overlays and floating palettes
            if ((style & WS_POPUP) != 0 || (extendedStyle & (WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW)) != 0)
            {
                return false;
            }

            if ((extendedStyle & WS_EX_LAYERED) != 0)
            {
                BYTE alpha{ 0 };
                DWORD flags{ 0 };

                // This fails for a window drawn with UpdateLayeredWindow, which can be clear anywhere.
                if (!::GetLayeredWindowAttributes(window, nullptr, &alpha, &flags) ||
                    (flags & LWA_COLORKEY) != 0 ||
                    ((flags & LWA_ALPHA) != 0 && alpha < 255))
                {
                    return false;
                }
            }

            wil::unique_hrgn shape{ ::CreateRectRgn(0, 0, 0, 0) };

            if (shape == nullptr || ::GetWindowRgn(window, shape.get()) == COMPLEXREGION)
            {
                return false;
            }

            return TryGetVisibleBounds(window, cover) && !IsCloaked(window);
        }

        BOOL CALLBACK AddMonitorArea(HMONITOR, HDC, LPRECT area, LPARAM region) noexcept
        {
            wil::unique_hrgn monitor{ ::CreateRectRgnIndirect(area) };

            if (monitor != nullptr)
            {
                auto const target = reinterpret_cast<HRGN>(region);
                ::CombineRgn(target, target, monitor.get(), RGN_OR);
            }

            return TRUE;
        }

        // Fails while the session is locked, on the secure desktop, or disconnected.
        bool IsInputDesktopReachable() noexcept
        {
            auto const desktop = ::OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);

            if (desktop == nullptr)
            {
                return false;
            }

            ::CloseDesktop(desktop);

            return true;
        }
    }

    _Use_decl_annotations_
    bool IsWindowOnScreen(HWND window) noexcept
    {
        if (window == nullptr || !::IsWindowVisible(window) || ::IsIconic(window))
        {
            return false;
        }

        if (!s_displayOn.load() || !IsInputDesktopReachable() || IsCloaked(window))
        {
            return false;
        }

        RECT bounds{};

        if (!TryGetVisibleBounds(window, bounds))
        {
            return true;
        }

        wil::unique_hrgn remaining{ ::CreateRectRgnIndirect(&bounds) };
        wil::unique_hrgn monitors{ ::CreateRectRgn(0, 0, 0, 0) };

        if (remaining == nullptr || monitors == nullptr)
        {
            return true;
        }

        ::EnumDisplayMonitors(nullptr, nullptr, &AddMonitorArea, reinterpret_cast<LPARAM>(monitors.get()));

        if (::CombineRgn(remaining.get(), remaining.get(), monitors.get(), RGN_AND) == NULLREGION)
        {
            return false;
        }

        // Up the z-order from this window. Capped, because the order can change during the walk.
        auto above = ::GetWindow(window, GW_HWNDPREV);

        for (uint32_t count = 0; above != nullptr && count < 1024; count++, above = ::GetWindow(above, GW_HWNDPREV))
        {
            RECT outer{};

            // A cheap test first, so the DWM is only asked about windows that overlap what is left.
            if (!::GetWindowRect(above, &outer) || !::RectInRegion(remaining.get(), &outer))
            {
                continue;
            }

            RECT cover{};

            if (!TryGetCover(above, cover))
            {
                continue;
            }

            wil::unique_hrgn coverRegion{ ::CreateRectRgnIndirect(&cover) };

            if (coverRegion != nullptr &&
                ::CombineRgn(remaining.get(), remaining.get(), coverRegion.get(), RGN_DIFF) == NULLREGION)
            {
                return false;
            }
        }

        return true;
    }

    void StartWatchingDisplayPower() noexcept
    {
        if (s_displayPowerNotification != nullptr)
        {
            return;
        }

        // Until the first notification the display counts as on, which it is when the app starts.
        if (::PowerSettingRegisterNotification(
            &GUID_SESSION_DISPLAY_STATUS,
            DEVICE_NOTIFY_CALLBACK,
            static_cast<HANDLE>(&s_displayPowerSubscription),
            &s_displayPowerNotification) != ERROR_SUCCESS)
        {
            s_displayPowerNotification = nullptr;
        }
    }

    void StopWatchingDisplayPower() noexcept
    {
        if (s_displayPowerNotification != nullptr)
        {
            ::PowerSettingUnregisterNotification(s_displayPowerNotification);
            s_displayPowerNotification = nullptr;
        }
    }
}
