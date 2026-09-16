// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================

// Presents the synthesizer to Windows MIDI Services as an app-to-app MIDI device, which means it
// needs no service transport and no code inside midisrv. MidiSynthLib knows nothing about the SDK,
// so the same engine can move behind a transport later without being rewritten.

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.CapabilityInquiry.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.h>
#include <winrt/Windows.Devices.Midi2.Transports.Virtual.h>

#include <windows.h>

#include "MidiSynth/AudioSink.h"
#include "MidiSynth/DlsCollection.h"
#include "MidiSynth/ProgramList.h"
#include "MidiSynth/SpscRingBuffer.h"
#include "MidiSynth/SynthEngine.h"
#include "MidiSynth/UmpDispatcher.h"
#include "MidiSynth/UmpRenderSource.h"

#include "MidiCiProgramList.h"
#include "MidiDefs.h"

// windows.h defines GetObject as a macro, which collides with a method on the JSON projection.
#pragma push_macro("GetObject")
#undef GetObject
#include <winrt/Windows.Data.Json.h>
#pragma pop_macro("GetObject")

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::CapabilityInquiry;
using namespace winrt::Windows::Devices::Midi2::Enumeration;
using namespace winrt::Windows::Devices::Midi2::Transports::Virtual;

using namespace MidiSynth;

namespace
{
    std::wstring DefaultDlsPath()
    {
        wchar_t systemDirectory[MAX_PATH]{};

        if (GetSystemDirectoryW(systemDirectory, MAX_PATH) == 0)
        {
            return L"C:\\Windows\\System32\\drivers\\gm.dls";
        }

        return std::wstring{ systemDirectory } + L"\\drivers\\gm.dls";
    }

    // Replies are produced on whichever thread dispatched the message, so they are queued here and
    // put on the wire by the sender thread.
    using OutboundQueue = SpscRingBuffer<QueuedUmp, 256>;

    class QueuedUmpOutput final : public IUmpOutput
    {
    public:
        explicit QueuedUmpOutput(OutboundQueue& queue) noexcept : m_queue(queue) {}

        void SendUmp(const uint32_t* words, uint32_t wordCount) noexcept override
        {
            if (wordCount == 0 || wordCount > 4)
            {
                return;
            }

            QueuedUmp message{};

            for (uint32_t i = 0; i < wordCount; i++)
            {
                message.Words[i] = words[i];
            }

            message.WordCount = static_cast<uint8_t>(wordCount);

            (void)m_queue.TryPush(message);
        }

    private:
        OutboundQueue& m_queue;
    };

    class SynthHost final
    {
    public:
        SynthHost()
        {
            m_drainedEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        }

        ~SynthHost()
        {
            ReleaseAudio();

            if (m_drainedEvent != nullptr)
            {
                CloseHandle(m_drainedEvent);
            }
        }

        SynthHost(const SynthHost&) = delete;
        SynthHost& operator=(const SynthHost&) = delete;

        // The sound set stays loaded for the life of the process; only the audio device comes and
        // goes, so a client connecting pays for stream start rather than re-reading 3.4 MB.
        bool LoadSoundSet(_In_ const std::wstring& path)
        {
            // This engine is destined for the service, so only sound sets Windows installed are
            // accepted. The library still supports other paths for offline tools.
            const auto status = DlsCollection::LoadFromFile(
                path, DlsParseLimits{}, SoundSetOrigin::SystemOnly, m_collection);

            if (status != DlsParseStatus::Ok)
            {
                wprintf(L"Could not load %s (status %d)\n", path.c_str(), static_cast<int>(status));
                return false;
            }

            wprintf(L"Sound set: %s, %zu instruments, %zu waves\n",
                path.c_str(), m_collection.Instruments().size(), m_collection.Waves().size());

            BuildPropertyResources();

            return true;
        }

