// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Decodes Universal MIDI Packets into synthesizer calls, and answers MIDI-CI exactly as the in-box
// General MIDI synthesizer transport does: the shared responder in MidiCiResponder.h does the work.
// Adapted from MidiSynthLib's UmpDispatcher, which is bound to the DLS engine.

#pragma once

#include "Synthesizer.h"

#include <windows.h>

#include "MidiCiResponder.h"

#include <atomic>

namespace SoundFontSynth
{
    // One synthesizer is one function block over one group, the shape the UMP specification
    // recommends for an input and output that work as a pair.
    namespace SynthEndpointShape
    {
        constexpr uint8_t FirstGroupIndex = 0;
        constexpr uint8_t GroupCount = 1;
        constexpr uint8_t FunctionBlockNumber = 0;

        constexpr size_t MaximumProductInstanceIdBytes = 42;
    }

    // Who this synthesizer says it is. The MIDI 1.0 identity reply, the MIDI-CI discovery reply,
    // the UMP device identity notification and the DeviceInfo resource are all built from this one
    // value, so they cannot disagree.
    struct SynthIdentity
    {
        uint8_t ManufacturerSysExId[3]{ 0x00, 0x00, 0x41 };

        uint16_t FamilyCode{ 11 };
        uint16_t FamilyMemberCode{ 3 };

        uint8_t SoftwareRevision[4]{ 1, 0, 0, 0 };

        uint8_t DeviceId{ 0 };

        bool IsConfigured() const noexcept
        {
            return ManufacturerSysExId[0] != 0 || ManufacturerSysExId[1] != 0 || ManufacturerSysExId[2] != 0;
        }
    };

    // Takes whole System Exclusive messages, without F0 and F7. Whole messages, because packets of
    // two different messages must never be interleaved on one group.
    struct ISysExSink
    {
        virtual ~ISysExSink() = default;

        virtual void SendSysEx(_In_reads_(count) uint8_t const* payload, _In_ size_t count) noexcept = 0;
    };

    struct DispatcherStatistics
    {
        uint64_t Midi1ChannelVoice{ 0 };
        uint64_t Midi2ChannelVoice{ 0 };
        uint64_t SystemMessages{ 0 };
        uint64_t SystemExclusive{ 0 };
        uint64_t Ignored{ 0 };
        uint64_t Malformed{ 0 };
        uint64_t IdentityRepliesSent{ 0 };
        uint64_t DiscoveryRepliesSent{ 0 };
        uint64_t MuidInvalidations{ 0 };
        uint64_t PropertyRequests{ 0 };
    };

    class SynthDispatcher
    {
    public:
        // Zero disables MIDI-CI.
        void Initialize(_In_ Synthesizer* synthesizer, _In_ uint8_t group, _In_ uint32_t muid) noexcept;

        void SetOutput(_In_opt_ ISysExSink* output, _In_ SynthIdentity const& identity) noexcept;

        // What MIDI-CI Inquiry: Endpoint answers with. It must match the UMP product instance id.
        void SetProductInstanceId(_In_opt_z_ char const* productInstanceId) noexcept;

        // Returns the words consumed. A trailing partial packet is left for the caller.
        uint32_t ProcessWords(_In_reads_(wordCount) uint32_t const* words, _In_ uint32_t wordCount) noexcept;

        static uint32_t PacketWordCount(_In_ uint32_t firstWord) noexcept;

        DispatcherStatistics Statistics() const noexcept { return m_statistics; }

        uint32_t Muid() const noexcept { return m_responder.Muid(); }
        bool MuidNeedsReplacement() const noexcept { return m_responder.MuidNeedsReplacement(); }
        void SetMuid(_In_ uint32_t muid) noexcept;

        static constexpr size_t MaximumPropertyHeaderBytes = 256;

        struct PendingPropertyRequest
        {
            uint32_t InitiatorMuid{ 0 };
            uint8_t RequestId{ 0 };
            uint16_t HeaderByteCount{ 0 };
            uint8_t Header[MaximumPropertyHeaderBytes]{};
            bool IsSubscription{ false };
        };

        // A property request is parked for a worker thread: answering it parses JSON and builds
        // kilobytes of reply, neither of which belongs on the thread that renders audio.
        bool TakePendingPropertyRequest(_Out_ PendingPropertyRequest& request) noexcept;

        bool TakeInvalidatedInitiatorMuid(_Out_ uint32_t& muid) noexcept;

        // The specification's min-center-max scaling, which keeps the center value centered.
        static uint32_t ScaleUp(_In_ uint32_t value, _In_ uint8_t sourceBits, _In_ uint8_t destinationBits) noexcept;

        // Largest reassembled System Exclusive message. Anything longer is dropped.
        static constexpr size_t MaximumSysExBytes = 1024;

    private:
        void HandleMidi1ChannelVoice(_In_ uint32_t word) noexcept;
        void HandleMidi2ChannelVoice(_In_ uint32_t word0, _In_ uint32_t word1) noexcept;
        void HandleSystem(_In_ uint32_t word) noexcept;
        void HandleSysEx7(_In_ uint32_t word0, _In_ uint32_t word1) noexcept;
        void HandleCompletedSysEx() noexcept;
        void HandleMidiCi(_In_reads_(size) uint8_t const* message, _In_ size_t size) noexcept;
        void HandleGlobalParameterControl(_In_reads_(size) uint8_t const* message, _In_ size_t size) noexcept;
        void SendIdentityReply(_In_ uint8_t requestedDeviceId) noexcept;
        void SendSysEx(_In_reads_(count) uint8_t const* payload, _In_ size_t count) noexcept;
        void ConfigureResponder(_In_ uint32_t muid) noexcept;

        void ParkPropertyRequest(
            _In_ WindowsMidiServicesCapabilityInquiry::ParsedMessage const& parsed,
            _In_ uint8_t const* message,
            _In_ bool isSubscription) noexcept;

        Synthesizer* m_synthesizer{ nullptr };
        ISysExSink* m_output{ nullptr };
        SynthIdentity m_identity{};
        uint8_t m_group{ 0 };

        char m_productInstanceId[SynthEndpointShape::MaximumProductInstanceIdBytes + 1]{};

        WindowsMidiServicesCapabilityInquiry::Responder m_responder{};

        // Written only by the dispatching thread while the flag is clear, read and cleared only by
        // the worker, so neither sees a half written request.
        PendingPropertyRequest m_propertyRequest{};
        std::atomic<bool> m_propertyRequestPending{ false };

        std::atomic<uint32_t> m_invalidatedInitiatorMuid{ 0 };

        uint8_t m_sysex[MaximumSysExBytes]{};
        size_t m_sysexLength{ 0 };

        DispatcherStatistics m_statistics{};
    };
}
