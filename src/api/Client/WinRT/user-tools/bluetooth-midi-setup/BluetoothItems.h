// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "BluetoothDeviceItem.g.h"
#include "PeripheralClientItem.g.h"
#include "PendingClientItem.g.h"

#include "AppSettings.h"
#include "StringResources.h"

#include <chrono>

// The pages refresh from the service on a timer, so every row type raises property changed
// rather than being replaced. Nothing here throws: a failing notification must never take
// down a UI callback.
#define MIDI_BTSETUP_OBSERVABLE_ITEM()                                                         \
    public:                                                                                    \
        winrt::event_token PropertyChanged(                                                    \
            winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)      \
        {                                                                                      \
            return m_propertyChanged.add(handler);                                             \
        }                                                                                      \
                                                                                               \
        void PropertyChanged(winrt::event_token const& token) noexcept                         \
        {                                                                                      \
            m_propertyChanged.remove(token);                                                   \
        }                                                                                      \
                                                                                               \
    private:                                                                                   \
        void RaisePropertyChanged(std::wstring_view const name) noexcept                       \
        {                                                                                      \
            try                                                                                \
            {                                                                                  \
                m_propertyChanged(                                                             \
                    *this,                                                                     \
                    winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ name });       \
            }                                                                                  \
            catch (...)                                                                        \
            {                                                                                  \
            }                                                                                  \
        }                                                                                      \
                                                                                               \
        template <typename TValue>                                                             \
        bool UpdateField(TValue& field, TValue const& value, std::wstring_view const name) noexcept \
        {                                                                                      \
            if (field == value)                                                                \
            {                                                                                  \
                return false;                                                                  \
            }                                                                                  \
                                                                                               \
            field = value;                                                                     \
            RaisePropertyChanged(name);                                                        \
                                                                                               \
            return true;                                                                       \
        }                                                                                      \
                                                                                               \
        winrt::event<winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged{};

namespace winrt::midibluetoothsetup::implementation
{
    // Signal strength over a rolling window, rendered as a filled sparkline.
    //
    // The axis is real elapsed time rather than one step per sample, because a refresh is also
    // requested by navigation and by the Refresh button, and a tick is skipped when the previous
    // refresh is still running.
    //
    // Unlike a latency trace this uses a FIXED decibel scale rather than scaling to the peak.
    // Received signal strength is already logarithmic, and auto-scaling would make a device
    // sitting still on a desk look like it was swinging wildly. A fixed scale means "the line
    // went up" always means "it got closer", and two devices can be compared directly.
    struct SignalHistory
    {
        // How many polls' worth of history the window covers. The x axis is real elapsed time
        // rather than sample index, so that a stall in polling shows as a gap instead of being
        // drawn as though the readings were evenly spaced. A minute at the default poll interval
        // is short enough to respond while somebody walks a device around the room.
        static constexpr size_t SampleCapacity = 30;

        // hard ceiling, in case something requests refreshes far faster than the poll interval
        static constexpr size_t MaxSamples = 400;

        // A radio reports roughly -30 dBm when a device is touching the PC and around -100 when
        // it is at the edge of usable range, so the trace is drawn between those.
        static constexpr double StrongestDecibels = -30.0;
        static constexpr double WeakestDecibels = -100.0;

        // what the radio reports when it has no reading at all, rather than a very weak one
        static constexpr int16_t NoReadingDecibels = -127;

        winrt::Microsoft::UI::Xaml::Media::PointCollection LinePoints{ nullptr };
        winrt::Microsoft::UI::Xaml::Media::PointCollection FillPoints{ nullptr };
        winrt::hstring SignalText{};

        bool HasSamples() const noexcept { return !m_samples.empty(); }

        static bool IsUsableReading(_In_ int16_t const decibels) noexcept
        {
            return decibels != 0 && decibels > NoReadingDecibels;
        }

