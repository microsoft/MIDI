// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiServiceSessionConnectionInfo.g.h"

namespace MIDI_CPP_NAMESPACE::implementation
{
    struct MidiServiceSessionConnectionInfo : MidiServiceSessionConnectionInfoT<MidiServiceSessionConnectionInfo>
    {
        MidiServiceSessionConnectionInfo() = default;

        winrt::hstring EndpointDeviceId() { return m_endpointDeviceId; }
        uint16_t InstanceCount() { return m_instanceCount; }
        foundation::DateTime EarliestConnectionTime() { return m_earliestConnectionTime; }


        void InternalInitialize(
            _In_ winrt::hstring const endpointDeviceId,
            _In_ uint16_t const instanceCount,
            _In_ foundation::DateTime const earliestConnectionTime
        );

    private:
        winrt::hstring m_endpointDeviceId{};
        uint16_t m_instanceCount{};
        foundation::DateTime m_earliestConnectionTime{};
    };
}
