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
    _Use_decl_annotations_
    void MidiVirtualDeviceManager::Initialize(winrt::com_ptr<IMidiAbstraction> serviceAbstraction)
    { 
        m_serviceAbstraction = serviceAbstraction; 

        // todo: create the virtual device abstraction


    }

    _Use_decl_annotations_
    bool MidiVirtualDeviceManager::CreateDevice(
        winrt::hstring const& /*name*/,
        bool /*exclusive*/,
        foundation::IInspectable const& /*tag*/,
        winrt::hstring& /*createdDeviceId*/
    )
    {
        // this is just a test

        throw hresult_not_implemented();

    }

    _Use_decl_annotations_
    bool MidiVirtualDeviceManager::CreateEndpoint(
        winrt::hstring const& /*parentDeviceId*/,
        winrt::hstring const& /*name*/,
        bool /*exclusive*/,
        midi2::MidiProtocol const& /*protocol*/,
        foundation::IInspectable const& /*tag*/,
        winrt::hstring& /*createdEndpointId*/
    )
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    bool MidiVirtualDeviceManager::DeleteDeviceAndEndpoints(
        winrt::hstring const& /*deviceId*/
    )
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    bool MidiVirtualDeviceManager::DeleteEndpoint(
        winrt::hstring const& /*endpointId*/
    )
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    bool MidiVirtualDeviceManager::RouteConnectEndpoints(
        winrt::hstring const& /*outputEndpointId*/,
        winrt::hstring const& /*inputEndpointId*/
    )
    {
        throw hresult_not_implemented();
    }
}