        void Record(
            _In_ int16_t const decibels,
            _In_ bool const isAdvertising,
            _In_ double const graphWidth,
            _In_ double const graphHeight) noexcept
        {
            // A device stops advertising the moment it connects, so its last reading freezes.
            // Keeping the trace would show a confident flat line that means nothing.
            if (!isAdvertising || !IsUsableReading(decibels))
            {
                if (!m_samples.empty())
                {
                    m_samples.clear();
                    Rebuild(Clock::now(), graphWidth, graphHeight);
                }

                return;
            }

            auto const now = Clock::now();

            m_samples.push_back({ now, static_cast<double>(decibels) });

            auto const window = std::chrono::duration<double>{ WindowSeconds() };

            while (m_samples.size() > 1 && (now - m_samples.front().first) > window)
            {
                m_samples.pop_front();
            }

            while (m_samples.size() > MaxSamples)
            {
                m_samples.pop_front();
            }

            Rebuild(now, graphWidth, graphHeight);
        }

    private:
        using Clock = std::chrono::steady_clock;

        static double WindowSeconds() noexcept
        {
            auto const interval = ::midibluetoothsetup::AppSettings::Current().RefreshIntervalSeconds();

            return static_cast<double>(SampleCapacity) *
                static_cast<double>(interval == 0 ? 1 : interval);
        }

        void Rebuild(
            _In_ Clock::time_point const now,
            _In_ double const graphWidth,
            _In_ double const graphHeight) noexcept
        {
            try
            {
                winrt::Microsoft::UI::Xaml::Media::PointCollection line{};
                winrt::Microsoft::UI::Xaml::Media::PointCollection fill{};

                if (m_samples.empty())
                {
                    LinePoints = line;
                    FillPoints = fill;
                    SignalText = winrt::hstring{};

                    return;
                }

                auto const window = WindowSeconds();
                auto const range = StrongestDecibels - WeakestDecibels;

                for (auto const& sample : m_samples)
                {
                    auto const age = std::chrono::duration<double>{ now - sample.first }.count();

                    auto x = graphWidth - ((age / window) * graphWidth);

                    x = std::clamp(x, 0.0, graphWidth);

                    auto const normalized = std::clamp((sample.second - WeakestDecibels) / range, 0.0, 1.0);

                    auto const y = graphHeight - (normalized * graphHeight);

                    line.Append(winrt::Windows::Foundation::Point{
                        static_cast<float>(x), static_cast<float>(y) });
                }

                for (auto const& point : line)
                {
                    fill.Append(point);
                }

                // close the area down to the baseline so the polygon fills under the trace
                fill.Append(winrt::Windows::Foundation::Point{
                    static_cast<float>(line.GetAt(line.Size() - 1).X), static_cast<float>(graphHeight) });

                fill.Append(winrt::Windows::Foundation::Point{
                    static_cast<float>(line.GetAt(0).X), static_cast<float>(graphHeight) });

                LinePoints = line;
                FillPoints = fill;

                SignalText = ::midibluetoothsetup::resources::FormatString(
                    L"SignalStrengthFormat",
                    static_cast<int32_t>(m_samples.back().second),
                    static_cast<int32_t>(WeakestDecibels),
                    static_cast<int32_t>(StrongestDecibels));
            }
            catch (...)
            {
            }
        }

        std::deque<std::pair<Clock::time_point, double>> m_samples{};
    };


    struct BluetoothDeviceItem : BluetoothDeviceItemT<BluetoothDeviceItem>
    {
        BluetoothDeviceItem() = default;

        winrt::hstring BluetoothDeviceId() const noexcept { return m_bluetoothDeviceId; }
        winrt::hstring DisplayName() const noexcept { return m_displayName; }
        winrt::hstring SubtitleText() const noexcept { return m_subtitleText; }
        winrt::hstring StatusText() const noexcept { return m_statusText; }
        winrt::hstring StatisticsText() const noexcept { return m_statisticsText; }
        winrt::hstring IntervalText() const noexcept { return m_intervalText; }
        winrt::hstring EndpointDeviceId() const noexcept { return m_endpointDeviceId; }
        winrt::hstring EndpointDeviceInstanceId() const noexcept { return m_endpointDeviceInstanceId; }

