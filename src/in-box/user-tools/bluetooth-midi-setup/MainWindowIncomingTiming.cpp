// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Incoming timing: whether messages from a device are smoothed out to the times the device
// stamped on them, or handed on as soon as each packet arrives. Per device on the Devices page,
// and the transport-wide default on the Settings page.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StringResources.h"

namespace res = ::midibluetoothsetup::resources;

namespace winrt::midibluetoothsetup::implementation
{
    namespace
    {
        // Order must match the ComboBoxItems in the transport settings card in MainWindow.xaml.
        // It has no "use default" entry, so its indices are one lower than the per-device combo's.
        int32_t DefaultComboIndexFromValue(_In_ midi2bt::MidiBluetoothIncomingTiming const timing) noexcept
        {
            return timing == midi2bt::MidiBluetoothIncomingTiming::AsSoonAsPossible ? 1 : 0;
        }

        // The resolved value, as shown beside the combo and in the status lines.
        winrt::hstring IncomingTimingDescription(_In_ midi2bt::MidiBluetoothIncomingTiming const timing) noexcept
        {
            switch (timing)
            {
            case midi2bt::MidiBluetoothIncomingTiming::UseTransportDefault:
                return res::GetString(L"IncomingTimingUseDefault");

            case midi2bt::MidiBluetoothIncomingTiming::AsSoonAsPossible:
                return res::GetString(L"IncomingTimingAsSoonAsPossible");

            default:
                return res::GetString(L"IncomingTimingSmooth");
            }
        }

        // Applies the change now and saves it, the same way offline retention does. An empty
        // message with a false result means nothing more specific was reported.
        bool SendAndSaveIncomingTiming(
            _In_ midi2bt::MidiBluetoothIncomingTimingConfig const& config,
            _Out_ winrt::hstring& errorMessage) noexcept
        {
            errorMessage = {};

            try
            {
                auto const sendResponse = midi2svc::MidiServiceTransportPluginConfigManager::SendUpdate(config);

                if (sendResponse != nullptr &&
                    sendResponse.Status() == midi2svc::MidiServiceConfigResponseStatus::Success)
                {
                    auto const saveResponse = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

                    if (saveResponse != nullptr && saveResponse.Success())
                    {
                        return true;
                    }

                    if (saveResponse != nullptr)
                    {
                        errorMessage = saveResponse.ErrorMessage();
                    }
                }
                else if (sendResponse != nullptr)
                {
                    errorMessage = sendResponse.ServiceErrorMessage();
                }
            }
            catch (...)
            {
            }

            return false;
        }
    }

    _Use_decl_annotations_
    void MainWindow::ApplyDeviceIncomingTiming(
        midibluetoothsetup::BluetoothDeviceItem const& item,
        midi2bt::MidiBluetoothDeviceInformation const& device) noexcept
    {
        try
        {
            if (item == nullptr || device == nullptr)
            {
                return;
            }

            auto const delay = device.IncomingSmoothingDelayMilliseconds();

            winrt::get_self<implementation::BluetoothDeviceItem>(item)->InternalUpdateIncomingTiming(
                device.IncomingTiming(),
                IncomingTimingDescription(device.EffectiveIncomingTiming()),
                (device.IsConnected() && delay > 0.0) ?
                    res::FormatString(L"SmoothingDelayFormat", delay) :
                    winrt::hstring{});
        }
        MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to show a device's incoming timing.")
    }

    _Use_decl_annotations_
    void MainWindow::ApplyDefaultIncomingTiming(ServiceSnapshot const& snapshot) noexcept
    {
        try
        {
            if (!snapshot.Gathered)
            {
                return;
            }

            auto const value = static_cast<int32_t>(snapshot.DefaultIncomingTiming);

            if (value == m_defaultIncomingTiming)
            {
                return;
            }

            // Recorded before the selection changes, so the SelectionChanged this raises is seen
            // as the page being populated rather than a choice to apply.
            m_defaultIncomingTiming = value;

            DefaultIncomingTimingCombo().SelectedIndex(DefaultComboIndexFromValue(snapshot.DefaultIncomingTiming));
        }
        MIDI_BTSETUP_CATCH_AND_LOG(L"Unable to show the default incoming timing.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnDefaultIncomingTimingChanged(
        foundation::IInspectable const& sender,
        xaml::Controls::SelectionChangedEventArgs const&)
    {
        auto strongThis = get_strong();

        auto const combo = sender.try_as<xaml::Controls::ComboBox>();

        if (combo == nullptr)
        {
            co_return;
        }

        auto const selectedIndex = combo.SelectedIndex();

        if (selectedIndex < 0)
        {
            co_return;
        }

        // No "use default" here: this is what a device defers TO.
        auto const timing = selectedIndex == 1 ?
            midi2bt::MidiBluetoothIncomingTiming::AsSoonAsPossible :
            midi2bt::MidiBluetoothIncomingTiming::Smooth;

        // Set while the page was being populated rather than by the customer, so there is nothing
        // to apply. Without this every refresh would rewrite the configuration file.
        if (static_cast<int32_t>(timing) == m_defaultIncomingTiming)
        {
            co_return;
        }

        winrt::hstring errorMessage{};
        bool succeeded{ false };

        try
        {
            midi2bt::MidiBluetoothIncomingTimingConfig config{ timing };

            succeeded = SendAndSaveIncomingTiming(config, errorMessage);
        }
        catch (...)
        {
        }

        if (succeeded)
        {
            m_defaultIncomingTiming = static_cast<int32_t>(timing);

            DefaultIncomingTimingStatusText().Text(
                res::FormatString(L"StatusDefaultIncomingTimingSetFormat", IncomingTimingDescription(timing)));
        }
        else
        {
            DefaultIncomingTimingStatusText().Text(
                errorMessage.empty() ? res::GetString(L"StatusIncomingTimingFailed") : errorMessage);
        }

        co_return;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnIncomingTimingChanged(
        foundation::IInspectable const& sender,
        xaml::Controls::SelectionChangedEventArgs const&)
    {
        auto strongThis = get_strong();

        auto const item = ItemFromSender<midibluetoothsetup::BluetoothDeviceItem>(sender);

        if (item == nullptr || item.IsBusy())
        {
            co_return;
        }

        auto const combo = sender.try_as<xaml::Controls::ComboBox>();

        if (combo == nullptr)
        {
            co_return;
        }

        auto const selectedIndex = combo.SelectedIndex();

        // This fires while the row is being bound and whenever a refresh rewrites the selection,
        // so anything which is not a real change from what the service already holds is ignored.
        // Without this every refresh would write the configuration file again.
        if (selectedIndex < 0 || selectedIndex == item.IncomingTimingIndex())
        {
            co_return;
        }

        auto const deviceId = item.BluetoothDeviceId();
        auto const timing = implementation::BluetoothDeviceItem::IncomingTimingFromIndex(selectedIndex);

        item.IsBusy(true);

        winrt::hstring errorMessage{};
        bool succeeded{ false };

        try
        {
            midi2bt::MidiBluetoothIncomingTimingConfig config{ deviceId, timing };

            succeeded = SendAndSaveIncomingTiming(config, errorMessage);
        }
        catch (...)
        {
        }

        item.IsBusy(false);

        if (succeeded)
        {
            SetDevicesStatus(res::FormatString(
                L"StatusIncomingTimingSetFormat",
                item.DisplayName(),
                IncomingTimingDescription(timing)));
        }
        else
        {
            SetDevicesStatus(errorMessage.empty() ?
                res::GetString(L"StatusIncomingTimingFailed") : errorMessage);
        }

        RequestRefreshAsync();
    }
}
