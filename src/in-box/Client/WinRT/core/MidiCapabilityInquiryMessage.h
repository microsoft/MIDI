// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "CapabilityInquiry.MidiCapabilityInquiryMessage.g.h"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    struct MidiCapabilityInquiryMessage : MidiCapabilityInquiryMessageT<MidiCapabilityInquiryMessage>
    {
        MidiCapabilityInquiryMessage() = default;

        bool IsValid() const noexcept { return m_isValid; }

        ci::MidiCapabilityInquiryMessageType MessageType() const noexcept { return m_messageType; }
        uint8_t DeviceId() const noexcept { return m_deviceId; }
        uint8_t SourceVersion() const noexcept { return m_sourceVersion; }

        ci::MidiUniqueId SourceMuid() const noexcept { return m_sourceMuid; }
        ci::MidiUniqueId DestinationMuid() const noexcept { return m_destinationMuid; }
        ci::MidiUniqueId TargetMuid() const noexcept { return m_targetMuid; }

        uint8_t OutputPathId() const noexcept { return m_outputPathId; }

        foundation::Collections::IVector<uint8_t> Data() const noexcept { return m_data; }

        bool HasPropertyExchangeFields() const noexcept { return m_hasPropertyExchangeFields; }
        uint8_t RequestId() const noexcept { return m_requestId; }

        winrt::hstring HeaderText() const noexcept { return m_headerText; }
        json::JsonObject Header() const noexcept { return m_header; }

        uint16_t ChunkNumber() const noexcept { return m_chunkNumber; }
        uint16_t ChunkCount() const noexcept { return m_chunkCount; }

        foundation::Collections::IVector<uint8_t> Body() const noexcept { return m_body; }

        static ci::MidiCapabilityInquiryMessage FromSystemExclusiveData(
            _In_ foundation::Collections::IIterable<uint8_t> const& data) noexcept;

        static ci::MidiCapabilityInquiryMessage FromUmpMessages(
            _In_ foundation::Collections::IIterable<midi2::MidiMessage64> const& messages) noexcept;

        static bool IsCapabilityInquiryData(
            _In_ foundation::Collections::IIterable<uint8_t> const& data) noexcept;

    private:
        bool m_isValid{ false };

        ci::MidiCapabilityInquiryMessageType m_messageType{ ci::MidiCapabilityInquiryMessageType::Discovery };
        uint8_t m_deviceId{ 0x7F };
        uint8_t m_sourceVersion{ 0 };

        ci::MidiUniqueId m_sourceMuid{ nullptr };
        ci::MidiUniqueId m_destinationMuid{ nullptr };
        ci::MidiUniqueId m_targetMuid{ nullptr };

        uint8_t m_outputPathId{ 0 };

        foundation::Collections::IVector<uint8_t> m_data
            { winrt::single_threaded_vector<uint8_t>() };

        bool m_hasPropertyExchangeFields{ false };
        uint8_t m_requestId{ 0 };

        winrt::hstring m_headerText{};
        json::JsonObject m_header{ nullptr };

        uint16_t m_chunkNumber{ 0 };
        uint16_t m_chunkCount{ 0 };

        foundation::Collections::IVector<uint8_t> m_body
            { winrt::single_threaded_vector<uint8_t>() };
    };
}

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::factory_implementation
{
    struct MidiCapabilityInquiryMessage : MidiCapabilityInquiryMessageT<MidiCapabilityInquiryMessage, implementation::MidiCapabilityInquiryMessage>
    {
    };
}
