// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// "Update ready": the layout this window runs was replaced by a newer copy from another PC. The
// surface never changes under somebody's hands; the new copy loads when they ask for it, the same
// way it would load if they opened it from the library.

#include "pch.h"
#include "RuntimeWindow.xaml.h"
#include "App.xaml.h"

namespace winrt::midiglass::implementation
{
    void RuntimeWindow::ShowUpdateReady()
    {
        try
        {
            UpdateReadyButton().Visibility(xaml::Visibility::Visible);
            ToolbarUpdateItem().Visibility(xaml::Visibility::Visible);
            CornerUpdateItem().Visibility(xaml::Visibility::Visible);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show that an update is ready.")
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnUpdateReadyClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            // After this handler returns: the window that raised it is about to close.
            DispatcherQueue().TryEnqueue([path = m_filePath]()
                {
                    App::ReloadRuntimeWindow(path);
                });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to load the update.")
    }
}
