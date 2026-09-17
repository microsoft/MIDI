// Decodes Universal MIDI Packets into engine calls.
//
// This is deliberately free of any transport dependency. A service transport receives raw UMP
// words through IMidiBidirectional::SendMidiMessage, and a client side host receives them from
// the SDK; both can feed this directly.

#pragma once

#include "SynthEngine.h"

#include "MidiCiResponder.h"

#include <sal.h>

#include <atomic>
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
        constexpr uint8_t FunctionBlockNumber = 0;
        constexpr bool FunctionBlockIsBidirectional = true;

        // UTF-8 byte counts, matching what a UMP Stream text notification can carry. The
        // specification's own limits; duplicated as plain numbers only to keep this header
        // readable next to the buffers they size.
        constexpr size_t MaxEndpointNameBytes = 98;
        constexpr size_t MaxProductInstanceIdBytes = 42;
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
        uint64_t DiscoveryRepliesSent{ 0 };
        uint64_t MuidInvalidations{ 0 };
        uint64_t PropertyRequests{ 0 };
    };

    class UmpDispatcher
    {
    public:
        // The MUID is supplied rather than generated here, so this library stays free of any SDK
        // dependency. Applications should pass MidiUniqueId::CreateRandom(), which keeps clear of
        // the reserved range. Zero disables MIDI-CI.
        void Initialize(_In_ SynthEngine* engine, _In_ uint8_t group, _In_ uint32_t muid) noexcept;

        // Supplying an output lets the dispatcher answer an Identity Request.
        void SetOutput(_In_opt_ IUmpOutput* output, _In_ const SynthIdentity& identity) noexcept;

        // What this endpoint answers UMP Stream Endpoint Discovery with. Both are UTF-8 and are
        // truncated rather than refused, because a name that is too long is still worth reporting.
        // An empty product instance id suppresses that notification, which is how a device says it
        // does not have one.
        void SetEndpointIdentity(
            _In_opt_z_ const char* endpointName,
            _In_opt_z_ const char* productInstanceId) noexcept;

        // Processes a run of UMP words. Returns the number of words consumed; a trailing partial
        // message is left unconsumed so the caller can present it again with the rest.
        uint32_t ProcessWords(_In_reads_(wordCount) const uint32_t* words, _In_ uint32_t wordCount) noexcept;

        // Number of 32 bit words in a packet, from its message type. Zero for an unknown type.
        static uint32_t PacketWordCount(_In_ uint32_t firstWord) noexcept;

        UmpDispatcherStats Stats() const noexcept { return m_stats; }
        void ResetStats() noexcept { m_stats = {}; }

        // The 28 bit identifier this device uses in MIDI-CI exchanges.
        uint32_t Muid() const noexcept { return m_responder.Muid(); }

        // Set when another device told us our identifier collided. MIDI-CI stays silent until the
        // host supplies a replacement, because answering with a known duplicate is worse.
        bool MuidNeedsReplacement() const noexcept { return m_responder.MuidNeedsReplacement(); }
        void SetMuid(_In_ uint32_t muid) noexcept;

        // Largest property exchange header we will accept. Headers are short JSON objects such as
        // {"resource":"ProgramList"}; anything larger is refused rather than truncated.
        static constexpr size_t MaxPropertyHeaderBytes = 256;

        struct PendingPropertyRequest
        {
            uint32_t InitiatorMuid{ 0 };
            uint8_t RequestId{ 0 };
            uint16_t HeaderByteCount{ 0 };
            uint8_t Header[MaxPropertyHeaderBytes]{};
        };

        // Answering a property request means parsing JSON and building many kilobytes of reply,
        // neither of which belongs on the thread that renders audio. The request is parked here and
        // a worker thread collects it. Returns false when nothing is waiting.
        bool TakePendingPropertyRequest(_Out_ PendingPropertyRequest& request) noexcept;

        // Public because a host answering a property request needs exactly this packing and must
        // not grow a second copy of it.
        static void PacketizeSysEx7(
            _In_ IUmpOutput& output,
            _In_ uint8_t group,
            _In_reads_(count) const uint8_t* payload,
            _In_ size_t count) noexcept;

    private:
        void HandleMidi1ChannelVoice(_In_ uint32_t word) noexcept;
        void HandleMidi2ChannelVoice(_In_ uint32_t word0, _In_ uint32_t word1) noexcept;
        void HandleSystem(_In_ uint32_t word) noexcept;
        void HandleSysEx7(_In_ uint32_t word0, _In_ uint32_t word1) noexcept;
        void HandleCompletedSysEx() noexcept;
        void HandleMidiCi(_In_reads_(size) const uint8_t* message, _In_ size_t size) noexcept;
        void HandleGlobalParameterControl(_In_reads_(size) const uint8_t* message, _In_ size_t size) noexcept;
        void SendIdentityReply(_In_ uint8_t requestedDeviceId) noexcept;
        void SendSysEx7(_In_reads_(count) const uint8_t* payload, _In_ size_t count) noexcept;

        // UMP Stream. This is how a MIDI 2.0 endpoint describes itself, and it is the only way the
        // service learns the endpoint name, product instance id and function blocks.
        void HandleStream(_In_reads_(4) const uint32_t* words) noexcept;
        void SendStreamMessage(
            _In_ uint32_t word0, _In_ uint32_t word1,
            _In_ uint32_t word2, _In_ uint32_t word3) noexcept;
        void SendEndpointInfoNotification() noexcept;
        void SendDeviceIdentityNotification() noexcept;
        void SendStreamConfigurationNotification() noexcept;
        void SendFunctionBlockInfoNotification() noexcept;

        // One text notification, split across as many packets as the text needs. A function block
        // number of -1 means the text belongs to the endpoint rather than to a block, which is
        // also what decides whether fourteen or thirteen bytes fit in a packet.
        void SendTextNotification(
            _In_ uint16_t status,
            _In_ int functionBlockNumber,
            _In_z_ const char* text) noexcept;

        // Rebuilds the responder configuration from the identity and group we were given.
        void ConfigureResponder(_In_ uint32_t muid) noexcept;

        void ParkPropertyRequest(
            _In_ const WindowsMidiServicesCapabilityInquiry::ParsedMessage& parsed,
            _In_ const uint8_t* message) noexcept;

        SynthEngine* m_engine{ nullptr };
        IUmpOutput* m_output{ nullptr };
        SynthIdentity m_identity{};
        uint8_t m_group{ 0 };

        char m_endpointName[SynthEndpoint::MaxEndpointNameBytes + 1]{};
        char m_productInstanceId[SynthEndpoint::MaxProductInstanceIdBytes + 1]{};

        // All MIDI-CI behavior lives in the shared responder, which produces replies into a buffer
        // and never sends. Only this class knows how to put them on the wire.
        WindowsMidiServicesCapabilityInquiry::Responder m_responder{};

        // Written only by the dispatch thread and only while the flag is clear, read and cleared
        // only by the worker, so neither can see a half written request.
        PendingPropertyRequest m_propertyRequest{};
        std::atomic<bool> m_propertyRequestPending{ false };

        // Fixed capacity deliberately. This runs on the audio thread, where allocating is a real
        // time fault, and a throwing allocation inside a noexcept path would terminate the whole
        // process rather than just failing the message.
        static constexpr size_t MaxSysExBytes = 1024;

        uint8_t m_sysex[MaxSysExBytes]{};
        size_t m_sysexLength{ 0 };

        UmpDispatcherStats m_stats{};
    };
}