        bool IsConnected() const noexcept { return m_isConnected; }
        bool IsPresent() const noexcept { return m_isPresent; }
        bool IsPaired() const noexcept { return m_isPaired; }
        bool HasEndpoint() const noexcept { return m_hasEndpoint; }

        bool IsBusy() const noexcept { return m_isBusy; }
        void IsBusy(_In_ bool const value) noexcept { UpdateField(m_isBusy, value, L"IsBusy"); }

        winrt::Microsoft::UI::Xaml::Visibility ConnectVisibility() const noexcept
        {
            return m_isConnected ?
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed :
                winrt::Microsoft::UI::Xaml::Visibility::Visible;
        }

        winrt::Microsoft::UI::Xaml::Visibility DisconnectVisibility() const noexcept
        {
            return m_isConnected ?
                winrt::Microsoft::UI::Xaml::Visibility::Visible :
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed;
        }

        // Forgetting is only meaningful for a device the configuration file remembers, and a
        // connected one is disconnected rather than forgotten.
        winrt::Microsoft::UI::Xaml::Visibility ForgetVisibility() const noexcept
        {
            return (!m_isConnected && m_isRemembered) ?
                winrt::Microsoft::UI::Xaml::Visibility::Visible :
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed;
        }

        // The customization is keyed on the endpoint instance id, which the transport derives
        // from the device, so a name can be set before the device has ever connected.
        winrt::Microsoft::UI::Xaml::Visibility CustomizeVisibility() const noexcept
        {
            return m_endpointDeviceInstanceId.empty() ?
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed :
                winrt::Microsoft::UI::Xaml::Visibility::Visible;
        }

        // Order must match the ComboBoxItems in MainWindow.xaml.
        static int32_t OfflineRetentionSecondsFromIndex(_In_ int32_t const index) noexcept
        {
            switch (index)
            {
            case 0: return -2;      // defer to the transport setting
            case 1: return -1;      // keep the endpoint always
            case 2: return 0;       // remove it as soon as the device goes offline
            case 3: return 30;
            case 4: return 300;
            default: return -2;
            }
        }

        static int32_t OfflineRetentionIndexFromSeconds(_In_ int32_t const seconds) noexcept
        {
            switch (seconds)
            {
            case -2: return 0;
            case -1: return 1;
            case 0:  return 2;
            case 30: return 3;
            case 300: return 4;

            // A duration set from the console or by editing the configuration file which is not
            // one of the presets. Shown as no selection rather than rounded to one of them, and
            // the real value is in OfflineRetentionText either way.
            default: return -1;
            }
        }

        int32_t OfflineRetentionIndex() const noexcept
        {
            return OfflineRetentionIndexFromSeconds(m_offlineRetentionSeconds);
        }

        winrt::hstring OfflineRetentionText() const noexcept { return m_offlineRetentionText; }

        // Only a remembered device has an endpoint worth keeping when it goes away.
        winrt::Microsoft::UI::Xaml::Visibility OfflineRetentionVisibility() const noexcept
        {
            return m_isRemembered ?
                winrt::Microsoft::UI::Xaml::Visibility::Visible :
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed;
        }

        winrt::Microsoft::UI::Xaml::Visibility ConnectedBadgeVisibility() const noexcept
        {
            return m_isConnected ?
                winrt::Microsoft::UI::Xaml::Visibility::Visible :
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed;
        }

        winrt::Microsoft::UI::Xaml::Visibility EndpointDeviceIdVisibility() const noexcept
        {
            return m_endpointDeviceId.empty() ?
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed :
                winrt::Microsoft::UI::Xaml::Visibility::Visible;
        }

        winrt::Microsoft::UI::Xaml::Visibility IntervalVisibility() const noexcept
        {
            return m_intervalText.empty() ?
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed :
                winrt::Microsoft::UI::Xaml::Visibility::Visible;
        }

        winrt::hstring LastErrorText() const noexcept { return m_lastErrorText; }

        winrt::Microsoft::UI::Xaml::Visibility LastErrorVisibility() const noexcept
        {
            return m_lastErrorText.empty() ?
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed :
                winrt::Microsoft::UI::Xaml::Visibility::Visible;
        }