        // Every property exchange resource is serialized once, here, so that answering a request
        // is only ever a byte range slice.
        void BuildPropertyResources()
        {
            namespace ci = WindowsMidiServicesCapabilityInquiry;

            m_programListJson = MidiSynth::BuildProgramListJson(m_collection);

            ci::DeviceInfoFields info{};

            info.ManufacturerId[0] = MIDI_MANUFACTURER_SYSEX_ID_MICROSOFT_BYTE1;
            info.ManufacturerId[1] = MIDI_MANUFACTURER_SYSEX_ID_MICROSOFT_BYTE2;
            info.ManufacturerId[2] = MIDI_MANUFACTURER_SYSEX_ID_MICROSOFT_BYTE3;
            info.Manufacturer = "Microsoft";
            info.FamilyId[0] = MIDI_DEVICE_FAMILY_WINDOWS_11;
            info.Family = "Windows";
            info.ModelId[0] = MIDI_DEVICE_FAMILY_MODEL_NUMBER_GM_SYNTH;
            info.Model = "General MIDI Synth";

            m_deviceInfoJson.resize(ci::BuildDeviceInfoJson(info, nullptr, 0));
            (void)ci::BuildDeviceInfoJson(info, m_deviceInfoJson.data(), m_deviceInfoJson.size());

            char const* const names[]{ "ResourceList", "DeviceInfo", "ChannelList", "ProgramList" };

            m_resourceListJson.resize(ci::BuildResourceListJson(names, 4, nullptr, 0));
            (void)ci::BuildResourceListJson(names, 4, m_resourceListJson.data(), m_resourceListJson.size());

            wprintf(L"Property Exchange: ResourceList %zu, DeviceInfo %zu, ProgramList %zu bytes\n",
                m_resourceListJson.size(), m_deviceInfoJson.size(), m_programListJson.size());
        }

        // Called from the sender thread. Emits at most one chunk per call, because a full program
        // list is far more system exclusive packets than the outbound queue can hold at once.
        // Nothing may escape: this runs on a thread whose body has no other guard.
        void ServicePropertyRequests() noexcept
        {
            try
            {
                ServicePropertyRequestsInner();
            }
            catch (...)
            {
                wprintf(L"Property request handling failed, request abandoned\n");
                m_nextChunk = 0;
            }
        }

        void ServicePropertyRequestsInner()
        {
            namespace ci = WindowsMidiServicesCapabilityInquiry;

            if (m_nextChunk == 0)
            {
                UmpDispatcher::PendingPropertyRequest request{};

                if (!m_dispatcher.TakePendingPropertyRequest(request))
                {
                    return;
                }

                const std::vector<char>* blob = nullptr;

                const auto lookup = ResourceForHeader(request.Header, request.HeaderByteCount, &blob);

                if (lookup != ResourceLookup::Found)
                {
                    const std::string asked(
                        reinterpret_cast<const char*>(request.Header), request.HeaderByteCount);

                    wprintf(L"Property request %s: %S\n",
                        lookup == ResourceLookup::HeaderNotJson ? L"header was not JSON" : L"for a resource we do not have",
                        asked.c_str());

                    SendNotFound(request);
                    return;
                }

                m_replyInitiatorMuid = request.InitiatorMuid;
                m_replyRequestId = request.RequestId;

                m_chunker = {};
                m_chunker.Resource = reinterpret_cast<const uint8_t*>(blob->data());
                m_chunker.ResourceByteCount = blob->size();
                m_chunker.Header = m_replyIsCacheable ? ReplyHeaderOkCacheable : ReplyHeaderOk;
                m_chunker.HeaderByteCount = static_cast<uint16_t>(
                    m_replyIsCacheable ? sizeof(ReplyHeaderOkCacheable) : sizeof(ReplyHeaderOk));
                // What the initiator said it can receive. 512 is the smallest seen in practice.
                if (!m_chunker.Plan(512))
                {
                    return;
                }

                m_nextChunk = 1;

                wprintf(L"Property request: %zu bytes in %u chunks\n",
                    blob->size(), m_chunker.ChunkCount);
            }

            uint8_t buffer[640]{};

            const auto written = m_chunker.BuildChunk(
                m_nextChunk, m_dispatcher.Muid(), m_replyInitiatorMuid, m_replyRequestId,
                buffer, sizeof(buffer));

            if (written == 0)
            {
                m_nextChunk = 0;
                return;
            }

            UmpDispatcher::PacketizeSysEx7(m_output, 0, buffer, written);

            m_nextChunk = (m_nextChunk >= m_chunker.ChunkCount) ? 0 : static_cast<uint16_t>(m_nextChunk + 1);
        }

