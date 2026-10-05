// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "CapabilityInquiryTests.h"
#include "TestMessages.h"

#include "CapabilityInquiry.h"
#include "RouteGraph.h"
#include "StatefulBlocks.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace midipatchbay;
using namespace patchbaytests;

namespace
{
    namespace ci = ::WindowsMidiServicesCapabilityInquiry;

    constexpr uint8_t NoteOff = 0x8;
    constexpr uint8_t NoteOn = 0x9;
    constexpr uint8_t ControlChange = 0xB;
    constexpr uint8_t ProgramChange = 0xC;
    constexpr uint8_t ChannelPressure = 0xD;
    constexpr uint8_t PitchBend = 0xE;

    constexpr uint32_t Initiator = 0x0123456;
    constexpr uint32_t OtherInitiator = 0x0654321;

    constexpr uint32_t Path = 1;
    constexpr uint32_t Leaf = 7;

    constexpr std::array<uint8_t, 5> Organ{ 0x7E, 0x21, 0x00, 0x01, 0x01 };
    constexpr std::array<uint8_t, 5> Piano{ 0x7E, 0x22, 0x00, 0x01, 0x01 };
    constexpr std::array<uint8_t, 5> Mixer{ 0x7E, 0x28, 0x00, 0x01, 0x01 };
    constexpr std::array<uint8_t, 5> Unknown{ 0x7E, 0x30, 0x00, 0x01, 0x01 };

