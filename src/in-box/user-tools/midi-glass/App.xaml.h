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

        // One process, however many windows. Two copies would each open their own connection to
        // the same instrument and neither would know what the other had sent, so a running layout
        // and a Panic have to mean the same thing across all of them.
        //
        // Not projected. Opening a layout that is already running brings its window forward
        // rather than starting a second copy of it.
        static void OpenRuntimeWindow(_In_ std::wstring const& filePath);

        // The editor is one window at a time. Editing the same layout in two windows is a
        // conflict nobody needs, and a second request focuses the first.
        static void OpenEditorWindow(_In_ std::wstring const& filePath);

        static void ActivateLibraryWindow();

    private:
        void OnUnhandledException(
            foundation::IInspectable const& sender,
            xaml::UnhandledExceptionEventArgs const& args);

        xaml::Window m_window{ nullptr };
    };
}
