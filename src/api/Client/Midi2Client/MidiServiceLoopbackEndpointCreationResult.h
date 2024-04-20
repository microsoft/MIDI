// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiServiceLoopbackEndpointCreationResult.g.h"

namespace MIDI_CPP_NAMESPACE::implementation
{
    struct MidiServiceLoopbackEndpointCreationResult : MidiServiceLoopbackEndpointCreationResultT<MidiServiceLoopbackEndpointCreationResult>
    {
        MidiServiceLoopbackEndpointCreationResult() = default;

        bool Success() const { return m_success; }

        winrt::guid AssociationId()  const { return m_associationId; }
        winrt::hstring EndpointDeviceIdA() const { return m_endpointDeviceIdA; }
        winrt::hstring EndpointDeviceIdB()  const { return m_endpointDeviceIdB; }


        void SetSuccess(
            _In_ winrt::guid associationId,
            _In_ winrt::hstring endpointDeviceIdA,
            _In_ winrt::hstring endpointDeviceIdB
            )
        {
            m_success = true;
            m_associationId = associationId;
            m_endpointDeviceIdA = endpointDeviceIdA;
            m_endpointDeviceIdB = endpointDeviceIdB;
        }

    private:
        bool m_success{ false };
        winrt::guid m_associationId{};
        winrt::hstring m_endpointDeviceIdA{};
        winrt::hstring m_endpointDeviceIdB{};



    };
}
