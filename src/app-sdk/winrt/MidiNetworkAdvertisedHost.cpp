// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiNetworkAdvertisedHost.h"
#include "Endpoints.Network.MidiNetworkAdvertisedHost.g.cpp"


namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Network::implementation
{

    _Use_decl_annotations_
    void MidiNetworkAdvertisedHost::InternalInitialize(
        winrt::hstring deviceId,
        winrt::hstring deviceName,
        winrt::hstring fullName,
        winrt::hstring serviceInstanceName,
        winrt::hstring serviceType,
        winrt::hstring hostName,
        uint16_t port,
        winrt::hstring domain,
        winrt::hstring umpEndpointName,
        winrt::hstring productInstanceId,
        collections::IVector<winrt::hstring> ipAddresses
    ) noexcept
    {
        m_deviceId = deviceId;
        m_deviceName = deviceName;
        m_fullName = fullName;
        m_serviceInstanceName = serviceInstanceName;
        m_serviceType = serviceType;
        m_hostName = hostName;
        m_port = port;
        m_domain = domain;
        m_umpEndpointName = umpEndpointName;
        m_productInstanceId = productInstanceId;

        m_ipAddresses.Clear();

        for (auto const& ip : ipAddresses)
        {
            m_ipAddresses.Append(ip);
        }
    }


    _Use_decl_annotations_
    void MidiNetworkAdvertisedHost::InternalUpdateFromDeviceInformation(
        enumeration::DeviceInformation device
    ) noexcept
    {
        //props.Append(L"System.Devices.AepService.ProtocolId");  // guid
        //props.Append(L"System.Devices.Dnssd.HostName");         // string
        //props.Append(L"System.Devices.Dnssd.FullName");         // string
        //props.Append(L"System.Devices.Dnssd.ServiceName");      // string
        //props.Append(L"System.Devices.Dnssd.Domain");           // string
        //props.Append(L"System.Devices.Dnssd.InstanceName");     // string
        //props.Append(L"System.Devices.IpAddress");              // multivalue string
        //props.Append(L"System.Devices.Dnssd.PortNumber");       // uint16_t
        //props.Append(L"System.Devices.Dnssd.TextAttributes");   // multivalue string

        auto ipAddresses = winrt::single_threaded_vector<winrt::hstring>();

        if (device.Properties().HasKey(L"System.Devices.IpAddress"))
        {
            auto prop = device.Properties().Lookup(L"System.Devices.IpAddress").as<foundation::IReferenceArray<winrt::hstring>>();
            winrt::com_array<winrt::hstring> array;
            prop.GetStringArray(array);

            for (auto const& ip : array)
            {
                ipAddresses.Append(ip);
            }
        }

        winrt::hstring umpEndpointName{ };
        winrt::hstring productInstanceId{ };

        if (device.Properties().HasKey(L"System.Devices.Dnssd.TextAttributes"))
        {
            auto txtEntryProp = device.Properties().Lookup(L"System.Devices.Dnssd.TextAttributes").as<foundation::IReferenceArray<winrt::hstring>>();
            winrt::com_array<winrt::hstring> txtEntryArray;
            txtEntryProp.GetStringArray(txtEntryArray);

            for (auto const& txt : txtEntryArray)
            {
                // TODO: we potentially lose info here by converting from a wide string. Need to check spec to see if ascii-only.
                auto s = winrt::to_string(txt);

                // split on the = sign
                std::string name = s.substr(0, s.find("="));
                std::string value = s.substr(s.find("=") + 1);

                if (name == "UMPEndpointName")
                {
                    umpEndpointName = winrt::to_hstring(value);
                }
                else if (name == "ProductInstanceId")
                {
                    productInstanceId = winrt::to_hstring(value);
                }
            }

        }

        InternalInitialize(
            device.Id(),
            device.Name(),
            internal::GetDeviceInfoProperty<winrt::hstring>(device.Properties(), L"System.Devices.Dnssd.FullName", L""),
            internal::GetDeviceInfoProperty<winrt::hstring>(device.Properties(), L"System.Devices.Dnssd.InstanceName", L""),
            internal::GetDeviceInfoProperty<winrt::hstring>(device.Properties(), L"System.Devices.Dnssd.ServiceName", L""),
            internal::GetDeviceInfoProperty<winrt::hstring>(device.Properties(), L"System.Devices.Dnssd.HostName", L""),
            internal::GetDeviceInfoProperty<uint16_t>(device.Properties(), L"System.Devices.Dnssd.PortNumber", 0),
            internal::GetDeviceInfoProperty<winrt::hstring>(device.Properties(), L"System.Devices.Dnssd.Domain", L""),
            umpEndpointName,
            productInstanceId,
            ipAddresses
        );

    }


}