    // An organ on channel 1, a piano on channel 4 that is off and takes two channels, and a mixer
    // for the group. Nothing for the function block.
    constexpr wchar_t ProfilesFile[] = LR"({
        "profiles": [
            { "id": [126, 33, 0, 1, 1], "name": "Drawbar organ", "channel": 0 },
            { "id": "7E 22 00 01 01", "target": "channel", "channel": 3, "channels": 2, "enabled": false,
              "details": [ { "target": 64, "data": [1, 2, 3] } ] },
            { "id": [126, 40, 0, 1, 1], "target": "group" }
        ]
    })";

    std::wstring PropertiesFile()
    {
        return std::wstring{ LR"({
            "deviceInfo": { "manufacturer": "Contoso", "family": "Organs", "model": "Model 2", "version": "1.0" },
            "resources": [
                { "resource": "ProgramList", "data": [
                    { "title": "Jazz", "bankPC": [0, 0, 0] },
                    { "title": "Gospel", "bankPC": [0, 0, 1] },
                    { "title": "Rock", "bankPC": [0, 0, 2] } ] },
                { "resource": "ChannelList", "data": [ { "title": "Upper", "channel": 1 } ] },
                { "resource": "X-Notes", "resId": "a", "data": { "text": "Gr)" } + L"\u00FC\u00DF" + LR"(e" } }
            ]
        })";
    }

    class Replies final : public CiReplyWriter
    {
    public:
        void Write(uint32_t const* words, uint8_t wordCount) noexcept override
        {
            Message message{};
            std::copy_n(words, wordCount, message.Words.begin());
            message.Count = wordCount;

            Messages.push_back(message);
        }

        std::vector<Message> Messages{};
    };

    std::vector<Message> Packets(uint8_t group, std::vector<uint8_t> const& bytes)
    {
        Replies packets{};
        WriteSysEx(group, bytes.data(), bytes.size(), packets);

        return packets.Messages;
    }

    struct Answer
    {
        uint8_t Group{ 0 };
        std::vector<uint8_t> Bytes{};
        ci::ParsedMessage Parsed{};
    };

    // The system exclusive among what came back, put together again and read.
    std::vector<Answer> Answers(std::vector<Message> const& messages)
    {
        std::vector<Answer> answers{};
        std::vector<uint8_t> bytes{};

        for (auto const& message : messages)
        {
            SysExPacket packet{};

            if (!ReadSysExPacket(message.Words.data(), message.Count, packet))
            {
                continue;
            }

            if (packet.Status == SysExComplete || packet.Status == SysExStart)
            {
                bytes.clear();
            }

            bytes.insert(bytes.end(), packet.Bytes.begin(), packet.Bytes.begin() + packet.ByteCount);

            if (packet.Status == SysExComplete || packet.Status == SysExEnd)
            {
                Answer answer{};
                answer.Group = packet.Group;
                answer.Bytes = bytes;

                ci::Parse(answer.Bytes.data(), answer.Bytes.size(), answer.Parsed);

                answers.push_back(std::move(answer));
            }
        }

        return answers;
    }

    // Everything that isn't system exclusive, in the order it came.
    std::vector<Message> ChannelMessages(std::vector<Message> const& messages)
    {
        std::vector<Message> result{};

        for (auto const& message : messages)
        {
            if ((message.Words[0] >> 28) != 0x3)
            {
                result.push_back(message);
            }
        }

        return result;
    }

    // One whole message through a responder, packet by packet. Returns how many packets went on.
    size_t Ask(
        CiResponderSettings const& settings,
        CiResponderState& state,
        std::vector<uint8_t> const& bytes,
        Replies& replies,
        uint8_t group = 0,
        uint32_t path = Path,
        uint32_t leaf = Leaf)
    {
        size_t passed{ 0 };

        for (auto const& packet : Packets(group, bytes))
        {
            if (RunCiResponder(settings, state, path, CiReturn{ true, leaf, group }, packet.Words.data(), packet.Count, replies))
            {
                passed++;
            }
        }

        return passed;
    }

    bool Play(CiResponderSettings const& settings, CiResponderState& state, Message const& message)
    {
        Replies ignored{};

        return RunCiResponder(settings, state, Path, CiReturn{ true, Leaf, 0 }, message.Words.data(), message.Count, ignored);
    }

    uint32_t MuidOf(CiResponderState const& state)
    {
        return state.Snapshot().Muid;
    }

    std::vector<uint8_t> Discovery(uint32_t from = Initiator, uint32_t maximumSysEx = 512, uint8_t outputPath = 0)
    {
        ci::DiscoveryReplyFields fields{};
        fields.SourceMuid = from;
        fields.ManufacturerSysExId[0] = 0x7D;
        fields.CapabilityCategories = 0x1C;
        fields.ReceivableMaximumSysExSize = maximumSysEx;
        fields.OutputPathId = outputPath;

        std::vector<uint8_t> bytes(64);
        bytes.resize(ci::BuildDiscovery(fields, bytes.data(), bytes.size()));

        return bytes;
    }

    std::vector<uint8_t> ProfileMessage(
        ci::MessageType type,
        uint8_t address,
        uint32_t to,
        std::array<uint8_t, 5> const& id = {},
        uint8_t target = 0)
    {
        ci::ProfileMessageFields fields{};
        fields.Type = type;
        fields.DeviceId = address;
        fields.SourceMuid = Initiator;
        fields.DestinationMuid = to;
        std::copy(id.begin(), id.end(), fields.ProfileId);
        fields.ChannelCount = 1;
        fields.InquiryTarget = target;

        std::vector<uint8_t> bytes(64);
        bytes.resize(ci::BuildProfileMessage(fields, bytes.data(), bytes.size()));

        return bytes;
    }

    std::vector<uint8_t> PropertyMessage(ci::MessageType type, uint32_t to, uint8_t requestId, std::string const& header)
    {
        ci::PropertyExchangeMessageFields fields{};
        fields.Type = type;
        fields.SourceMuid = Initiator;
        fields.DestinationMuid = to;
        fields.RequestId = requestId;
        fields.Header = reinterpret_cast<uint8_t const*>(header.data());
        fields.HeaderByteCount = static_cast<uint16_t>(header.size());
        fields.ChunkCount = 1;
        fields.ChunkNumber = 1;

        std::vector<uint8_t> bytes(ci::PropertyExchangeFixedByteCount + header.size());
        bytes.resize(ci::BuildPropertyExchangeMessage(fields, bytes.data(), bytes.size()));

        return bytes;
    }

    std::vector<uint8_t> Report(uint8_t address, uint32_t to, uint8_t control)
    {
        ci::MidiMessageReportFields fields{};
        fields.MessageDataControl = control;
        fields.ChannelControllerMessages = 0x3F;
        fields.NoteDataMessages = 0x1F;

        std::vector<uint8_t> bytes(32);
        bytes.resize(ci::BuildMidiMessageReport(address, Initiator, to, fields, bytes.data(), bytes.size()));

        return bytes;
    }

    struct PropertyReply
    {
        bool Complete{ false };
        uint8_t Type{ 0 };
        uint8_t RequestId{ 0 };
        std::string Header{};
        std::string Data{};
        size_t Chunks{ 0 };
    };

    // The chunks of one reply, put together.
    PropertyReply ReadPropertyReply(std::vector<Answer> const& answers)
    {
        PropertyReply reply{};

        for (auto const& answer : answers)
        {
            auto const& parsed = answer.Parsed;

            if (!parsed.HasPropertyExchangeFields)
            {
                continue;
            }

            auto const& fields = parsed.PropertyExchange;
            auto const* bytes = answer.Bytes.data();

            reply.Type = static_cast<uint8_t>(parsed.Type);
            reply.RequestId = fields.RequestId;
            reply.Chunks++;

            if (fields.ChunkNumber == 1)
            {
                reply.Header.assign(bytes + fields.HeaderOffset, bytes + fields.HeaderOffset + fields.HeaderByteCount);
            }

            reply.Data.append(bytes + fields.DataOffset, bytes + fields.DataOffset + fields.DataByteCount);
            reply.Complete = fields.ChunkNumber == fields.ChunkCount;
        }

        return reply;
    }

    PropertyReply AskForProperty(
        CiResponderSettings const& settings,
        CiResponderState& state,
        ci::MessageType type,
        uint8_t requestId,
        std::string const& header)
    {
        Replies replies{};
        Ask(settings, state, PropertyMessage(type, MuidOf(state), requestId, header), replies);

        return ReadPropertyReply(Answers(replies.Messages));
    }

    bool Contains(std::string const& text, std::string const& part)
    {
        return text.find(part) != std::string::npos;
    }

    std::shared_ptr<CiDescription const> Describe(std::wstring_view text)
    {
        std::vector<CiFileProblem> problems{};
        return ParseCiDescription(text, problems);
    }

    bool HasProblem(std::vector<CiFileProblem> const& problems, CiFileProblemKind kind, CiFileSection section, int32_t index)
    {
        return std::any_of(problems.begin(), problems.end(), [&](CiFileProblem const& problem)
            {
                return problem.Kind == kind && problem.Section == section && problem.Index == index;
            });
    }

    std::array<uint8_t, 5> IdAt(Answer const& answer, size_t offset)
    {
        std::array<uint8_t, 5> id{};
        std::copy_n(answer.Bytes.begin() + offset, id.size(), id.begin());

        return id;
    }

    // Keeps each step's state from one message to the next, the way the engine does.
    class Sink : public RouteSink
    {
    public:
        explicit Sink(RouteGraph const& graph) :
            m_graph(graph),
            m_states(std::make_unique<BlockState[]>((std::max)(graph.States.size(), size_t{ 1 })))
        {
            for (size_t i = 0; i < graph.States.size(); i++)
            {
                PrepareBlockState(graph.States[i].Kind, m_states[i]);
            }
        }

        void CountLink(uint32_t) noexcept override {}
        void CountBlock(uint32_t, bool) noexcept override {}
        void Throttle(uint32_t, uint32_t const*, uint8_t) noexcept override {}
        void Clock(uint32_t, uint32_t const*, uint8_t) noexcept override {}

        void Send(uint32_t leaf, uint32_t const* words, uint8_t wordCount) noexcept override
        {
            Message message{};
            std::copy_n(words, wordCount, message.Words.begin());
            message.Count = wordCount;

            Sent.emplace_back(m_graph.Leaves[leaf].DestinationDeviceId, message);
        }

        BlockState* StateOf(uint32_t state) noexcept override
        {
            return state < m_graph.States.size() ? &m_states[state] : nullptr;
        }

        void Arrive(std::wstring const& device, Message const& message)
        {
            for (auto const& root : m_graph.Roots)
            {
                if (root.SourceDeviceId == device)
                {
                    RunRoot(m_graph, root, message.Words.data(), message.Count, *this);
                }
            }
        }

        std::vector<Message> To(std::wstring const& device) const
        {
            std::vector<Message> result{};

            for (auto const& [destination, message] : Sent)
            {
                if (destination == device)
                {
                    result.push_back(message);
                }
            }

            return result;
        }

        std::vector<std::pair<std::wstring, Message>> Sent{};

    private:
        RouteGraph const& m_graph;
        std::unique_ptr<BlockState[]> m_states{};
    };

    void AddEndpoint(PatchDocument& patch, std::wstring const& id)
    {
        PatchEndpoint endpoint{};
        endpoint.Id = id;
        endpoint.DisplayName = id;
        patch.Endpoints.push_back(endpoint);
    }

    void AddBlock(PatchDocument& patch, std::wstring const& id, BlockKind kind)
    {
        PatchBlock block{};
        block.Id = id;
        block.Kind = kind;
        block.Settings = DefaultBlockSettings(kind);
        patch.Blocks.push_back(block);
    }

    void Link(PatchDocument& patch, std::wstring const& id, std::wstring const& from, std::wstring const& to)
    {
        PatchConnection link{};
        link.Id = id;
        link.SourceId = from;
        link.DestinationId = to;
        patch.Connections.push_back(link);
    }

    RoutePatch Input(PatchDocument const& patch)
    {
        RoutePatch input{};
        input.Key = L"k";
        input.Patch = &patch;

        for (auto const& endpoint : patch.Endpoints)
        {
            input.DeviceIds[endpoint.Id] = L"dev-" + endpoint.Id;
        }

        return input;
    }
}