        void SendNotFound(_In_ const UmpDispatcher::PendingPropertyRequest& request)
        {
            namespace ci = WindowsMidiServicesCapabilityInquiry;

            ci::PropertyExchangeMessageFields fields{};

            fields.Type = ci::MessageType::PropertyGetDataReply;
            fields.SourceMuid = m_dispatcher.Muid();
            fields.DestinationMuid = request.InitiatorMuid;
            fields.RequestId = request.RequestId;
            fields.Header = ReplyHeaderNotFound;
            fields.HeaderByteCount = static_cast<uint16_t>(sizeof(ReplyHeaderNotFound));
            fields.ChunkCount = 1;
            fields.ChunkNumber = 1;

            uint8_t buffer[64]{};

            const auto written = ci::BuildPropertyExchangeMessage(fields, buffer, sizeof(buffer));

            if (written > 0)
            {
                UmpDispatcher::PacketizeSysEx7(m_output, 0, buffer, written);
            }
        }

        void SetMode(_In_ SynthMode mode) noexcept { m_mode = mode; }

        void QueueInbound(_In_reads_(wordCount) const uint32_t* words, _In_ uint8_t wordCount) noexcept
        {
            if (wordCount == 0 || wordCount > 4)
            {
                return;
            }

            LARGE_INTEGER now{};
            QueryPerformanceCounter(&now);

            QueuedUmp message;
            message.WordCount = wordCount;
            message.Timestamp = now.QuadPart;

            for (uint8_t i = 0; i < wordCount; i++)
            {
                message.Words[i] = words[i];
            }

            // The SDK is not documented to deliver on a single thread, so the producer side is
            // guarded. The audio thread still pops without a lock.
            std::lock_guard<std::mutex> guard(m_producerLock);

            if (!m_inbound.TryPush(message))
            {
                m_droppedCount.fetch_add(1, std::memory_order_relaxed);
            }
        }

        bool TryPopOutbound(_Out_ QueuedUmp& message) noexcept
        {
            return m_outbound.TryPop(message);
        }

        // Sends a MIDI 2.0 program change with the bank flag set, on every channel, so an
        // instrument can be auditioned without a controller that can send bank or program.
        void SelectProgram(_In_ uint8_t program, _In_ uint8_t bankMsb, _In_ uint8_t bankLsb) noexcept
        {
            for (uint8_t channel = 0; channel < 16; channel++)
            {
                const uint32_t words[2]
                {
                    (4u << 28) | (static_cast<uint32_t>(SynthEndpoint::FirstGroupIndex) << 24)
                        | (0xCu << 20) | (static_cast<uint32_t>(channel) << 16) | 0x01u,

                    (static_cast<uint32_t>(program) << 24)
                        | (static_cast<uint32_t>(bankMsb) << 8)
                        | static_cast<uint32_t>(bankLsb),
                };

                QueueInbound(words, 2);
            }
        }

        void SetClientInUse(_In_ bool inUse)
        {
            if (inUse)
            {
                AcquireAudio();
            }
            else
            {
                ReleaseAudio();
            }
        }

        bool AudioRunning()
        {
            std::lock_guard<std::mutex> guard(m_audioLock);
            return m_sink != nullptr;
        }

        void PrintStats()
        {
            std::lock_guard<std::mutex> guard(m_audioLock);

            if (m_sink == nullptr)
            {
                wprintf(L"  audio idle, no client connected\n");
                return;
            }

            const auto stats = m_sink->Stats();
            const auto engineStats = m_engine.ActiveVoiceCount();

            wprintf(L"  %s, %u Hz, period %.2f ms\n",
                m_sink->DeviceName().c_str(), m_sink->SampleRate(), m_sink->PeriodMilliseconds());

            wprintf(L"  callbacks %llu, glitches %llu, worst load %.2f, queued %.2f ms\n",
                stats.CallbackCount, stats.GlitchCount, stats.WorstLoadFactor,
                stats.AverageQueuedMilliseconds);

            wprintf(L"  active voices %u, dropped messages %llu, peak %.1f dBFS\n",
                engineStats, m_droppedCount.load(std::memory_order_relaxed),
                m_engine.PeakOutputDbfs());
        }

