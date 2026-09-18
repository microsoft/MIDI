// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "CapabilityInquiry.MidiCapabilityInquiryResponder.g.h"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    struct MidiCapabilityInquiryResponder : MidiCapabilityInquiryResponderT<MidiCapabilityInquiryResponder>
    {
        MidiCapabilityInquiryResponder() = default;

        ci::MidiUniqueId Muid() const noexcept { return m_muid; }
        midi2enum::MidiDeclaredDeviceIdentity Identity() const noexcept { return m_identity; }

        ci::MidiCapabilityInquiryCategories SupportedCategories() const noexcept { return m_categories; }

        uint32_t ReceivableMaximumSystemExclusiveSize() const noexcept { return m_maximumSystemExclusiveSize; }

        uint8_t OutputPathId() const noexcept { return m_outputPathId; }
        uint8_t FunctionBlockNumber() const noexcept { return m_functionBlockNumber; }
        uint8_t MessageVersion() const noexcept { return m_messageVersion; }

        bool SupportsPropertyExchange() const noexcept
        {
            return (static_cast<uint32_t>(m_categories) &
                static_cast<uint32_t>(ci::MidiCapabilityInquiryCategories::PropertyExchange)) != 0;
        }

        bool SupportsProfiles() const noexcept
        {
            return (static_cast<uint32_t>(m_categories) &
                static_cast<uint32_t>(ci::MidiCapabilityInquiryCategories::ProfileConfiguration)) != 0;
        }

        bool SupportsProcessInquiry() const noexcept
        {
            return (static_cast<uint32_t>(m_categories) &
                static_cast<uint32_t>(ci::MidiCapabilityInquiryCategories::ProcessInquiry)) != 0;
        }

        uint8_t MaximumSimultaneousPropertyRequests() const noexcept { return m_maximumSimultaneousPropertyRequests; }

        winrt::hstring ToString();

        // Not projected. The session fills these in from what a responder said about itself.
        void InternalSetMuid(_In_ ci::MidiUniqueId const& value) noexcept { m_muid = value; }
        void InternalSetIdentity(_In_ midi2enum::MidiDeclaredDeviceIdentity const& value) noexcept { m_identity = value; }
        void InternalSetCategories(_In_ ci::MidiCapabilityInquiryCategories const value) noexcept { m_categories = value; }
        void InternalSetMaximumSystemExclusiveSize(_In_ uint32_t const value) noexcept { m_maximumSystemExclusiveSize = value; }
        void InternalSetOutputPathId(_In_ uint8_t const value) noexcept { m_outputPathId = value; }
        void InternalSetFunctionBlockNumber(_In_ uint8_t const value) noexcept { m_functionBlockNumber = value; }
        void InternalSetMessageVersion(_In_ uint8_t const value) noexcept { m_messageVersion = value; }
        void InternalSetMaximumSimultaneousPropertyRequests(_In_ uint8_t const value) noexcept { m_maximumSimultaneousPropertyRequests = value; }

    private:
        ci::MidiUniqueId m_muid{ nullptr };
        midi2enum::MidiDeclaredDeviceIdentity m_identity{ nullptr };

        ci::MidiCapabilityInquiryCategories m_categories{ ci::MidiCapabilityInquiryCategories::None };

        uint32_t m_maximumSystemExclusiveSize{ 0 };

        uint8_t m_outputPathId{ 0 };
        uint8_t m_functionBlockNumber{ 0 };
        uint8_t m_messageVersion{ 0 };
        uint8_t m_maximumSimultaneousPropertyRequests{ 0 };
    };
}