        winrt::Microsoft::UI::Xaml::Visibility SignalGraphVisibility() const noexcept
        {
            return m_signal.HasSamples() ?
                winrt::Microsoft::UI::Xaml::Visibility::Visible :
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed;
        }

        winrt::Microsoft::UI::Xaml::Media::PointCollection SignalLinePoints() const noexcept
        {
            return m_signal.LinePoints;
        }

        winrt::Microsoft::UI::Xaml::Media::PointCollection SignalFillPoints() const noexcept
        {
            return m_signal.FillPoints;
        }

        winrt::hstring SignalText() const noexcept { return m_signal.SignalText; }

        void InternalInitialize(_In_ winrt::hstring const& bluetoothDeviceId) noexcept
        {
            m_bluetoothDeviceId = bluetoothDeviceId;
        }

        void InternalUpdate(
            _In_ winrt::hstring const& displayName,
            _In_ winrt::hstring const& subtitleText,
            _In_ winrt::hstring const& statusText,
            _In_ winrt::hstring const& statisticsText,
            _In_ winrt::hstring const& intervalText,
            _In_ winrt::hstring const& endpointDeviceId,
            _In_ winrt::hstring const& endpointDeviceInstanceId,
            _In_ winrt::hstring const& lastErrorText,
            _In_ int16_t const signalDecibels,
            _In_ bool const isConnected,
            _In_ bool const isPresent,
            _In_ bool const isPaired,
            _In_ bool const hasEndpoint,
            _In_ bool const isRemembered,
            _In_ int32_t const offlineRetentionSeconds,
            _In_ winrt::hstring const& offlineRetentionText) noexcept
        {
            UpdateField(m_displayName, displayName, L"DisplayName");
            UpdateField(m_subtitleText, subtitleText, L"SubtitleText");
            UpdateField(m_statusText, statusText, L"StatusText");
            UpdateField(m_statisticsText, statisticsText, L"StatisticsText");
            UpdateField(m_isPaired, isPaired, L"IsPaired");

            if (UpdateField(m_intervalText, intervalText, L"IntervalText"))
            {
                RaisePropertyChanged(L"IntervalVisibility");
            }

            if (UpdateField(m_endpointDeviceId, endpointDeviceId, L"EndpointDeviceId"))
            {
                RaisePropertyChanged(L"EndpointDeviceIdVisibility");
            }

            if (UpdateField(m_endpointDeviceInstanceId, endpointDeviceInstanceId, L"EndpointDeviceInstanceId"))
            {
                RaisePropertyChanged(L"CustomizeVisibility");
            }

            if (UpdateField(m_lastErrorText, lastErrorText, L"LastErrorText"))
            {
                RaisePropertyChanged(L"LastErrorVisibility");
            }

            auto const connectedChanged = UpdateField(m_isConnected, isConnected, L"IsConnected");
            auto const rememberedChanged = UpdateField(m_isRemembered, isRemembered, L"IsRemembered");

            if (UpdateField(m_offlineRetentionSeconds, offlineRetentionSeconds, L"OfflineRetentionSeconds"))
            {
                RaisePropertyChanged(L"OfflineRetentionIndex");
            }

            UpdateField(m_offlineRetentionText, offlineRetentionText, L"OfflineRetentionText");

            UpdateField(m_isPresent, isPresent, L"IsPresent");
            UpdateField(m_hasEndpoint, hasEndpoint, L"HasEndpoint");

            if (connectedChanged)
            {
                RaisePropertyChanged(L"ConnectedBadgeVisibility");
            }

            if (connectedChanged || rememberedChanged)
            {
                RaisePropertyChanged(L"ConnectVisibility");
                RaisePropertyChanged(L"DisconnectVisibility");
                RaisePropertyChanged(L"ForgetVisibility");
                RaisePropertyChanged(L"OfflineRetentionVisibility");
            }

            RecordSignalSample(signalDecibels, isPresent && !isConnected);
        }

