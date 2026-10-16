// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace SoundFontSynth;

namespace ci = WindowsMidiServicesCapabilityInquiry;

namespace
{
    constexpr uint32_t InitiatorMuid = 0x01234567;
    constexpr char const* ProductInstanceId = "SF2SYNTH-TEST0001";

    struct CaptureSink final : ISysExSink, IUmpSink
    {
        std::vector<std::vector<uint8_t>> Messages;

        // Channel voice messages that came as UMP words, and the order of everything that came:
        // 'S' for a system exclusive message, 'V' for a channel voice message.
        std::vector<std::pair<uint32_t, uint32_t>> ChannelVoice;
        std::string Order;

        void SendSysEx(uint8_t const* payload, size_t count) noexcept override
        {
            try
            {
                Messages.emplace_back(payload, payload + count);
                Order.push_back('S');
            }
            catch (...)
            {
            }
        }

        void SendWords(uint32_t const* words, size_t count) noexcept override
        {
            try
            {
                std::vector<uint8_t> sysex;

                // Every packet a synth sends this way is two words long.
                for (size_t i = 0; i + 1 < count; i += 2)
                {
                    auto const type = (words[i] >> 28) & 0xF;

                    if (type == 0x3)
                    {
                        auto const status = (words[i] >> 20) & 0xF;
                        auto const byteCount = (std::min)((words[i] >> 16) & 0xFu, 6u);

                        uint8_t const bytes[6]
                        {
                            static_cast<uint8_t>((words[i] >> 8) & 0x7F),
                            static_cast<uint8_t>(words[i] & 0x7F),
                            static_cast<uint8_t>((words[i + 1] >> 24) & 0x7F),
                            static_cast<uint8_t>((words[i + 1] >> 16) & 0x7F),
                            static_cast<uint8_t>((words[i + 1] >> 8) & 0x7F),
                            static_cast<uint8_t>(words[i + 1] & 0x7F),
                        };

                        if (status == 0 || status == 1)
                        {
                            sysex.clear();
                        }

                        sysex.insert(sysex.end(), bytes, bytes + byteCount);

                        if (status == 0 || status == 3)
                        {
                            Messages.push_back(sysex);
                            Order.push_back('S');
                        }
                    }
                    else if (type == 0x4)
                    {
                        ChannelVoice.emplace_back(words[i], words[i + 1]);
                        Order.push_back('V');
                    }
                    else
                    {
                        Order.push_back('?');
                    }
                }

                if ((count % 2) != 0)
                {
                    Order.push_back('?');
                }
            }
            catch (...)
            {
            }
        }
    };

    std::shared_ptr<SoundFont const> LoadBasicFont()
    {
        auto const bytes = Sf2Test::Build(Sf2Test::MakeBasicFont());

        MemoryByteSource source(bytes.data(), bytes.size());

        auto font = std::make_shared<SoundFont>();

        if (SoundFont::Load(source, Sf2LoadLimits{}, *font, nullptr) != Sf2LoadStatus::Ok)
        {
            return nullptr;
        }

        return font;
    }

    // Every string a person reads comes from the host. These say "Test" so a test can tell the
    // host's text from a built-in default.
    PropertyExchangeText MakeText()
    {
        PropertyExchangeText text{};

        text.Model = "Test Synth";
        text.MelodicChannel = "Test Melodic";
        text.PercussionChannel = "Test Percussion";
        text.MelodicProgramList = "Test Melodic Programs";
        text.DrumKitProgramList = "Test Drum Kits";
        text.ControllerList = "Test Controllers";
        text.Volume = "Test Volume";
        text.ChannelPressure = "Test Channel Pressure";

        return text;
    }

    std::unique_ptr<SynthCore> MakeCore()
    {
        auto font = LoadBasicFont();

        if (font == nullptr)
        {
            return nullptr;
        }

        return std::make_unique<SynthCore>(font, SynthIdentity{}, ProductInstanceId, MakeText());
    }

    void QueueSysEx(SynthCore& core, std::vector<uint8_t> const& payload)
    {
        std::vector<uint32_t> words(SysEx7WordCount(payload.size()));

        auto const written = BuildSysEx7Packets(0, payload.data(), payload.size(), words.data(), words.size());

        VERIFY_ARE_EQUAL(words.size(), written);

        core.QueueInbound(words.data(), static_cast<uint32_t>(written), 0);
    }

    // Runs the worker side as the app does, until nothing more comes out.
    void Pump(SynthCore& core, CaptureSink& sink)
    {
        core.PumpWithoutAudio(false);

        size_t quietPasses{ 0 };

        while (quietPasses < 4)
        {
            auto const before = sink.Messages.size() + sink.ChannelVoice.size();

            core.ServiceOutbound(sink);
            core.ServiceMessageReport(sink);
            ServicePropertyRequests(core, sink);
            core.ServiceOutbound(sink);

            quietPasses = (sink.Messages.size() + sink.ChannelVoice.size() == before) ? quietPasses + 1 : 0;
        }
    }

    std::vector<uint8_t> Bytes(size_t count, uint8_t const* buffer)
    {
        return std::vector<uint8_t>(buffer, buffer + count);
    }

