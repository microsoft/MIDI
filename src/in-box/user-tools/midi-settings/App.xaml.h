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

    private:
        void OnUnhandledException(
            foundation::IInspectable const& sender,
            xaml::UnhandledExceptionEventArgs const& args);

        static bool s_isElevated;
        static bool s_showNotificationsOnLaunch;

        xaml::Window m_window{ nullptr };
    };
}