void CapabilityInquiryTests::SysExPacketsGoBothWays()
{
    for (size_t length = 1; length <= 20; length++)
    {
        std::vector<uint8_t> bytes(length);

        for (size_t i = 0; i < length; i++)
        {
            bytes[i] = static_cast<uint8_t>((i * 37 + 5) & 0x7F);
        }

        auto const packets = Packets(5, bytes);

        VERIFY_ARE_EQUAL((length + 5) / 6, packets.size());

        for (size_t i = 0; i < packets.size(); i++)
        {
            SysExPacket packet{};

            VERIFY_IS_TRUE(ReadSysExPacket(packets[i].Words.data(), packets[i].Count, packet));

            auto const expected = packets.size() == 1 ? SysExComplete
                : i == 0 ? SysExStart
                : i + 1 == packets.size() ? SysExEnd
                : SysExContinue;

            VERIFY_ARE_EQUAL(static_cast<int>(expected), static_cast<int>(packet.Status));
            VERIFY_ARE_EQUAL(5, static_cast<int>(packet.Group));
        }

        auto const answers = Answers(packets);

        VERIFY_ARE_EQUAL(size_t{ 1 }, answers.size());
        VERIFY_IS_TRUE(answers[0].Bytes == bytes);
    }

    SysExPacket packet{};
    auto const note = Midi1(0, NoteOn, 0, 60, 100);

    VERIFY_IS_FALSE(ReadSysExPacket(note.Words.data(), note.Count, packet));
}

void CapabilityInquiryTests::TheFileReadsWhatItCan()
{
    std::wstring const text = std::wstring{ LR"({
        "profiles": [
            { "id": [126, 33, 0, 1, 1], "name": "Drawbar organ", "channel": 0 },
            { "id": "7E 22 00 01 01", "target": "channel", "channel": 3, "channels": 2, "enabled": false,
              "details": [ { "target": 64, "data": [1, 2, 3] } ] },
            { "id": [126, 40, 0, 1, 1], "target": "group" },
            { "id": [126, 40, 0, 1, 1], "target": "group" },
            { "id": [128, 0, 0, 0, 0] },
            { "id": [126, 50, 0, 1, 1], "comment": "not a key the file uses" }
        ],
        "deviceInfo": { "manufacturer": "Contoso", "family": "Organs", "model": "Model 2", "version": "1.0", "serial": "1" },
        "resources": [
            { "resource": "ProgramList", "data": [ { "title": "Jazz" }, { "title": "Gospel" } ] },
            { "resource": "X-Notes", "resId": "a", "data": { "text": "Gr)" } + L"\u00FC\u00DF" + LR"(e" } },
            { "resource": "DeviceInfo", "data": {} },
            { "resource": "ProgramList", "data": [] },
            { "resource": "X-Size", "resId": "a", "comment": 1, "data": 1 }
        ],
        "extra": 1
    })";

    std::vector<CiFileProblem> problems{};
    auto const description = ParseCiDescription(text, problems);

    VERIFY_IS_NOT_NULL(description.get());

    // What could be read is used.
    VERIFY_ARE_EQUAL(size_t{ 4 }, description->Profiles.size());

    auto const& organ = description->Profiles[0];
    VERIFY_IS_TRUE(organ.Id == Organ);
    VERIFY_IS_TRUE(organ.Target == CiProfileTarget::Channel);
    VERIFY_ARE_EQUAL(0, static_cast<int>(organ.Channel));
    VERIFY_IS_TRUE(organ.Enabled);

    auto const& piano = description->Profiles[1];
    VERIFY_IS_TRUE(piano.Id == Piano);
    VERIFY_ARE_EQUAL(3, static_cast<int>(piano.Channel));
    VERIFY_ARE_EQUAL(2, static_cast<int>(piano.ChannelCount));
    VERIFY_IS_FALSE(piano.Enabled);
    VERIFY_ARE_EQUAL(size_t{ 1 }, piano.Details.size());
    VERIFY_ARE_EQUAL(64, static_cast<int>(piano.Details[0].Target));
    VERIFY_IS_TRUE(piano.Details[0].Data == std::vector<uint8_t>({ 1, 2, 3 }));

    VERIFY_IS_TRUE(description->Profiles[2].Target == CiProfileTarget::Group);
    VERIFY_IS_TRUE(description->Profiles[3].Target == CiProfileTarget::FunctionBlock);

    VERIFY_IS_TRUE(description->HasDeviceInfo);
    VERIFY_IS_TRUE(description->Manufacturer == "Contoso");

    VERIFY_ARE_EQUAL(size_t{ 3 }, description->Resources.size());
    VERIFY_IS_TRUE(description->Resources[0].IsArray);
    VERIFY_ARE_EQUAL(size_t{ 2 }, description->Resources[0].Items.size());

    // MIDI-CI carries seven bits, so everything else goes as an escape.
    auto const& notes = description->Resources[1].Json;
    VERIFY_IS_TRUE(Contains(notes, "\\u00FC\\u00DFe"));
    VERIFY_IS_TRUE(std::all_of(notes.begin(), notes.end(), [](char c) { return static_cast<uint8_t>(c) < 0x80; }));

    // And what couldn't is listed.
    VERIFY_ARE_EQUAL(size_t{ 8 }, problems.size());
    VERIFY_IS_TRUE(HasProblem(problems, CiFileProblemKind::DuplicateProfile, CiFileSection::Profiles, 3));
    VERIFY_IS_TRUE(HasProblem(problems, CiFileProblemKind::BadProfile, CiFileSection::Profiles, 4));
    VERIFY_IS_TRUE(HasProblem(problems, CiFileProblemKind::UnknownKey, CiFileSection::Profiles, 5));
    VERIFY_IS_TRUE(HasProblem(problems, CiFileProblemKind::UnknownKey, CiFileSection::DeviceInfo, -1));
    VERIFY_IS_TRUE(HasProblem(problems, CiFileProblemKind::BadResource, CiFileSection::Resources, 2));
    VERIFY_IS_TRUE(HasProblem(problems, CiFileProblemKind::DuplicateResource, CiFileSection::Resources, 3));
    VERIFY_IS_TRUE(HasProblem(problems, CiFileProblemKind::UnknownKey, CiFileSection::Resources, 4));
    VERIFY_IS_TRUE(HasProblem(problems, CiFileProblemKind::UnknownKey, CiFileSection::File, -1));

    // The same file reads the same, and any change shows.
    auto const again = Describe(text);
    VERIFY_ARE_EQUAL(description->Fingerprint, again->Fingerprint);

    auto changed = text;
    changed.replace(changed.find(L"\"enabled\": false"), 16, L"\"enabled\": true ");

    VERIFY_ARE_NOT_EQUAL(description->Fingerprint, Describe(changed)->Fingerprint);

    // Something that isn't a JSON object can't be read at all.
    for (auto const* bad : { L"[1, 2]", L"{ nope", L"" })
    {
        VERIFY_IS_NULL(ParseCiDescription(bad, problems).get());
        VERIFY_ARE_EQUAL(size_t{ 1 }, problems.size());
        VERIFY_IS_TRUE(problems[0].Kind == CiFileProblemKind::NotJson);
    }
}