    // Finds the first message of a type and parses it.
    bool FindMessage(CaptureSink const& sink, ci::MessageType type, ci::ParsedMessage& parsed, std::vector<uint8_t>* raw = nullptr)
    {
        for (auto const& message : sink.Messages)
        {
            if (ci::Parse(message.data(), message.size(), parsed) == ci::ParseStatus::Ok && parsed.Type == type)
            {
                if (raw != nullptr)
                {
                    *raw = message;
                }

                return true;
            }
        }

        return false;
    }

    uint32_t Discover(SynthCore& core, CaptureSink& sink)
    {
        ci::DiscoveryReplyFields discovery{};
        discovery.SourceMuid = InitiatorMuid;
        discovery.ManufacturerSysExId[0] = 0x7D;
        discovery.CapabilityCategories = ci::CategoryPropertyExchange;
        discovery.ReceivableMaximumSysExSize = 4096;
        discovery.MessageVersion = 0x02;

        uint8_t buffer[64]{};
        auto const size = ci::BuildDiscovery(discovery, buffer, sizeof(buffer));

        VERIFY_IS_TRUE(size > 0);

        QueueSysEx(core, Bytes(size, buffer));
        Pump(core, sink);

        ci::ParsedMessage reply{};

        if (!FindMessage(sink, ci::MessageType::DiscoveryReply, reply))
        {
            return 0;
        }

        return reply.SourceMuid;
    }

    struct PropertyReply
    {
        std::string Header;
        std::string Data;
        uint16_t Chunks{ 0 };
    };

    // Reassembles a chunked Property Exchange reply.
    bool CollectReply(CaptureSink const& sink, ci::MessageType type, uint8_t requestId, PropertyReply& reply)
    {
        reply = {};

        for (auto const& message : sink.Messages)
        {
            ci::ParsedMessage parsed{};

            if (ci::Parse(message.data(), message.size(), parsed) != ci::ParseStatus::Ok ||
                parsed.Type != type ||
                !parsed.HasPropertyExchangeFields ||
                parsed.PropertyExchange.RequestId != requestId)
            {
                continue;
            }

            auto const& fields = parsed.PropertyExchange;

            if (fields.ChunkNumber == 1)
            {
                reply.Header.assign(reinterpret_cast<char const*>(message.data() + fields.HeaderOffset), fields.HeaderByteCount);
            }

            reply.Data.append(reinterpret_cast<char const*>(message.data() + fields.DataOffset), fields.DataByteCount);
            reply.Chunks++;

            if (fields.ChunkNumber == fields.ChunkCount)
            {
                return true;
            }
        }

        return false;
    }

    PropertyReply Get(SynthCore& core, uint32_t synthMuid, uint8_t requestId, std::string const& header)
    {
        CaptureSink sink;

        ci::PropertyExchangeMessageFields fields{};
        fields.Type = ci::MessageType::PropertyGetDataInquiry;
        fields.SourceMuid = InitiatorMuid;
        fields.DestinationMuid = synthMuid;
        fields.RequestId = requestId;
        fields.Header = reinterpret_cast<uint8_t const*>(header.data());
        fields.HeaderByteCount = static_cast<uint16_t>(header.size());
        fields.ChunkCount = 1;
        fields.ChunkNumber = 1;

        uint8_t buffer[512]{};
        auto const size = ci::BuildPropertyExchangeMessage(fields, buffer, sizeof(buffer));

        VERIFY_IS_TRUE(size > 0);

        QueueSysEx(core, Bytes(size, buffer));
        Pump(core, sink);

        PropertyReply reply{};
        VERIFY_IS_TRUE(CollectReply(sink, ci::MessageType::PropertyGetDataReply, requestId, reply));

        return reply;
    }

    bool Contains(std::string const& text, std::string_view what)
    {
        return text.find(what) != std::string::npos;
    }

    // Sends Inquiry: MIDI Message Report and collects everything that comes back.
    void RequestMessageReport(SynthCore& core, uint32_t synthMuid, uint8_t deviceId, uint8_t dataControl, uint8_t channelControllers, CaptureSink& sink)
    {
        ci::MidiMessageReportFields fields{};
        fields.MessageDataControl = dataControl;
        fields.ChannelControllerMessages = channelControllers;

        uint8_t buffer[32]{};
        auto const size = ci::BuildMidiMessageReport(deviceId, InitiatorMuid, synthMuid, fields, buffer, sizeof(buffer));

        VERIFY_IS_TRUE(size > 0);

        QueueSysEx(core, Bytes(size, buffer));
        Pump(core, sink);
    }

    // Data entry for a registered parameter, the way a MIDI 1.0 sender does it.
    void SetRegisteredParameter(Synthesizer& engine, uint8_t channel, uint8_t index, uint8_t msb, uint8_t lsb)
    {
        engine.ControlChange(channel, 101, 0);
        engine.ControlChange(channel, 100, index);
        engine.ControlChange(channel, 6, msb);
        engine.ControlChange(channel, 38, lsb);
    }

