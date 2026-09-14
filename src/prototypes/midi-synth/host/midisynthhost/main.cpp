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
#include "MidiSynth/SpscRingBuffer.h"
#include "MidiSynth/SynthEngine.h"
#include "MidiSynth/UmpDispatcher.h"

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
    constexpr uint32_t MaxPendingPerBlock = 256;

    struct QueuedUmp
    {
        uint32_t Words[4]{};
        uint8_t WordCount{ 0 };

        // QPC at arrival, so the render thread can place the event at the right sample offset
        // rather than collapsing a whole period of messages onto the block start.
        int64_t Timestamp{ 0 };
    };

    using InboundQueue = SpscRingBuffer<QueuedUmp, 8192>;
    using OutboundQueue = SpscRingBuffer<QueuedUmp, 256>;

    std::wstring DefaultDlsPath()
    {
        wchar_t systemDirectory[MAX_PATH]{};

        if (GetSystemDirectoryW(systemDirectory, ARRAYSIZE(systemDirectory)) == 0)
        {
            return L"gm.dls";
        }

        return std::wstring(systemDirectory) + L"\\drivers\\gm.dls";
    }

    // Replies leave the audio thread through a queue; sending through the SDK there would
    // allocate and block.
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

            QueuedUmp message;
            message.WordCount = static_cast<uint8_t>(wordCount);

            for (uint32_t i = 0; i < wordCount; i++)
            {
                message.Words[i] = words[i];
            }

            (void)m_queue.TryPush(message);
        }

    private:
        OutboundQueue& m_queue;
    };

    class SynthRenderSource final : public IAudioRenderSource
    {
    public:
        SynthRenderSource(
            _In_ SynthEngine& engine,
            _In_ UmpDispatcher& dispatcher,
            _In_ InboundQueue& queue,
            _In_ uint32_t deviceSampleRate,
            _In_ HANDLE drainedEvent) noexcept
            : m_engine(engine)
            , m_dispatcher(dispatcher)
            , m_queue(queue)
            , m_deviceSampleRate(deviceSampleRate)
            , m_drainedEvent(drainedEvent)
        {
            LARGE_INTEGER frequency{};
            QueryPerformanceFrequency(&frequency);
            m_ticksPerSecond = frequency.QuadPart;
        }

        void RequestDrain() noexcept { m_draining.store(true, std::memory_order_release); }

        void RenderAudio(float* interleavedStereo, uint32_t frameCount) noexcept override
        {
            LARGE_INTEGER now{};
            QueryPerformanceCounter(&now);

            // Audio in this block represents the period that just elapsed, so a message that
            // arrived part way through it belongs part way through the block.
            const double blockTicks =
                static_cast<double>(frameCount) * static_cast<double>(m_ticksPerSecond)
                / static_cast<double>(m_deviceSampleRate);

            const double blockStartTicks = static_cast<double>(now.QuadPart) - blockTicks;

            uint32_t pendingCount = 0;
            QueuedUmp message;

            while (pendingCount < MaxPendingPerBlock && m_queue.TryPop(message))
            {
                double offset = 0.0;

                if (blockTicks > 0.0)
                {
                    offset = (static_cast<double>(message.Timestamp) - blockStartTicks)
                        / blockTicks * static_cast<double>(frameCount);
                }

                const double maxOffset = static_cast<double>(frameCount > 0 ? frameCount - 1 : 0);

                m_pending[pendingCount] = message;
                m_pendingOffset[pendingCount] =
                    static_cast<uint32_t>((std::clamp)(offset, 0.0, maxOffset));

                pendingCount++;
            }

            uint32_t rendered = 0;
            uint32_t nextEvent = 0;

            while (rendered < frameCount)
            {
                while (nextEvent < pendingCount && m_pendingOffset[nextEvent] <= rendered)
                {
                    (void)m_dispatcher.ProcessWords(
                        m_pending[nextEvent].Words, m_pending[nextEvent].WordCount);

                    nextEvent++;
                }

                uint32_t limit = frameCount;

                if (nextEvent < pendingCount)
                {
                    limit = (std::min)(limit, (std::max)(m_pendingOffset[nextEvent], rendered + 1));
                }

                const uint32_t frames = limit - rendered;

                m_engine.Render(interleavedStereo + static_cast<size_t>(rendered) * 2, frames);
                rendered += frames;
            }

            if (m_draining.load(std::memory_order_acquire) && m_engine.ActiveVoiceCount() == 0)
            {
                SetEvent(m_drainedEvent);
            }
        }

    private:
        SynthEngine& m_engine;
        UmpDispatcher& m_dispatcher;
        InboundQueue& m_queue;

        uint32_t m_deviceSampleRate{ 48000 };
        int64_t m_ticksPerSecond{ 1 };

        HANDLE m_drainedEvent{ nullptr };
        std::atomic<bool> m_draining{ false };

        QueuedUmp m_pending[MaxPendingPerBlock]{};
        uint32_t m_pendingOffset[MaxPendingPerBlock]{};
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
            const auto status = DlsCollection::LoadFromFile(path, DlsParseLimits{}, m_collection);

            if (status != DlsParseStatus::Ok)
            {
                wprintf(L"Could not load %s (status %d)\n", path.c_str(), static_cast<int>(status));
                return false;
            }

            wprintf(L"Sound set: %s, %zu instruments, %zu waves\n",
                path.c_str(), m_collection.Instruments().size(), m_collection.Waves().size());

            return true;
        }

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
            const auto config = SynthConfig::ForMode(SynthMode::Modern, sink->SampleRate());

            m_engine.Initialize(&m_collection, config);
            m_dispatcher.Initialize(&m_engine, 0, m_muid);
            m_dispatcher.SetOutput(&m_output, SynthIdentity{});

            auto source = std::make_unique<SynthRenderSource>(
                m_engine, m_dispatcher, m_inbound, sink->SampleRate(), m_drainedEvent);

            if (!sink->Start(source.get()))
            {
                wprintf(L"  could not start the audio stream\n");
                return false;
            }

            wprintf(L"  audio acquired: %s, %u Hz, period %.2f ms\n",
                sink->DeviceName().c_str(), sink->SampleRate(), sink->PeriodMilliseconds());

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

        DlsCollection m_collection;
        SynthEngine m_engine;
        UmpDispatcher m_dispatcher;

        InboundQueue m_inbound;
        OutboundQueue m_outbound;
        QueuedUmpOutput m_output{ m_outbound };

        std::unique_ptr<WasapiAudioSink> m_sink;
        std::unique_ptr<SynthRenderSource> m_source;

        std::mutex m_audioLock;
        std::mutex m_producerLock;

        // The API's generator keeps clear of the reserved range, so do not roll our own.
        uint32_t m_muid{ MidiUniqueId::CreateRandom().AsCombined28BitValue() };

        std::atomic<uint64_t> m_droppedCount{ 0 };

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

int main()
{
    winrt::init_apartment();

    if (!MidiApi::EnsureServiceAvailable())
    {
        wprintf(L"Could not start Windows MIDI Services.\n");
        return 1;
    }

    SynthHost host;

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
            while (running.load(std::memory_order_acquire))
            {
                QueuedUmp message;

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