    private:
        // must match the sparkline's size in the device item template
        // must match the Border in the device item template in MainWindow.xaml
        static constexpr double SignalGraphWidth = 160.0;
        static constexpr double SignalGraphHeight = 32.0;

        void RecordSignalSample(_In_ int16_t const decibels, _In_ bool const isAdvertising) noexcept
        {
            m_signal.Record(decibels, isAdvertising, SignalGraphWidth, SignalGraphHeight);

            RaisePropertyChanged(L"SignalLinePoints");
            RaisePropertyChanged(L"SignalFillPoints");
            RaisePropertyChanged(L"SignalText");
            RaisePropertyChanged(L"SignalGraphVisibility");
        }

        winrt::hstring m_bluetoothDeviceId{};
        winrt::hstring m_displayName{};
        winrt::hstring m_subtitleText{};
        winrt::hstring m_statusText{};
        winrt::hstring m_statisticsText{};
        winrt::hstring m_intervalText{};
        winrt::hstring m_endpointDeviceId{};
        winrt::hstring m_endpointDeviceInstanceId{};
        winrt::hstring m_lastErrorText{};

        bool m_isConnected{ false };
        bool m_isPresent{ false };
        bool m_isPaired{ false };
        bool m_hasEndpoint{ false };

        int32_t m_offlineRetentionSeconds{ -2 };
        winrt::hstring m_offlineRetentionText{};
        bool m_isRemembered{ false };
        bool m_isBusy{ false };

        SignalHistory m_signal{};

        MIDI_BTSETUP_OBSERVABLE_ITEM()
    };


    struct PeripheralClientItem : PeripheralClientItemT<PeripheralClientItem>
    {
        PeripheralClientItem() = default;

        winrt::hstring DisplayName() const noexcept { return m_displayName; }
        winrt::hstring AddressText() const noexcept { return m_addressText; }
        winrt::hstring StatusText() const noexcept { return m_statusText; }
        winrt::hstring StatisticsText() const noexcept { return m_statisticsText; }
        winrt::hstring IntervalText() const noexcept { return m_intervalText; }
        winrt::hstring EndpointDeviceId() const noexcept { return m_endpointDeviceId; }
        winrt::hstring EndpointDeviceInstanceId() const noexcept { return m_endpointDeviceInstanceId; }

        bool IsPaired() const noexcept { return m_isPaired; }
        bool HasGenericName() const noexcept { return m_hasGenericName; }

        bool IsBusy() const noexcept { return m_isBusy; }
        void IsBusy(_In_ bool const value) noexcept { UpdateField(m_isBusy, value, L"IsBusy"); }

        winrt::Microsoft::UI::Xaml::Visibility EndpointDeviceIdVisibility() const noexcept
        {
            return m_endpointDeviceId.empty() ?
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed :
                winrt::Microsoft::UI::Xaml::Visibility::Visible;
        }

        // Without a bond the remote has no stable identity, so anything named here applies to
        // whichever unpaired device connects next.
        winrt::Microsoft::UI::Xaml::Visibility PairingHintVisibility() const noexcept
        {
            return m_isPaired ?
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed :
                winrt::Microsoft::UI::Xaml::Visibility::Visible;
        }

        winrt::Microsoft::UI::Xaml::Visibility GenericNameHintVisibility() const noexcept
        {
            return (m_hasGenericName && !m_isPaired) ?
                winrt::Microsoft::UI::Xaml::Visibility::Visible :
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed;
        }