    void VerifyChannelVoice(CaptureSink const& sink, std::pair<uint32_t, uint32_t> const* expected, size_t count)
    {
        for (auto const& message : sink.ChannelVoice)
        {
            Log::Comment(String().Format(L"%08X %08X", message.first, message.second));
        }

        VERIFY_ARE_EQUAL(count, sink.ChannelVoice.size());

        for (size_t i = 0; i < count && i < sink.ChannelVoice.size(); i++)
        {
            VERIFY_ARE_EQUAL(expected[i].first, sink.ChannelVoice[i].first);
            VERIFY_ARE_EQUAL(expected[i].second, sink.ChannelVoice[i].second);
        }
    }

    void VerifyReportFrame(CaptureSink const& sink, uint32_t synthMuid, uint8_t deviceId, uint8_t channelControllers)
    {
        VERIFY_IS_TRUE(sink.Messages.size() >= 2);

        if (sink.Messages.size() < 2)
        {
            return;
        }

        ci::ParsedMessage reply{};
        VERIFY_IS_TRUE(ci::Parse(sink.Messages.front().data(), sink.Messages.front().size(), reply) == ci::ParseStatus::Ok);
        VERIFY_IS_TRUE(reply.Type == ci::MessageType::MidiMessageReportReply);
        VERIFY_IS_TRUE(reply.HasMidiMessageReportFields);
        VERIFY_ARE_EQUAL(synthMuid, reply.SourceMuid);
        VERIFY_ARE_EQUAL(InitiatorMuid, reply.DestinationMuid);
        VERIFY_ARE_EQUAL(static_cast<int>(deviceId), static_cast<int>(reply.DeviceId));
        VERIFY_ARE_EQUAL(static_cast<int>(channelControllers), static_cast<int>(reply.MidiMessageReport.ChannelControllerMessages));
        VERIFY_ARE_EQUAL(0, static_cast<int>(reply.MidiMessageReport.SystemMessages));
        VERIFY_ARE_EQUAL(0, static_cast<int>(reply.MidiMessageReport.NoteDataMessages));

        ci::ParsedMessage end{};
        VERIFY_IS_TRUE(ci::Parse(sink.Messages.back().data(), sink.Messages.back().size(), end) == ci::ParseStatus::Ok);
        VERIFY_IS_TRUE(end.Type == ci::MessageType::MidiMessageReportEnd);
        VERIFY_ARE_EQUAL(static_cast<int>(deviceId), static_cast<int>(end.DeviceId));
        VERIFY_ARE_EQUAL(InitiatorMuid, end.DestinationMuid);
    }
}