    private:
        bool AcquireAudio()
        {
            std::lock_guard<std::mutex> guard(m_audioLock);

            if (m_sink != nullptr)
            {
                return true;
            }

            auto sink = std::make_unique<WasapiAudioSink>();

            if (!sink->Open(false))
            {
                wprintf(L"  could not open the audio device; MIDI will flow but nothing will sound\n");
                return false;
            }

            // Modern mode renders at the device rate, so nothing resamples.
            const auto config = SynthConfig::ForMode(m_mode, sink->SampleRate());

            m_engine.Initialize(&m_collection, config);
            m_dispatcher.Initialize(&m_engine, 0, m_muid);
            m_dispatcher.SetOutput(&m_output, SynthIdentity{});

            auto source = std::make_unique<UmpRenderSource>(
                m_engine, m_dispatcher, m_inbound, sink->SampleRate(),
                sink->BufferFrames(), m_drainedEvent);

            if (!sink->Start(source.get()))
            {
                wprintf(L"  could not start the audio stream\n");
                return false;
            }

            wprintf(L"  audio acquired: %s, %u Hz, period %.2f ms\n",
                sink->DeviceName().c_str(), sink->SampleRate(), sink->PeriodMilliseconds());

            if (config.RenderSampleRate() != sink->SampleRate())
            {
                wprintf(L"  compatible mode renders at %u Hz and is resampled\n",
                    config.RenderSampleRate());
            }

            m_source = std::move(source);
            m_sink = std::move(sink);

            return true;
        }

        void ReleaseAudio()
        {
            std::lock_guard<std::mutex> guard(m_audioLock);

            if (m_sink == nullptr)
            {
                return;
            }

            // Cutting a ringing voice mid-waveform clicks, so fade everything and let the render
            // thread tell us it has gone quiet before the device is released.
            for (uint8_t channel = 0; channel < 16; channel++)
            {
                m_engine.AllSoundOff(channel);
            }

            if (m_source != nullptr && m_drainedEvent != nullptr)
            {
                m_source->RequestDrain();
                WaitForSingleObject(m_drainedEvent, 100);
            }

            m_sink->Stop();
            m_sink.reset();
            m_source.reset();

            wprintf(L"  audio released\n");
        }

        // Reply header for a successful property exchange request.
        static constexpr uint8_t ReplyHeaderOk[]{ '{', '"', 's', 't', 'a', 't', 'u', 's', '"', ':', '2', '0', '0', '}' };

        // The sound set is loaded once and cannot change while this host runs, so the resources
        // built from it are worth caching. Without this a client refetches nine kilobytes on every
        // reconnect. ChannelList deliberately does not get one: it changes on every program change.
        static constexpr uint8_t ReplyHeaderOkCacheable[]
        {
            '{', '"', 's', 't', 'a', 't', 'u', 's', '"', ':', '2', '0', '0', ',',
            '"', 'c', 'a', 'c', 'h', 'e', 'T', 'i', 'm', 'e', '"', ':', '3', '6', '0', '0', '}'
        };

        // Asking for something we do not have is answered, not ignored. An initiator that gets
        // silence waits for a timeout and may give up on the device entirely.
        static constexpr uint8_t ReplyHeaderNotFound[]{ '{', '"', 's', 't', 'a', 't', 'u', 's', '"', ':', '4', '0', '4', '}' };

        enum class ResourceLookup
        {
            Found = 0,
            HeaderNotJson,
            UnknownResource,
        };

