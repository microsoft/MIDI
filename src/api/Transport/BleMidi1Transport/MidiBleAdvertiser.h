// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class MidiBleAdvertiser
{
public:
    HRESULT Initialize();

    HRESULT Advertise(

    );

    HRESULT Shutdown();

private:
    //winrt::Windows::Networking::ServiceDiscovery::Dnssd::DnssdServiceInstance m_serviceInstance{ nullptr };
    bt::Advertisement::BluetoothLEAdvertisementPublisher m_publisher{ nullptr };


};