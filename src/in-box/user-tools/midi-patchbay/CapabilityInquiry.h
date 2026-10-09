// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// MIDI-CI in a patch: a responder that answers for a device that can't, a filter that keeps
// MIDI-CI away from one, and the file that describes a device's profiles and properties. Pure, so
// the unit tests compile this exactly as it ships. The messages themselves are read and built by
// the MIDI-CI library the service and the SDK use.

#include "ProcessingBlock.h"

#include <MidiCiResponder.h>

#include <array>
#include <atomic>
#include <bitset>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace midipatchbay
{
    // ------------------------------------------------------------------ system exclusive

    constexpr uint8_t SysExComplete = 0;
    constexpr uint8_t SysExStart = 1;
    constexpr uint8_t SysExContinue = 2;
    constexpr uint8_t SysExEnd = 3;

    // One SysEx7 packet: up to six bytes of a message.
    struct SysExPacket
    {
        uint8_t Status{ SysExComplete };
        uint8_t Group{ 0 };
        uint8_t ByteCount{ 0 };
        std::array<uint8_t, 6> Bytes{};
    };

    // False for anything that isn't a SysEx7 packet.
    bool ReadSysExPacket(
        _In_reads_(wordCount) uint32_t const* words,
        _In_ uint8_t wordCount,
        _Out_ SysExPacket& packet) noexcept;

    // Where a responder's answers go.
    class CiReplyWriter
    {
    public:
        virtual void Write(_In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) noexcept = 0;

    protected:
        ~CiReplyWriter() = default;
    };

    // One whole message as packets: its bytes, without F0 and F7.
    void WriteSysEx(
        _In_ uint8_t group,
        _In_reads_(byteCount) uint8_t const* bytes,
        _In_ size_t byteCount,
        _Inout_ CiReplyWriter& writer) noexcept;

    // Which kind of MIDI-CI message a packet starts, as one of the CiCategory bits. Zero when it
    // doesn't start one.
    uint8_t CiCategoryOfStart(_In_ SysExPacket const& packet) noexcept;

    // ------------------------------------------------------------------ the MIDI-CI file

    enum class CiProfileTarget : uint8_t
    {
        Channel = 0,
        Group = 1,
        FunctionBlock = 2,
    };

    // What a Profile Details Inquiry for one target is answered with.
    struct CiProfileDetail
    {
        uint8_t Target{ 0 };
        std::vector<uint8_t> Data{};
    };

    struct CiProfile
    {
        std::array<uint8_t, 5> Id{};
        CiProfileTarget Target{ CiProfileTarget::FunctionBlock };

        // A channel profile's first channel, counted from 0.
        uint8_t Channel{ 0 };

        // How many channels a channel profile takes, counting the first.
        uint16_t ChannelCount{ 1 };

        // A device that always follows the profile has it on. One that never can has it off.
        bool Enabled{ true };

        std::vector<CiProfileDetail> Details{};
    };

    struct CiResource
    {
        std::string Name{};
        std::string ResourceId{};

        // The data as JSON, plain ASCII, ready to send.
        std::string Json{};

        // Each item on its own when the data is an array, so a page can be sent without parsing.
        bool IsArray{ false };
        std::vector<std::string> Items{};
    };

    struct CiDescription
    {
        std::vector<CiProfile> Profiles{};

        // DeviceInfo's names, in UTF-8. Its numbers come from the step.
        bool HasDeviceInfo{ false };
        std::string Manufacturer{};
        std::string Family{};
        std::string Model{};
        std::string Version{};

        std::vector<CiResource> Resources{};

        // Changes when anything above does, so the patch routes again when the file changes.
        uint64_t Fingerprint{ 0 };

        bool HasProfiles() const noexcept { return !Profiles.empty(); }
        bool HasProperties() const noexcept { return HasDeviceInfo || !Resources.empty(); }
    };

    enum class CiFileProblemKind : int32_t
    {
        // Not JSON, or not an object.
        NotJson = 0,
        UnknownKey = 1,
        BadProfile = 2,
        DuplicateProfile = 3,
        BadDeviceInfo = 4,
        BadResource = 5,
        DuplicateResource = 6,
        TooMany = 7,
        TooLarge = 8,
    };

    // Which part of the file a problem is in.
    enum class CiFileSection : int32_t
    {
        File = 0,
        Profiles = 1,
        DeviceInfo = 2,
        Resources = 3,
    };

    struct CiFileProblem
    {
        CiFileProblemKind Kind{ CiFileProblemKind::NotJson };

        // Which profile or resource, counted from 0, or -1 for the section as a whole.
        int32_t Index{ -1 };

        // The key, for an unknown key or a bad device info name.
        std::wstring Key{};

        CiFileSection Section{ CiFileSection::File };
    };

    constexpr size_t MaximumCiFileBytes = 1024 * 1024;
    constexpr size_t MaximumCiProfiles = 64;
    constexpr size_t MaximumCiProfileDetails = 16;
    constexpr size_t MaximumCiResources = 64;
    constexpr size_t MaximumCiResourceBytes = 256 * 1024;
    constexpr size_t MaximumCiNameLength = 64;

    // Anything that doesn't read is left out and listed, so one mistake doesn't lose the rest. Null
    // only when the file isn't a JSON object at all.
    std::shared_ptr<CiDescription> ParseCiDescription(
        _In_ std::wstring_view text,
        _Out_ std::vector<CiFileProblem>& problems) noexcept;

    // ------------------------------------------------------------------ the responder

    // The longest MIDI-CI message a responder takes in, without F0 and F7.
    constexpr size_t MaximumCiMessageBytes = 4096;

    enum class CiOutcome : uint8_t
    {
        Answered = 0,

        // Answered with a NAK, for something the step doesn't do.
        Refused = 1,

        // Took a new MUID, because another device had the same one or asked for it to change.
        NewMuid = 2,
    };

    struct CiActivity
    {
        std::chrono::system_clock::time_point Time{};
        uint8_t MessageType{ 0 };
        uint32_t InitiatorMuid{ 0 };
        CiOutcome Outcome{ CiOutcome::Answered };
    };

    constexpr size_t MaximumCiActivity = 12;

    // What the inspector shows.
    struct CiResponderSnapshot
    {
        uint32_t Muid{ 0 };

        // Newest first.
        std::vector<CiActivity> Recent{};
    };

    // What a responder knows has been sent on one channel, for a MIDI Message Report.
    struct CiChannelState
    {
        std::array<uint32_t, 128> Controllers{};
        std::bitset<128> ControllersSent{};

        std::array<uint16_t, 128> Velocities{};
        std::bitset<128> NotesOn{};

        uint32_t PitchBend{ 0x80000000 };
        bool PitchBendSent{ false };

        uint32_t Pressure{ 0 };
        bool PressureSent{ false };

        uint8_t Program{ 0 };
        bool ProgramSent{ false };

        uint8_t BankMsb{ 0 };
        uint8_t BankLsb{ 0 };
        bool BankSent{ false };

        // The last message on the channel was MIDI 2.0, so the report is too.
        bool Midi2{ false };
    };

    // Groups whose channels a responder keeps track of: the first ones it sees.
    constexpr size_t MaximumCiTrackedGroups = 4;

    // A message part way through arriving, one for each path and group.
    struct CiAssembly
    {
        bool Active{ false };
        bool KeepOut{ false };
        bool Overflowed{ false };
        uint32_t Path{ 0 };
        uint32_t Leaf{ 0 };
        uint8_t Group{ 0 };
        uint64_t LastUsed{ 0 };
        size_t ByteCount{ 0 };
        std::array<uint8_t, MaximumCiMessageBytes> Bytes{};
    };

    constexpr size_t CiAssemblySlots = 4;
    constexpr size_t CiInitiatorSlots = 8;
    constexpr size_t CiRecentRequestSlots = 8;

    // One for each responder step, shared by every path through it and kept when the patch
    // changes, so its MUID stays the same for as long as the patch routes. Only the functions
    // below touch what is in it.
    struct CiResponderState
    {
        CiResponderState();

        CiResponderSnapshot Snapshot() const;

        // Held while a MIDI-CI message is answered. Never held while anything is sent.
        mutable std::mutex Lock{};

        ::WindowsMidiServicesCapabilityInquiry::Responder Responder{};
        uint64_t ConfiguredFingerprint{ 0 };
        bool Configured{ false };

        uint64_t Clock{ 0 };
        std::array<CiAssembly, CiAssemblySlots> Assemblies{};

        struct Initiator
        {
            uint32_t Muid{ 0 };
            uint32_t MaximumSysEx{ 0 };
            uint64_t LastUsed{ 0 };
        };

        std::array<Initiator, CiInitiatorSlots> Initiators{};

        struct RecentRequest
        {
            uint64_t Hash{ 0 };
            std::chrono::steady_clock::time_point Time{};
        };

        std::array<RecentRequest, CiRecentRequestSlots> RecentRequests{};
        size_t NextRecentRequest{ 0 };

        std::array<CiActivity, MaximumCiActivity> Activity{};
        size_t ActivityCount{ 0 };
        size_t NextActivity{ 0 };

        std::array<uint8_t, MaximumCiMessageBytes + 64> Reply{};

        // Notes and controllers, under a lock of their own that is only ever held for one message,
        // because every note through the step takes it.
        mutable std::atomic_flag TrackingLock{};
        std::array<int8_t, 16> TrackedGroupSlot{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
        size_t TrackedGroupCount{ 0 };
        std::array<std::array<CiChannelState, 16>, MaximumCiTrackedGroups> Tracked{};
    };

    // Where an answer goes: back to the endpoint the question came from, on its group.
    struct CiReturn
    {
        bool CanReply{ false };
        uint32_t Leaf{ 0 };
        uint8_t Group{ 0 };
    };

    // False keeps the message out: MIDI-CI goes no further unless the step passes it on, and
    // everything else goes through. Answers go to the writer. A path is one copy of the step in the
    // routing, so messages arriving two ways are never mixed together. Allocates only while
    // answering MIDI-CI, never for anything else.
    bool RunCiResponder(
        _In_ CiResponderSettings const& settings,
        _Inout_ CiResponderState& state,
        _In_ uint32_t path,
        _In_ CiReturn const& from,
        _In_reads_(wordCount) uint32_t const* words,
        _In_ uint8_t wordCount,
        _Inout_ CiReplyWriter& writer) noexcept;

    // ------------------------------------------------------------------ the filter

    // What a filter decided about the message part way through, for each path and group.
    struct CiFilterMemory
    {
        std::array<std::atomic<uint64_t>, 16> Slots{};
    };

    bool RunCiFilter(
        _In_ CiFilterSettings const& settings,
        _Inout_ CiFilterMemory& memory,
        _In_ uint32_t path,
        _In_reads_(wordCount) uint32_t const* words,
        _In_ uint8_t wordCount) noexcept;
}
