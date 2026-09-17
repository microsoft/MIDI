// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

// libmidi2 calls sprintf in a debug helper, which this project compiles as an error. The
// suppression is scoped to these includes so the rest of the app keeps the check.
#pragma warning(push)
#pragma warning(disable: 4996)
#include <libmidi2/midiCIMessageCreate.h>
#include <libmidi2/midiCIProcessor.h>
#include <libmidi2/utils.h>
#pragma warning(pop)

#include "MidiCiProgramList.h"
#include "Telemetry.h"

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Data::Json;

namespace midikeyboard
{
    namespace
    {
        constexpr uint8_t CiVersion = 0x02;

        // SysEx7 UMP carries at most six data bytes per packet
        constexpr uint32_t SysEx7BytesPerPacket = 6;
        constexpr uint8_t SysEx7StatusComplete = 0x0;
        constexpr uint8_t SysEx7StatusStart = 0x1;
        constexpr uint8_t SysEx7StatusContinue = 0x2;
        constexpr uint8_t SysEx7StatusEnd = 0x3;

        constexpr uint32_t MessageTypeSysEx7 = 0x3;

        // Windows MIDI Services has no registered CI identity of its own, so the query
        // introduces itself with the "for educational use" manufacturer id.
        constexpr std::array<uint8_t, 3> ManufacturerId{ 0x7D, 0x00, 0x00 };
        constexpr std::array<uint8_t, 2> FamilyId{ 0x00, 0x00 };
        constexpr std::array<uint8_t, 2> ModelId{ 0x00, 0x00 };
        constexpr std::array<uint8_t, 4> DeviceVersion{ 0x01, 0x00, 0x00, 0x00 };

        // Anything bigger than this from a device is not a program list we can show.
        constexpr uint32_t MaximumProgramEntries = 4096;
        constexpr int32_t ProgramListPageSize = 128;

        // Largest CI message we build. A Get carries only a short JSON header.
        constexpr uint16_t CiSendBufferBytes = 512;

        uint32_t GenerateMuid() noexcept
        {
            std::random_device device{};
            std::mt19937 engine{ device() };

            // MUIDs are 28 bits, and the all-ones value is reserved for broadcast
            std::uniform_int_distribution<uint32_t> distribution{ 0, M2_CI_BROADCAST - 1 };

            return distribution(engine);
        }

        std::wstring Utf8ToWide(std::string const& value) noexcept
        {
            if (value.empty())
            {
                return {};
            }

            auto const required = ::MultiByteToWideChar(
                CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);

            if (required <= 0)
            {
                return {};
            }

            std::wstring result(static_cast<size_t>(required), L'\0');

            ::MultiByteToWideChar(
                CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), required);

            return result;
        }

        // Property Exchange headers and bodies are JSON. A device that answers with something
        // malformed should leave the app on its numeric spinners, not crash it.
        JsonObject TryParseObject(std::string const& text) noexcept
        {
            JsonObject result{ nullptr };

            if (!JsonObject::TryParse(winrt::hstring{ Utf8ToWide(text) }, result))
            {
                return nullptr;
            }

            return result;
        }

        JsonArray TryParseArray(std::vector<uint8_t> const& body) noexcept
        {
            if (body.empty())
            {
                return nullptr;
            }

            std::string const text{ reinterpret_cast<char const*>(body.data()), body.size() };

            JsonArray result{ nullptr };

            if (!JsonArray::TryParse(winrt::hstring{ Utf8ToWide(text) }, result))
            {
                return nullptr;
            }

            return result;
        }

        int32_t GetNumber(JsonObject const& object, wchar_t const* name, int32_t fallback) noexcept
        {
            if (object == nullptr || !object.HasKey(name))
            {
                return fallback;
            }

            try
            {
                return static_cast<int32_t>(object.GetNamedNumber(name, static_cast<double>(fallback)));
            }
            catch (...)
            {
                return fallback;
            }
        }

        std::wstring GetText(JsonObject const& object, wchar_t const* name) noexcept
        {
            if (object == nullptr || !object.HasKey(name))
            {
                return {};
            }

            try
            {
                return std::wstring{ object.GetNamedString(name, L"") };
            }
            catch (...)
            {
                return {};
            }
        }

