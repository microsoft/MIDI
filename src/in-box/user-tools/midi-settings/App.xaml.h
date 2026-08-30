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

    private:
        void OnUnhandledException(
            foundation::IInspectable const& sender,
            xaml::UnhandledExceptionEventArgs const& args);

        static bool s_isElevated;

        xaml::Window m_window{ nullptr };
    };
}