void CapabilityInquiryTests::AResponderNeverReadsAPath()
{
    for (auto const* name : { L"..\\Organ.midici", L"C:\\Organ.midici", L"sub/Organ.midici", L"..", L"Organ.midici ", L"Organ.", L"" })
    {
        VERIFY_IS_FALSE(IsCiFileName(name));
    }

    VERIFY_IS_TRUE(IsCiFileName(L"Organ.midici"));
    VERIFY_IS_TRUE(IsCiFileName(L"My organ (2).midici"));

    json::JsonObject object{};
    object.SetNamedValue(L"file", json::JsonValue::CreateStringValue(L"..\\..\\secret.json"));

    auto const settings = BlockSettingsFromJson(BlockKind::CiResponder, object);

    VERIFY_IS_TRUE(settings.CiResponder.FileName.empty());

    // Numbers MIDI-CI can't carry leave the defaults alone.
    json::JsonObject numbers{};

    json::JsonArray manufacturer{};
    manufacturer.Append(json::JsonValue::CreateNumberValue(128));
    manufacturer.Append(json::JsonValue::CreateNumberValue(0));
    manufacturer.Append(json::JsonValue::CreateNumberValue(0));
    numbers.SetNamedValue(L"manufacturer", manufacturer);

    json::JsonArray version{};
    version.Append(json::JsonValue::CreateNumberValue(1));
    numbers.SetNamedValue(L"version", version);

    numbers.SetNamedValue(L"productInstanceId", json::JsonValue::CreateStringValue(L"SN\u00E9-1"));

    auto const read = BlockSettingsFromJson(BlockKind::CiResponder, numbers);

    VERIFY_IS_TRUE(read.CiResponder.Manufacturer == (std::array<uint8_t, 3>{ 0x7D, 0x00, 0x00 }));
    VERIFY_IS_TRUE(read.CiResponder.Version == (std::array<uint8_t, 4>{ 0, 0, 0, 0 }));
    VERIFY_ARE_EQUAL(std::wstring{ L"SN-1" }, read.CiResponder.ProductInstanceId);
}

void CapabilityInquiryTests::DiscoveryIsAnsweredWithWhoItIs()
{
    CiResponderSettings settings{};
    settings.Manufacturer = { 0x00, 0x20, 0x29 };
    settings.Family = 0x1234;
    settings.Model = 0x0155;
    settings.Version = { 1, 2, 3, 4 };

    auto state = std::make_unique<CiResponderState>();
    Replies replies{};

    // Kept from the device, and answered on the group it came in on.
    VERIFY_ARE_EQUAL(size_t{ 0 }, Ask(settings, *state, Discovery(Initiator, 1024, 5), replies, 2));

    auto const answers = Answers(replies.Messages);

    VERIFY_ARE_EQUAL(size_t{ 1 }, answers.size());
    VERIFY_ARE_EQUAL(2, static_cast<int>(answers[0].Group));

    auto const& reply = answers[0].Bytes;

    VERIFY_IS_TRUE(answers[0].Parsed.Type == ci::MessageType::DiscoveryReply);
    VERIFY_ARE_EQUAL(MuidOf(*state), answers[0].Parsed.SourceMuid);
    VERIFY_ARE_EQUAL(Initiator, answers[0].Parsed.DestinationMuid);
    VERIFY_ARE_EQUAL(ci::DiscoveryReplyByteCount, reply.size());

    VERIFY_ARE_EQUAL(0x00, static_cast<int>(reply[13]));
    VERIFY_ARE_EQUAL(0x20, static_cast<int>(reply[14]));
    VERIFY_ARE_EQUAL(0x29, static_cast<int>(reply[15]));
    VERIFY_ARE_EQUAL(0x1234, static_cast<int>(ci::ReadFourteenBitValue(reply.data() + 16)));
    VERIFY_ARE_EQUAL(0x0155, static_cast<int>(ci::ReadFourteenBitValue(reply.data() + 18)));
    VERIFY_ARE_EQUAL(1, static_cast<int>(reply[20]));
    VERIFY_ARE_EQUAL(4, static_cast<int>(reply[23]));

    // Process Inquiry only, with no file.
    VERIFY_ARE_EQUAL(0x10, static_cast<int>(reply[24]));
    VERIFY_ARE_EQUAL(static_cast<uint32_t>(MaximumCiMessageBytes), ci::ReadTwentyEightBitValue(reply.data() + 25));

    // The output path comes back as it was, and a MIDI 1.0 device has no function block.
    VERIFY_ARE_EQUAL(5, static_cast<int>(reply[29]));
    VERIFY_ARE_EQUAL(0x7F, static_cast<int>(reply[30]));

    auto const recent = state->Snapshot().Recent;

    VERIFY_ARE_EQUAL(size_t{ 1 }, recent.size());
    VERIFY_ARE_EQUAL(0x70, static_cast<int>(recent[0].MessageType));
    VERIFY_ARE_EQUAL(Initiator, recent[0].InitiatorMuid);
    VERIFY_IS_TRUE(recent[0].Outcome == CiOutcome::Answered);
}

void CapabilityInquiryTests::DiscoverySaysWhatTheFileAdds()
{
    auto const categories = [](CiResponderSettings const& settings, uint32_t from)
        {
            auto state = std::make_unique<CiResponderState>();
            Replies replies{};

            Ask(settings, *state, Discovery(from), replies);

            auto const answers = Answers(replies.Messages);

            return answers.size() == 1 ? static_cast<int>(answers[0].Bytes[24]) : -1;
        };

    CiResponderSettings settings{};
    settings.ProcessInquiry = false;

    VERIFY_ARE_EQUAL(0x00, categories(settings, Initiator));

    settings.Description = Describe(ProfilesFile);
    VERIFY_ARE_EQUAL(0x04, categories(settings, Initiator + 1));

    settings.Description = Describe(PropertiesFile());
    VERIFY_ARE_EQUAL(0x08, categories(settings, Initiator + 2));

    settings.ProcessInquiry = true;
    VERIFY_ARE_EQUAL(0x18, categories(settings, Initiator + 3));
}

void CapabilityInquiryTests::EndpointInquiryGetsTheProductInstanceId()
{
    CiResponderSettings settings{};
    settings.ProductInstanceId = L"SN-42";

    auto state = std::make_unique<CiResponderState>();

    auto const inquiry = [&state](uint32_t from)
        {
            std::vector<uint8_t> bytes(32);
            bytes.resize(ci::BuildEndpointInquiry(from, MuidOf(*state), ci::EndpointStatusProductInstanceId, bytes.data(), bytes.size()));

            return bytes;
        };

    Replies replies{};
    Ask(settings, *state, inquiry(Initiator), replies);

    auto answers = Answers(replies.Messages);

    VERIFY_ARE_EQUAL(size_t{ 1 }, answers.size());
    VERIFY_IS_TRUE(answers[0].Parsed.Type == ci::MessageType::EndpointReply);
    VERIFY_IS_TRUE(answers[0].Parsed.HasEndpointFields);

    auto const& endpoint = answers[0].Parsed.Endpoint;
    std::string const id(answers[0].Bytes.begin() + endpoint.InformationOffset,
        answers[0].Bytes.begin() + endpoint.InformationOffset + endpoint.InformationByteCount);

    VERIFY_IS_TRUE(id == "SN-42");

    // With none to give, a NAK.
    settings.ProductInstanceId.clear();
    replies.Messages.clear();

    Ask(settings, *state, inquiry(OtherInitiator), replies);
    answers = Answers(replies.Messages);

    VERIFY_ARE_EQUAL(size_t{ 1 }, answers.size());
    VERIFY_IS_TRUE(answers[0].Parsed.Type == ci::MessageType::Nak);
    VERIFY_ARE_EQUAL(0x72, static_cast<int>(answers[0].Parsed.Acknowledgment.OriginalMessageType));
}

