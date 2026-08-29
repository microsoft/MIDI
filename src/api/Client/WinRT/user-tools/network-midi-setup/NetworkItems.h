// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "PendingInvitationItem.g.h"
#include "RemoteHostItem.g.h"
#include "HostConnectionItem.g.h"
#include "KnownClientItem.g.h"
#include "LocalHostItem.g.h"

#include "AppSettings.h"

#include <chrono>

// The pages refresh from the service on a timer, so every row type raises property changed
// rather than being replaced. Nothing here throws: a failing notification must never take
// down a UI callback.
#define MIDI_NETSETUP_OBSERVABLE_ITEM()                                                        \
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

namespace winrt::midinetworksetup::implementation
{
    // Round trip latency over a rolling window, rendered as a filled sparkline. Shared by the
    // remote host rows and by the per-connection rows on the This PC page.
    //
    // The axis is real elapsed time rather than one step per sample: a refresh is also requested
    // by navigation, the Refresh button and settings changes, and a tick is skipped when the
    // previous refresh is still running, so a per-sample axis scrolls at an uneven rate.
    struct LatencyHistory
    {
        // how many polls' worth of history the window covers
        static constexpr size_t SampleCapacity = 100;

        // hard ceiling, in case something requests refreshes far faster than the poll interval
        static constexpr size_t MaxSamples = 400;

        winrt::Microsoft::UI::Xaml::Media::PointCollection LinePoints{ nullptr };
        winrt::Microsoft::UI::Xaml::Media::PointCollection FillPoints{ nullptr };
        winrt::hstring PeakText{};

        bool HasSamples() const noexcept { return !m_samples.empty(); }

        void Record(
            _In_ uint64_t const ticks,
            _In_ bool const isConnected,
            _In_ double const graphWidth,
            _In_ double const graphHeight) noexcept
        {
            if (!isConnected)
            {
                if (!m_samples.empty())
                {
                    m_samples.clear();
                    Rebuild(Clock::now(), graphWidth, graphHeight);
                }

                return;
            }

            auto const now = Clock::now();

            // Zero means the ping has not been answered yet. Carrying the previous value keeps a
            // gap from reading as a latency collapse to zero.
            auto const milliseconds = ticks == 0 ?
                (m_samples.empty() ? 0.0 : m_samples.back().second) :
                static_cast<double>(ticks) / 10000.0;

            m_samples.push_back({ now, milliseconds });

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
            auto const interval = ::midinetworksetup::AppSettings::Current().RefreshIntervalSeconds();

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
                    PeakText = winrt::hstring{};

                    return;
                }

                auto const peak = std::max_element(
                    m_samples.begin(),
                    m_samples.end(),
                    [](auto const& left, auto const& right) { return left.second < right.second; })->second;

                // A flat trace at the very top would imply the scale means something absolute, so
                // the axis keeps a little headroom above the peak.
                auto const scale = peak > 0.0 ? peak * 1.15 : 1.0;

                // Logarithmic, because a single wake-from-idle spike is often two orders of
                // magnitude above the settled latency and would otherwise flatten the whole trace
                // onto the baseline. log1p keeps zero at the baseline instead of diverging.
                auto const scaleLog = std::log1p(scale);

                auto const window = WindowSeconds();

                for (auto const& sample : m_samples)
                {
                    auto const age = std::chrono::duration<double>{ now - sample.first }.count();

                    auto x = graphWidth - ((age / window) * graphWidth);

                    x = std::clamp(x, 0.0, graphWidth);

                    auto const normalized = scaleLog > 0.0 ?
                        std::clamp(std::log1p(sample.second) / scaleLog, 0.0, 1.0) : 0.0;

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

                PeakText = winrt::hstring{ std::format(L"peak {:.2f} ms, log scale", peak) };
            }
            catch (...)
            {
            }
        }

