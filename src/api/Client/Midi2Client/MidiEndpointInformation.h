// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiEndpointInformation.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiEndpointInformation : MidiEndpointInformationT<MidiEndpointInformation>
    {
        MidiEndpointInformation() = default;

        winrt::hstring Name() const noexcept { return m_name; }
        winrt::hstring ProductInstanceId() const noexcept { return m_productInstanceId; }
        uint8_t SpecificationVersionMajor() const noexcept { return m_umpVersionMajor; }
        uint8_t SpecificationVersionMinor() const noexcept { return m_umpVersionMinor; }
        bool HasStaticFunctionBlocks() const noexcept { return m_hasStaticFunctionBlocks; }
        uint8_t FunctionBlockCount() const noexcept { return m_functionBlockCount; }
        bool SupportsMidi10Protocol() const noexcept { return m_supportsMidi10Protocol; }
        bool SupportsMidi20Protocol() const noexcept { return m_supportsMidi20Protocol; }
        bool SupportsReceivingJRTimestamps() const noexcept { return m_supportsReceivingJRTimestamps; }
        bool SupportsSendingJRTimestamps() const noexcept { return m_supportsSendingJRTimestamps; }

        midi2::MidiProtocol ConfiguredProtocol() const noexcept { return m_configuredProtocol; }


        bool UpdateFromJson(_In_ winrt::Windows::Data::Json::JsonObject const json) noexcept;
        bool UpdateFromJsonString(_In_ winrt::hstring const json) noexcept;
        bool UpdateFromMessages(_In_ collections::IIterable<midi2::MidiMessage128> messages) noexcept;
        bool UpdateFromInfoNotificationMessage(_In_ midi2::MidiMessage128 messages) noexcept;
        winrt::hstring GetJsonString() noexcept;


    private:
        winrt::hstring m_name{};
        winrt::hstring m_productInstanceId;
        uint8_t m_umpVersionMajor{ 0 };
        uint8_t m_umpVersionMinor{ 0 };
        bool m_hasStaticFunctionBlocks{ false };
        uint8_t m_functionBlockCount{ 0 };
        bool m_supportsMidi10Protocol{ false };
        bool m_supportsMidi20Protocol{ false };
        bool m_supportsReceivingJRTimestamps{ false };
        bool m_supportsSendingJRTimestamps{ false };

        midi2::MidiProtocol m_configuredProtocol{midi2::MidiProtocol::Default};

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiEndpointInformation : MidiEndpointInformationT<MidiEndpointInformation, implementation::MidiEndpointInformation>
    {
    };
}