        // The header is short ASCII JSON. Windows.Data.Json is the only parser allowed, so it is
        // used here rather than anywhere in the library, which has to stay free of WinRT.
        ResourceLookup ResourceForHeader(
            _In_reads_(headerBytes) const uint8_t* header,
            _In_ uint16_t headerBytes,
            _Outptr_result_maybenull_ const std::vector<char>** blob) const
        {
            namespace json = ::winrt::Windows::Data::Json;

            *blob = nullptr;

            m_replyIsCacheable = true;

            const std::string text(reinterpret_cast<const char*>(header), headerBytes);

            json::JsonObject parsed{ nullptr };

            if (!json::JsonObject::TryParse(winrt::to_hstring(text), parsed))
            {
                return ResourceLookup::HeaderNotJson;
            }

            const auto resource = parsed.GetNamedString(L"resource", L"");

            if (resource == L"ResourceList") { *blob = &m_resourceListJson; return ResourceLookup::Found; }
            if (resource == L"DeviceInfo") { *blob = &m_deviceInfoJson; return ResourceLookup::Found; }
            if (resource == L"ProgramList") { *blob = &m_programListJson; return ResourceLookup::Found; }

            // Rebuilt per request: unlike the others this reflects what is selected right now.
            if (resource == L"ChannelList")
            {
                RebuildChannelList();
                *blob = &m_channelListJson;
                m_replyIsCacheable = false;
                return ResourceLookup::Found;
            }

            return ResourceLookup::UnknownResource;
        }

        void RebuildChannelList() const
        {
            namespace ci = WindowsMidiServicesCapabilityInquiry;

            ci::ChannelListEntry entries[MidiChannelCount]{};
            char titles[MidiChannelCount][16]{};
            std::string programTitles[MidiChannelCount];

            for (uint8_t channel = 0; channel < MidiChannelCount; channel++)
            {
                const auto state = m_engine.ChannelState(channel);

                snprintf(titles[channel], sizeof(titles[channel]), "Channel %u", channel + 1u);

                entries[channel].Title = titles[channel];
                entries[channel].Channel = static_cast<uint16_t>(channel + 1);
                entries[channel].BankMsb = state.BankMsb;
                entries[channel].BankLsb = state.BankLsb;
                entries[channel].Program = state.Program;

                // Channel 10 is the drum channel by convention, and kits are addressed by a flag
                // in this sound set rather than by a bank.
                const auto* instrument = m_collection.FindInstrument(
                    state.BankMsb, state.BankLsb, state.Program, channel == 9);

                if (instrument != nullptr)
                {
                    programTitles[channel] = ToNarrow(instrument->Name);
                    entries[channel].ProgramTitle = programTitles[channel].c_str();
                }
            }

            const auto required = ci::BuildChannelListJson(entries, MidiChannelCount, nullptr, 0);

            m_channelListJson.resize(required);

            (void)ci::BuildChannelListJson(
                entries, MidiChannelCount, m_channelListJson.data(), m_channelListJson.size());
        }

        static std::string ToNarrow(_In_ const std::wstring& text)
        {
            std::string result;

            for (const auto character : text)
            {
                result += (character > 0 && character < 0x80) ? static_cast<char>(character) : '?';
            }

            return result;
        }

        DlsCollection m_collection;
        SynthEngine m_engine;
        UmpDispatcher m_dispatcher;

        UmpInboundQueue m_inbound;
        OutboundQueue m_outbound;
        QueuedUmpOutput m_output{ m_outbound };

        std::vector<char> m_programListJson;
        std::vector<char> m_deviceInfoJson;
        std::vector<char> m_resourceListJson;
        mutable std::vector<char> m_channelListJson;
        mutable bool m_replyIsCacheable{ true };

        WindowsMidiServicesCapabilityInquiry::PropertyReplyChunker m_chunker{};
        uint16_t m_nextChunk{ 0 };
        uint32_t m_replyInitiatorMuid{ 0 };
        uint8_t m_replyRequestId{ 0 };

        std::unique_ptr<WasapiAudioSink> m_sink;
        std::unique_ptr<UmpRenderSource> m_source;

        std::mutex m_audioLock;
        std::mutex m_producerLock;

        // The API's generator keeps clear of the reserved range, so do not roll our own.
        uint32_t m_muid{ MidiUniqueId::CreateRandom().AsCombined28BitValue() };

