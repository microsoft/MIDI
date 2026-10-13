// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// How long an RTP-MIDI device's endpoint stays while the device is away. The same choices as the
// Bluetooth MIDI Setup app, without "Use default": RTP-MIDI has no setting for the whole transport.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StringResources.h"

namespace res = ::midinetworksetup::resources;

namespace winrt::midinetworksetup::implementation
{
    namespace
    {
        winrt::hstring OfflineRetentionChoiceText(_In_ int32_t const seconds) noexcept
        {
            if (seconds == static_cast<int32_t>(midi2rtp::MidiRtpOfflineRetention::KeepAlways)) return res::GetString(L"OfflineRetentionAlways");
            if (seconds == static_cast<int32_t>(midi2rtp::MidiRtpOfflineRetention::Immediate)) return res::GetString(L"OfflineRetentionImmediate");

            return res::FormatString(L"OfflineRetentionSecondsFormat", seconds);
        }

        bool TryParseClientId(_In_ winrt::hstring const& text, _Out_ winrt::guid& value) noexcept
        {
            value = winrt::guid{};

            if (text.empty())
            {
                return false;
            }

            try
            {
                value = winrt::guid{ std::wstring_view{ text } };
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        midi2rtp::MidiRtpSavedClient FindSavedRtpClient(_In_ winrt::guid const& clientId) noexcept
        {
            try
            {
                for (auto const& saved : midi2rtp::MidiRtpTransportManager::GetSavedClients())
                {
                    if (saved != nullptr && saved.ClientId() == clientId)
                    {
                        return saved;
                    }
                }
            }
            catch (...)
            {
            }

            return nullptr;
        }

        // Puts the choice back to what the service holds, in whichever row shows the device by now
        void ShowServiceValue(_In_ midinetworksetup::RtpRemoteHostItem const& item) noexcept
        {
            try
            {
                winrt::get_self<RtpRemoteHostItem>(item)->InternalRefreshOfflineRetention();
            }
            catch (...)
            {
            }
        }
    }

    // When the list reuses a row for another device, the new choice is set before the row's
    // DataContext moves to that device, so reading both now would pair the choice with the device
    // the row used to show. Waiting until the list has finished binding avoids that.
    _Use_decl_annotations_
    void MainWindow::OnRtpOfflineRetentionChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const&)
    {
        try
        {
            auto const combo = sender.try_as<controls::ComboBox>();

            if (combo == nullptr)
            {
                return;
            }

            auto weak = get_weak();

            DispatcherQueue().TryEnqueue(
                winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority::Low,
                [weak, combo]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->ApplyRtpOfflineRetentionChoiceAsync(combo);
                    }
                });
        }
        catch (...)
        {
        }
    }

    // RTP-MIDI has no verb for changing a client entry. Creating it again with the same id replaces
    // it, and changing only this never drops the connection.
    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::ApplyRtpOfflineRetentionChoiceAsync(controls::ComboBox const combo)
    {
        auto strongThis = get_strong();

        midinetworksetup::RtpRemoteHostItem item{ nullptr };

        try
        {
            item = combo.DataContext().try_as<midinetworksetup::RtpRemoteHostItem>();
        }
        catch (...)
        {
            co_return;
        }

        if (item == nullptr)
        {
            co_return;
        }

        auto const selectedIndex = combo.SelectedIndex();

        // This fires while the row is being bound and whenever a refresh sets the selection, so
        // only a real change from what the service holds goes any further. Without this every
        // refresh would write the configuration file again.
        if (selectedIndex < 0 || selectedIndex == item.OfflineRetentionIndex())
        {
            co_return;
        }

        winrt::guid clientId{};

        if (item.IsBusy() || !TryParseClientId(item.ClientId(), clientId))
        {
            ShowServiceValue(item);

            co_return;
        }

        // everything else in the entry is copied from what was saved
        auto const savedClient = FindSavedRtpClient(clientId);

        // creating it again would also switch on an entry which is switched off
        if (savedClient == nullptr || !savedClient.IsEnabled())
        {
            ShowServiceValue(item);
            SetRtpRemoteStatus(res::FormatString(L"OfflineRetentionNeedsSavedEntryFormat", item.DisplayName()));

            co_return;
        }

        auto const seconds = RtpRemoteHostItem::OfflineRetentionSecondsFromIndex(selectedIndex);
        auto const displayName = item.DisplayName();

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        item.IsBusy(true);

        co_await winrt::resume_background();

        bool applied{ false };
        winrt::hstring message{};

        try
        {
            midi2rtp::MidiRtpClientConnectConfig replacement{};

            replacement.ClientId(clientId);
            replacement.Comment(savedClient.Comment());
            replacement.Name(savedClient.Name());
            replacement.CustomEndpointName(savedClient.CustomEndpointName());
            replacement.MatchCriteria(savedClient.MatchCriteria());
            replacement.AutoReconnect(savedClient.AutoReconnect());
            replacement.SendRecoveryJournal(savedClient.SendRecoveryJournal());
            replacement.SendSpeedLimit(savedClient.SendSpeedLimit());
            replacement.OfflineRetentionSeconds(seconds);

            auto const response = co_await midi2rtp::MidiRtpTransportManager::ConnectRtpClientAsync(replacement);

            if (response != nullptr && response.Success())
            {
                // the service has it now, even if saving it fails
                applied = true;

                auto const saved = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(replacement);

                if (saved != nullptr && saved.Success())
                {
                    message = res::FormatString(L"OfflineRetentionSetFormat", displayName, OfflineRetentionChoiceText(seconds));
                }
                else
                {
                    message = res::FormatString(L"ChangeNotSavedFormat", saved == nullptr ? winrt::hstring{} : saved.ErrorMessage());
                }
            }
            else
            {
                message = (response == nullptr || response.ErrorMessage().empty()) ?
                    res::GetString(L"OfflineRetentionChangeFailedGeneral") :
                    res::FormatString(L"OfflineRetentionChangeFailedFormat", response.ErrorMessage());
            }
        }
        catch (...)
        {
            message = res::GetString(L"OfflineRetentionChangeFailedGeneral");
        }

        if (queue != nullptr)
        {
            queue.TryEnqueue([weak, item, applied, message]()
                {
                    item.IsBusy(false);

                    if (!applied)
                    {
                        ShowServiceValue(item);
                    }

                    if (auto strong = weak.get())
                    {
                        strong->SetRtpRemoteStatus(message);
                        strong->RequestRefreshAsync();
                    }
                });
        }
    }
}
