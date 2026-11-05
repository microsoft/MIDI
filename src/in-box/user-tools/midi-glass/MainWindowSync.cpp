// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The library's way into "Sync to device": a card's "Send to device…", and "Receive from another
// PC…" and "Paired devices…" on the More menu. The work is in SyncDialogs.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "SyncDialogs.h"

namespace winrt::midiglass::implementation
{
    _Use_decl_annotations_
    void MainWindow::OnCardMenuSendToDevice(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            if (m_menuCard == nullptr || m_menuCard.IsNewTile())
            {
                return;
            }

            ::midiglass::devicesync::SendLayoutAsync(RootGrid().XamlRoot(), std::wstring{ m_menuCard.FilePath() }, false);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to send a layout to another PC.")
    }

    _Use_decl_annotations_
    void MainWindow::OnReceiveFromDeviceClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto weak = get_weak();

            ::midiglass::devicesync::OpenReceiveWindow([weak](std::wstring const&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->m_cardSignature.clear();
                        strong->RefreshLibrary();
                    }
                });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to open the Receive screen.")
    }

    _Use_decl_annotations_
    void MainWindow::OnPairedDevicesClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            ::midiglass::devicesync::ShowPairedDevicesAsync(RootGrid().XamlRoot());
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the paired PCs.")
    }
}