void CapabilityInquiryTests::ProfileInquiryRepliesInOrder()
{
    CiResponderSettings settings{};
    settings.Description = Describe(ProfilesFile);

    auto state = std::make_unique<CiResponderState>();
    auto const muid = MuidOf(*state);

    Replies replies{};
    Ask(settings, *state, ProfileMessage(ci::MessageType::ProfileInquiry, 0x7F, muid), replies);

    auto answers = Answers(replies.Messages);

    // Channels first, then the group, and the function block last even with nothing in it.
    VERIFY_ARE_EQUAL(size_t{ 4 }, answers.size());

    uint8_t const addresses[]{ 0x00, 0x03, 0x7E, 0x7F };
    uint16_t const enabled[]{ 1, 0, 1, 0 };
    uint16_t const disabled[]{ 0, 1, 0, 0 };

    for (size_t i = 0; i < answers.size(); i++)
    {
        auto const& parsed = answers[i].Parsed;

        VERIFY_IS_TRUE(parsed.Type == ci::MessageType::ProfileInquiryReply);
        VERIFY_ARE_EQUAL(static_cast<int>(addresses[i]), static_cast<int>(parsed.DeviceId));
        VERIFY_ARE_EQUAL(Initiator, parsed.DestinationMuid);
        VERIFY_ARE_EQUAL(static_cast<int>(enabled[i]), static_cast<int>(parsed.Profile.EnabledProfileCount));
        VERIFY_ARE_EQUAL(static_cast<int>(disabled[i]), static_cast<int>(parsed.Profile.DisabledProfileCount));
    }

    VERIFY_IS_TRUE(IdAt(answers[0], answers[0].Parsed.Profile.EnabledProfileOffset) == Organ);
    VERIFY_IS_TRUE(IdAt(answers[1], answers[1].Parsed.Profile.DisabledProfileOffset) == Piano);
    VERIFY_IS_TRUE(IdAt(answers[2], answers[2].Parsed.Profile.EnabledProfileOffset) == Mixer);

    // A channel with none gets a reply with none.
    replies.Messages.clear();
    Ask(settings, *state, ProfileMessage(ci::MessageType::ProfileInquiry, 0x05, muid), replies);
    answers = Answers(replies.Messages);

    VERIFY_ARE_EQUAL(size_t{ 1 }, answers.size());
    VERIFY_ARE_EQUAL(5, static_cast<int>(answers[0].Parsed.DeviceId));
    VERIFY_ARE_EQUAL(0, static_cast<int>(answers[0].Parsed.Profile.EnabledProfileCount));
    VERIFY_ARE_EQUAL(0, static_cast<int>(answers[0].Parsed.Profile.DisabledProfileCount));

    // The group gets only the group's.
    replies.Messages.clear();
    Ask(settings, *state, ProfileMessage(ci::MessageType::ProfileInquiry, 0x7E, muid), replies);
    answers = Answers(replies.Messages);

    VERIFY_ARE_EQUAL(size_t{ 1 }, answers.size());
    VERIFY_ARE_EQUAL(1, static_cast<int>(answers[0].Parsed.Profile.EnabledProfileCount));

    // Somebody else's conversation is left alone.
    replies.Messages.clear();
    Ask(settings, *state, ProfileMessage(ci::MessageType::ProfileInquiry, 0x7F, muid + 1), replies);

    VERIFY_IS_TRUE(Answers(replies.Messages).empty());
}

void CapabilityInquiryTests::SetProfileReportsHowItIs()
{
    CiResponderSettings settings{};
    settings.Description = Describe(ProfilesFile);

    auto state = std::make_unique<CiResponderState>();
    auto const muid = MuidOf(*state);

    // The piano can't be turned on, so the report says it is still off, to everyone.
    Replies replies{};
    Ask(settings, *state, ProfileMessage(ci::MessageType::SetProfileOn, 0x03, muid, Piano), replies);

    auto answers = Answers(replies.Messages);

    VERIFY_ARE_EQUAL(size_t{ 1 }, answers.size());
    VERIFY_IS_TRUE(answers[0].Parsed.Type == ci::MessageType::ProfileDisabledReport);
    VERIFY_ARE_EQUAL(3, static_cast<int>(answers[0].Parsed.DeviceId));
    VERIFY_ARE_EQUAL(ci::MuidBroadcast, answers[0].Parsed.DestinationMuid);
    VERIFY_IS_TRUE(answers[0].Parsed.Profile.HasChannelCount);
    VERIFY_ARE_EQUAL(2, static_cast<int>(answers[0].Parsed.Profile.ChannelCount));

    // And the organ can't be turned off.
    replies.Messages.clear();
    Ask(settings, *state, ProfileMessage(ci::MessageType::SetProfileOff, 0x00, muid, Organ), replies);
    answers = Answers(replies.Messages);

    VERIFY_ARE_EQUAL(size_t{ 1 }, answers.size());
    VERIFY_IS_TRUE(answers[0].Parsed.Type == ci::MessageType::ProfileEnabledReport);

    // A profile it doesn't have, or one on another channel, gets a NAK naming it.
    for (auto const& [address, id] : { std::pair{ uint8_t{ 0x00 }, Unknown }, std::pair{ uint8_t{ 0x01 }, Organ } })
    {
        replies.Messages.clear();
        Ask(settings, *state, ProfileMessage(ci::MessageType::SetProfileOn, address, muid, id), replies);
        answers = Answers(replies.Messages);

        VERIFY_ARE_EQUAL(size_t{ 1 }, answers.size());
        VERIFY_IS_TRUE(answers[0].Parsed.Type == ci::MessageType::Nak);

        auto const& nak = answers[0].Parsed.Acknowledgment;

        VERIFY_ARE_EQUAL(0x22, static_cast<int>(nak.OriginalMessageType));
        VERIFY_ARE_EQUAL(0x04, static_cast<int>(nak.StatusCode));
        VERIFY_IS_TRUE(std::equal(id.begin(), id.end(), nak.Details));
    }
}

void CapabilityInquiryTests::ProfileDetailsComeFromTheFile()
{
    CiResponderSettings settings{};
    settings.Description = Describe(ProfilesFile);

    auto state = std::make_unique<CiResponderState>();
    auto const muid = MuidOf(*state);

    Replies replies{};
    Ask(settings, *state, ProfileMessage(ci::MessageType::ProfileDetailsInquiry, 0x03, muid, Piano, 64), replies);

    auto answers = Answers(replies.Messages);

    VERIFY_ARE_EQUAL(size_t{ 1 }, answers.size());
    VERIFY_IS_TRUE(answers[0].Parsed.Type == ci::MessageType::ProfileDetailsInquiryReply);

    auto const& profile = answers[0].Parsed.Profile;
    std::vector<uint8_t> const data(answers[0].Bytes.begin() + profile.TargetDataOffset,
        answers[0].Bytes.begin() + profile.TargetDataOffset + profile.TargetDataByteCount);

    VERIFY_ARE_EQUAL(64, static_cast<int>(profile.InquiryTarget));
    VERIFY_IS_TRUE(data == std::vector<uint8_t>({ 1, 2, 3 }));

    // A detail the file doesn't give, and a profile that isn't there, are both turned down.
    for (auto const& [id, target, status] : { std::tuple{ Piano, uint8_t{ 65 }, 0x00 }, std::tuple{ Organ, uint8_t{ 64 }, 0x04 } })
    {
        replies.Messages.clear();
        Ask(settings, *state, ProfileMessage(ci::MessageType::ProfileDetailsInquiry, 0x03, muid, id, target), replies);
        answers = Answers(replies.Messages);

        VERIFY_ARE_EQUAL(size_t{ 1 }, answers.size());
        VERIFY_IS_TRUE(answers[0].Parsed.Type == ci::MessageType::Nak);
        VERIFY_ARE_EQUAL(status, static_cast<int>(answers[0].Parsed.Acknowledgment.StatusCode));
    }
}

