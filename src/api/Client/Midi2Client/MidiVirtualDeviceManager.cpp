// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiVirtualDeviceManager.h"
#include "MidiVirtualDeviceManager.g.cpp"

namespace winrt::Windows::Devices::Midi2::implementation
{
    void MidiVirtualDeviceManager::Initialize(winrt::com_ptr<IMidiAbstraction> serviceAbstraction)
    { 
        m_serviceAbstraction = serviceAbstraction; 

        // todo: create the virtual device abstraction


    }



    bool MidiVirtualDeviceManager::CreateDevice(
        hstring const& /*name*/,
        bool /*exclusive*/,
        winrt::Windows::Foundation::IInspectable const& /*tag*/,
        hstring& /*createdDeviceId*/
    )
    {
        // this is just a test

        throw hresult_not_implemented();

    }

    bool MidiVirtualDeviceManager::CreateEndpoint(
        hstring const& /*parentDeviceId*/,
        hstring const& /*name*/,
        winrt::Windows::Devices::Midi2::MidiEndpointType const& /*endpointType*/,
        bool /*exclusive*/,
        winrt::Windows::Devices::Midi2::MidiProtocol const& /*protocol*/,
        winrt::Windows::Foundation::IInspectable const& /*tag*/,
        hstring& /*createdEndpointId*/
    )
    {
        throw hresult_not_implemented();
    }

    bool MidiVirtualDeviceManager::DeleteDeviceAndEndpoints(
        hstring const& /*deviceId*/
    )
    {
        throw hresult_not_implemented();
    }

    bool MidiVirtualDeviceManager::DeleteEndpoint(
        hstring const& /*endpointId*/
    )
    {
        throw hresult_not_implemented();
    }

    bool MidiVirtualDeviceManager::RouteConnectEndpoints(
        hstring const& /*outputEndpointId*/,
        hstring const& /*inputEndpointId*/
    )
    {
        throw hresult_not_implemented();
    }
}
