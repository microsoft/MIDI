// Decodes Universal MIDI Packets into engine calls.
//
// This is deliberately free of any transport dependency. A service transport receives raw UMP
// words through IMidiBidirectional::SendMidiMessage, and a client side host receives them from
// the SDK; both can feed this directly.

#pragma once

#include "SynthEngine.h"

#include <sal.h>

#include <cstdint>
#include <vector>

namespace MidiSynth
{
    // How this synthesizer should present itself as a UMP Endpoint.
    //
    // General MIDI is defined over sixteen channels, which is exactly one UMP Group, so one Group
    // is sufficient; GM1 says "All 16 MIDI channels". The UMP specification recommends that an
    // input and output intended to work as a pair be a single Function Block spanning a single
    // Group, and notes MIDI-CI is more likely to operate successfully that way. So: one
    // bidirectional Function Block, one Group.
    namespace SynthEndpoint
    {
        constexpr uint8_t FirstGroupIndex = 0;
        constexpr uint8_t GroupCount = 1;
        constexpr uint8_t FunctionBlockCount = 1;
        constexpr bool FunctionBlockIsBidirectional = true;
    }

    // Reply sent for a Universal System Exclusive Identity Request.
    //
    // Note the in-box synthesizer cannot do this at all: it is an output only WinMM device with no
    // input, so it has no way to answer anything. Responding is new behavior, not compatibility.
    struct SynthIdentity
    {
        // Microsoft's registered MMA manufacturer identifier, three bytes because the first is zero.
        // The canonical copy is MIDI_MANUFACTURER_SYSEX_ID_MICROSOFT_BYTE* in src/in-box/Inc/MidiDefs.h;
        // repeated here only to keep the prototype free of repository dependencies.
        uint8_t ManufacturerSysExId[3]{ 0x00, 0x00, 0x41 };

        uint16_t FamilyCode{ 11 };          // Windows 11
        uint16_t FamilyMemberCode{ 1 };     // this synthesizer

        uint8_t SoftwareRevision[4]{ 1, 0, 0, 0 };

        uint8_t DeviceId{ 0 };

        // Answering with an identifier that is not ours would be worse than not answering, so a
        // cleared identifier silences the reply rather than sending zeros.
        bool IsConfigured() const noexcept
        {
            return ManufacturerSysExId[0] != 0 || ManufacturerSysExId[1] != 0 || ManufacturerSysExId[2] != 0;
        }
    };

    // Where a reply goes. The same path carries MIDI-CI later, which is why the Function Block is
    // declared bidirectional.
    struct IUmpOutput
    {
        virtual ~IUmpOutput() = default;

        virtual void SendUmp(
            _In_reads_(wordCount) const uint32_t* words,
            _In_ uint32_t wordCount) noexcept = 0;
    };

    struct UmpDispatcherStats
    {
        uint64_t Midi1ChannelVoice{ 0 };
        uint64_t Midi2ChannelVoice{ 0 };
        uint64_t SystemMessages{ 0 };
        uint64_t SystemExclusive{ 0 };
        uint64_t Utility{ 0 };

        // Well formed but not something a General MIDI synthesizer acts on.
        uint64_t Ignored{ 0 };

        // Truncated or otherwise unparseable, which indicates a caller problem.
        uint64_t Malformed{ 0 };

        uint64_t IdentityRepliesSent{ 0 };
    };

    class UmpDispatcher
    {
    public:
        void Initialize(_In_ SynthEngine* engine, _In_ uint8_t group) noexcept;

        // Supplying an output lets the dispatcher answer an Identity Request.
        void SetOutput(_In_opt_ IUmpOutput* output, _In_ const SynthIdentity& identity) noexcept;

        // Processes a run of UMP words. Returns the number of words consumed; a trailing partial
        // message is left unconsumed so the caller can present it again with the rest.
        uint32_t ProcessWords(_In_reads_(wordCount) const uint32_t* words, _In_ uint32_t wordCount) noexcept;

        // Number of 32 bit words in a packet, from its message type. Zero for an unknown type.
        static uint32_t PacketWordCount(_In_ uint32_t firstWord) noexcept;

        UmpDispatcherStats Stats() const noexcept { return m_stats; }
        void ResetStats() noexcept { m_stats = {}; }

    private:
        void HandleMidi1ChannelVoice(_In_ uint32_t word) noexcept;
        void HandleMidi2ChannelVoice(_In_ uint32_t word0, _In_ uint32_t word1) noexcept;
        void HandleSystem(_In_ uint32_t word) noexcept;
        void HandleSysEx7(_In_ uint32_t word0, _In_ uint32_t word1) noexcept;
        void HandleCompletedSysEx() noexcept;
        void SendIdentityReply(_In_ uint8_t requestedDeviceId) noexcept;

        SynthEngine* m_engine{ nullptr };
        IUmpOutput* m_output{ nullptr };
        SynthIdentity m_identity{};
        uint8_t m_group{ 0 };

        std::vector<uint8_t> m_sysex;
        UmpDispatcherStats m_stats{};
    };
}
