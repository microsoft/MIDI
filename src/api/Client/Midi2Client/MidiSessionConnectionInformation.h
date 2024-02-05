#pragma once
#include "MidiSessionConnectionInformation.g.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiSessionConnectionInformation : MidiSessionConnectionInformationT<MidiSessionConnectionInformation>
    {
        MidiSessionConnectionInformation() = default;

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
