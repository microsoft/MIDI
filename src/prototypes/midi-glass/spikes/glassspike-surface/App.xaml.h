// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#pragma once

#include "App.xaml.g.h"
#include "SpikeOptions.h"

namespace winrt::glassspike::implementation
{
    struct App : AppT<App>
    {
        App();

        void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const& args);

        static gspike::SpikeOptions const& Options() noexcept { return s_options; }

    private:
        void OnUnhandledException(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::UnhandledExceptionEventArgs const& args);

        static gspike::SpikeOptions s_options;

        Microsoft::UI::Xaml::Window m_window{ nullptr };
    };
}