void CapabilityInquiryTests::PropertiesComeFromTheFile()
{
    CiResponderSettings settings{};
    settings.Description = Describe(PropertiesFile());

    auto state = std::make_unique<CiResponderState>();
    auto const muid = MuidOf(*state);

    // What it can do.
    std::vector<uint8_t> capabilities(32);
    auto size = ci::WriteCommonHeader(capabilities.data(), capabilities.size(), 0x7F,
        ci::MessageType::PropertyExchangeCapabilitiesInquiry, Initiator, muid);

    capabilities[size++] = 1;
    capabilities[size++] = 0;
    capabilities[size++] = 0;
    capabilities.resize(size);

    Replies replies{};
    Ask(settings, *state, capabilities, replies);

    auto const answers = Answers(replies.Messages);

    VERIFY_ARE_EQUAL(size_t{ 1 }, answers.size());
    VERIFY_IS_TRUE(answers[0].Parsed.Type == ci::MessageType::PropertyExchangeCapabilitiesReply);

    // The list says what each one is, leaving out what goes without saying.
    auto reply = AskForProperty(settings, *state, ci::MessageType::PropertyGetDataInquiry, 1, R"({"resource":"ResourceList"})");

    VERIFY_IS_TRUE(reply.Complete);
    VERIFY_ARE_EQUAL(0x35, static_cast<int>(reply.Type));
    VERIFY_ARE_EQUAL(1, static_cast<int>(reply.RequestId));
    VERIFY_IS_TRUE(Contains(reply.Header, R"("status":200)"));
    VERIFY_IS_TRUE(reply.Data == R"([{"resource":"DeviceInfo"},{"resource":"ProgramList","requireResId":false},{"resource":"ChannelList","canPaginate":true},{"resource":"X-Notes","requireResId":true}])");

    // The numbers come from the step and the names from the file.
    reply = AskForProperty(settings, *state, ci::MessageType::PropertyGetDataInquiry, 2, R"({"resource":"DeviceInfo"})");

    VERIFY_IS_TRUE(Contains(reply.Data, R"("manufacturerId":[125,0,0])"));
    VERIFY_IS_TRUE(Contains(reply.Data, R"("manufacturer":"Contoso")"));
    VERIFY_IS_TRUE(Contains(reply.Data, R"("model":"Model 2")"));

    // A page of a list, with how long the whole list is.
    reply = AskForProperty(settings, *state, ci::MessageType::PropertyGetDataInquiry, 3, R"({"resource":"ProgramList","offset":1,"limit":1})");

    VERIFY_IS_TRUE(Contains(reply.Header, R"("totalCount":3)"));
    VERIFY_IS_TRUE(Contains(reply.Data, "Gospel"));
    VERIFY_IS_FALSE(Contains(reply.Data, "Jazz"));
    VERIFY_IS_FALSE(Contains(reply.Data, "Rock"));
    VERIFY_IS_TRUE(reply.Data.front() == '[');
    VERIFY_IS_TRUE(reply.Data.back() == ']');

    reply = AskForProperty(settings, *state, ci::MessageType::PropertyGetDataInquiry, 4, R"({"resource":"ProgramList"})");

    VERIFY_IS_TRUE(Contains(reply.Data, "Jazz"));
    VERIFY_IS_TRUE(Contains(reply.Data, "Rock"));

    // A resource id picks which one.
    reply = AskForProperty(settings, *state, ci::MessageType::PropertyGetDataInquiry, 5, R"({"resource":"X-Notes"})");
    VERIFY_IS_TRUE(Contains(reply.Header, R"("status":404)"));

    reply = AskForProperty(settings, *state, ci::MessageType::PropertyGetDataInquiry, 6, R"({"resource":"X-Notes","resId":"a"})");
    VERIFY_IS_TRUE(Contains(reply.Header, R"("status":200)"));
    VERIFY_IS_TRUE(Contains(reply.Data, "\\u00FC"));

    reply = AskForProperty(settings, *state, ci::MessageType::PropertyGetDataInquiry, 7, R"({"resource":"Nothing"})");
    VERIFY_IS_TRUE(Contains(reply.Header, R"("status":404)"));

    // Nothing changes, so there is nothing to subscribe to.
    reply = AskForProperty(settings, *state, ci::MessageType::PropertySubscriptionInquiry, 8, R"({"command":"start","resource":"ProgramList"})");

    VERIFY_ARE_EQUAL(0x39, static_cast<int>(reply.Type));
    VERIFY_IS_TRUE(Contains(reply.Header, R"("status":405)"));

    reply = AskForProperty(settings, *state, ci::MessageType::PropertySubscriptionInquiry, 9, R"({"command":"end","subscribeId":"x"})");
    VERIFY_IS_TRUE(Contains(reply.Header, R"("status":200)"));

    // Setting a property isn't something it does.
    replies.Messages.clear();
    Ask(settings, *state, PropertyMessage(ci::MessageType::PropertySetDataInquiry, muid, 10, R"({"resource":"ProgramList"})"), replies);

    auto const refused = Answers(replies.Messages);

    VERIFY_ARE_EQUAL(size_t{ 1 }, refused.size());
    VERIFY_IS_TRUE(refused[0].Parsed.Type == ci::MessageType::Nak);
    VERIFY_ARE_EQUAL(0x36, static_cast<int>(refused[0].Parsed.Acknowledgment.OriginalMessageType));
}