class SynthCapabilityInquiryTests
{
public:
    BEGIN_TEST_CLASS(SynthCapabilityInquiryTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    TEST_METHOD(SysEx7PacketsRoundTrip)
    {
        uint8_t const payload[]{ 0x7E, 0x7F, 0x06, 0x01, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x01, 0x02 };

        uint32_t words[8]{};

        VERIFY_ARE_EQUAL(size_t{ 6 }, BuildSysEx7Packets(3, payload, sizeof(payload), words, 8));

        // Start, continue, end, each on group 3.
        VERIFY_ARE_EQUAL(0x33160000u | (0x7Eu << 8) | 0x7Fu, words[0]);
        VERIFY_ARE_EQUAL(0x2u, (words[2] >> 20) & 0xF);
        VERIFY_ARE_EQUAL(0x3u, (words[4] >> 20) & 0xF);
        VERIFY_ARE_EQUAL(1u, (words[4] >> 16) & 0xF);

        // Too small a buffer, and a byte with the top bit set, are both refused.
        VERIFY_ARE_EQUAL(size_t{ 0 }, BuildSysEx7Packets(0, payload, sizeof(payload), words, 4));

        uint8_t const bad[]{ 0x7E, 0x80 };
        VERIFY_ARE_EQUAL(size_t{ 0 }, BuildSysEx7Packets(0, bad, sizeof(bad), words, 8));
    }

    TEST_METHOD(AnswersTheMidi1IdentityRequest)
    {
        auto core = MakeCore();
        VERIFY_IS_NOT_NULL(core.get());

        CaptureSink sink;

        QueueSysEx(*core, { 0x7E, 0x7F, 0x06, 0x01 });
        Pump(*core, sink);

        VERIFY_ARE_EQUAL(size_t{ 1 }, sink.Messages.size());

        auto const& reply = sink.Messages[0];

        // 7E, device, 06, 02, a three byte manufacturer, family, model and revision.
        VERIFY_ARE_EQUAL(size_t{ 15 }, reply.size());
        VERIFY_ARE_EQUAL(0x06, static_cast<int>(reply[2]));
        VERIFY_ARE_EQUAL(0x02, static_cast<int>(reply[3]));

        // Microsoft, family 11, model 3.
        VERIFY_ARE_EQUAL(0x00, static_cast<int>(reply[4]));
        VERIFY_ARE_EQUAL(0x00, static_cast<int>(reply[5]));
        VERIFY_ARE_EQUAL(0x41, static_cast<int>(reply[6]));
        VERIFY_ARE_EQUAL(11, static_cast<int>(reply[7]));
        VERIFY_ARE_EQUAL(0, static_cast<int>(reply[8]));
        VERIFY_ARE_EQUAL(3, static_cast<int>(reply[9]));
        VERIFY_ARE_EQUAL(0, static_cast<int>(reply[10]));
    }

    TEST_METHOD(AnswersDiscovery)
    {
        auto core = MakeCore();
        VERIFY_IS_NOT_NULL(core.get());

        CaptureSink sink;
        auto const synthMuid = Discover(*core, sink);

        VERIFY_ARE_NOT_EQUAL(0u, synthMuid);
        VERIFY_ARE_EQUAL(core->Dispatcher().Muid(), synthMuid);

        ci::ParsedMessage reply{};
        std::vector<uint8_t> raw;

        VERIFY_IS_TRUE(FindMessage(sink, ci::MessageType::DiscoveryReply, reply, &raw));
        VERIFY_ARE_EQUAL(InitiatorMuid, reply.DestinationMuid);

        // Manufacturer, family and model follow the common header.
        VERIFY_ARE_EQUAL(0x00, static_cast<int>(raw[13]));
        VERIFY_ARE_EQUAL(0x00, static_cast<int>(raw[14]));
        VERIFY_ARE_EQUAL(0x41, static_cast<int>(raw[15]));
        VERIFY_ARE_EQUAL(11, static_cast<int>(raw[16]));
        VERIFY_ARE_EQUAL(3, static_cast<int>(raw[18]));

        // Property exchange and process inquiry are declared, because both are answered.
        VERIFY_ARE_EQUAL(static_cast<int>(ci::CategoryPropertyExchange), raw[24] & ci::CategoryPropertyExchange);
        VERIFY_ARE_EQUAL(static_cast<int>(ci::CategoryProcessInquiry), raw[24] & ci::CategoryProcessInquiry);
    }

    TEST_METHOD(AnswersEndpointInquiryWithTheProductInstanceId)
    {
        auto core = MakeCore();
        VERIFY_IS_NOT_NULL(core.get());

        CaptureSink discoverySink;
        auto const synthMuid = Discover(*core, discoverySink);
        VERIFY_ARE_NOT_EQUAL(0u, synthMuid);

        uint8_t buffer[32]{};
        auto const size = ci::BuildEndpointInquiry(InitiatorMuid, synthMuid, ci::EndpointStatusProductInstanceId, buffer, sizeof(buffer));
        VERIFY_IS_TRUE(size > 0);

        CaptureSink sink;
        QueueSysEx(*core, Bytes(size, buffer));
        Pump(*core, sink);

        ci::ParsedMessage reply{};
        std::vector<uint8_t> raw;

        VERIFY_IS_TRUE(FindMessage(sink, ci::MessageType::EndpointReply, reply, &raw));
        VERIFY_IS_TRUE(reply.HasEndpointFields);

        std::string const id(reinterpret_cast<char const*>(raw.data() + reply.Endpoint.InformationOffset), reply.Endpoint.InformationByteCount);

        VERIFY_ARE_EQUAL(std::string{ ProductInstanceId }, id);
    }

    TEST_METHOD(IgnoresMessagesForAnotherMuid)
    {
        auto core = MakeCore();
        VERIFY_IS_NOT_NULL(core.get());

        CaptureSink discoverySink;
        auto const synthMuid = Discover(*core, discoverySink);
        VERIFY_ARE_NOT_EQUAL(0u, synthMuid);

        uint8_t buffer[32]{};
        auto const size = ci::BuildEndpointInquiry(InitiatorMuid, synthMuid ^ 1u, ci::EndpointStatusProductInstanceId, buffer, sizeof(buffer));

        CaptureSink sink;
        QueueSysEx(*core, Bytes(size, buffer));
        Pump(*core, sink);

        VERIFY_ARE_EQUAL(size_t{ 0 }, sink.Messages.size());
    }

    TEST_METHOD(ServesTheResourceList)
    {
        auto core = MakeCore();
        VERIFY_IS_NOT_NULL(core.get());

        CaptureSink discoverySink;
        auto const synthMuid = Discover(*core, discoverySink);

        auto const reply = Get(*core, synthMuid, 1, R"({"resource":"ResourceList"})");

        Log::Comment(String().Format(L"%S", reply.Data.c_str()));

        VERIFY_IS_TRUE(Contains(reply.Header, "\"status\":200"));
        VERIFY_IS_TRUE(Contains(reply.Data, "DeviceInfo"));
        VERIFY_IS_TRUE(Contains(reply.Data, "ChannelList"));
        VERIFY_IS_TRUE(Contains(reply.Data, "ProgramList"));
        VERIFY_IS_TRUE(Contains(reply.Data, "ChCtrlList"));

        // M2-103-UM section 14: the list never lists itself.
        VERIFY_IS_FALSE(Contains(reply.Data, "ResourceList"));
    }

    TEST_METHOD(ServesDeviceInfo)
    {
        auto core = MakeCore();
        VERIFY_IS_NOT_NULL(core.get());

        CaptureSink discoverySink;
        auto const synthMuid = Discover(*core, discoverySink);

        auto const reply = Get(*core, synthMuid, 2, R"({"resource":"DeviceInfo"})");

        Log::Comment(String().Format(L"%S", reply.Data.c_str()));

        VERIFY_IS_TRUE(Contains(reply.Data, "\"manufacturer\":\"Microsoft\""));
        VERIFY_IS_TRUE(Contains(reply.Data, "Test Synth"));
    }

    TEST_METHOD(ServesBothProgramLists)
    {
        auto core = MakeCore();
        VERIFY_IS_NOT_NULL(core.get());

        CaptureSink discoverySink;
        auto const synthMuid = Discover(*core, discoverySink);

        auto const melodic = Get(*core, synthMuid, 3, R"({"resource":"ProgramList","resId":"melodic"})");

        VERIFY_IS_TRUE(Contains(melodic.Header, "\"totalCount\":1"));
        VERIFY_IS_TRUE(Contains(melodic.Data, "Sine Piano"));
        VERIFY_IS_FALSE(Contains(melodic.Data, "Sine Kit"));

        auto const drums = Get(*core, synthMuid, 4, R"({"resource":"ProgramList","resId":"drums"})");

        VERIFY_IS_TRUE(Contains(drums.Data, "Sine Kit"));
        VERIFY_IS_FALSE(Contains(drums.Data, "Sine Piano"));

        // Past the end is an empty page, not an error.
        auto const pastTheEnd = Get(*core, synthMuid, 5, R"({"resource":"ProgramList","resId":"melodic","offset":10,"limit":5})");

        VERIFY_IS_TRUE(Contains(pastTheEnd.Header, "\"status\":200"));
        VERIFY_IS_TRUE(Contains(pastTheEnd.Header, "\"totalCount\":1"));
        VERIFY_IS_FALSE(Contains(pastTheEnd.Data, "Sine Piano"));
    }

    TEST_METHOD(ServesTheChannelList)
    {
        auto core = MakeCore();
        VERIFY_IS_NOT_NULL(core.get());

        CaptureSink discoverySink;
        auto const synthMuid = Discover(*core, discoverySink);

        auto const reply = Get(*core, synthMuid, 6, R"({"resource":"ChannelList"})");

        Log::Comment(String().Format(L"%u chunks", reply.Chunks));

        // Channel 10 is the drum channel from the start; the rest are melodic. Both titles come
        // from the host, so they can be translated.
        VERIFY_IS_TRUE(Contains(reply.Data, "\"title\":\"Test Melodic\""));
        VERIFY_IS_TRUE(Contains(reply.Data, "\"title\":\"Test Percussion\""));
        VERIFY_IS_FALSE(Contains(reply.Data, "Channel 1\""));
        VERIFY_IS_TRUE(Contains(reply.Data, "Sine Piano"));
        VERIFY_IS_TRUE(Contains(reply.Data, "Sine Kit"));
        VERIFY_IS_TRUE(Contains(reply.Data, "\"resId\":\"drums\""));
        VERIFY_IS_TRUE(Contains(reply.Data, "Test Drum Kits"));

        // Every channel also links to the controllers it answers.
        VERIFY_IS_TRUE(Contains(reply.Data, "\"resource\":\"ChCtrlList\",\"resId\":\"channel\",\"title\":\"Test Controllers\""));
        VERIFY_IS_FALSE(Contains(reply.Header, "cacheTime"));
    }

    TEST_METHOD(AnswersAnUnknownResourceWith404)
    {
        auto core = MakeCore();
        VERIFY_IS_NOT_NULL(core.get());

        CaptureSink discoverySink;
        auto const synthMuid = Discover(*core, discoverySink);

        auto const unknown = Get(*core, synthMuid, 7, R"({"resource":"NoSuchThing"})");
        VERIFY_IS_TRUE(Contains(unknown.Header, "404"));

        auto const notJson = Get(*core, synthMuid, 8, R"({"resource":)");
        VERIFY_IS_TRUE(Contains(notJson.Header, "404"));
    }

    TEST_METHOD(SubscribersHearAboutProgramChanges)
    {
        auto core = MakeCore();
        VERIFY_IS_NOT_NULL(core.get());

        CaptureSink discoverySink;
        auto const synthMuid = Discover(*core, discoverySink);

        std::string const header{ R"({"command":"start","resource":"ChannelList"})" };

        ci::PropertyExchangeMessageFields fields{};
        fields.Type = ci::MessageType::PropertySubscriptionInquiry;
        fields.SourceMuid = InitiatorMuid;
        fields.DestinationMuid = synthMuid;
        fields.RequestId = 9;
        fields.Header = reinterpret_cast<uint8_t const*>(header.data());
        fields.HeaderByteCount = static_cast<uint16_t>(header.size());
        fields.ChunkCount = 1;
        fields.ChunkNumber = 1;

        uint8_t buffer[256]{};
        auto const size = ci::BuildPropertyExchangeMessage(fields, buffer, sizeof(buffer));

        CaptureSink sink;
        QueueSysEx(*core, Bytes(size, buffer));
        Pump(*core, sink);

        PropertyReply accepted{};
        VERIFY_IS_TRUE(CollectReply(sink, ci::MessageType::PropertySubscriptionReply, 9, accepted));
        VERIFY_IS_TRUE(Contains(accepted.Header, "\"status\":200"));
        VERIFY_IS_TRUE(Contains(accepted.Header, "subscribeId"));

        // A program change on channel 1. Program changes are channel voice messages, so they
        // are played by the audio thread; here the engine is driven directly instead.
        CaptureSink notifications;
        Pump(*core, notifications);

        core->Engine().ProgramChange(0, 5);
        Pump(*core, notifications);

        ci::ParsedMessage notify{};
        std::vector<uint8_t> raw;

        VERIFY_IS_TRUE(FindMessage(notifications, ci::MessageType::PropertySubscriptionInquiry, notify, &raw));

        std::string const notifyHeader(reinterpret_cast<char const*>(raw.data() + notify.PropertyExchange.HeaderOffset), notify.PropertyExchange.HeaderByteCount);

        VERIFY_IS_TRUE(Contains(notifyHeader, "\"command\":\"notify\""));
    }

    TEST_METHOD(ServesTheControllerList)
    {
        auto core = MakeCore();
        VERIFY_IS_NOT_NULL(core.get());

        CaptureSink discoverySink;
        auto const synthMuid = Discover(*core, discoverySink);

        auto const reply = Get(*core, synthMuid, 10, R"({"resource":"ChCtrlList","resId":"channel"})");

        Log::Comment(String().Format(L"%S", reply.Data.c_str()));

        VERIFY_IS_TRUE(Contains(reply.Header, "\"status\":200"));

        // Volume powers up at 100, which a protocol translator scales up to 0xC9249249. The
        // title is the host's.
        VERIFY_IS_TRUE(Contains(reply.Data, "{\"title\":\"Test Volume\",\"ctrlType\":\"cc\",\"ctrlIndex\":[7],\"priority\":1,\"default\":3374617161,\"transmit\":\"none\""));

        // Two semitones of bend range: 2 << 7 at fourteen bits, scaled up to 32.
        VERIFY_IS_TRUE(Contains(reply.Data, "\"ctrlType\":\"rpn\",\"ctrlIndex\":[0,0],\"priority\":4,\"default\":67108864,\"transmit\":\"none\",\"numSigBits\":14"));
        VERIFY_IS_TRUE(Contains(reply.Data, "{\"title\":\"Test Channel Pressure\",\"ctrlType\":\"chPress\""));
        VERIFY_IS_TRUE(Contains(reply.Data, "\"ctrlType\":\"pBend\""));
        VERIFY_IS_TRUE(Contains(reply.Data, "\"ctrlType\":\"pnp\""));

        // Text the host left alone stays in English.
        VERIFY_IS_TRUE(Contains(reply.Data, "\"title\":\"Sustain Pedal\""));

        // Bank select and the parameter number selectors have MIDI 2.0 messages of their own.
        VERIFY_IS_FALSE(Contains(reply.Data, "\"ctrlIndex\":[0]"));
        VERIFY_IS_FALSE(Contains(reply.Data, "\"ctrlIndex\":[101]"));

        // Without a resId the list is the same. Any other resId names nothing.
        auto const bare = Get(*core, synthMuid, 11, R"({"resource":"ChCtrlList"})");
        VERIFY_IS_TRUE(Contains(bare.Header, "\"status\":200"));
        VERIFY_ARE_EQUAL(reply.Data, bare.Data);

        auto const unknown = Get(*core, synthMuid, 12, R"({"resource":"ChCtrlList","resId":"drums"})");
        VERIFY_IS_TRUE(Contains(unknown.Header, "404"));
    }

    TEST_METHOD(AnswersProcessInquiryCapabilities)
    {
        auto core = MakeCore();
        VERIFY_IS_NOT_NULL(core.get());

        CaptureSink discoverySink;
        auto const synthMuid = Discover(*core, discoverySink);
        VERIFY_ARE_NOT_EQUAL(0u, synthMuid);

        uint8_t buffer[32]{};
        auto const size = ci::BuildProcessInquiryCapabilities(InitiatorMuid, synthMuid, buffer, sizeof(buffer));
        VERIFY_IS_TRUE(size > 0);

        CaptureSink sink;
        QueueSysEx(*core, Bytes(size, buffer));
        Pump(*core, sink);

        ci::ParsedMessage reply{};

        VERIFY_IS_TRUE(FindMessage(sink, ci::MessageType::ProcessInquiryCapabilitiesReply, reply));
        VERIFY_IS_TRUE(reply.HasProcessInquiryFeatures);
        VERIFY_ARE_EQUAL(static_cast<int>(ci::ProcessInquiryFeatureMidiMessageReport), static_cast<int>(reply.ProcessInquiryFeatures));
        VERIFY_ARE_EQUAL(synthMuid, reply.SourceMuid);
        VERIFY_ARE_EQUAL(InitiatorMuid, reply.DestinationMuid);
    }

    TEST_METHOD(ReportsAChannelInFull)
    {
        auto core = MakeCore();
        VERIFY_IS_NOT_NULL(core.get());

        CaptureSink discoverySink;
        auto const synthMuid = Discover(*core, discoverySink);
        VERIFY_ARE_NOT_EQUAL(0u, synthMuid);

        // MIDI 1.0 values on channel 1, each with a known MIDI 2.0 translation. Channel voice
        // messages are played by the audio thread, so the engine is driven directly.
        auto& engine = core->Engine();

        engine.PitchBend(0, 4096);
        engine.ControlChange(0, 1, 1);
        engine.ControlChange(0, 7, 100);
        engine.ControlChange(0, 10, 0);
        engine.ControlChange(0, 11, 127);
        engine.ControlChange(0, 64, 0);
        engine.ControlChange(0, 66, 127);
        engine.ControlChange(0, 91, 40);
        engine.ControlChange(0, 93, 0);
        SetRegisteredParameter(engine, 0, 0, 12, 0);
        SetRegisteredParameter(engine, 0, 1, 80, 0);
        SetRegisteredParameter(engine, 0, 2, 66, 0);
        engine.ControlChange(0, 0, 1);
        engine.ControlChange(0, 32, 2);
        engine.ProgramChange(0, 5);
        engine.ChannelPressure(0, 64);

        // Every channel family, assignable controllers included. This synth has none of those,
        // so the reply leaves that bit out.
        CaptureSink sink;
        RequestMessageReport(*core, synthMuid, 0x00, ci::MessageDataControlFull, 0x3F, sink);

        // The reply, fourteen channel voice messages, then the end, in that order.
        VERIFY_ARE_EQUAL(std::string{ "SVVVVVVVVVVVVVVS" }, sink.Order);
        VerifyReportFrame(sink, synthMuid, 0x00, 0x37);

        std::pair<uint32_t, uint32_t> const expected[]
        {
            { 0x40E00000u, 0x40000000u },   // pitch bend 4096
            { 0x40B00100u, 0x02000000u },   // modulation 1
            { 0x40B00700u, 0xC9249249u },   // volume 100
            { 0x40B00A00u, 0x00000000u },   // pan 0
            { 0x40B00B00u, 0xFFFFFFFFu },   // expression 127
            { 0x40B04000u, 0x00000000u },   // sustain off
            { 0x40B04200u, 0xFFFFFFFFu },   // sostenuto on
            { 0x40B05B00u, 0x50000000u },   // reverb send 40
            { 0x40B05D00u, 0x00000000u },   // chorus send 0
            { 0x40200000u, 0x18000000u },   // bend range 12 semitones
            { 0x40200001u, 0xA0010008u },   // fine tuning 80/0, 25 cents sharp
            { 0x40200002u, 0x84002001u },   // coarse tuning 66, 2 semitones sharp
            { 0x40C00001u, 0x05000102u },   // program 6 in bank 1/2, bank valid
            { 0x40D00000u, 0x80000000u },   // channel pressure 64
        };

        VerifyChannelVoice(sink, expected, std::size(expected));
    }

    TEST_METHOD(ReportsOnlyWhatChangedWhenAsked)
    {
        auto core = MakeCore();
        VERIFY_IS_NOT_NULL(core.get());

        CaptureSink discoverySink;
        auto const synthMuid = Discover(*core, discoverySink);
        VERIFY_ARE_NOT_EQUAL(0u, synthMuid);

        core->Engine().ControlChange(3, 7, 90);

        // The whole function block: sixteen channels, one changed value.
        CaptureSink sink;
        RequestMessageReport(*core, synthMuid, ci::DeviceIdFunctionBlock, ci::MessageDataControlNonDefault, 0x37, sink);

        VERIFY_ARE_EQUAL(std::string{ "SVS" }, sink.Order);
        VerifyReportFrame(sink, synthMuid, ci::DeviceIdFunctionBlock, 0x37);

        std::pair<uint32_t, uint32_t> const expected[]
        {
            { 0x40B30700u, 0xB4D34D34u },   // volume 90 on channel 4
        };

        VerifyChannelVoice(sink, expected, std::size(expected));
    }

    TEST_METHOD(ReportsMidi2ValuesUnchanged)
    {
        auto core = MakeCore();
        VERIFY_IS_NOT_NULL(core.get());

        CaptureSink discoverySink;
        auto const synthMuid = Discover(*core, discoverySink);
        VERIFY_ARE_NOT_EQUAL(0u, synthMuid);

        // Values a MIDI 1.0 message cannot carry come back at the resolution they arrived in.
        auto& engine = core->Engine();

        engine.PitchBend32(2, 0x9ABCDEF0);
        engine.ControlChange32(2, 7, 0x12345678);
        engine.ControlChange32(2, 10, 0xDEADBEEF);
        engine.ChannelPressure32(2, 0x87654321);

        CaptureSink sink;
        RequestMessageReport(*core, synthMuid, 0x02, ci::MessageDataControlNonDefault, 0x37, sink);

        VERIFY_ARE_EQUAL(std::string{ "SVVVVS" }, sink.Order);
        VerifyReportFrame(sink, synthMuid, 0x02, 0x37);

        std::pair<uint32_t, uint32_t> const expected[]
        {
            { 0x40E20000u, 0x9ABCDEF0u },
            { 0x40B20700u, 0x12345678u },
            { 0x40B20A00u, 0xDEADBEEFu },
            { 0x40D20000u, 0x87654321u },
        };

        VerifyChannelVoice(sink, expected, std::size(expected));
    }

    TEST_METHOD(SaysWhatItCanReportWhenAskedForNothing)
    {
        auto core = MakeCore();
        VERIFY_IS_NOT_NULL(core.get());

        CaptureSink discoverySink;
        auto const synthMuid = Discover(*core, discoverySink);
        VERIFY_ARE_NOT_EQUAL(0u, synthMuid);

        core->Engine().ControlChange(0, 7, 90);

        CaptureSink sink;
        RequestMessageReport(*core, synthMuid, 0x00, ci::MessageDataControlNone, 0x3F, sink);

        VERIFY_ARE_EQUAL(std::string{ "SS" }, sink.Order);
        VerifyReportFrame(sink, synthMuid, 0x00, 0x37);
    }

    TEST_METHOD(RefusesABadReportInquiry)
    {
        auto core = MakeCore();
        VERIFY_IS_NOT_NULL(core.get());

        CaptureSink discoverySink;
        auto const synthMuid = Discover(*core, discoverySink);
        VERIFY_ARE_NOT_EQUAL(0u, synthMuid);

        // A data control from the reserved range is malformed.
        CaptureSink reservedControl;
        RequestMessageReport(*core, synthMuid, 0x00, 0x05, 0x3F, reservedControl);

        ci::ParsedMessage nak{};

        VERIFY_IS_TRUE(FindMessage(reservedControl, ci::MessageType::Nak, nak));
        VERIFY_IS_TRUE(nak.HasAcknowledgmentFields);
        VERIFY_ARE_EQUAL(static_cast<int>(ci::NakStatusMessageMalformed), static_cast<int>(nak.Acknowledgment.StatusCode));
        VERIFY_ARE_EQUAL(size_t{ 0 }, reservedControl.ChannelVoice.size());

        // A device id from the reserved range names nothing in use.
        CaptureSink reservedDevice;
        RequestMessageReport(*core, synthMuid, 0x20, ci::MessageDataControlFull, 0x3F, reservedDevice);

        VERIFY_IS_TRUE(FindMessage(reservedDevice, ci::MessageType::Nak, nak));
        VERIFY_ARE_EQUAL(static_cast<int>(ci::NakStatusNotInUse), static_cast<int>(nak.Acknowledgment.StatusCode));
        VERIFY_ARE_EQUAL(size_t{ 0 }, reservedDevice.ChannelVoice.size());

        VERIFY_ARE_EQUAL(uint64_t{ 0 }, core->Dispatcher().Statistics().MessageReportRequests);
    }

    TEST_METHOD(SurvivesRandomSysEx)
    {
        auto core = MakeCore();
        VERIFY_IS_NOT_NULL(core.get());

        std::mt19937 random(1012);
        std::uniform_int_distribution<int> length(1, 300);
        std::uniform_int_distribution<int> value(0, 127);
        std::uniform_int_distribution<int> type(0x20, 0x7F);

        CaptureSink sink;

        for (int i = 0; i < 2000; i++)
        {
            std::vector<uint8_t> payload(static_cast<size_t>(length(random)));

            for (auto& byte : payload)
            {
                byte = static_cast<uint8_t>(value(random));
            }

            // Mostly MIDI-CI shaped, so the parser gets past its first checks.
            if (payload.size() > 4 && (i % 4) != 0)
            {
                payload[0] = 0x7E;
                payload[1] = 0x7F;
                payload[2] = 0x0D;
                payload[3] = static_cast<uint8_t>(type(random));
            }

            QueueSysEx(*core, payload);
            Pump(*core, sink);
        }

        // Still answers afterwards.
        CaptureSink after;
        QueueSysEx(*core, { 0x7E, 0x7F, 0x06, 0x01 });
        Pump(*core, after);

        VERIFY_ARE_EQUAL(size_t{ 1 }, after.Messages.size());
    }

    TEST_METHOD(SurvivesRandomPackets)
    {
        auto core = MakeCore();
        VERIFY_IS_NOT_NULL(core.get());

        std::mt19937 random(31337);
        std::uniform_int_distribution<uint32_t> word;

        std::vector<float> audio(256 * 2);
        CaptureSink sink;

        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);

        for (int block = 0; block < 400; block++)
        {
            uint32_t words[64]{};

            for (auto& w : words)
            {
                w = word(random);
            }

            core->QueueInbound(words, 64, 0);

            LARGE_INTEGER now{};
            QueryPerformanceCounter(&now);

            core->RenderAudio(audio.data(), 256, now.QuadPart, frequency.QuadPart);

            for (auto const sample : audio)
            {
                VERIFY_IS_TRUE(std::isfinite(sample));
            }

            core->ServiceOutbound(sink);
            ServicePropertyRequests(*core, sink);
        }
    }
};
