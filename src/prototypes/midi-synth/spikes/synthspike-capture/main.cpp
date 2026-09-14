// Spike: play a score to a WinMM MIDI device while capturing the default render endpoint via
// WASAPI loopback, so the in-box synth's output can be compared against ours objectively.
//
// The endpoint is opened shared mode, read only, using the mix format it already reports. No
// device setting is read-modify-written and nothing here can change the user's audio configuration.

#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <mmeapi.h>
#include <timeapi.h>
#include <functiondiscoverykeys_devpkey.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
    struct TimedEvent
    {
        double Seconds{ 0.0 };
        uint32_t ShortMessage{ 0 };
    };

    template <typename T>
    struct ComPtr
    {
        T* Pointer{ nullptr };

        ~ComPtr() { if (Pointer != nullptr) { Pointer->Release(); } }

        T** operator&() noexcept { return &Pointer; }
        T* operator->() const noexcept { return Pointer; }
        explicit operator bool() const noexcept { return Pointer != nullptr; }
    };

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
        WideCharToMultiByte(
            CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), required, nullptr, nullptr);

        return result;
    }

    uint32_t MakeShortMessage(_In_ uint8_t status, _In_ uint8_t data1, _In_ uint8_t data2) noexcept
    {
        return static_cast<uint32_t>(status) |
               (static_cast<uint32_t>(data1) << 8) |
               (static_cast<uint32_t>(data2) << 16);
    }

    // Same score format as synthspike-render, so one file drives both sides of the comparison.
    bool TryParseScore(_In_ const std::wstring& path, _Out_ std::vector<TimedEvent>& events)
    {
        events.clear();

        std::ifstream file(path);

        if (!file.is_open())
        {
            return false;
        }

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
                double start = 0.0;
                double duration = 0.0;
                int channel = 0;
                int note = 0;
                int velocity = 0;

                if (parser >> start >> channel >> note >> velocity >> duration)
                {
                    events.push_back({ start, MakeShortMessage(
                        static_cast<uint8_t>(0x90 | (channel & 0x0F)),
                        static_cast<uint8_t>(note), static_cast<uint8_t>(velocity)) });
                    events.push_back({ start + duration, MakeShortMessage(
                        static_cast<uint8_t>(0x80 | (channel & 0x0F)),
                        static_cast<uint8_t>(note), 0) });
                }
            }
            else if (command == "program")
            {
                double start = 0.0;
                int channel = 0;
                int bankMsb = 0;
                int bankLsb = 0;
                int program = 0;

                if (parser >> start >> channel >> bankMsb >> bankLsb >> program)
                {
                    const auto status = static_cast<uint8_t>(0xB0 | (channel & 0x0F));
                    events.push_back({ start, MakeShortMessage(status, 0, static_cast<uint8_t>(bankMsb)) });
                    events.push_back({ start, MakeShortMessage(status, 32, static_cast<uint8_t>(bankLsb)) });
                    events.push_back({ start, MakeShortMessage(
                        static_cast<uint8_t>(0xC0 | (channel & 0x0F)), static_cast<uint8_t>(program), 0) });
                }
            }
            else if (command == "cc")
            {
                double start = 0.0;
                int channel = 0;
                int controller = 0;
                int value = 0;

                if (parser >> start >> channel >> controller >> value)
                {
                    events.push_back({ start, MakeShortMessage(
                        static_cast<uint8_t>(0xB0 | (channel & 0x0F)),
                        static_cast<uint8_t>(controller), static_cast<uint8_t>(value)) });
                }
            }
            else if (command == "bend")
            {
                double start = 0.0;
                int channel = 0;
                int value = 0;

                if (parser >> start >> channel >> value)
                {
                    events.push_back({ start, MakeShortMessage(
                        static_cast<uint8_t>(0xE0 | (channel & 0x0F)),
                        static_cast<uint8_t>(value & 0x7F),
                        static_cast<uint8_t>((value >> 7) & 0x7F)) });
                }
            }
        }

        std::stable_sort(events.begin(), events.end(),
            [](const TimedEvent& a, const TimedEvent& b) { return a.Seconds < b.Seconds; });

        return true;
    }

    int FindMidiDeviceByName(_In_ const std::wstring& needle)
    {
        const UINT count = midiOutGetNumDevs();

        for (UINT id = 0; id < count; id++)
        {
            MIDIOUTCAPSW caps{};

            if (midiOutGetDevCapsW(id, &caps, sizeof(caps)) != MMSYSERR_NOERROR)
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

    bool WriteWaveFile(
        _In_ const std::wstring& path,
        _In_ const std::vector<float>& interleavedStereo,
        _In_ uint32_t sampleRate)
    {
        std::ofstream file(path, std::ios::binary);

        if (!file.is_open())
        {
            return false;
        }

        const auto frameCount = static_cast<uint32_t>(interleavedStereo.size() / 2);
        const uint16_t channels = 2;
        const uint16_t bitsPerSample = 16;
        const uint16_t blockAlign = channels * (bitsPerSample / 8);
        const uint32_t dataBytes = frameCount * blockAlign;

        auto writeUInt32 = [&file](uint32_t value) { file.write(reinterpret_cast<const char*>(&value), 4); };
        auto writeUInt16 = [&file](uint16_t value) { file.write(reinterpret_cast<const char*>(&value), 2); };

        file.write("RIFF", 4);
        writeUInt32(36 + dataBytes);
        file.write("WAVE", 4);
        file.write("fmt ", 4);
        writeUInt32(16);
        writeUInt16(1);
        writeUInt16(channels);
        writeUInt32(sampleRate);
        writeUInt32(sampleRate * blockAlign);
        writeUInt16(blockAlign);
        writeUInt16(bitsPerSample);
        file.write("data", 4);
        writeUInt32(dataBytes);

        std::vector<int16_t> pcm(interleavedStereo.size());

        for (size_t i = 0; i < interleavedStereo.size(); i++)
        {
            const float clamped = (std::clamp)(interleavedStereo[i], -1.0f, 1.0f);
            pcm[i] = static_cast<int16_t>(clamped * 32767.0f);
        }

        file.write(reinterpret_cast<const char*>(pcm.data()),
            static_cast<std::streamsize>(pcm.size() * sizeof(int16_t)));

        return file.good();
    }

    void SleepUntil(_In_ int64_t targetTicks, _In_ int64_t ticksPerSecond) noexcept
    {
        for (;;)
        {
            LARGE_INTEGER now{};
            QueryPerformanceCounter(&now);

            const int64_t remaining = targetTicks - now.QuadPart;

            if (remaining <= 0)
            {
                return;
            }

            // Sleep while there is time to spare, then spin for the last stretch so event
            // placement is not at the mercy of the scheduler tick.
            if (remaining > ticksPerSecond / 500)
            {
                Sleep(1);
            }
            else
            {
                YieldProcessor();
            }
        }
    }

    // Renders a tone of known amplitude while capturing loopback, so the gain of the capture path
    // can be measured instead of assumed. Without this there is no way to tell whether a level
    // difference between two captures is real or is just the endpoint volume control.
    int RunCalibration(_In_ IMMDevice* device, _In_ double amplitude, _In_ double seconds)
    {
        ComPtr<IAudioClient> renderClient;
        ComPtr<IAudioRenderClient> renderService;
        ComPtr<IAudioClient> captureAudioClient;
        ComPtr<IAudioCaptureClient> captureService;

        HRESULT hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
            reinterpret_cast<void**>(&renderClient));

        WAVEFORMATEX* mixFormat = nullptr;

        if (SUCCEEDED(hr)) { hr = renderClient->GetMixFormat(&mixFormat); }

        if (FAILED(hr) || mixFormat == nullptr)
        {
            printf("could not get the mix format: 0x%08X\n", static_cast<unsigned>(hr));
            return 1;
        }

        const uint32_t rate = mixFormat->nSamplesPerSec;
        const uint16_t channels = mixFormat->nChannels;
        const bool isFloat = (mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
            (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
             reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mixFormat)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);

        if (!isFloat)
        {
            printf("calibration expects a float mix format\n");
            CoTaskMemFree(mixFormat);
            return 1;
        }

        hr = renderClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, mixFormat, nullptr);

        if (SUCCEEDED(hr)) { hr = renderClient->GetService(IID_PPV_ARGS(&renderService)); }

        if (SUCCEEDED(hr))
        {
            hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                reinterpret_cast<void**>(&captureAudioClient));
        }

        if (SUCCEEDED(hr))
        {
            hr = captureAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
                10000000, 0, mixFormat, nullptr);
        }

        if (SUCCEEDED(hr)) { hr = captureAudioClient->GetService(IID_PPV_ARGS(&captureService)); }

        if (FAILED(hr))
        {
            printf("could not set up calibration streams: 0x%08X\n", static_cast<unsigned>(hr));
            CoTaskMemFree(mixFormat);
            return 1;
        }

        UINT32 renderBufferFrames = 0;
        renderClient->GetBufferSize(&renderBufferFrames);

        const auto totalFrames = static_cast<uint32_t>(seconds * rate);
        constexpr double Pi = 3.14159265358979323846;
        constexpr double ToneHertz = 440.0;
        const auto fadeFrames = static_cast<uint32_t>(0.05 * rate);

        std::vector<float> captured;
        captured.reserve(static_cast<size_t>(totalFrames) * 2);

        std::atomic<bool> stopRequested{ false };

        captureAudioClient->Start();

        std::thread captureThread([&]
        {
            while (!stopRequested.load(std::memory_order_relaxed))
            {
                UINT32 packetFrames = 0;

                if (FAILED(captureService->GetNextPacketSize(&packetFrames)) || packetFrames == 0)
                {
                    Sleep(1);
                    continue;
                }

                BYTE* data = nullptr;
                UINT32 frames = 0;
                DWORD flags = 0;

                if (FAILED(captureService->GetBuffer(&data, &frames, &flags, nullptr, nullptr)))
                {
                    continue;
                }

                if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0)
                {
                    captured.insert(captured.end(), static_cast<size_t>(frames) * 2, 0.0f);
                }
                else
                {
                    const auto* samples = reinterpret_cast<const float*>(data);

                    for (UINT32 frame = 0; frame < frames; frame++)
                    {
                        captured.push_back(samples[frame * channels]);
                        captured.push_back(channels > 1 ? samples[frame * channels + 1] : samples[frame * channels]);
                    }
                }

                captureService->ReleaseBuffer(frames);
            }
        });

        renderClient->Start();

        uint32_t rendered = 0;

        while (rendered < totalFrames)
        {
            UINT32 padding = 0;

            if (FAILED(renderClient->GetCurrentPadding(&padding)))
            {
                break;
            }

            const UINT32 available = renderBufferFrames - padding;

            if (available == 0)
            {
                Sleep(1);
                continue;
            }

            const UINT32 frames = (std::min)(available, totalFrames - rendered);

            BYTE* buffer = nullptr;

            if (FAILED(renderService->GetBuffer(frames, &buffer)))
            {
                break;
            }

            auto* output = reinterpret_cast<float*>(buffer);

            for (UINT32 frame = 0; frame < frames; frame++)
            {
                const uint32_t position = rendered + frame;

                double envelope = 1.0;

                if (position < fadeFrames)
                {
                    envelope = static_cast<double>(position) / fadeFrames;
                }
                else if (position > totalFrames - fadeFrames)
                {
                    envelope = static_cast<double>(totalFrames - position) / fadeFrames;
                }

                const auto value = static_cast<float>(
                    amplitude * envelope * std::sin(2.0 * Pi * ToneHertz * position / rate));

                for (uint16_t channel = 0; channel < channels; channel++)
                {
                    output[frame * channels + channel] = (channel < 2) ? value : 0.0f;
                }
            }

            renderService->ReleaseBuffer(frames, 0);
            rendered += frames;
        }

        Sleep(300);

        stopRequested.store(true, std::memory_order_relaxed);
        captureThread.join();

        renderClient->Stop();
        captureAudioClient->Stop();

        // Measure the steady portion only, past the fade in and any stream startup.
        const size_t analysisStart = static_cast<size_t>(0.4 * rate) * 2;
        const size_t analysisEnd = captured.size() > static_cast<size_t>(0.3 * rate) * 2
            ? captured.size() - static_cast<size_t>(0.3 * rate) * 2
            : captured.size();

        double energy = 0.0;
        size_t counted = 0;

        for (size_t i = analysisStart; i < analysisEnd; i++)
        {
            energy += static_cast<double>(captured[i]) * captured[i];
            counted++;
        }

        CoTaskMemFree(mixFormat);

        if (counted == 0)
        {
            printf("calibration captured nothing\n");
            return 1;
        }

        const double capturedRms = std::sqrt(energy / counted);
        const double expectedRms = amplitude / std::sqrt(2.0);

        auto toDb = [](double value) { return (value > 0.0) ? 20.0 * std::log10(value) : -200.0; };

        const double pathGainDb = toDb(capturedRms) - toDb(expectedRms);

        printf("Calibration\n");
        printf("  tone emitted           %.1f Hz at %.1f dBFS peak (%.1f dBFS RMS)\n",
            ToneHertz, toDb(amplitude), toDb(expectedRms));
        printf("  captured               %.2f dBFS RMS\n", toDb(capturedRms));
        printf("  loopback path gain     %+.2f dB\n", pathGainDb);

        return 0;
    }
}