        // "tags" and "category" are arrays of strings; join them for display
        std::wstring GetStringArray(JsonObject const& object, wchar_t const* name) noexcept
        {
            std::wstring result{};

            try
            {
                if (object == nullptr || !object.HasKey(name))
                {
                    return result;
                }

                auto const values = object.GetNamedArray(name, nullptr);

                if (values == nullptr)
                {
                    return result;
                }

                for (auto const& value : values)
                {
                    if (value == nullptr || value.ValueType() != JsonValueType::String)
                    {
                        continue;
                    }

                    auto const text = std::wstring{ value.GetString() };

                    if (text.empty())
                    {
                        continue;
                    }

                    if (!result.empty())
                    {
                        result += L", ";
                    }

                    result += text;
                }
            }
            catch (...)
            {
            }

            return result;
        }
    }

    // The processor is stateful per exchange and its callbacks capture the query, so each query
    // owns one. Kept out of the header so libmidi2 does not leak into the rest of the app.
    struct CiProcessorHolder
    {
        midiCIProcessor Processor{};
    };

    namespace
    {
        std::unordered_map<MidiCiProgramListQuery const*, std::shared_ptr<CiProcessorHolder>> g_processors{};
        std::mutex g_processorsLock{};

        std::shared_ptr<CiProcessorHolder> GetProcessor(MidiCiProgramListQuery const* owner) noexcept
        {
            std::scoped_lock lock{ g_processorsLock };

            auto const it = g_processors.find(owner);

            return it == g_processors.end() ? nullptr : it->second;
        }
    }

    _Use_decl_annotations_
    std::shared_ptr<MidiCiProgramListQuery> MidiCiProgramListQuery::Start(
        MidiEndpointConnection const& connection,
        uint8_t group,
        uint8_t channel,
        CompletedHandler handler) noexcept
    {
        if (connection == nullptr || handler == nullptr)
        {
            return nullptr;
        }

        auto query = std::make_shared<MidiCiProgramListQuery>();

        query->Begin(connection, group, channel, std::move(handler));

        return query;
    }

    MidiCiProgramListQuery::~MidiCiProgramListQuery() noexcept
    {
        Cancel();
    }

    _Use_decl_annotations_
    void MidiCiProgramListQuery::Begin(
        MidiEndpointConnection const& connection,
        uint8_t group,
        uint8_t channel,
        CompletedHandler handler) noexcept
    {
        try
        {
            std::scoped_lock lock{ m_lock };

            m_connection = connection;
            m_group = group;
            m_channel = channel;
            m_handler = std::move(handler);
            m_localMuid = GenerateMuid();
            m_self = shared_from_this();

            auto holder = std::make_shared<CiProcessorHolder>();

            {
                std::scoped_lock processorLock{ g_processorsLock };
                g_processors[this] = holder;
            }

            auto const self = this;

            // Ignore traffic addressed to some other initiator on the same wire.
            holder->Processor.setCheckMUID(
                [self](uint8_t, uint32_t muid, void*) { return muid == self->m_localMuid; });

            holder->Processor.setRecvDiscoveryReply(
                [self](MIDICI ciDetails, std::array<uint8_t, 3>, std::array<uint8_t, 2>,
                       std::array<uint8_t, 2>, std::array<uint8_t, 4>, uint8_t, uint16_t, uint8_t, uint8_t)
                {
                    self->OnDiscoveryReply(ciDetails.remoteMUID);
                });

            holder->Processor.setRecvPEGetReply(
                [self](MIDICI ciDetails, std::string requestDetails, uint16_t bodyLen, uint8_t* body,
                       bool, bool lastByteOfSet)
                {
                    // a reply to something we did not ask for must not land in the body we are
                    // assembling, or two resources get spliced together
                    if (ciDetails.requestId != self->m_requestId)
                    {
                        return;
                    }

                    if (body != nullptr && bodyLen > 0)
                    {
                        self->m_replyBody.insert(self->m_replyBody.end(), body, body + bodyLen);
                    }

                    if (lastByteOfSet)
                    {
                        self->OnPropertyReply(requestDetails, self->m_replyBody);
                    }
                });

            // A device that cannot answer says so rather than staying silent.
            holder->Processor.setRecvNAK(
                [self](MIDICI, uint8_t, uint8_t, uint8_t, uint8_t*, uint16_t, uint8_t*)
                {
                    self->Complete(ProgramListResult::NotSupported);
                });

            m_messageToken = m_connection.MessageReceived(
                { this, &MidiCiProgramListQuery::OnMessageReceived });

            m_stage = Stage::AwaitingDiscoveryReply;

            uint8_t buffer[CiSendBufferBytes]{};

            auto const length = CIMessage::sendDiscoveryRequest(
                buffer,
                CiVersion,
                m_localMuid,
                ManufacturerId,
                FamilyId,
                ModelId,
                DeviceVersion,
                0x1C,                       // supports everything except protocol negotiation
                CiSendBufferBytes,
                0);

            SendCiMessage(buffer, length);

            ArmTimeout();
        }
        catch (...)
        {
            Complete(ProgramListResult::NoResponse);
        }
    }

    void MidiCiProgramListQuery::Cancel() noexcept
    {
        try
        {
            std::scoped_lock lock{ m_lock };

            if (m_connection != nullptr && m_messageToken.value != 0)
            {
                m_connection.MessageReceived(m_messageToken);
                m_messageToken = {};
            }

            if (m_timeoutTimer != nullptr)
            {
                m_timeoutTimer.Cancel();
                m_timeoutTimer = nullptr;
            }

            m_handler = nullptr;
            m_stage = Stage::Finished;
            m_connection = nullptr;

            {
                std::scoped_lock processorLock{ g_processorsLock };
                g_processors.erase(this);
            }

            m_self.reset();
        }
        catch (...)
        {
        }
    }

    void MidiCiProgramListQuery::ArmTimeout() noexcept
    {
        try
        {
            if (m_timeoutTimer != nullptr)
            {
                m_timeoutTimer.Cancel();
                m_timeoutTimer = nullptr;
            }

            auto const generation = ++m_timeoutGeneration;
            auto const self = shared_from_this();

            m_timeoutTimer = winrt::Windows::System::Threading::ThreadPoolTimer::CreateTimer(
                [self, generation](auto&&)
                {
                    std::scoped_lock lock{ self->m_lock };

                    // a reply landed and moved the query on, so this timer is stale
                    if (generation != self->m_timeoutGeneration || self->m_stage == Stage::Finished)
                    {
                        return;
                    }

                    self->Complete(self->m_stage == Stage::AwaitingDiscoveryReply
                        ? ProgramListResult::NoResponse
                        : ProgramListResult::NotSupported);
                },
                std::chrono::milliseconds{ StepTimeoutMilliseconds });
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void MidiCiProgramListQuery::OnMessageReceived(
        winrt::Windows::Foundation::IInspectable const&,
        MidiMessageReceivedEventArgs const& args) noexcept
    {
        try
        {
            // The completion handler can make the owner drop its reference before this call
            // returns, and Cancel releases ours, so the whole chain below runs on a borrowed
            // pointer unless one is held here.
            auto const keepAlive = shared_from_this();

            uint32_t word0{};
            uint32_t word1{};
            uint32_t word2{};
            uint32_t word3{};

            auto const wordCount = args.FillWords(word0, word1, word2, word3);

            if (wordCount < 2 || (word0 >> 28) != MessageTypeSysEx7)
            {
                return;
            }

            if (((word0 >> 24) & 0x0F) != m_group)
            {
                return;
            }

            auto const status = static_cast<uint8_t>((word0 >> 20) & 0x0F);
            auto const count = static_cast<uint8_t>((word0 >> 16) & 0x0F);

            if (count > SysEx7BytesPerPacket)
            {
                return;
            }

            auto const holder = GetProcessor(this);

            if (holder == nullptr)
            {
                return;
            }

            std::scoped_lock lock{ m_lock };

            if (m_stage == Stage::Finished)
            {
                return;
            }

            uint8_t const bytes[SysEx7BytesPerPacket]{
                static_cast<uint8_t>((word0 >> 8) & 0x7F),
                static_cast<uint8_t>(word0 & 0x7F),
                static_cast<uint8_t>((word1 >> 24) & 0x7F),
                static_cast<uint8_t>((word1 >> 16) & 0x7F),
                static_cast<uint8_t>((word1 >> 8) & 0x7F),
                static_cast<uint8_t>(word1 & 0x7F) };

            if (status == SysEx7StatusComplete || status == SysEx7StatusStart)
            {
                // the UMP payload is the CI message itself: no F0 or F7 to strip
                holder->Processor.startSysex7(m_group, FUNCTION_BLOCK);
            }

            for (uint8_t i = 0; i < count; i++)
            {
                holder->Processor.processMIDICI(bytes[i]);
            }

            if (status == SysEx7StatusComplete || status == SysEx7StatusEnd)
            {
                holder->Processor.endSysex7();
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void MidiCiProgramListQuery::SendCiMessage(uint8_t const* bytes, uint16_t length) noexcept
    {
        try
        {
            if (m_connection == nullptr || bytes == nullptr || length == 0)
            {
                return;
            }

            uint16_t sent{ 0 };

            while (sent < length)
            {
                auto const remaining = static_cast<uint32_t>(length - sent);
                auto const count = remaining > SysEx7BytesPerPacket ? SysEx7BytesPerPacket : remaining;

                uint8_t status{};

                if (sent == 0)
                {
                    status = (count == remaining) ? SysEx7StatusComplete : SysEx7StatusStart;
                }
                else
                {
                    status = (count == remaining) ? SysEx7StatusEnd : SysEx7StatusContinue;
                }

                uint8_t packet[SysEx7BytesPerPacket]{};

                for (uint32_t i = 0; i < count; i++)
                {
                    packet[i] = bytes[sent + i] & 0x7F;
                }

                uint32_t const word0 =
                    (MessageTypeSysEx7 << 28) |
                    (static_cast<uint32_t>(m_group & 0x0F) << 24) |
                    (static_cast<uint32_t>(status) << 20) |
                    (static_cast<uint32_t>(count) << 16) |
                    (static_cast<uint32_t>(packet[0]) << 8) |
                    static_cast<uint32_t>(packet[1]);

                uint32_t const word1 =
                    (static_cast<uint32_t>(packet[2]) << 24) |
                    (static_cast<uint32_t>(packet[3]) << 16) |
                    (static_cast<uint32_t>(packet[4]) << 8) |
                    static_cast<uint32_t>(packet[5]);

                m_connection.SendSingleMessageWords(0, word0, word1);

                sent = static_cast<uint16_t>(sent + count);
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void MidiCiProgramListQuery::SendPropertyGet(std::string const& headerJson) noexcept
    {
        try
        {
            m_replyHeader.clear();
            m_replyBody.clear();

            uint8_t buffer[CiSendBufferBytes]{};

            auto header = std::vector<uint8_t>(headerJson.begin(), headerJson.end());

            auto const length = CIMessage::sendPEGet(
                buffer,
                CiVersion,
                m_localMuid,
                m_remoteMuid,
                ++m_requestId,
                static_cast<uint16_t>(header.size()),
                header.data());

            SendCiMessage(buffer, length);

            ArmTimeout();
        }
        catch (...)
        {
            Complete(ProgramListResult::NotSupported);
        }
    }

    _Use_decl_annotations_
    void MidiCiProgramListQuery::OnDiscoveryReply(uint32_t remoteMuid) noexcept
    {
        try
        {
            std::scoped_lock lock{ m_lock };

            if (m_stage != Stage::AwaitingDiscoveryReply)
            {
                return;
            }

            m_remoteMuid = remoteMuid;

            RequestChannelList();
        }
        catch (...)
        {
            Complete(ProgramListResult::NotSupported);
        }
    }

    void MidiCiProgramListQuery::RequestChannelList() noexcept
    {
        m_stage = Stage::AwaitingChannelList;

        SendPropertyGet(R"({"resource":"ChannelList"})");
    }

    void MidiCiProgramListQuery::RequestProgramList() noexcept
    {
        try
        {
            m_stage = Stage::AwaitingProgramList;

            std::string header{ R"({"resource":"ProgramList")" };

            if (!m_currentResourceId.empty())
            {
                header += R"(,"resId":")" + m_currentResourceId + R"(")";
            }

            if (m_offset > 0)
            {
                header += R"(,"offset":)" + std::to_string(m_offset) +
                          R"(,"limit":)" + std::to_string(ProgramListPageSize);
            }

            header += "}";

            SendPropertyGet(header);
        }
        catch (...)
        {
            Complete(ProgramListResult::NotSupported);
        }
    }

    bool MidiCiProgramListQuery::RequestNextCollection() noexcept
    {
        try
        {
            if (m_pendingCollections.empty())
            {
                return false;
            }

            auto const next = m_pendingCollections.front();
            m_pendingCollections.erase(m_pendingCollections.begin());

            m_currentResourceId = next.ResourceId;
            m_currentCollectionTitle = next.Title;
            m_offset = 0;
            m_totalCount = 0;

            RequestProgramList();

            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    void MidiCiProgramListQuery::OnPropertyReply(
        std::string const& headerJson,
        std::vector<uint8_t> const& body) noexcept
    {
        try
        {
            std::scoped_lock lock{ m_lock };

            // Only the two stages that asked for a resource may act on one. Anything else is
            // a stray reply, or another initiator's, and must not drive the exchange.
            if (m_stage != Stage::AwaitingChannelList && m_stage != Stage::AwaitingProgramList)
            {
                return;
            }

            auto const header = TryParseObject(headerJson);
            auto const status = GetNumber(header, L"status", 0);

            // Anything other than OK means this resource is not available to us. A device with
            // no ChannelList may still have a single unnamed program list worth asking for.
            if (status != MIDICI_PE_STATUS_OK)
            {
                if (m_stage == Stage::AwaitingChannelList)
                {
                    m_currentResourceId.clear();
                    m_currentCollectionTitle.clear();
                    m_offset = 0;
                    m_totalCount = 0;

                    RequestProgramList();
                    return;
                }

                Complete(m_entries.empty() ? ProgramListResult::NotSupported : ProgramListResult::Success);
                return;
            }

            // A body we cannot read as plain JSON means the device chose an encoding we did
            // not ask for. Better to fall back to the spinners than to show garbage.
            auto const array = TryParseArray(body);

            if (array == nullptr)
            {
                if (m_stage == Stage::AwaitingChannelList)
                {
                    m_currentResourceId.clear();
                    m_currentCollectionTitle.clear();
                    RequestProgramList();
                    return;
                }

                Complete(m_entries.empty() ? ProgramListResult::Empty : ProgramListResult::Success);
                return;
            }

            if (m_stage == Stage::AwaitingChannelList)
            {
                CollectCollectionsFromChannelList(array);

                if (!RequestNextCollection())
                {
                    // the device has a ChannelList but no program collections on our channel
                    m_currentResourceId.clear();
                    m_currentCollectionTitle.clear();
                    m_offset = 0;
                    m_totalCount = 0;

                    RequestProgramList();
                }

                return;
            }

            m_totalCount = GetNumber(header, L"totalCount", 0);

            auto const added = CollectProgramsFromList(array);


            // Page only while the device keeps handing back new rows, so a device that ignores
            // the offset cannot put this into a loop.
            if (added > 0 && m_totalCount > 0 && m_offset + added < m_totalCount &&
                m_entries.size() < MaximumProgramEntries)
            {
                m_offset += added;
                RequestProgramList();
                return;
            }

            if (!RequestNextCollection())
            {
                Complete(m_entries.empty() ? ProgramListResult::Empty : ProgramListResult::Success);
            }
        }
        catch (...)
        {
            Complete(ProgramListResult::Empty);
        }
    }

    _Use_decl_annotations_
    void MidiCiProgramListQuery::CollectCollectionsFromChannelList(JsonArray const& array) noexcept
    {
        try
        {
            for (auto const& item : array)
            {
                // try_as<JsonObject> is always null for an array element; GetObject is the
                // only accessor that works here
                if (item == nullptr || item.ValueType() != JsonValueType::Object)
                {
                    continue;
                }

                auto const entry = item.GetObject();

                if (entry == nullptr)
                {
                    continue;
                }

                // "channel" in a ChannelList is 1-16; the app holds a 0-15 index
                auto const channel = GetNumber(entry, L"channel", 0);

                if (channel != static_cast<int32_t>(m_channel) + 1)
                {
                    continue;
                }

                if (!entry.HasKey(L"links"))
                {
                    continue;
                }

                auto const links = entry.GetNamedArray(L"links", nullptr);

                if (links == nullptr)
                {
                    continue;
                }

                for (auto const& linkItem : links)
                {
                    if (linkItem == nullptr || linkItem.ValueType() != JsonValueType::Object)
                    {
                        continue;
                    }

                    auto const link = linkItem.GetObject();

                    if (link == nullptr)
                    {
                        continue;
                    }

                    if (GetText(link, L"resource") != L"ProgramList")
                    {
                        continue;
                    }

                    auto const resourceId = GetText(link, L"resId");

                    if (resourceId.empty())
                    {
                        continue;
                    }

                    PendingCollection collection{};
                    collection.ResourceId = winrt::to_string(winrt::hstring{ resourceId });
                    collection.Title = GetText(link, L"title");

                    m_pendingCollections.push_back(collection);
                }
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    int32_t MidiCiProgramListQuery::CollectProgramsFromList(JsonArray const& array) noexcept
    {
        int32_t added{ 0 };

        try
        {
            // more than one collection is worth labeling; a single one would just be noise
            auto const labelWithCollection =
                !m_currentCollectionTitle.empty() && !m_pendingCollections.empty();

            for (auto const& item : array)
            {
                if (m_entries.size() >= MaximumProgramEntries)
                {
                    break;
                }

                if (item == nullptr || item.ValueType() != JsonValueType::Object)
                {
                    continue;
                }

                auto const entry = item.GetObject();

                if (entry == nullptr || !entry.HasKey(L"bankPC"))
                {
                    continue;
                }
                auto const bankPC = entry.GetNamedArray(L"bankPC", nullptr);

                if (bankPC == nullptr || bankPC.Size() < 3)
                {
                    continue;
                }

                ProgramListEntry program{};

                program.Title = GetText(entry, L"title");

                if (program.Title.empty())
                {
                    continue;
                }

                program.Tags = GetStringArray(entry, L"tags");

                // These three go on the wire exactly as they arrive. M2-107-UM section 2.3
                // gives a worked example: bankPC [121,2,49] is sent as Program Change 49.
                program.BankMsb = static_cast<uint8_t>(
                    std::clamp(static_cast<int32_t>(bankPC.GetNumberAt(0)), 0, 127));
                program.BankLsb = static_cast<uint8_t>(
                    std::clamp(static_cast<int32_t>(bankPC.GetNumberAt(1)), 0, 127));
                program.ProgramChange = static_cast<uint8_t>(
                    std::clamp(static_cast<int32_t>(bankPC.GetNumberAt(2)), 0, 127));

                if (labelWithCollection || !m_currentCollectionTitle.empty())
                {
                    program.CollectionTitle = m_currentCollectionTitle;
                }

                m_entries.push_back(std::move(program));
                added++;
            }
        }
        catch (...)
        {
        }

        return added;
    }

    _Use_decl_annotations_
    void MidiCiProgramListQuery::Complete(ProgramListResult result) noexcept
    {
        CompletedHandler handler{};
        std::vector<ProgramListEntry> entries{};

        // also reachable from the timeout, which holds no reference of its own
        std::shared_ptr<MidiCiProgramListQuery> keepAlive{};

        try
        {
            keepAlive = shared_from_this();
        }
        catch (...)
        {
        }

        {
            std::scoped_lock lock{ m_lock };

            if (m_stage == Stage::Finished)
            {
                return;
            }

            m_stage = Stage::Finished;

            handler = m_handler;
            entries = std::move(m_entries);

            m_handler = nullptr;
        }

        if (handler != nullptr)
        {
            try
            {
                handler(result, std::move(entries));
            }
            catch (...)
            {
            }
        }

        // unhooks the callback and drops the self reference that kept the query alive
        Cancel();
    }
}
