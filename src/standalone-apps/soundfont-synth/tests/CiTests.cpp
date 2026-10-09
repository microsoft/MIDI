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

    struct CaptureSink final : ISysExSink
    {
        std::vector<std::vector<uint8_t>> Messages;

        void SendSysEx(uint8_t const* payload, size_t count) noexcept override
        {
            try
            {
                Messages.emplace_back(payload, payload + count);
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

    std::unique_ptr<SynthCore> MakeCore()
    {
        auto font = LoadBasicFont();

        if (font == nullptr)
        {
            return nullptr;
        }

        return std::make_unique<SynthCore>(font, SynthIdentity{}, ProductInstanceId, "Test Synth");
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
            auto const before = sink.Messages.size();

            core.ServiceOutbound(sink);
            ServicePropertyRequests(core, sink);
            core.ServiceOutbound(sink);

            quietPasses = (sink.Messages.size() == before) ? quietPasses + 1 : 0;
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

        // Property exchange is declared, because it is answered.
        VERIFY_ARE_EQUAL(static_cast<int>(ci::CategoryPropertyExchange), raw[24] & ci::CategoryPropertyExchange);
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

        VERIFY_IS_TRUE(Contains(reply.Data, "Channel 1\""));
        VERIFY_IS_TRUE(Contains(reply.Data, "Channel 16\""));
        VERIFY_IS_TRUE(Contains(reply.Data, "Sine Piano"));
        VERIFY_IS_TRUE(Contains(reply.Data, "Sine Kit"));
        VERIFY_IS_TRUE(Contains(reply.Data, "\"resId\":\"drums\""));
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
