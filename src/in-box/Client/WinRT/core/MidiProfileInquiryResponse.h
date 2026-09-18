// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "CapabilityInquiry.MidiProfileInquiryResponse.g.h"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    struct MidiProfileInquiryResponse : MidiProfileInquiryResponseT<MidiProfileInquiryResponse>
    {
        MidiProfileInquiryResponse() = default;

        ci::MidiCapabilityInquiryStatus Status() const noexcept { return m_status; }
        ci::MidiUniqueId ResponderMuid() const noexcept { return m_responderMuid; }
        uint8_t DeviceId() const noexcept { return m_deviceId; }

        foundation::Collections::IVector<ci::MidiProfileId> EnabledProfiles() const noexcept { return m_enabledProfiles; }
        foundation::Collections::IVector<ci::MidiProfileId> DisabledProfiles() const noexcept { return m_disabledProfiles; }

        // Not projected.
        void InternalSetStatus(_In_ ci::MidiCapabilityInquiryStatus const value) noexcept { m_status = value; }
        void InternalSetResponderMuid(_In_ ci::MidiUniqueId const& value) noexcept { m_responderMuid = value; }
        void InternalSetDeviceId(_In_ uint8_t const value) noexcept { m_deviceId = value; }

        void InternalSetProfiles(
            _In_ foundation::Collections::IVector<ci::MidiProfileId> const& enabled,
            _In_ foundation::Collections::IVector<ci::MidiProfileId> const& disabled) noexcept
        {
            m_enabledProfiles = enabled;
            m_disabledProfiles = disabled;
        }

    private:
        ci::MidiCapabilityInquiryStatus m_status{ ci::MidiCapabilityInquiryStatus::NoResponse };

        ci::MidiUniqueId m_responderMuid{ nullptr };
        uint8_t m_deviceId{ 0x7F };

        foundation::Collections::IVector<ci::MidiProfileId> m_enabledProfiles
            { winrt::single_threaded_vector<ci::MidiProfileId>() };

        foundation::Collections::IVector<ci::MidiProfileId> m_disabledProfiles
            { winrt::single_threaded_vector<ci::MidiProfileId>() };
    };
}
