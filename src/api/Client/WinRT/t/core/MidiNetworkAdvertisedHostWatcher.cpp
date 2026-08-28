// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkAdvertisedHostWatcher.h"
#include "Transports.Network.MidiNetworkAdvertisedHostWatcher.g.cpp"

#include "MidiNetworkAdvertisedHost.h"
#include "MidiNetworkAdvertisedHostAddedEventArgs.h"
#include "MidiNetworkAdvertisedHostRemovedEventArgs.h"
#include "MidiNetworkAdvertisedHostUpdatedEventArgs.h"


namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    MidiNetworkAdvertisedHostWatcher::~MidiNetworkAdvertisedHostWatcher()
    {
        try
        {
            m_browser.Stop();
            m_enumeratedHosts.Clear();
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"exception shutting down the browser", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

        }
    }


    network::MidiNetworkAdvertisedHostWatcher MidiNetworkAdvertisedHostWatcher::Create() noexcept
    {
        try
        {
            auto watcher = winrt::make_self<MidiNetworkAdvertisedHostWatcher>();

            return *watcher;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception creating MidiNetworkAdvertisedHostWatcher.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return nullptr;
        }
    }


    void MidiNetworkAdvertisedHostWatcher::Start() noexcept
    {
        try
        {
            m_enumeratedHosts.Clear();
            m_enumerationCompletedRaised = false;

            auto const hr = m_browser.Start(
                std::wstring{ network::MidiNetworkTransportManager::MidiNetworkUdpDnsSdQueryName() },
                [this](::WindowsMidiServicesInternal::MidiDnssdService const& service) { OnServiceAdded(service); },
                [this](::WindowsMidiServicesInternal::MidiDnssdService const& service, uint32_t const changed) { OnServiceUpdated(service, changed); },
                [this](std::wstring const& fullName, std::wstring const& deviceId) { OnServiceRemoved(fullName, deviceId); });

            if (FAILED(hr))
            {
                LOG_IF_FAILED(hr);

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Unable to start the DNS-SD browse", MIDI_SDK_TRACE_MESSAGE_FIELD)
                );

                return;
            }

            ScheduleEnumerationCompleted();
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);
        }
    }

    void MidiNetworkAdvertisedHostWatcher::Stop() noexcept
    {
        try
        {
            if (!m_browser.IsRunning()) return;

            m_browser.Stop();

            if (m_stoppedEvent) m_stoppedEvent(*this, nullptr);
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);
        }
    }


    bool MidiNetworkAdvertisedHostWatcher::IsStarted() noexcept
    {
        try
        {
            return m_browser.IsRunning();
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);
        }

        return false;
    }

    _Use_decl_annotations_
    winrt::event_token MidiNetworkAdvertisedHostWatcher::Added(
        foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, network::MidiNetworkAdvertisedHostAddedEventArgs> const& handler)
    {
        return m_deviceAddedEvent.add(handler);
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostWatcher::Added(winrt::event_token const& token) noexcept
    {
        m_deviceAddedEvent.remove(token);
    }


    _Use_decl_annotations_
    winrt::event_token MidiNetworkAdvertisedHostWatcher::Removed(
        foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, network::MidiNetworkAdvertisedHostRemovedEventArgs> const& handler)
    {
        return m_deviceRemovedEvent.add(handler);
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostWatcher::Removed(
        winrt::event_token const& token) noexcept
    {
        m_deviceRemovedEvent.remove(token);
    }


    _Use_decl_annotations_
    winrt::event_token MidiNetworkAdvertisedHostWatcher::Updated(
        foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, network::MidiNetworkAdvertisedHostUpdatedEventArgs> const& handler)
    {
        return m_deviceUpdatedEvent.add(handler);
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostWatcher::Updated(
        winrt::event_token const& token) noexcept
    {
        m_deviceUpdatedEvent.remove(token);
    }


    _Use_decl_annotations_
    winrt::event_token MidiNetworkAdvertisedHostWatcher::EnumerationCompleted(
        foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, foundation::IInspectable> const& handler)
    {
        return m_enumerationCompletedEvent.add(handler);
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostWatcher::EnumerationCompleted(
        winrt::event_token const& token) noexcept
    {
        m_enumerationCompletedEvent.remove(token);
    }


    _Use_decl_annotations_
    winrt::event_token MidiNetworkAdvertisedHostWatcher::Stopped(
        foundation::TypedEventHandler<network::MidiNetworkAdvertisedHostWatcher, foundation::IInspectable> const& handler)
    {
        return m_stoppedEvent.add(handler);
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostWatcher::Stopped(
        winrt::event_token const& token) noexcept
    {
        m_stoppedEvent.remove(token);
    }




    _Use_decl_annotations_
    network::MidiNetworkAdvertisedHost MidiNetworkAdvertisedHostWatcher::BuildHost(
        ::WindowsMidiServicesInternal::MidiDnssdService const& service) noexcept
    {
        auto host = winrt::make_self<network::implementation::MidiNetworkAdvertisedHost>();

        host->InternalInitializeFromDnssdService(service);

        return *host;
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostWatcher::OnServiceAdded(
        ::WindowsMidiServicesInternal::MidiDnssdService const& service) noexcept
    {
        try
        {
            auto host = BuildHost(service);

            m_enumeratedHosts.Insert(host.DeviceId(), host);

            if (m_deviceAddedEvent)
            {
                auto newArgs = winrt::make_self<network::implementation::MidiNetworkAdvertisedHostAddedEventArgs>();
                newArgs->InternalInitialize(host);

                m_deviceAddedEvent(*this, *newArgs);
            }
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception in Added event, likely thrown by the application using this API", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
        }
    }

    // Only raised when the advertised content actually changed, and it says what changed. The
    // old device enumeration backing fired this on every re-announcement, which made it useless.
    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostWatcher::OnServiceUpdated(
        ::WindowsMidiServicesInternal::MidiDnssdService const& service,
        uint32_t const changedFields) noexcept
    {
        try
        {
            auto host = BuildHost(service);

            m_enumeratedHosts.Insert(host.DeviceId(), host);

            if (m_deviceUpdatedEvent)
            {
                auto newArgs = winrt::make_self<network::implementation::MidiNetworkAdvertisedHostUpdatedEventArgs>();

                newArgs->InternalInitialize(
                    host.DeviceId(),
                    static_cast<network::MidiNetworkAdvertisedHostChangedProperties>(changedFields),
                    host);

                m_deviceUpdatedEvent(*this, *newArgs);
            }
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"exception in Updated event, likely thrown by the application using this API", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
        }
    }

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHostWatcher::OnServiceRemoved(
        std::wstring const& fullName,
        std::wstring const& deviceId) noexcept
    {
        try
        {
            winrt::hstring const key{ deviceId };

            if (m_enumeratedHosts.HasKey(key))
            {
                m_enumeratedHosts.Remove(key);
            }

            if (m_deviceRemovedEvent)
            {
                auto newArgs = winrt::make_self<network::implementation::MidiNetworkAdvertisedHostRemovedEventArgs>();
                newArgs->InternalInitialize(key, winrt::hstring{ fullName });

                m_deviceRemovedEvent(*this, *newArgs);
            }
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"exception in Removed event, likely thrown by the application using this API", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
        }
    }

    void MidiNetworkAdvertisedHostWatcher::ScheduleEnumerationCompleted() noexcept
    {
        try
        {
            auto strongThis = get_strong();

            std::thread([strongThis, this]()
                {
                    // A responder answers a fresh query almost immediately. This is a settling
                    // period, not a scan: the browse keeps running afterwards.
                    std::this_thread::sleep_for(std::chrono::seconds(2));

                    if (m_enumerationCompletedRaised.exchange(true)) return;

                    try
                    {
                        if (m_enumerationCompletedEvent) m_enumerationCompletedEvent(*this, nullptr);
                    }
                    catch (...)
                    {
                        LOG_IF_FAILED(E_FAIL);
                    }
                }).detach();
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);
        }
    }
}