void CapabilityInquiryTests::ALargePropertyComesInChunks()
{
    auto const text = std::wstring{ LR"({"resources":[{"resource":"X-Big","data":")" } + std::wstring(3000, L'a') + LR"("}]})";

    CiResponderSettings settings{};
    settings.Description = Describe(text);

    auto state = std::make_unique<CiResponderState>();

    // An initiator that takes 512 bytes at a time.
    Replies replies{};
    Ask(settings, *state, Discovery(Initiator, 512), replies);

    replies.Messages.clear();
    Ask(settings, *state, PropertyMessage(ci::MessageType::PropertyGetDataInquiry, MuidOf(*state), 1, R"({"resource":"X-Big"})"), replies);

    auto const answers = Answers(replies.Messages);
    auto const reply = ReadPropertyReply(answers);

    VERIFY_IS_TRUE(reply.Chunks > 5);
    VERIFY_IS_TRUE(reply.Complete);
    VERIFY_IS_TRUE(reply.Data == std::string(1, '"') + std::string(3000, 'a') + std::string(1, '"'));

    for (auto const& answer : answers)
    {
        // F0 and F7 count too.
        VERIFY_IS_TRUE(answer.Bytes.size() + 2 <= 512);
    }
}

void CapabilityInquiryTests::AReportSaysWhatWasSent()
{
    CiResponderSettings settings{};

    auto state = std::make_unique<CiResponderState>();
    auto const muid = MuidOf(*state);

    for (auto const& message : {
        Midi1(0, ControlChange, 0, 7, 90),
        Midi1(0, ControlChange, 0, 0, 1),
        Midi1(0, ControlChange, 0, 32, 2),
        Midi1(0, ProgramChange, 0, 5, 0),
        Midi1(0, NoteOn, 0, 60, 100),
        Midi1(0, NoteOn, 0, 64, 90),
        Midi1(0, NoteOff, 0, 64, 0),
        Midi1(0, PitchBend, 0, 0, 64),
        Midi1(0, ChannelPressure, 1, 70, 0),
        Midi2(0, ControlChange, 2, 74, 0, 0x12345678u) })
    {
        VERIFY_IS_TRUE(Play(settings, *state, message));
    }

    auto const report = [&](uint8_t address, uint8_t control, std::vector<Message> const& expected)
        {
            Replies replies{};

            Ask(settings, *state, Report(address, muid, control), replies);

            auto const answers = Answers(replies.Messages);

            VERIFY_ARE_EQUAL(size_t{ 2 }, answers.size());
            VERIFY_IS_TRUE(answers.front().Parsed.Type == ci::MessageType::MidiMessageReportReply);
            VERIFY_IS_TRUE(answers.back().Parsed.Type == ci::MessageType::MidiMessageReportEnd);

            // What it can report: pitch bend, control change, program change and channel
            // pressure, and the notes that are on.
            VERIFY_ARE_EQUAL(0x33, static_cast<int>(answers.front().Parsed.MidiMessageReport.ChannelControllerMessages));
            VERIFY_ARE_EQUAL(0x01, static_cast<int>(answers.front().Parsed.MidiMessageReport.NoteDataMessages));

            auto const sent = ChannelMessages(replies.Messages);

            VERIFY_ARE_EQUAL(expected.size(), sent.size());

            for (size_t i = 0; i < expected.size() && i < sent.size(); i++)
            {
                VERIFY_IS_TRUE(sent[i] == expected[i]);
            }
        };

    // Everything it knows, in the order of the bits, with the bank before the program.
    report(0x7F, ci::MessageDataControlFull, {
        Midi1(0, PitchBend, 0, 0, 64),
        Midi1(0, ControlChange, 0, 7, 90),
        Midi1(0, ControlChange, 0, 0, 1),
        Midi1(0, ControlChange, 0, 32, 2),
        Midi1(0, ProgramChange, 0, 5, 0),
        Midi1(0, NoteOn, 0, 60, 100),
        Midi1(0, ChannelPressure, 1, 70, 0),
        Midi2(0, ControlChange, 2, 74, 0, 0x12345678u) });

    // Only what isn't where it starts: the pitch bend is in the middle.
    report(0x7F, ci::MessageDataControlNonDefault, {
        Midi1(0, ControlChange, 0, 7, 90),
        Midi1(0, ControlChange, 0, 0, 1),
        Midi1(0, ControlChange, 0, 32, 2),
        Midi1(0, ProgramChange, 0, 5, 0),
        Midi1(0, NoteOn, 0, 60, 100),
        Midi1(0, ChannelPressure, 1, 70, 0),
        Midi2(0, ControlChange, 2, 74, 0, 0x12345678u) });

    // Asking what it could report gets no messages.
    report(0x7F, ci::MessageDataControlNone, {});

    // One channel, and MIDI 2.0 the way it was sent.
    report(0x01, ci::MessageDataControlFull, { Midi1(0, ChannelPressure, 1, 70, 0) });
    report(0x02, ci::MessageDataControlFull, { Midi2(0, ControlChange, 2, 74, 0, 0x12345678u) });
}

void CapabilityInquiryTests::AnInvalidatedMuidIsReplaced()
{
    CiResponderSettings settings{};

    auto state = std::make_unique<CiResponderState>();
    auto const first = MuidOf(*state);

    VERIFY_ARE_NOT_EQUAL(0u, first);
    VERIFY_IS_TRUE(first < ci::MuidReservedStart);

    std::vector<uint8_t> invalidate(32);
    invalidate.resize(ci::BuildInvalidateMuid(Initiator, first, invalidate.data(), invalidate.size()));

    Replies replies{};
    Ask(settings, *state, invalidate, replies);

    auto const second = MuidOf(*state);

    VERIFY_IS_TRUE(replies.Messages.empty());
    VERIFY_ARE_NOT_EQUAL(first, second);
    VERIFY_ARE_NOT_EQUAL(0u, second);
    VERIFY_IS_TRUE(second < ci::MuidReservedStart);
    VERIFY_IS_TRUE(state->Snapshot().Recent.front().Outcome == CiOutcome::NewMuid);

    // Another device with the same MUID: both give it up.
    Ask(settings, *state, Discovery(second), replies);

    auto const answers = Answers(replies.Messages);

    VERIFY_ARE_EQUAL(size_t{ 1 }, answers.size());
    VERIFY_IS_TRUE(answers[0].Parsed.Type == ci::MessageType::InvalidateMuid);
    VERIFY_ARE_EQUAL(second, answers[0].Parsed.TargetMuid);
    VERIFY_ARE_NOT_EQUAL(second, MuidOf(*state));
}

void CapabilityInquiryTests::MidiCiIsKeptFromTheDeviceUnlessPassed()
{
    CiResponderSettings settings{};

    auto state = std::make_unique<CiResponderState>();
    Replies replies{};

    VERIFY_ARE_EQUAL(size_t{ 0 }, Ask(settings, *state, Discovery(), replies));

    // Other system exclusive, short or long, goes on.
    std::vector<uint8_t> const identity{ 0x7E, 0x7F, 0x06, 0x01 };
    std::vector<uint8_t> const dump{ 0x43, 0x10, 0x4C, 0x00, 0x00, 0x7E, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };

    VERIFY_ARE_EQUAL(size_t{ 1 }, Ask(settings, *state, identity, replies));
    VERIFY_ARE_EQUAL(Packets(0, dump).size(), Ask(settings, *state, dump, replies));

    VERIFY_IS_TRUE(Play(settings, *state, Midi1(0, NoteOn, 0, 60, 100)));

    // Passed on when asked to, and still answered.
    settings.PassMidiCi = true;
    replies.Messages.clear();

    auto const discovery = Discovery(OtherInitiator);

    VERIFY_ARE_EQUAL(Packets(0, discovery).size(), Ask(settings, *state, discovery, replies));
    VERIFY_ARE_EQUAL(size_t{ 1 }, Answers(replies.Messages).size());
}

void CapabilityInquiryTests::TwoPathsAreKeptApart()
{
    CiResponderSettings settings{};

    auto state = std::make_unique<CiResponderState>();

    // Two questions arriving at once, packet by packet, from two places.
    auto const one = Packets(0, Discovery(Initiator));
    auto const two = Packets(0, Discovery(OtherInitiator));

    Replies first{};
    Replies second{};

    for (size_t i = 0; i < (std::max)(one.size(), two.size()); i++)
    {
        if (i < one.size())
        {
            RunCiResponder(settings, *state, 1, CiReturn{ true, 7, 0 }, one[i].Words.data(), one[i].Count, first);
        }

        if (i < two.size())
        {
            RunCiResponder(settings, *state, 2, CiReturn{ true, 8, 0 }, two[i].Words.data(), two[i].Count, second);
        }
    }

    auto const toFirst = Answers(first.Messages);
    auto const toSecond = Answers(second.Messages);

    VERIFY_ARE_EQUAL(size_t{ 1 }, toFirst.size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, toSecond.size());
    VERIFY_ARE_EQUAL(Initiator, toFirst[0].Parsed.DestinationMuid);
    VERIFY_ARE_EQUAL(OtherInitiator, toSecond[0].Parsed.DestinationMuid);

    // One question reaching the step two ways from one source is answered once.
    Replies both{};

    for (auto const& packet : Packets(0, Discovery(0x0111111)))
    {
        RunCiResponder(settings, *state, 1, CiReturn{ true, 7, 0 }, packet.Words.data(), packet.Count, both);
        RunCiResponder(settings, *state, 2, CiReturn{ true, 7, 0 }, packet.Words.data(), packet.Count, both);
    }

    VERIFY_ARE_EQUAL(size_t{ 1 }, Answers(both.Messages).size());

    // With nowhere to answer, nothing is.
    Replies nowhere{};

    for (auto const& packet : Packets(0, Discovery(0x0222222)))
    {
        RunCiResponder(settings, *state, 3, CiReturn{ false, 0, 0 }, packet.Words.data(), packet.Count, nowhere);
    }

    VERIFY_IS_TRUE(nowhere.Messages.empty());
}

void CapabilityInquiryTests::TheFilterSortsByCategory()
{
    CiFilterMemory memory{};

    auto const through = [&memory](CiFilterSettings const& settings, std::vector<Message> const& packets)
        {
            size_t passed{ 0 };

            for (auto const& packet : packets)
            {
                if (RunCiFilter(settings, memory, Path, packet.Words.data(), packet.Count))
                {
                    passed++;
                }
            }

            return passed;
        };

    auto const inquiry = Packets(0, ProfileMessage(ci::MessageType::ProfileInquiry, 0x7F, 0x0222222));
    auto const discovery = Packets(0, Discovery());
    auto const get = Packets(0, PropertyMessage(ci::MessageType::PropertyGetDataInquiry, 0x0222222, 1, R"({"resource":"DeviceInfo"})"));
    auto const identity = Packets(0, { 0x7E, 0x7F, 0x06, 0x01 });
    auto const note = Midi1(0, NoteOn, 0, 60, 100);

    VERIFY_IS_TRUE(inquiry.size() > 1);

    // Keeping profiles out leaves everything else alone.
    CiFilterSettings keep{};
    keep.Action = FilterAction::KeepOut;
    keep.Categories = CiCategoryProfiles;

    VERIFY_ARE_EQUAL(size_t{ 0 }, through(keep, inquiry));
    VERIFY_ARE_EQUAL(discovery.size(), through(keep, discovery));
    VERIFY_ARE_EQUAL(get.size(), through(keep, get));
    VERIFY_ARE_EQUAL(identity.size(), through(keep, identity));
    VERIFY_IS_TRUE(RunCiFilter(keep, memory, Path, note.Words.data(), note.Count));

    // Letting only property exchange through keeps out everything else.
    CiFilterSettings only{};
    only.Action = FilterAction::LetThrough;
    only.Categories = CiCategoryPropertyExchange;

    VERIFY_ARE_EQUAL(get.size(), through(only, get));
    VERIFY_ARE_EQUAL(size_t{ 0 }, through(only, discovery));
    VERIFY_ARE_EQUAL(size_t{ 0 }, through(only, identity));
    VERIFY_IS_FALSE(RunCiFilter(only, memory, Path, note.Words.data(), note.Count));
}

void CapabilityInquiryTests::AnswersGoBackWhereTheQuestionCameFrom()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"app");
    AddEndpoint(patch, L"synth");
    AddBlock(patch, L"ci", BlockKind::CiResponder);
    Link(patch, L"1", L"app", L"ci");
    Link(patch, L"2", L"ci", L"synth");

    auto const graph = CompileRoutes({ Input(patch) });

    VERIFY_IS_TRUE(graph.Problems.empty());
    VERIFY_ARE_EQUAL(size_t{ 1 }, graph.Roots.size());
    VERIFY_IS_TRUE(graph.Roots[0].ReplyLeaf < graph.Leaves.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"dev-app" }, graph.Leaves[graph.Roots[0].ReplyLeaf].DestinationDeviceId);

    Sink sink{ graph };

    for (auto const& packet : Packets(3, Discovery()))
    {
        sink.Arrive(L"dev-app", packet);
    }

    // The answer goes back on the question's group, and the device never sees either.
    auto const answers = Answers(sink.To(L"dev-app"));

    VERIFY_ARE_EQUAL(size_t{ 1 }, answers.size());
    VERIFY_ARE_EQUAL(3, static_cast<int>(answers[0].Group));
    VERIFY_IS_TRUE(answers[0].Parsed.Type == ci::MessageType::DiscoveryReply);
    VERIFY_IS_TRUE(sink.To(L"dev-synth").empty());

    // Everything else goes on to the device.
    sink.Arrive(L"dev-app", Midi1(3, NoteOn, 0, 60, 100));

    auto const played = sink.To(L"dev-synth");

    VERIFY_ARE_EQUAL(size_t{ 1 }, played.size());
    VERIFY_IS_TRUE(played[0] == Midi1(3, NoteOn, 0, 60, 100));

    // Bypassed, it answers nothing and needs no way back.
    patch.Blocks[0].Bypassed = true;

    auto const bypassed = CompileRoutes({ Input(patch) });

    VERIFY_ARE_EQUAL(NoReturnLeaf, bypassed.Roots[0].ReplyLeaf);

    Sink quiet{ bypassed };
    auto const packets = Packets(3, Discovery());

    for (auto const& packet : packets)
    {
        quiet.Arrive(L"dev-app", packet);
    }

    VERIFY_IS_TRUE(quiet.To(L"dev-app").empty());
    VERIFY_ARE_EQUAL(packets.size(), quiet.To(L"dev-synth").size());
}