int wmain(int argc, wchar_t** argv)
{
    SetConsoleOutputCP(CP_UTF8);

    std::wstring scorePath;
    std::wstring outputPath = L"capture.wav";
    std::wstring deviceName = L"Microsoft GS Wavetable Synth";
    double silenceSeconds = 0.0;
    double tailSeconds = 2.0;
    bool calibrate = false;
    double calibrationAmplitude = 0.1;

    for (int i = 1; i < argc; i++)
    {
        const std::wstring argument = argv[i];

        if (argument == L"--score" && i + 1 < argc) { scorePath = argv[++i]; }
        else if (argument == L"--out" && i + 1 < argc) { outputPath = argv[++i]; }
        else if (argument == L"--device" && i + 1 < argc) { deviceName = argv[++i]; }
        else if (argument == L"--silence" && i + 1 < argc) { silenceSeconds = _wtof(argv[++i]); }
        else if (argument == L"--tail" && i + 1 < argc) { tailSeconds = _wtof(argv[++i]); }
        else if (argument == L"--calibrate") { calibrate = true; }
        else if (argument == L"--amplitude" && i + 1 < argc) { calibrationAmplitude = _wtof(argv[++i]); }
        else
        {
            printf("usage: synthspike-capture --score <file> [--out <file.wav>]\n");
            printf("                          [--device <name substring>] [--tail <seconds>]\n");
            printf("       synthspike-capture --silence <seconds> [--out <file.wav>]\n");
            printf("       synthspike-capture --calibrate [--amplitude <0..1>]\n");
            return 2;
        }
    }

    std::vector<TimedEvent> events;

    if (!calibrate && silenceSeconds <= 0.0)
    {
        if (scorePath.empty() || !TryParseScore(scorePath, events))
        {
            printf("could not read score, use --score <file>, --silence <seconds> or --calibrate\n");
            return 1;
        }
    }

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    if (FAILED(comResult))
    {
        printf("CoInitializeEx failed: 0x%08X\n", static_cast<unsigned>(comResult));
        return 1;
    }

    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice> device;
    ComPtr<IAudioClient> audioClient;
    ComPtr<IAudioCaptureClient> captureClient;

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        IID_PPV_ARGS(&enumerator));

    if (SUCCEEDED(hr)) { hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device); }

    if (FAILED(hr))
    {
        printf("could not open the default render endpoint: 0x%08X\n", static_cast<unsigned>(hr));
        CoUninitialize();
        return 1;
    }

    {
        ComPtr<IPropertyStore> properties;

        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &properties)))
        {
            PROPVARIANT name{};
            PropVariantInit(&name);

            if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &name)) && name.vt == VT_LPWSTR)
            {
                printf("Endpoint    %s\n", ToUtf8(name.pwszVal).c_str());
            }

            PropVariantClear(&name);
        }
    }

    if (calibrate)
    {
        const int result = RunCalibration(device.Pointer, calibrationAmplitude, 2.0);
        CoUninitialize();
        return result;
    }

    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&audioClient));

    WAVEFORMATEX* mixFormat = nullptr;

    if (SUCCEEDED(hr)) { hr = audioClient->GetMixFormat(&mixFormat); }
    if (FAILED(hr) || mixFormat == nullptr)
    {
        printf("could not get the endpoint mix format: 0x%08X\n", static_cast<unsigned>(hr));
        CoUninitialize();
        return 1;
    }

    const uint32_t captureRate = mixFormat->nSamplesPerSec;
    const uint16_t captureChannels = mixFormat->nChannels;
    const bool isFloat = (mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
        (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
         reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mixFormat)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);

    printf("Mix format  %u Hz, %u channels, %u bit %s (read as-is, not modified)\n",
        captureRate, captureChannels, mixFormat->wBitsPerSample, isFloat ? "float" : "int");

    if (!isFloat && mixFormat->wBitsPerSample != 16)
    {
        printf("unsupported capture format\n");
        CoTaskMemFree(mixFormat);
        CoUninitialize();
        return 1;
    }

    // One second buffer. Shared mode, so this is a request for the client's own buffer and does
    // not alter the engine period or anything device wide.
    hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
        10000000, 0, mixFormat, nullptr);

    if (SUCCEEDED(hr)) { hr = audioClient->GetService(IID_PPV_ARGS(&captureClient)); }

    if (FAILED(hr))
    {
        printf("could not start loopback capture: 0x%08X\n", static_cast<unsigned>(hr));
        CoTaskMemFree(mixFormat);
        CoUninitialize();
        return 1;
    }

    HMIDIOUT midiOut = nullptr;
    int deviceId = -1;

    if (!events.empty())
    {
        deviceId = FindMidiDeviceByName(deviceName);

        if (deviceId < 0)
        {
            printf("MIDI device not found: %s\n", ToUtf8(deviceName).c_str());
            CoTaskMemFree(mixFormat);
            CoUninitialize();
            return 1;
        }

        MIDIOUTCAPSW caps{};
        midiOutGetDevCapsW(static_cast<UINT>(deviceId), &caps, sizeof(caps));
        printf("MIDI device [%d] %ws\n", deviceId, caps.szPname);

        // Opened before capture starts so the synth's own render stream is already running and
        // the loopback clock is advancing when the first note arrives.
        if (midiOutOpen(&midiOut, static_cast<UINT>(deviceId), 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        {
            printf("midiOutOpen failed\n");
            CoTaskMemFree(mixFormat);
            CoUninitialize();
            return 1;
        }
    }

    const double totalSeconds = events.empty()
        ? silenceSeconds
        : events.back().Seconds + tailSeconds;

    printf("Capturing   %.2f s\n\n", totalSeconds);

    std::vector<float> captured;
    captured.reserve(static_cast<size_t>(totalSeconds * captureRate) * 2 + captureRate);

    std::atomic<bool> stopRequested{ false };
    std::atomic<uint64_t> silentFrames{ 0 };

    hr = audioClient->Start();

    if (FAILED(hr))
    {
        printf("audio client start failed: 0x%08X\n", static_cast<unsigned>(hr));
        if (midiOut != nullptr) { midiOutClose(midiOut); }
        CoTaskMemFree(mixFormat);
        CoUninitialize();
        return 1;
    }

    std::thread captureThread([&]
    {
        while (!stopRequested.load(std::memory_order_relaxed))
        {
            UINT32 packetFrames = 0;

            if (FAILED(captureClient->GetNextPacketSize(&packetFrames)) || packetFrames == 0)
            {
                Sleep(2);
                continue;
            }

            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;

            if (FAILED(captureClient->GetBuffer(&data, &frames, &flags, nullptr, nullptr)))
            {
                continue;
            }

            const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;

            if (silent)
            {
                silentFrames.fetch_add(frames, std::memory_order_relaxed);
                captured.insert(captured.end(), static_cast<size_t>(frames) * 2, 0.0f);
            }
            else
            {
                for (UINT32 frame = 0; frame < frames; frame++)
                {
                    float left = 0.0f;
                    float right = 0.0f;

                    if (isFloat)
                    {
                        const auto* samples = reinterpret_cast<const float*>(data) + frame * captureChannels;
                        left = samples[0];
                        right = (captureChannels > 1) ? samples[1] : samples[0];
                    }
                    else
                    {
                        const auto* samples = reinterpret_cast<const int16_t*>(data) + frame * captureChannels;
                        left = samples[0] / 32768.0f;
                        right = (captureChannels > 1) ? samples[1] / 32768.0f : left;
                    }

                    captured.push_back(left);
                    captured.push_back(right);
                }
            }

            captureClient->ReleaseBuffer(frames);
        }
    });

    timeBeginPeriod(1);

    LARGE_INTEGER frequency{};
    LARGE_INTEGER start{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);

    // Let the capture stream settle before the first note.
    SleepUntil(start.QuadPart + frequency.QuadPart / 4, frequency.QuadPart);

    LARGE_INTEGER scoreStart{};
    QueryPerformanceCounter(&scoreStart);

    for (const auto& event : events)
    {
        const auto target = scoreStart.QuadPart +
            static_cast<int64_t>(event.Seconds * static_cast<double>(frequency.QuadPart));

        SleepUntil(target, frequency.QuadPart);
        midiOutShortMsg(midiOut, event.ShortMessage);
    }

    const auto endTarget = scoreStart.QuadPart +
        static_cast<int64_t>(totalSeconds * static_cast<double>(frequency.QuadPart));

    SleepUntil(endTarget, frequency.QuadPart);

    timeEndPeriod(1);

    stopRequested.store(true, std::memory_order_relaxed);
    captureThread.join();

    audioClient->Stop();

    if (midiOut != nullptr)
    {
        for (uint8_t channel = 0; channel < 16; channel++)
        {
            midiOutShortMsg(midiOut, MakeShortMessage(static_cast<uint8_t>(0xB0 | channel), 123, 0));
        }

        midiOutClose(midiOut);
    }

    double peak = 0.0;
    double energy = 0.0;

    for (const auto sample : captured)
    {
        peak = (std::max)(peak, static_cast<double>(std::abs(sample)));
        energy += static_cast<double>(sample) * sample;
    }

    const double rms = captured.empty() ? 0.0 : std::sqrt(energy / captured.size());
    auto toDb = [](double value) { return (value > 0.0) ? 20.0 * std::log10(value) : -200.0; };

    printf("Captured    %zu frames (%.2f s)\n", captured.size() / 2,
        static_cast<double>(captured.size() / 2) / captureRate);
    printf("  silent packets         %llu frames\n",
        static_cast<unsigned long long>(silentFrames.load()));
    printf("  peak                   %.1f dBFS\n", toDb(peak));
    printf("  RMS                    %.1f dBFS\n", toDb(rms));

    if (!WriteWaveFile(outputPath, captured, captureRate))
    {
        printf("could not write %s\n", ToUtf8(outputPath).c_str());
        CoTaskMemFree(mixFormat);
        CoUninitialize();
        return 1;
    }

    printf("\nWrote %s\n", ToUtf8(outputPath).c_str());

    CoTaskMemFree(mixFormat);
    CoUninitialize();

    return 0;
}
