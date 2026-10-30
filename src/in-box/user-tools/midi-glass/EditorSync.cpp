// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// "Sync to device" in the editor: saves, then sends the layout to the PC it last went to, or asks
// which PC when that one can't be found. The work is in SyncDialogs.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "StringResources.h"
#include "SyncDialogs.h"

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    void EditorWindow::UpdateSyncToDeviceLabel()
    {
        try
        {
            auto const name = ::midiglass::devicesync::LastDeviceName();

            auto const text = name.empty()
                ? resources::GetString(L"EditorSyncToDevice")
                : resources::FormatString(L"EditorSyncToNamedFormat", name);

            SyncToDeviceLabel().Text(text);
            xaml::Automation::AutomationProperties::SetName(SyncToDeviceButton(), text);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to name the Sync to device button.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnSyncToDeviceLoaded(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        UpdateSyncToDeviceLabel();
    }

    _Use_decl_annotations_
    void EditorWindow::OnSyncToDeviceClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        SyncToDeviceAsync();
    }

    winrt::fire_and_forget EditorWindow::SyncToDeviceAsync()
    {
        auto lifetime = get_strong();

        try
        {
            if (m_filePath.empty())
            {
                co_return;
            }

            // What goes to the other PC has to be what's on the screen here.
            SaveNow();

            if (m_saveFailed)
            {
                co_return;
            }

            SyncToDeviceButton().IsEnabled(false);

            co_await ::midiglass::devicesync::SendLayoutAsync(Content().XamlRoot(), m_filePath, true);

            SyncToDeviceButton().IsEnabled(true);
            UpdateSyncToDeviceLabel();
        }
        catch (...)
        {
            try
            {
                SyncToDeviceButton().IsEnabled(true);
            }
            catch (...)
            {
            }

            LOG_CAUGHT_EXCEPTION_MSG("Unable to sync the layout to another PC.");
        }
    }
}