        void InternalUpdate(
            _In_ winrt::hstring const& displayName,
            _In_ winrt::hstring const& addressText,
            _In_ winrt::hstring const& statusText,
            _In_ winrt::hstring const& statisticsText,
            _In_ winrt::hstring const& intervalText,
            _In_ winrt::hstring const& endpointDeviceId,
            _In_ winrt::hstring const& endpointDeviceInstanceId,
            _In_ bool const isPaired,
            _In_ bool const hasGenericName) noexcept
        {
            UpdateField(m_displayName, displayName, L"DisplayName");
            UpdateField(m_addressText, addressText, L"AddressText");
            UpdateField(m_statusText, statusText, L"StatusText");
            UpdateField(m_statisticsText, statisticsText, L"StatisticsText");
            UpdateField(m_intervalText, intervalText, L"IntervalText");
            UpdateField(m_endpointDeviceInstanceId, endpointDeviceInstanceId, L"EndpointDeviceInstanceId");

            if (UpdateField(m_endpointDeviceId, endpointDeviceId, L"EndpointDeviceId"))
            {
                RaisePropertyChanged(L"EndpointDeviceIdVisibility");
            }

            if (UpdateField(m_isPaired, isPaired, L"IsPaired"))
            {
                RaisePropertyChanged(L"PairingHintVisibility");
                RaisePropertyChanged(L"GenericNameHintVisibility");
            }

            if (UpdateField(m_hasGenericName, hasGenericName, L"HasGenericName"))
            {
                RaisePropertyChanged(L"GenericNameHintVisibility");
            }
        }

    private:
        winrt::hstring m_displayName{};
        winrt::hstring m_addressText{};
        winrt::hstring m_statusText{};
        winrt::hstring m_statisticsText{};
        winrt::hstring m_intervalText{};
        winrt::hstring m_endpointDeviceId{};
        winrt::hstring m_endpointDeviceInstanceId{};

        bool m_isPaired{ false };
        bool m_hasGenericName{ false };
        bool m_isBusy{ false };

        MIDI_BTSETUP_OBSERVABLE_ITEM()
    };

    struct PendingClientItem : PendingClientItemT<PendingClientItem>
    {
        PendingClientItem() = default;

        winrt::hstring BluetoothAddress() const noexcept { return m_bluetoothAddress; }
        winrt::hstring Headline() const noexcept { return m_headline; }
        winrt::hstring Detail() const noexcept { return m_detail; }
        bool CanRememberDecision() const noexcept { return m_canRememberDecision; }
        bool CanDecide() const noexcept { return !m_isBusy; }
        bool CanDecidePermanently() const noexcept { return m_canRememberDecision && !m_isBusy; }

        bool IsBusy() const noexcept { return m_isBusy; }

        void IsBusy(_In_ bool const value) noexcept
        {
            if (UpdateField(m_isBusy, value, L"IsBusy"))
            {
                RaisePropertyChanged(L"CanDecide");
                RaisePropertyChanged(L"CanDecidePermanently");
            }
        }

        xaml::Visibility RotatingAddressHintVisibility() const noexcept
        {
            return m_canRememberDecision ? xaml::Visibility::Collapsed : xaml::Visibility::Visible;
        }

        void InternalUpdate(
            _In_ winrt::hstring const& bluetoothAddress,
            _In_ winrt::hstring const& headline,
            _In_ winrt::hstring const& detail,
            _In_ bool const canRememberDecision) noexcept
        {
            UpdateField(m_bluetoothAddress, bluetoothAddress, L"BluetoothAddress");
            UpdateField(m_headline, headline, L"Headline");
            UpdateField(m_detail, detail, L"Detail");

            if (UpdateField(m_canRememberDecision, canRememberDecision, L"CanRememberDecision"))
            {
                RaisePropertyChanged(L"RotatingAddressHintVisibility");
                RaisePropertyChanged(L"CanDecidePermanently");
            }
        }

    private:
        winrt::hstring m_bluetoothAddress{};
        winrt::hstring m_headline{};
        winrt::hstring m_detail{};

        bool m_canRememberDecision{ false };
        bool m_isBusy{ false };

        MIDI_BTSETUP_OBSERVABLE_ITEM()
    };
}

namespace winrt::midibluetoothsetup::factory_implementation
{
    struct BluetoothDeviceItem : BluetoothDeviceItemT<BluetoothDeviceItem, implementation::BluetoothDeviceItem>
    {
    };

    struct PeripheralClientItem : PeripheralClientItemT<PeripheralClientItem, implementation::PeripheralClientItem>
    {
    };

    struct PendingClientItem : PendingClientItemT<PendingClientItem, implementation::PendingClientItem>
    {
    };
}
