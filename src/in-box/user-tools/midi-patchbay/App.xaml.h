// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "App.xaml.g.h"
#include "CommandLineOptions.h"

namespace winrt::midipatchbay::implementation
{
    struct App : AppT<App>
    {
        App();

        void OnLaunched(_In_ xaml::LaunchActivatedEventArgs const& args);

        static ::midipatchbay::CommandLineOptions const& StartupOptions() noexcept { return s_startupOptions; }

        // One editor per patch. Opening a patch that already has one brings that one forward.
        static void OpenEditorWindow(_In_ std::wstring const& patchKey);

        static void ActivateLibraryWindow();

        // The theme, the backdrop or always on top changed in the library.
        static void ApplyAppearanceToEditors();

        // The app is closing. Routing lives in the library, so an editor closing loses nothing.
        static void CloseAllEditors();

    private:
        void OnUnhandledException(
            _In_ foundation::IInspectable const& sender,
            _In_ xaml::UnhandledExceptionEventArgs const& args);

        static ::midipatchbay::CommandLineOptions s_startupOptions;

        xaml::Window m_window{ nullptr };
    };
}