        std::atomic<uint64_t> m_droppedCount{ 0 };

        SynthMode m_mode{ SynthMode::Modern };

        HANDLE m_drainedEvent{ nullptr };
    };

    MidiVirtualDevice CreateVirtualDevice()
    {
        MidiDeclaredEndpointInfo endpointInfo;
        endpointInfo.HasStaticFunctionBlocks(true);
        endpointInfo.Name(L"GM Synth Prototype");
        endpointInfo.ProductInstanceId(L"MIDISYNTH-PROTOTYPE-1");
        endpointInfo.SupportsMidi10Protocol(true);
        endpointInfo.SupportsMidi20Protocol(true);
        endpointInfo.SupportsReceivingJitterReductionTimestamps(false);
        endpointInfo.SupportsSendingJitterReductionTimestamps(false);
        endpointInfo.SpecificationVersionMajor(1);
        endpointInfo.SpecificationVersionMinor(1);

        // Built from the same values the SysEx Identity Reply uses, so endpoint discovery and a
        // device inquiry cannot disagree.
        const SynthIdentity identity{};

        MidiDeclaredDeviceIdentity declaredIdentity(
            identity.ManufacturerSysExId[0],
            identity.ManufacturerSysExId[1],
            identity.ManufacturerSysExId[2],
            static_cast<uint8_t>(identity.FamilyCode & 0x7F),
            static_cast<uint8_t>((identity.FamilyCode >> 7) & 0x7F),
            static_cast<uint8_t>(identity.FamilyMemberCode & 0x7F),
            static_cast<uint8_t>((identity.FamilyMemberCode >> 7) & 0x7F),
            identity.SoftwareRevision[0],
            identity.SoftwareRevision[1],
            identity.SoftwareRevision[2],
            identity.SoftwareRevision[3]);

        MidiVirtualDeviceCreationConfig creationConfig(
            endpointInfo.Name(),
            L"General MIDI synthesizer prototype",
            L"Microsoft",
            endpointInfo,
            declaredIdentity);

        // General MIDI is defined over sixteen channels, which is exactly one group, and the UMP
        // specification wants a paired in and out to be one bidirectional block over one group.
        MidiFunctionBlock block;
        block.Number(0);
        block.IsActive(true);
        block.Name(L"GM Synth");
        block.FirstGroup(MidiGroup(SynthEndpoint::FirstGroupIndex));
        block.GroupCount(SynthEndpoint::GroupCount);
        block.Direction(MidiFunctionBlockDirection::Bidirectional);
        creationConfig.FunctionBlocks().Append(block);

        return MidiVirtualDeviceManager::CreateVirtualDevice(creationConfig);
    }
}