void CapabilityInquiryTests::APassedQuestionGoesOutBeforeItsAnswer()
{
    // One endpoint both asks and is the device, so the question and the answer share a group.
    PatchDocument patch{};
    AddEndpoint(patch, L"loop");
    AddBlock(patch, L"ci", BlockKind::CiResponder);
    patch.Blocks[0].Settings.CiResponder.PassMidiCi = true;
    Link(patch, L"1", L"loop", L"ci");
    Link(patch, L"2", L"ci", L"loop");

    auto const graph = CompileRoutes({ Input(patch) });

    VERIFY_IS_TRUE(graph.Problems.empty());

    Sink sink{ graph };
    auto const packets = Packets(0, Discovery());

    for (auto const& packet : packets)
    {
        sink.Arrive(L"dev-loop", packet);
    }

    auto const sent = sink.To(L"dev-loop");

    VERIFY_IS_TRUE(sent.size() > packets.size());

    for (size_t i = 0; i < packets.size(); i++)
    {
        VERIFY_IS_TRUE(sent[i] == packets[i]);
    }

    auto const answers = Answers(std::vector<Message>(sent.begin() + packets.size(), sent.end()));

    VERIFY_ARE_EQUAL(size_t{ 1 }, answers.size());
    VERIFY_IS_TRUE(answers[0].Parsed.Type == ci::MessageType::DiscoveryReply);
}
