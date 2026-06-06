// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiServiceSessionConnectionInfo.g.h"

namespace winrt::Windows::Devices::Midi2::Reporting::implementation
{
    struct MidiServiceSessionConnectionInfo : MidiServiceSessionConnectionInfoT<MidiServiceSessionConnectionInfo>
    {
        MidiServiceSessionConnectionInfo() = default;

        winrt::hstring EndpointDeviceId() const noexcept { return m_endpointDeviceId; }
        uint16_t InstanceCount() const noexcept { return m_instanceCount; }
        foundation::DateTime EarliestConnectionTime() const noexcept { return m_earliestConnectionTime; }

        bool InternalInitialize(
            _In_ winrt::hstring const& endpointDeviceId, 
            _In_ uint16_t instanceCount, 
            _In_ foundation::DateTime earliestConnectionTime) noexcept
        {
            m_endpointDeviceId = internal::NormalizeEndpointInterfaceIdHStringCopy(endpointDeviceId);
            m_instanceCount = instanceCount;
            m_earliestConnectionTime = earliestConnectionTime;

            return true;
        }

    private:
        winrt::hstring m_endpointDeviceId;
        uint16_t m_instanceCount{ 0 };
        foundation::DateTime m_earliestConnectionTime{ };
    };
}