        std::deque<std::pair<Clock::time_point, double>> m_samples{};
    };

    struct PendingInvitationItem : PendingInvitationItemT<PendingInvitationItem>
    {
        PendingInvitationItem() = default;

        winrt::hstring HostId() const noexcept { return m_hostId; }
        winrt::hstring RemoteName() const noexcept { return m_remoteName; }
        winrt::hstring RemoteProductInstanceId() const noexcept { return m_remoteProductInstanceId; }

        winrt::hstring Headline() const noexcept { return m_headline; }
        winrt::hstring Detail() const noexcept { return m_detail; }

        bool IsBusy() const noexcept { return m_isBusy; }
        void IsBusy(bool const value) noexcept { UpdateField(m_isBusy, value, L"IsBusy"); }

        void InternalInitialize(
            _In_ winrt::hstring const& hostId,
            _In_ winrt::hstring const& remoteName,
            _In_ winrt::hstring const& remoteProductInstanceId) noexcept
        {
            m_hostId = hostId;
            m_remoteName = remoteName;
            m_remoteProductInstanceId = remoteProductInstanceId;
        }

        void InternalUpdateText(
            _In_ winrt::hstring const& headline,
            _In_ winrt::hstring const& detail) noexcept
        {
            UpdateField(m_headline, headline, L"Headline");
            UpdateField(m_detail, detail, L"Detail");
        }

    private:
        winrt::hstring m_hostId{};
        winrt::hstring m_remoteName{};
        winrt::hstring m_remoteProductInstanceId{};
        winrt::hstring m_headline{};
        winrt::hstring m_detail{};
        bool m_isBusy{ false };

        MIDI_NETSETUP_OBSERVABLE_ITEM()
    };


    struct RemoteHostItem : RemoteHostItemT<RemoteHostItem>
    {
        RemoteHostItem() = default;

        winrt::hstring MatchKey() const noexcept { return m_matchKey; }

        winrt::hstring DisplayName() const noexcept { return m_displayName; }
        winrt::hstring SubtitleText() const noexcept { return m_subtitleText; }
        winrt::hstring ProductInstanceId() const noexcept { return m_productInstanceId; }
        winrt::hstring AddressesText() const noexcept { return m_addressesText; }
        winrt::hstring DeviceId() const noexcept { return m_deviceId; }
        winrt::hstring ConnectAddress() const noexcept { return m_connectAddress; }
        uint16_t ConnectPort() const noexcept { return m_connectPort; }
        winrt::hstring StatusText() const noexcept { return m_statusText; }
        winrt::hstring StatisticsText() const noexcept { return m_statisticsText; }
        winrt::hstring EndpointDeviceId() const noexcept { return m_endpointDeviceId; }
        winrt::hstring ClientId() const noexcept { return m_clientId; }

        bool IsConnected() const noexcept { return m_isConnected; }
        bool IsConfigured() const noexcept { return m_isConfigured; }
        winrt::hstring DisconnectLabel() const noexcept { return m_disconnectLabel; }
        bool IsAdvertised() const noexcept { return m_isAdvertised; }

        bool IsBusy() const noexcept { return m_isBusy; }
        void IsBusy(bool const value) noexcept
        {
            if (UpdateField(m_isBusy, value, L"IsBusy"))
            {
                RaiseButtonVisibilities();
            }
        }

        winrt::Microsoft::UI::Xaml::Visibility ConnectVisibility() const noexcept
        {
            return (!m_isConfigured && !m_isBusy) ?
                winrt::Microsoft::UI::Xaml::Visibility::Visible :
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed;
        }

        // a configured entry which is not in a session can be re-armed rather than recreated
        winrt::Microsoft::UI::Xaml::Visibility RetryVisibility() const noexcept
        {
            return (m_isConfigured && !m_isConnected && !m_isBusy) ?
                winrt::Microsoft::UI::Xaml::Visibility::Visible :
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed;
        }

        winrt::Microsoft::UI::Xaml::Visibility DisconnectVisibility() const noexcept
        {
            return (m_isConfigured && !m_isBusy) ?
                winrt::Microsoft::UI::Xaml::Visibility::Visible :
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed;
        }

        winrt::Microsoft::UI::Xaml::Visibility NotAdvertisedVisibility() const noexcept
        {
            return m_isAdvertised ?
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed :
                winrt::Microsoft::UI::Xaml::Visibility::Visible;
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

        winrt::Microsoft::UI::Xaml::Visibility LatencyGraphVisibility() const noexcept
        {
            return m_latency.HasSamples() ?
                winrt::Microsoft::UI::Xaml::Visibility::Visible :
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed;
        }

        winrt::Microsoft::UI::Xaml::Media::PointCollection LatencyLinePoints() const noexcept
        {
            return m_latency.LinePoints;
        }

        winrt::Microsoft::UI::Xaml::Media::PointCollection LatencyFillPoints() const noexcept
        {
            return m_latency.FillPoints;
        }

        winrt::hstring LatencyPeakText() const noexcept { return m_latency.PeakText; }

        void InternalInitialize(_In_ winrt::hstring const& matchKey) noexcept
        {
            m_matchKey = matchKey;
        }

        void InternalUpdate(
            _In_ winrt::hstring const& displayName,
            _In_ winrt::hstring const& subtitleText,
            _In_ winrt::hstring const& productInstanceId,
            _In_ winrt::hstring const& addressesText,
            _In_ winrt::hstring const& deviceId,
            _In_ winrt::hstring const& connectAddress,
            _In_ uint16_t const connectPort,
            _In_ winrt::hstring const& statusText,
            _In_ winrt::hstring const& statisticsText,
            _In_ winrt::hstring const& endpointDeviceId,
            _In_ winrt::hstring const& clientId,
            _In_ uint64_t const latencyTicks,
            _In_ bool const isConnected,
            _In_ bool const isConfigured,
            _In_ bool const isAdvertised,
            _In_ winrt::hstring const& disconnectLabel) noexcept
        {
            UpdateField(m_displayName, displayName, L"DisplayName");
            UpdateField(m_subtitleText, subtitleText, L"SubtitleText");
            UpdateField(m_productInstanceId, productInstanceId, L"ProductInstanceId");
            UpdateField(m_addressesText, addressesText, L"AddressesText");
            UpdateField(m_deviceId, deviceId, L"DeviceId");
            UpdateField(m_connectAddress, connectAddress, L"ConnectAddress");
            UpdateField(m_connectPort, connectPort, L"ConnectPort");
            UpdateField(m_statusText, statusText, L"StatusText");
            UpdateField(m_statisticsText, statisticsText, L"StatisticsText");

            if (UpdateField(m_endpointDeviceId, endpointDeviceId, L"EndpointDeviceId"))
            {
                RaisePropertyChanged(L"EndpointDeviceIdVisibility");
            }

            UpdateField(m_clientId, clientId, L"ClientId");
            UpdateField(m_disconnectLabel, disconnectLabel, L"DisconnectLabel");

            auto const connectedChanged = UpdateField(m_isConnected, isConnected, L"IsConnected");
            auto const configuredChanged = UpdateField(m_isConfigured, isConfigured, L"IsConfigured");

            if (UpdateField(m_isAdvertised, isAdvertised, L"IsAdvertised"))
            {
                RaisePropertyChanged(L"NotAdvertisedVisibility");
            }

            if (connectedChanged)
            {
                RaisePropertyChanged(L"ConnectedBadgeVisibility");
            }

            RecordLatencySample(latencyTicks, isConnected);

            if (connectedChanged || configuredChanged)
            {
                RaiseButtonVisibilities();
            }
        }

    private:
        // must match the sparkline's size in the remote host item template
        static constexpr double LatencyGraphWidth = 360.0;
        static constexpr double LatencyGraphHeight = 44.0;

        void RecordLatencySample(_In_ uint64_t const ticks, _In_ bool const isConnected) noexcept
        {
            m_latency.Record(ticks, isConnected, LatencyGraphWidth, LatencyGraphHeight);

            RaiseLatencyProperties();
        }

        void RaiseLatencyProperties() noexcept
        {
            RaisePropertyChanged(L"LatencyLinePoints");
            RaisePropertyChanged(L"LatencyFillPoints");
            RaisePropertyChanged(L"LatencyPeakText");
            RaisePropertyChanged(L"LatencyGraphVisibility");
        }

        void RaiseButtonVisibilities() noexcept
        {
            RaisePropertyChanged(L"ConnectVisibility");
            RaisePropertyChanged(L"RetryVisibility");
            RaisePropertyChanged(L"DisconnectVisibility");
        }

        winrt::hstring m_matchKey{};
        winrt::hstring m_displayName{};
        winrt::hstring m_subtitleText{};
        winrt::hstring m_productInstanceId{};
        winrt::hstring m_addressesText{};
        winrt::hstring m_deviceId{};
        winrt::hstring m_connectAddress{};
        uint16_t m_connectPort{ 0 };
        winrt::hstring m_statusText{};
        winrt::hstring m_statisticsText{};

        LatencyHistory m_latency{};

        winrt::hstring m_endpointDeviceId{};
        winrt::hstring m_clientId{};
        bool m_isConnected{ false };
        bool m_isConfigured{ false };
        winrt::hstring m_disconnectLabel{};
        bool m_isAdvertised{ false };
        bool m_isBusy{ false };

        MIDI_NETSETUP_OBSERVABLE_ITEM()
    };


    struct HostConnectionItem : HostConnectionItemT<HostConnectionItem>
    {
        HostConnectionItem() = default;

        winrt::hstring MatchKey() const noexcept { return m_matchKey; }
        winrt::hstring HostId() const noexcept { return m_hostId; }

        winrt::hstring DisplayName() const noexcept { return m_displayName; }
        winrt::hstring ProductInstanceId() const noexcept { return m_productInstanceId; }
        winrt::hstring AddressText() const noexcept { return m_addressText; }
        winrt::hstring StatusText() const noexcept { return m_statusText; }
        winrt::hstring StatisticsText() const noexcept { return m_statisticsText; }

        bool IsPendingApproval() const noexcept { return m_isPendingApproval; }

        winrt::Microsoft::UI::Xaml::Visibility ConnectedBadgeVisibility() const noexcept
        {
            return m_isSessionActive ?
                winrt::Microsoft::UI::Xaml::Visibility::Visible :
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed;
        }

        winrt::Microsoft::UI::Xaml::Visibility LatencyGraphVisibility() const noexcept
        {
            return m_latency.HasSamples() ?
                winrt::Microsoft::UI::Xaml::Visibility::Visible :
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed;
        }

        winrt::Microsoft::UI::Xaml::Media::PointCollection LatencyLinePoints() const noexcept
        {
            return m_latency.LinePoints;
        }

        winrt::Microsoft::UI::Xaml::Media::PointCollection LatencyFillPoints() const noexcept
        {
            return m_latency.FillPoints;
        }

        winrt::hstring LatencyPeakText() const noexcept { return m_latency.PeakText; }

        bool IsBusy() const noexcept { return m_isBusy; }
        void IsBusy(bool const value) noexcept { UpdateField(m_isBusy, value, L"IsBusy"); }

        void InternalInitialize(
            _In_ winrt::hstring const& matchKey,
            _In_ winrt::hstring const& hostId,
            _In_ winrt::hstring const& productInstanceId) noexcept
        {
            m_matchKey = matchKey;
            m_hostId = hostId;
            m_productInstanceId = productInstanceId;
        }

        void InternalUpdate(
            _In_ winrt::hstring const& displayName,
            _In_ winrt::hstring const& addressText,
            _In_ winrt::hstring const& statusText,
            _In_ winrt::hstring const& statisticsText,
            _In_ uint64_t const latencyTicks,
            _In_ bool const isSessionActive,
            _In_ bool const isPendingApproval) noexcept
        {
            UpdateField(m_displayName, displayName, L"DisplayName");
            UpdateField(m_addressText, addressText, L"AddressText");
            UpdateField(m_statusText, statusText, L"StatusText");
            UpdateField(m_statisticsText, statisticsText, L"StatisticsText");
            UpdateField(m_isPendingApproval, isPendingApproval, L"IsPendingApproval");

            if (UpdateField(m_isSessionActive, isSessionActive, L"IsSessionActive"))
            {
                RaisePropertyChanged(L"ConnectedBadgeVisibility");
            }

            m_latency.Record(latencyTicks, isSessionActive, LatencyGraphWidth, LatencyGraphHeight);

            RaisePropertyChanged(L"LatencyLinePoints");
            RaisePropertyChanged(L"LatencyFillPoints");
            RaisePropertyChanged(L"LatencyPeakText");
            RaisePropertyChanged(L"LatencyGraphVisibility");
        }

    private:
        // Narrower than the remote host rows: these sit nested inside a host card. Must match the
        // sparkline's size in the connection item template.
        static constexpr double LatencyGraphWidth = 240.0;
        static constexpr double LatencyGraphHeight = 32.0;

        winrt::hstring m_matchKey{};
        winrt::hstring m_hostId{};
        winrt::hstring m_displayName{};
        winrt::hstring m_productInstanceId{};
        winrt::hstring m_addressText{};
        winrt::hstring m_statusText{};
        winrt::hstring m_statisticsText{};

        LatencyHistory m_latency{};

        bool m_isPendingApproval{ false };
        bool m_isSessionActive{ false };
        bool m_isBusy{ false };

        MIDI_NETSETUP_OBSERVABLE_ITEM()
    };


    struct KnownClientItem : KnownClientItemT<KnownClientItem>
    {
        KnownClientItem() = default;

        winrt::hstring MatchKey() const noexcept { return m_matchKey; }
        winrt::hstring HostId() const noexcept { return m_hostId; }

        winrt::hstring DisplayName() const noexcept { return m_displayName; }
        winrt::hstring ProductInstanceId() const noexcept { return m_productInstanceId; }
        winrt::hstring DecisionText() const noexcept { return m_decisionText; }

        bool IsAllowed() const noexcept { return m_isAllowed; }

        void InternalInitialize(
            _In_ winrt::hstring const& matchKey,
            _In_ winrt::hstring const& hostId,
            _In_ winrt::hstring const& displayName,
            _In_ winrt::hstring const& productInstanceId,
            _In_ winrt::hstring const& decisionText,
            _In_ bool const isAllowed) noexcept
        {
            m_matchKey = matchKey;
            m_hostId = hostId;
            m_displayName = displayName;
            m_productInstanceId = productInstanceId;
            m_decisionText = decisionText;
            m_isAllowed = isAllowed;
        }

    private:
        winrt::hstring m_matchKey{};
        winrt::hstring m_hostId{};
        winrt::hstring m_displayName{};
        winrt::hstring m_productInstanceId{};
        winrt::hstring m_decisionText{};
        bool m_isAllowed{ true };

        MIDI_NETSETUP_OBSERVABLE_ITEM()
    };


    struct LocalHostItem : LocalHostItemT<LocalHostItem>
    {
        LocalHostItem();

        winrt::hstring HostId() const noexcept { return m_hostId; }

        winrt::hstring DisplayName() const noexcept { return m_displayName; }
        winrt::hstring ServiceInstanceName() const noexcept { return m_serviceInstanceName; }
        winrt::hstring ProductInstanceId() const noexcept { return m_productInstanceId; }
        winrt::hstring AddressText() const noexcept { return m_addressText; }
        winrt::hstring PortText() const noexcept { return m_portText; }
        winrt::hstring StatusText() const noexcept { return m_statusText; }
        winrt::hstring PolicyText() const noexcept { return m_policyText; }
        winrt::hstring ConnectionCountText() const noexcept { return m_connectionCountText; }
        winrt::hstring StartStopLabel() const noexcept { return m_startStopLabel; }

        bool HasStarted() const noexcept { return m_hasStarted; }
        bool CreatesMidi1Ports() const noexcept { return m_createsMidi1Ports; }

        bool IsBusy() const noexcept { return m_isBusy; }
        void IsBusy(bool const value) noexcept { UpdateField(m_isBusy, value, L"IsBusy"); }

        winrt::Microsoft::UI::Xaml::Visibility NoConnectionsVisibility() const noexcept
        {
            return m_connections.Size() == 0 ?
                winrt::Microsoft::UI::Xaml::Visibility::Visible :
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed;
        }

        winrt::Microsoft::UI::Xaml::Visibility NoKnownClientsVisibility() const noexcept
        {
            return m_knownClients.Size() == 0 ?
                winrt::Microsoft::UI::Xaml::Visibility::Visible :
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed;
        }

        winrt::Microsoft::UI::Xaml::Visibility KnownClientsVisibility() const noexcept
        {
            return m_knownClients.Size() == 0 ?
                winrt::Microsoft::UI::Xaml::Visibility::Collapsed :
                winrt::Microsoft::UI::Xaml::Visibility::Visible;
        }

        winrt::Windows::Foundation::Collections::IObservableVector<midinetworksetup::HostConnectionItem> Connections() const noexcept
        {
            return m_connections;
        }

        winrt::Windows::Foundation::Collections::IObservableVector<midinetworksetup::KnownClientItem> KnownClients() const noexcept
        {
            return m_knownClients;
        }

        void InternalInitialize(_In_ winrt::hstring const& hostId) noexcept
        {
            m_hostId = hostId;
        }

        void InternalUpdate(
            _In_ winrt::hstring const& displayName,
            _In_ winrt::hstring const& serviceInstanceName,
            _In_ winrt::hstring const& productInstanceId,
            _In_ winrt::hstring const& addressText,
            _In_ winrt::hstring const& portText,
            _In_ winrt::hstring const& statusText,
            _In_ winrt::hstring const& policyText,
            _In_ winrt::hstring const& connectionCountText,
            _In_ winrt::hstring const& startStopLabel,
            _In_ bool const hasStarted,
            _In_ bool const createsMidi1Ports) noexcept
        {
            UpdateField(m_displayName, displayName, L"DisplayName");
            UpdateField(m_serviceInstanceName, serviceInstanceName, L"ServiceInstanceName");
            UpdateField(m_productInstanceId, productInstanceId, L"ProductInstanceId");
            UpdateField(m_addressText, addressText, L"AddressText");
            UpdateField(m_portText, portText, L"PortText");
            UpdateField(m_statusText, statusText, L"StatusText");
            UpdateField(m_policyText, policyText, L"PolicyText");
            UpdateField(m_connectionCountText, connectionCountText, L"ConnectionCountText");
            UpdateField(m_startStopLabel, startStopLabel, L"StartStopLabel");
            UpdateField(m_hasStarted, hasStarted, L"HasStarted");
            UpdateField(m_createsMidi1Ports, createsMidi1Ports, L"CreatesMidi1Ports");
        }

        // the empty state placeholders are computed from the collections, so they only change
        // when something is added or removed
        void InternalRaiseEmptyStateChanged() noexcept
        {
            RaisePropertyChanged(L"NoConnectionsVisibility");
            RaisePropertyChanged(L"NoKnownClientsVisibility");
            RaisePropertyChanged(L"KnownClientsVisibility");
        }

    private:
        winrt::hstring m_hostId{};
        winrt::hstring m_displayName{};
        winrt::hstring m_serviceInstanceName{};
        winrt::hstring m_productInstanceId{};
        winrt::hstring m_addressText{};
        winrt::hstring m_portText{};
        winrt::hstring m_statusText{};
        winrt::hstring m_policyText{};
        winrt::hstring m_connectionCountText{};
        winrt::hstring m_startStopLabel{};
        bool m_hasStarted{ false };
        bool m_createsMidi1Ports{ false };
        bool m_isBusy{ false };

        winrt::Windows::Foundation::Collections::IObservableVector<midinetworksetup::HostConnectionItem> m_connections{
            winrt::single_threaded_observable_vector<midinetworksetup::HostConnectionItem>() };

        winrt::Windows::Foundation::Collections::IObservableVector<midinetworksetup::KnownClientItem> m_knownClients{
            winrt::single_threaded_observable_vector<midinetworksetup::KnownClientItem>() };

        MIDI_NETSETUP_OBSERVABLE_ITEM()
    };
}

namespace winrt::midinetworksetup::factory_implementation
{
    struct PendingInvitationItem : PendingInvitationItemT<PendingInvitationItem, implementation::PendingInvitationItem> {};
    struct RemoteHostItem : RemoteHostItemT<RemoteHostItem, implementation::RemoteHostItem> {};
    struct HostConnectionItem : HostConnectionItemT<HostConnectionItem, implementation::HostConnectionItem> {};
    struct KnownClientItem : KnownClientItemT<KnownClientItem, implementation::KnownClientItem> {};
    struct LocalHostItem : LocalHostItemT<LocalHostItem, implementation::LocalHostItem> {};
}
