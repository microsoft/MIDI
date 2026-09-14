// Spike: play the synth live through WASAPI, driven by a MIDI input device or a score file.
//
// This is the first time the engine runs under a real-time constraint, so it reports glitches and
// how much of each audio period it is actually using.

#include "MidiSynth/AudioSink.h"
#include "MidiSynth/SpscRingBuffer.h"
#include "MidiSynth/SynthEngine.h"

#include <windows.h>
#include <objbase.h>
#include <mmeapi.h>
#include <timeapi.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <conio.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace MidiSynth;

namespace
{
    struct QueuedMessage
    {
        uint32_t ShortMessage{ 0 };

        // QPC at arrival, so the render thread can place the event at the right sample offset
        // rather than collapsing everything that arrived during a period onto the block start.
        int64_t Timestamp{ 0 };
    };

    using MessageQueue = SpscRingBuffer<QueuedMessage, 8192>;

    MessageQueue g_queue;
    std::atomic<uint64_t> g_receivedCount{ 0 };
    std::atomic<uint64_t> g_droppedCount{ 0 };

    std::string ToUtf8(_In_ const std::wstring& value)
    {
        if (value.empty())
        {
            return {};
        }

        const int required = WideCharToMultiByte(
            CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);

        if (required <= 0)
        {
            return {};
        }

        std::string result(static_cast<size_t>(required), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
            result.data(), required, nullptr, nullptr);

        return result;
    }

    std::wstring DefaultDlsPath()
    {
        wchar_t systemDirectory[MAX_PATH]{};

        if (GetSystemDirectoryW(systemDirectory, ARRAYSIZE(systemDirectory)) == 0)
        {
            return L"gm.dls";
        }

        return std::wstring(systemDirectory) + L"\\drivers\\gm.dls";
    }

    void CALLBACK MidiInputCallback(HMIDIIN, UINT message, DWORD_PTR, DWORD_PTR data, DWORD_PTR) noexcept
    {
        if (message != MIM_DATA)
        {
            return;
        }

        g_receivedCount.fetch_add(1, std::memory_order_relaxed);

        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);

