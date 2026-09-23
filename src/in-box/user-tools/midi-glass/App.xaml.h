// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "App.xaml.g.h"

namespace winrt::midiglass::implementation
{
    struct App : AppT<App>
    {
        App();

        void OnLaunched(xaml::LaunchActivatedEventArgs const& args);

    private:
        void OnUnhandledException(
            foundation::IInspectable const& sender,
            xaml::UnhandledExceptionEventArgs const& args);

        xaml::Window m_window{ nullptr };
    };
}
