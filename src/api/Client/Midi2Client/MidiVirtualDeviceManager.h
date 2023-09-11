// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once
#include "MidiVirtualDeviceManager.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiVirtualDeviceManager : MidiVirtualDeviceManagerT<MidiVirtualDeviceManager>
    {
        MidiVirtualDeviceManager() = default;

        bool CreateDevice(
            _In_ winrt::hstring const& name, 
            _In_ bool const exclusive, 
            _In_opt_ winrt::Windows::Foundation::IInspectable const& tag, 
            _Out_ winrt::hstring& createdDeviceId
        );

        bool CreateEndpoint(
            _In_ winrt::hstring const& parentDeviceId,
            _In_ winrt::hstring const& name,
            _In_ midi2::MidiEndpointType const& endpointType,
            _In_ bool exclusive,
            _In_ midi2::MidiProtocol const& protocol,
            _In_opt_ winrt::Windows::Foundation::IInspectable const& tag,
            _Out_ winrt::hstring& createdEndpointId
        );

        bool DeleteDeviceAndEndpoints(
            _In_ winrt::hstring const& deviceId
        );

        bool DeleteEndpoint(
            _In_ winrt::hstring const& endpointId
        );

        bool RouteConnectEndpoints(
            _In_ winrt::hstring const& outputEndpointId,
            _In_ winrt::hstring const& inputEndpointId
        );


        // internal. Called by the session when initializing

        void Initialize(_In_ winrt::com_ptr<IMidiAbstraction> serviceAbstraction);

    private:

        winrt::com_ptr<IMidiAbstraction> m_serviceAbstraction{ nullptr };

    };
}