        if (!g_queue.TryPush(QueuedMessage{ static_cast<uint32_t>(data), now.QuadPart }))
        {
            g_droppedCount.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // Feeds the engine from the audio thread, resampling when the engine's render rate differs
    // from the device rate, which is the case in Compatible mode.
    class SynthRenderSource final : public IAudioRenderSource
    {
    public:
        SynthRenderSource(_In_ SynthEngine& engine, _In_ uint32_t deviceSampleRate, _In_ uint32_t maxFrames)
            : m_engine(engine)
            , m_deviceSampleRate(deviceSampleRate)
        {
            m_ratio = static_cast<double>(engine.Config().RenderSampleRate()) / deviceSampleRate;

            // Worst case engine frames for one device block, plus slack for interpolation and
            // the fractional carry. Allocated once so the render thread never does.
            const auto capacity = static_cast<size_t>(maxFrames * m_ratio) + 8;
            m_engineFrames.assign(capacity * 2, 0.0f);

            LARGE_INTEGER frequency{};
            QueryPerformanceFrequency(&frequency);
            m_ticksPerSecond = frequency.QuadPart;
        }

        void RenderAudio(float* interleavedStereo, uint32_t frameCount) noexcept override
        {
            LARGE_INTEGER now{};
            QueryPerformanceCounter(&now);

            // Audio in this block represents the period that just elapsed, so an event that
            // arrived part way through it belongs part way through the block.
            const double blockTicks =
                static_cast<double>(frameCount) * m_ticksPerSecond / m_deviceSampleRate;
            const double blockStartTicks = static_cast<double>(now.QuadPart) - blockTicks;

            uint32_t pendingCount = 0;
            QueuedMessage message;

            while (pendingCount < MaxPendingPerBlock && g_queue.TryPop(message))
            {
                double offset = 0.0;

                if (blockTicks > 0.0)
                {
                    offset = (static_cast<double>(message.Timestamp) - blockStartTicks)
                        / blockTicks * frameCount;
                }

                m_pending[pendingCount].Message = message.ShortMessage;
                m_pending[pendingCount].Offset = static_cast<uint32_t>(
                    (std::clamp)(offset, 0.0, static_cast<double>(frameCount > 0 ? frameCount - 1 : 0)));
                pendingCount++;
            }

            uint32_t rendered = 0;
            uint32_t nextEvent = 0;

            while (rendered < frameCount)
            {
                while (nextEvent < pendingCount && m_pending[nextEvent].Offset <= rendered)
                {
                    Dispatch(m_pending[nextEvent].Message);
                    nextEvent++;
                }

                uint32_t limit = frameCount;

                if (nextEvent < pendingCount)
                {
                    limit = (std::min)(limit, (std::max)(m_pending[nextEvent].Offset, rendered + 1));
                }

                const uint32_t frames = limit - rendered;

                RenderChunk(interleavedStereo + static_cast<size_t>(rendered) * 2, frames);
                rendered += frames;
            }
        }

    private:
        void RenderChunk(_Out_writes_(frameCount * 2) float* interleavedStereo, _In_ uint32_t frameCount) noexcept
        {
            if (std::abs(m_ratio - 1.0) < 1e-9)
            {
                m_engine.Render(interleavedStereo, frameCount);
                return;
            }

            const double endPosition = m_position + frameCount * m_ratio;
            const auto needed = static_cast<uint32_t>(std::floor(endPosition)) + 2;

            if (static_cast<size_t>(needed) * 2 > m_engineFrames.size())
            {
                std::fill_n(interleavedStereo, static_cast<size_t>(frameCount) * 2, 0.0f);
                return;
            }

            if (needed > m_available)
            {
                m_engine.Render(m_engineFrames.data() + static_cast<size_t>(m_available) * 2,
                    needed - m_available);
                m_available = needed;
            }

            for (uint32_t frame = 0; frame < frameCount; frame++)
            {
                const auto index = static_cast<uint32_t>(m_position);
                const auto fraction = static_cast<float>(m_position - index);

                const size_t a = static_cast<size_t>(index) * 2;
                const size_t b = a + 2;

                interleavedStereo[frame * 2] =
                    m_engineFrames[a] + (m_engineFrames[b] - m_engineFrames[a]) * fraction;
                interleavedStereo[frame * 2 + 1] =
                    m_engineFrames[a + 1] + (m_engineFrames[b + 1] - m_engineFrames[a + 1]) * fraction;

                m_position += m_ratio;
            }

            const auto consumed = static_cast<uint32_t>(m_position);

            if (consumed > 0 && consumed <= m_available)
            {
                std::memmove(m_engineFrames.data(),
                    m_engineFrames.data() + static_cast<size_t>(consumed) * 2,
                    static_cast<size_t>(m_available - consumed) * 2 * sizeof(float));

                m_available -= consumed;
                m_position -= consumed;
            }
        }

        void Dispatch(_In_ uint32_t shortMessage) noexcept
        {
            const auto status = static_cast<uint8_t>(shortMessage & 0xFF);
            const auto data1 = static_cast<uint8_t>((shortMessage >> 8) & 0x7F);
            const auto data2 = static_cast<uint8_t>((shortMessage >> 16) & 0x7F);
            const auto channel = static_cast<uint8_t>(status & 0x0F);

            switch (status & 0xF0)
            {
            case 0x80:
                m_engine.NoteOff(channel, data1);
                break;

            case 0x90:
                if (data2 == 0)
                {
                    m_engine.NoteOff(channel, data1);
                }
                else
                {
                    // Spread the 7 bit velocity across the full 16 bit range the engine takes.
                    m_engine.NoteOn(channel, data1,
                        static_cast<uint16_t>((data2 << 9) | (data2 << 2) | (data2 >> 5)));
                }
                break;

            case 0xB0:
                m_engine.ControlChange(channel, data1, data2);
                break;

            case 0xC0:
                m_engine.ProgramChange(channel, data1);
                break;

            case 0xE0:
                m_engine.PitchBend(channel, (data2 << 7) | data1);
                break;

            default:
                break;
            }
        }

        SynthEngine& m_engine;
        std::vector<float> m_engineFrames;
        uint32_t m_available{ 0 };
        double m_position{ 0.0 };
        double m_ratio{ 1.0 };
        uint32_t m_deviceSampleRate{ 48000 };
        int64_t m_ticksPerSecond{ 1 };

        static constexpr uint32_t MaxPendingPerBlock = 512;

        struct PendingEvent
        {
            uint32_t Message;
            uint32_t Offset;
        };

        PendingEvent m_pending[MaxPendingPerBlock]{};
    };

    int FindMidiInputByName(_In_ const std::wstring& needle)
    {
        const UINT count = midiInGetNumDevs();

        for (UINT id = 0; id < count; id++)
        {
            MIDIINCAPSW caps{};

            if (midiInGetDevCapsW(id, &caps, sizeof(caps)) != MMSYSERR_NOERROR)
            {
                continue;
            }

            if (std::wstring(caps.szPname).find(needle) != std::wstring::npos)
            {
                return static_cast<int>(id);
            }
        }

        return -1;
    }

    void ListMidiInputs()
    {
        const UINT count = midiInGetNumDevs();

        printf("MIDI input devices: %u\n", count);

        for (UINT id = 0; id < count; id++)
        {
            MIDIINCAPSW caps{};

            if (midiInGetDevCapsW(id, &caps, sizeof(caps)) == MMSYSERR_NOERROR)
            {
                printf("  [%u] %ws\n", id, caps.szPname);
            }
        }
    }

    // Pushes a score into the same queue the MIDI callback uses, so the audio thread sees one path.
    void PlayScore(_In_ const std::wstring& path, _In_ const std::atomic<bool>& stopRequested)
    {
        std::ifstream file(path);

        if (!file.is_open())
        {
            return;
        }

        struct Event
        {
            double Seconds;
            uint32_t Message;
        };

        std::vector<Event> events;

        auto makeMessage = [](uint8_t status, uint8_t a, uint8_t b)
        {
            return static_cast<uint32_t>(status) |
                   (static_cast<uint32_t>(a) << 8) |
                   (static_cast<uint32_t>(b) << 16);
        };

        std::string line;

        while (std::getline(file, line))
        {
            const auto hash = line.find('#');

            if (hash != std::string::npos)
            {
                line.erase(hash);
            }

            std::istringstream parser(line);
            std::string command;

            if (!(parser >> command))
            {
                continue;
            }

            if (command == "note")
            {
                double start = 0.0, duration = 0.0;
                int channel = 0, note = 0, velocity = 0;

                if (parser >> start >> channel >> note >> velocity >> duration)
                {
                    events.push_back({ start, makeMessage(static_cast<uint8_t>(0x90 | (channel & 0x0F)),
                        static_cast<uint8_t>(note), static_cast<uint8_t>(velocity)) });
                    events.push_back({ start + duration, makeMessage(static_cast<uint8_t>(0x80 | (channel & 0x0F)),
                        static_cast<uint8_t>(note), 0) });
                }
            }
            else if (command == "program")
            {
                double start = 0.0;
                int channel = 0, bankMsb = 0, bankLsb = 0, program = 0;

                if (parser >> start >> channel >> bankMsb >> bankLsb >> program)
                {
                    const auto control = static_cast<uint8_t>(0xB0 | (channel & 0x0F));
                    events.push_back({ start, makeMessage(control, 0, static_cast<uint8_t>(bankMsb)) });
                    events.push_back({ start, makeMessage(control, 32, static_cast<uint8_t>(bankLsb)) });
                    events.push_back({ start, makeMessage(static_cast<uint8_t>(0xC0 | (channel & 0x0F)),
                        static_cast<uint8_t>(program), 0) });
                }
            }
            else if (command == "cc")
            {
                double start = 0.0;
                int channel = 0, controller = 0, value = 0;

                if (parser >> start >> channel >> controller >> value)
                {
                    events.push_back({ start, makeMessage(static_cast<uint8_t>(0xB0 | (channel & 0x0F)),
                        static_cast<uint8_t>(controller), static_cast<uint8_t>(value)) });
                }
            }
            else if (command == "bend")
            {
                double start = 0.0;
                int channel = 0, value = 0;

                if (parser >> start >> channel >> value)
                {
                    events.push_back({ start, makeMessage(static_cast<uint8_t>(0xE0 | (channel & 0x0F)),
                        static_cast<uint8_t>(value & 0x7F), static_cast<uint8_t>((value >> 7) & 0x7F)) });
                }
            }
        }

        std::stable_sort(events.begin(), events.end(),
            [](const Event& a, const Event& b) { return a.Seconds < b.Seconds; });

        LARGE_INTEGER frequency{};
        LARGE_INTEGER start{};
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&start);

        for (const auto& event : events)
        {
            if (stopRequested.load(std::memory_order_relaxed))
            {
                return;
            }

            const auto target = start.QuadPart +
                static_cast<int64_t>(event.Seconds * static_cast<double>(frequency.QuadPart));

            for (;;)
            {
                LARGE_INTEGER now{};
                QueryPerformanceCounter(&now);

                if (now.QuadPart >= target || stopRequested.load(std::memory_order_relaxed))
                {
                    break;
                }

                if (target - now.QuadPart > frequency.QuadPart / 500)
                {
                    Sleep(1);
                }
                else
                {
                    YieldProcessor();
                }
            }

            (void)g_queue.TryPush(QueuedMessage{ event.Message });
        }
    }
}

