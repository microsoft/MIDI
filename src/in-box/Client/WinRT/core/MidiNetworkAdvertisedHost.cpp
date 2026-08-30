// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkAdvertisedHost.h"
#include "Transports.Network.MidiNetworkAdvertisedHost.g.cpp"


namespace winrt::Windows::Devices::Midi2::Transports::Network::implementation
{
    _Use_decl_annotations_
    void MidiNetworkAdvertisedHost::InternalInitializeFromDnssdService(
        ::WindowsMidiServicesInternal::MidiDnssdService const& service
    ) noexcept
    {
        try
        {
            m_deviceId = winrt::hstring{ service.DeviceId() };
            m_fullName = winrt::hstring{ service.FullName };
            m_serviceInstanceName = winrt::hstring{ service.ServiceInstanceName };
            m_serviceType = winrt::hstring{ service.ServiceType };
            m_hostName = winrt::hstring{ service.HostName };
            m_port = service.Port;
            m_domain = winrt::hstring{ service.Domain };
            m_umpEndpointName = winrt::hstring{ service.UmpEndpointName() };
            m_productInstanceId = winrt::hstring{ service.ProductInstanceId() };

            // The advertised endpoint name is the only name a remote gives us, and it is more
            // use to a person than the DNS-SD label.
            m_deviceName = m_umpEndpointName.empty() ? m_serviceInstanceName : m_umpEndpointName;

            m_lastSeenTime = winrt::clock::now();

            m_textAttributes.Clear();

            for (auto const& attribute : service.TextAttributes)
            {
                m_textAttributes.Insert(winrt::hstring{ attribute.first }, winrt::hstring{ attribute.second });
            }

            m_ipAddresses.Clear();
            m_ipv4Addresses.Clear();
            m_ipv6Addresses.Clear();

            for (auto const& address : service.IPv4Addresses)
            {
                m_ipv4Addresses.Append(winrt::hstring{ address });
                m_ipAddresses.Append(winrt::hstring{ address });
            }

            for (auto const& address : service.IPv6Addresses)
            {
                m_ipv6Addresses.Append(winrt::hstring{ address });
                m_ipAddresses.Append(winrt::hstring{ address });
            }
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception building an advertised host", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );
        }
    }
}