int main(int argc, char** argv)
{
    winrt::init_apartment();

    SynthMode mode = SynthMode::Modern;

    for (int i = 1; i < argc; i++)
    {
        if (_stricmp(argv[i], "--compat") == 0)
        {
            mode = SynthMode::Compatible;
        }
    }

    if (!MidiApi::EnsureServiceAvailable())
    {
        wprintf(L"Could not start Windows MIDI Services.\n");
        return 1;
    }

    SynthHost host;
    host.SetMode(mode);

    wprintf(L"Mode: %s\n", (mode == SynthMode::Compatible) ? L"compatible" : L"modern");

    if (!host.LoadSoundSet(DefaultDlsPath()))
    {
        return 1;
    }

    auto session = MidiSession::Create(L"GM Synth Prototype");

    if (session == nullptr)
    {
        wprintf(L"Could not create a MIDI session.\n");
        return 1;
    }

    auto virtualDevice = CreateVirtualDevice();

    if (virtualDevice == nullptr)
    {
        wprintf(L"Could not create the virtual device.\n");
        return 1;
    }

    auto deviceEndpoint = session.CreateEndpointConnection(virtualDevice.DeviceEndpointDeviceId());

    if (deviceEndpoint == nullptr)
    {
        wprintf(L"Could not connect to the device endpoint.\n");
        return 1;
    }

    deviceEndpoint.MessageReceived(
        [&host](winrt::Windows::Devices::Midi2::IMidiMessageReceivedEventSource const&,
                MidiMessageReceivedEventArgs const& args)
        {
            uint32_t word0{}, word1{}, word2{}, word3{};
            const uint8_t wordCount = args.FillWords(word0, word1, word2, word3);

            const uint32_t words[4]{ word0, word1, word2, word3 };
            host.QueueInbound(words, wordCount);
        });

    // Audio is held only while something is actually connected, so an idle synthesizer owns no
    // audio device at all.
    virtualDevice.ClientEndpointInUseChanged(
        [&host](MidiVirtualDevice const& sender, auto const&)
        {
            const bool inUse = sender.IsClientEndpointInUse();

            wprintf(L"\nClient endpoint %s\n", inUse ? L"in use" : L"idle");
            host.SetClientInUse(inUse);
        });

    if (deviceEndpoint.AddMessageProcessingPlugin(virtualDevice) !=
        MidiMessageProcessingPluginAddResult::Succeeded)
    {
        wprintf(L"Could not attach the virtual device to the connection.\n");
        return 1;
    }

    deviceEndpoint.Open();

    if (virtualDevice.IsClientEndpointInUse())
    {
        host.SetClientInUse(true);
    }

    std::atomic<bool> running{ true };

    // Replies queued by the render thread are sent from here.
    std::thread sender([&host, &deviceEndpoint, &running]()
        {
            // Answering a property request parses JSON through WinRT, which needs an apartment on
            // this thread. The one in main does not cover it.
            winrt::init_apartment(winrt::apartment_type::multi_threaded);

            while (running.load(std::memory_order_acquire))
            {
                QueuedUmp message;

                host.ServicePropertyRequests();

                while (host.TryPopOutbound(message))
                {
                    const auto timestamp = MidiClock::Now();

                    switch (message.WordCount)
                    {
                    case 1:
                        (void)deviceEndpoint.SendSingleMessageWords(timestamp, message.Words[0]);
                        break;
                    case 2:
                        (void)deviceEndpoint.SendSingleMessageWords(
                            timestamp, message.Words[0], message.Words[1]);
                        break;
                    case 3:
                        (void)deviceEndpoint.SendSingleMessageWords(
                            timestamp, message.Words[0], message.Words[1], message.Words[2]);
                        break;
                    case 4:
                        (void)deviceEndpoint.SendSingleMessageWords(
                            timestamp, message.Words[0], message.Words[1], message.Words[2],
                            message.Words[3]);
                        break;
                    default:
                        break;
                    }
                }

                Sleep(2);
            }
        });

    wprintf(L"\nEndpoint is live. Connect to \"GM Synth Prototype\" from any MIDI application.\n");
    wprintf(L"  <enter>      status\n");
    wprintf(L"  p <0-127>    program on every channel\n");
    wprintf(L"  b <0-127>    bank MSB, then reselect the program\n");
    wprintf(L"  q            quit\n\n");

    uint8_t bankMsb = 0;

    for (;;)
    {
        char line[64]{};

        if (fgets(line, sizeof(line), stdin) == nullptr)
        {
            break;
        }

        if (line[0] == 'q')
        {
            break;
        }

        if (line[0] == 'p' || line[0] == 'b')
        {
            int value = 0;

            if (sscanf_s(line + 1, "%d", &value) == 1 && value >= 0 && value <= 127)
            {
                if (line[0] == 'b')
                {
                    bankMsb = static_cast<uint8_t>(value);
                    wprintf(L"  bank MSB %d\n", value);
                }
                else
                {
                    host.SelectProgram(static_cast<uint8_t>(value), bankMsb, 0);
                    wprintf(L"  program %d, bank MSB %u\n", value, bankMsb);
                }
            }
            else
            {
                wprintf(L"  expected a value from 0 to 127\n");
            }

            continue;
        }

        host.PrintStats();
    }

    running.store(false, std::memory_order_release);
    sender.join();

    session.DisconnectEndpointConnection(deviceEndpoint.ConnectionId());
    session.Close();

    return 0;
}