int wmain(int argc, wchar_t** argv)
{
    SetConsoleOutputCP(CP_UTF8);

    std::wstring dlsPath;
    std::wstring inputName;
    std::wstring scorePath;
    SynthMode mode = SynthMode::Modern;
    bool lowLatency = false;
    uint32_t maxVoices = 0;
    double runSeconds = 0.0;

    for (int i = 1; i < argc; i++)
    {
        const std::wstring argument = argv[i];

        if (argument == L"--compat") { mode = SynthMode::Compatible; }
        else if (argument == L"--modern") { mode = SynthMode::Modern; }
        else if (argument == L"--lowlatency") { lowLatency = true; }
        else if (argument == L"--dls" && i + 1 < argc) { dlsPath = argv[++i]; }
        else if (argument == L"--in" && i + 1 < argc) { inputName = argv[++i]; }
        else if (argument == L"--score" && i + 1 < argc) { scorePath = argv[++i]; }
        else if (argument == L"--voices" && i + 1 < argc) { maxVoices = static_cast<uint32_t>(_wtoi(argv[++i])); }
        else if (argument == L"--seconds" && i + 1 < argc) { runSeconds = _wtof(argv[++i]); }
        else if (argument == L"--list")
        {
            ListMidiInputs();
            return 0;
        }
        else
        {
            printf("usage: synthspike-play [--dls <file>] [--compat|--modern] [--lowlatency]\n");
            printf("                       [--in <midi input name>] [--score <file>]\n");
            printf("                       [--voices <n>] [--seconds <n>] [--list]\n");
            return 2;
        }
    }

    if (dlsPath.empty())
    {
        dlsPath = DefaultDlsPath();
    }

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    if (FAILED(comResult))
    {
        printf("CoInitializeEx failed\n");
        return 1;
    }

    DlsCollection collection;
    const auto status = DlsCollection::LoadFromFile(dlsPath, DlsParseLimits{}, collection);

    if (status != DlsParseStatus::Ok)
    {
        printf("failed to load %s: %s\n", ToUtf8(dlsPath).c_str(), DlsParseStatusToString(status));
        CoUninitialize();
        return 1;
    }

    WasapiAudioSink sink;

    if (!sink.Open(lowLatency))
    {
        printf("could not open the default render endpoint\n");
        CoUninitialize();
        return 1;
    }

    SynthConfig config = SynthConfig::ForMode(mode, sink.SampleRate());

    if (maxVoices > 0)
    {
        config.MaxVoices = maxVoices;
    }

    SynthEngine engine;

    if (!engine.Initialize(&collection, config))
    {
        printf("engine initialization failed\n");
        CoUninitialize();
        return 1;
    }

    printf("Sound set   %s\n", ToUtf8(collection.Name()).c_str());
    printf("Endpoint    %s\n", ToUtf8(sink.DeviceName()).c_str());
    printf("Mode        %s, %u voices\n",
        mode == SynthMode::Compatible ? "Compatible" : "Modern", config.MaxVoices);
    printf("Device      %u Hz, buffer %u frames (%.2f ms), period %u frames (%.2f ms)%s\n",
        sink.SampleRate(), sink.BufferFrames(), sink.BufferMilliseconds(),
        sink.PeriodFrames(), sink.PeriodMilliseconds(),
        sink.UsingLowLatencyPath() ? ", IAudioClient3 low latency" : "");

    if (sink.DefaultPeriodFrames() > 0)
    {
        const double rate = sink.SampleRate();
        printf("Periods     engine default %u (%.2f ms), minimum %u (%.2f ms)%s\n",
            sink.DefaultPeriodFrames(), 1000.0 * sink.DefaultPeriodFrames() / rate,
            sink.MinimumPeriodFrames(), 1000.0 * sink.MinimumPeriodFrames() / rate,
            (sink.MinimumPeriodFrames() >= sink.DefaultPeriodFrames())
                ? "  - this device offers no low latency headroom" : "");
    }

    if (config.RenderSampleRate() != sink.SampleRate())
    {
        printf("Engine      %u Hz, resampled to the device rate\n", config.RenderSampleRate());
    }
    else
    {
        printf("Engine      %u Hz, no conversion\n", config.RenderSampleRate());
    }

    SynthRenderSource source(engine, sink.SampleRate(), sink.BufferFrames());

    HMIDIIN midiIn = nullptr;

    if (!inputName.empty())
    {
        const int deviceId = FindMidiInputByName(inputName);

        if (deviceId < 0)
        {
            printf("\nMIDI input not found: %s\n\n", ToUtf8(inputName).c_str());
            ListMidiInputs();
            CoUninitialize();
            return 1;
        }

        MIDIINCAPSW caps{};
        midiInGetDevCapsW(static_cast<UINT>(deviceId), &caps, sizeof(caps));

        if (midiInOpen(&midiIn, static_cast<UINT>(deviceId),
            reinterpret_cast<DWORD_PTR>(MidiInputCallback), 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR)
        {
            printf("could not open MIDI input\n");
            CoUninitialize();
            return 1;
        }

        midiInStart(midiIn);
        printf("MIDI in     [%d] %ws\n", deviceId, caps.szPname);
    }

    if (!sink.Start(&source))
    {
        printf("could not start the audio stream\n");
        CoUninitialize();
        return 1;
    }

    std::atomic<bool> stopRequested{ false };
    std::thread scoreThread;

    if (!scorePath.empty())
    {
        timeBeginPeriod(1);
        scoreThread = std::thread([&] { PlayScore(scorePath, stopRequested); });
    }

    if (runSeconds > 0.0)
    {
        printf("\nPlaying for %.1f s.\n\n", runSeconds);
    }
    else
    {
        printf("\nPlaying. Press any key to stop.\n\n");
    }

    const DWORD startTicks = GetTickCount();

    for (;;)
    {
        if (runSeconds > 0.0)
        {
            if ((GetTickCount() - startTicks) >= static_cast<DWORD>(runSeconds * 1000.0))
            {
                break;
            }
        }
        else if (_kbhit())
        {
            break;
        }

        const auto stats = sink.Stats();

        printf("\r  voices %3u   glitches %llu   drops %llu   no-voice %llu   peak %6.1f dBFS   limiter -%.1f dB   clipped %llu   ",
            engine.ActiveVoiceCount(),
            static_cast<unsigned long long>(stats.GlitchCount),
            static_cast<unsigned long long>(g_droppedCount.load()),
            static_cast<unsigned long long>(engine.DroppedNoteCount()),
            engine.PeakOutputDbfs(),
            engine.PeakGainReductionDb(),
            static_cast<unsigned long long>(engine.ClippedSampleCount()));

        if (sink.DeviceLost())
        {
            printf("\n\nthe audio endpoint went away\n");
            break;
        }

        Sleep(100);
    }

    if (_kbhit())
    {
        (void)_getch();
    }

    stopRequested.store(true, std::memory_order_relaxed);

    if (scoreThread.joinable())
    {
        scoreThread.join();
        timeEndPeriod(1);
    }

    if (midiIn != nullptr)
    {
        midiInStop(midiIn);
        midiInReset(midiIn);
        midiInClose(midiIn);
    }

    sink.Stop();

    const auto stats = sink.Stats();

    printf("\n\nSession\n");
    printf("  audio callbacks        %llu\n", static_cast<unsigned long long>(stats.CallbackCount));
    printf("  glitches               %llu\n", static_cast<unsigned long long>(stats.GlitchCount));
    printf("  worst callback         %.2f ms of a %.2f ms period (%.0f %%)\n",
        stats.MaxCallbackMilliseconds, sink.PeriodMilliseconds(), stats.WorstLoadFactor * 100.0);
    printf("  audio queued ahead     %.2f ms average, %.2f ms worst, %.2f ms least\n",
        stats.AverageQueuedMilliseconds, stats.MaxQueuedMilliseconds, stats.MinQueuedMilliseconds);

    const double streamLatency = sink.StreamLatencyMilliseconds();

    if (streamLatency > 0.0)
    {
        printf("  engine stream latency  %.2f ms\n", streamLatency);
    }
    else
    {
        printf("  engine stream latency  not reported by this driver\n");
    }

    printf("  MIDI received          %llu\n", static_cast<unsigned long long>(g_receivedCount.load()));
    printf("  MIDI dropped at queue  %llu\n", static_cast<unsigned long long>(g_droppedCount.load()));
    printf("  notes with no voice    %llu   (region miss or voice limit, not latency)\n",
        static_cast<unsigned long long>(engine.DroppedNoteCount()));
    printf("  voices stolen          %llu\n",
        static_cast<unsigned long long>(engine.StolenVoiceCount()));
    printf("  peak output level      %.1f dBFS   (limiter threshold is -0.4)\n", engine.PeakOutputDbfs());
    printf("  peak limiter reduction %.1f dB\n", engine.PeakGainReductionDb());
    printf("  samples clipped        %llu   (should be zero with the limiter on)\n",
        static_cast<unsigned long long>(engine.ClippedSampleCount()));

    CoUninitialize();
    return 0;
}
