// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "CapabilityInquiry.MidiPropertyExchangeResponse.g.h"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    struct MidiPropertyExchangeResponse : MidiPropertyExchangeResponseT<MidiPropertyExchangeResponse>
    {
        MidiPropertyExchangeResponse() = default;

        ci::MidiCapabilityInquiryStatus Status() const noexcept { return m_status; }
        ci::MidiUniqueId ResponderMuid() const noexcept { return m_responderMuid; }
        uint8_t RequestId() const noexcept { return m_requestId; }

        json::JsonObject Header() const noexcept { return m_header; }
        winrt::hstring HeaderText() const noexcept { return m_headerText; }

        int32_t ResourceStatus() const noexcept { return m_resourceStatus; }
        uint16_t ChunkCount() const noexcept { return m_chunkCount; }

        foundation::Collections::IVector<uint8_t> Body() const noexcept { return m_body; }
        winrt::hstring BodyAsText() const noexcept { return m_bodyAsText; }
        json::IJsonValue BodyAsJson() const noexcept { return m_bodyAsJson; }

        uint8_t NakStatusCode() const noexcept { return m_nakStatusCode; }
        winrt::hstring NakStatusMessage() const noexcept { return m_nakStatusMessage; }

        // Not projected. The session fills these in as a reply is assembled.
        void InternalSetStatus(_In_ ci::MidiCapabilityInquiryStatus const value) noexcept { m_status = value; }
        void InternalSetResponderMuid(_In_ ci::MidiUniqueId const& value) noexcept { m_responderMuid = value; }
        void InternalSetRequestId(_In_ uint8_t const value) noexcept { m_requestId = value; }
        void InternalSetHeader(_In_ json::JsonObject const& value) noexcept { m_header = value; }
        void InternalSetHeaderText(_In_ winrt::hstring const& value) noexcept { m_headerText = value; }
        void InternalSetResourceStatus(_In_ int32_t const value) noexcept { m_resourceStatus = value; }
        void InternalSetChunkCount(_In_ uint16_t const value) noexcept { m_chunkCount = value; }
        void InternalSetBody(_In_ foundation::Collections::IVector<uint8_t> const& value) noexcept { m_body = value; }
        void InternalSetBodyAsText(_In_ winrt::hstring const& value) noexcept { m_bodyAsText = value; }
        void InternalSetBodyAsJson(_In_ json::IJsonValue const& value) noexcept { m_bodyAsJson = value; }
        void InternalSetNakStatusCode(_In_ uint8_t const value) noexcept { m_nakStatusCode = value; }
        void InternalSetNakStatusMessage(_In_ winrt::hstring const& value) noexcept { m_nakStatusMessage = value; }

    private:
        ci::MidiCapabilityInquiryStatus m_status{ ci::MidiCapabilityInquiryStatus::NoResponse };

        ci::MidiUniqueId m_responderMuid{ nullptr };
        uint8_t m_requestId{ 0 };

        json::JsonObject m_header{ nullptr };
        winrt::hstring m_headerText{};

        int32_t m_resourceStatus{ 0 };
        uint16_t m_chunkCount{ 0 };

        foundation::Collections::IVector<uint8_t> m_body
            { winrt::single_threaded_vector<uint8_t>() };

        winrt::hstring m_bodyAsText{};
        json::IJsonValue m_bodyAsJson{ nullptr };

        uint8_t m_nakStatusCode{ 0 };
        winrt::hstring m_nakStatusMessage{};
    };
}
