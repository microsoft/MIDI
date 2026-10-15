// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "App.xaml.g.h"

namespace winrt::midisettings::implementation
{
    struct App : AppT<App>
    {
        App();

        void OnLaunched(xaml::LaunchActivatedEventArgs const& args);

        // The global settings write HKLM and restart the service, so the window offers to
        // relaunch rather than the app demanding elevation just to browse endpoints.
        static bool IsElevated() noexcept { return s_isElevated; }

        // Set from wWinMain, because the command line is read before there is an application.
        static void ShowNotificationsOnLaunch(_In_ bool const value) noexcept { s_showNotificationsOnLaunch = value; }
        static bool ShowNotificationsOnLaunch() noexcept { return s_showNotificationsOnLaunch; }

        // The Configurations window to open once the main window is up: a page, or a package to
        // import. Both empty means it isn't opened.
        static void ConfigurationsOnLaunch(_In_ std::wstring const& page, _In_ std::wstring const& package) noexcept
        {
            try
            {
                s_configurationsPageOnLaunch = page;
                s_packageOnLaunch = package;
            }
            catch (...)
            {
            }
        }

        static std::wstring const& ConfigurationsPageOnLaunch() noexcept { return s_configurationsPageOnLaunch; }
        static std::wstring const& PackageOnLaunch() noexcept { return s_packageOnLaunch; }

    private:
        void OnUnhandledException(
            foundation::IInspectable const& sender,
            xaml::UnhandledExceptionEventArgs const& args);

        static bool s_isElevated;
        static bool s_showNotificationsOnLaunch;
        static std::wstring s_configurationsPageOnLaunch;
        static std::wstring s_packageOnLaunch;

        xaml::Window m_window{ nullptr };
    };
}
