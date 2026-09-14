#include "MidiSynth/AudioSink.h"

#include <windows.h>
#include <audioclient.h>
#include <avrt.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

namespace MidiSynth
{
    namespace
    {
        template <typename T>
        struct ComPtr
        {
            T* Pointer{ nullptr };

            ~ComPtr() { Reset(); }

            void Reset() noexcept
            {
                if (Pointer != nullptr)
                {
                    Pointer->Release();
                    Pointer = nullptr;
                }
            }

            T** operator&() noexcept { return &Pointer; }
            T* operator->() const noexcept { return Pointer; }
            explicit operator bool() const noexcept { return Pointer != nullptr; }
        };

        struct HandleCloser
        {
            HANDLE Value{ nullptr };

            ~HandleCloser()
            {
                if (Value != nullptr)
                {
                    CloseHandle(Value);
                }
            }
        };
    }

    struct WasapiAudioSink::Impl
    {
        ComPtr<IMMDeviceEnumerator> Enumerator;
        ComPtr<IMMDevice> Device;
        ComPtr<IAudioClient> AudioClient;
        ComPtr<IAudioRenderClient> RenderClient;

        WAVEFORMATEX* MixFormat{ nullptr };
        HandleCloser BufferEvent;

        uint32_t BufferFrameCount{ 0 };
        uint32_t PeriodFrameCount{ 0 };
        uint32_t DefaultPeriod{ 0 };
        uint32_t MinimumPeriod{ 0 };
        uint16_t ChannelCount{ 0 };
        bool IsFloat{ false };
        bool LowLatency{ false };

        std::wstring FriendlyName;

        std::thread RenderThread;
        std::atomic<bool> StopRequested{ false };
        std::atomic<bool> Lost{ false };

        IAudioRenderSource* Source{ nullptr };

        std::vector<float> Staging;

        std::atomic<uint64_t> CallbackCount{ 0 };
        std::atomic<uint64_t> GlitchCount{ 0 };
        std::atomic<double> LastCallbackMilliseconds{ 0.0 };
        std::atomic<double> MaxCallbackMilliseconds{ 0.0 };
        std::atomic<double> WorstLoadFactor{ 0.0 };
        std::atomic<uint64_t> PaddingSum{ 0 };
        std::atomic<uint64_t> PaddingSamples{ 0 };
        std::atomic<uint32_t> MaxPadding{ 0 };
        std::atomic<uint32_t> MinPadding{ UINT32_MAX };

        ~Impl()
        {
            if (MixFormat != nullptr)
            {
                CoTaskMemFree(MixFormat);
            }
        }

        void RenderLoop() noexcept;
    };

    void WasapiAudioSink::Impl::RenderLoop() noexcept
    {
        // The render thread touches COM interfaces, so it needs its own apartment.
        const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        DWORD taskIndex = 0;
        const HANDLE mmcssHandle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);

        const double periodMilliseconds = (PeriodFrameCount > 0 && MixFormat != nullptr)
            ? 1000.0 * PeriodFrameCount / MixFormat->nSamplesPerSec
            : 0.0;

        while (!StopRequested.load(std::memory_order_relaxed))
        {
            const DWORD waitResult = WaitForSingleObject(BufferEvent.Value, 2000);

            if (waitResult != WAIT_OBJECT_0)
            {
                if (StopRequested.load(std::memory_order_relaxed))
                {
                    break;
                }

                GlitchCount.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            UINT32 padding = 0;
            HRESULT hr = AudioClient->GetCurrentPadding(&padding);

            if (hr == AUDCLNT_E_DEVICE_INVALIDATED)
            {
                Lost.store(true, std::memory_order_relaxed);
                break;
            }

            if (FAILED(hr))
            {
                GlitchCount.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            const UINT32 available = BufferFrameCount - padding;

            if (available == 0)
            {
                continue;
            }

            // Write one period, not the whole buffer. Topping the buffer up to full every time
            // would keep a buffer's worth of audio permanently queued ahead of the device, which
            // is latency the player feels for no benefit.
            const UINT32 frames = (std::min)(available, PeriodFrameCount);

            PaddingSum.fetch_add(padding, std::memory_order_relaxed);
            PaddingSamples.fetch_add(1, std::memory_order_relaxed);

            if (padding > MaxPadding.load(std::memory_order_relaxed))
            {
                MaxPadding.store(padding, std::memory_order_relaxed);
            }

            if (padding < MinPadding.load(std::memory_order_relaxed))
            {
                MinPadding.store(padding, std::memory_order_relaxed);
            }

            BYTE* buffer = nullptr;
            hr = RenderClient->GetBuffer(frames, &buffer);

            if (hr == AUDCLNT_E_DEVICE_INVALIDATED)
            {
                Lost.store(true, std::memory_order_relaxed);
                break;
            }

            if (FAILED(hr))
            {
                GlitchCount.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            LARGE_INTEGER start{};
            QueryPerformanceCounter(&start);

            if (Source != nullptr && Staging.size() >= static_cast<size_t>(frames) * 2)
            {
                Source->RenderAudio(Staging.data(), frames);
            }
            else
            {
                std::fill_n(Staging.begin(), static_cast<size_t>(frames) * 2, 0.0f);
            }

            LARGE_INTEGER end{};
            QueryPerformanceCounter(&end);

            if (IsFloat)
            {
                auto* output = reinterpret_cast<float*>(buffer);

                for (UINT32 frame = 0; frame < frames; frame++)
                {
                    const float left = Staging[static_cast<size_t>(frame) * 2];
                    const float right = Staging[static_cast<size_t>(frame) * 2 + 1];

                    for (uint16_t channel = 0; channel < ChannelCount; channel++)
                    {
                        // Anything beyond the first pair is silenced rather than fanned out,
                        // so a multichannel endpoint does not get the mix on every speaker.
                        output[frame * ChannelCount + channel] =
                            (channel == 0) ? left : (channel == 1) ? right : 0.0f;
                    }
                }
            }
            else
            {
                auto* output = reinterpret_cast<int16_t*>(buffer);

                for (UINT32 frame = 0; frame < frames; frame++)
                {
                    for (uint16_t channel = 0; channel < ChannelCount; channel++)
                    {
                        const float value = (channel == 0)
                            ? Staging[static_cast<size_t>(frame) * 2]
                            : (channel == 1) ? Staging[static_cast<size_t>(frame) * 2 + 1] : 0.0f;

                        output[frame * ChannelCount + channel] =
                            static_cast<int16_t>((std::clamp)(value, -1.0f, 1.0f) * 32767.0f);
                    }
                }
            }

            RenderClient->ReleaseBuffer(frames, 0);

            const double elapsedMilliseconds =
                1000.0 * static_cast<double>(end.QuadPart - start.QuadPart) / frequency.QuadPart;

            CallbackCount.fetch_add(1, std::memory_order_relaxed);
            LastCallbackMilliseconds.store(elapsedMilliseconds, std::memory_order_relaxed);

            if (elapsedMilliseconds > MaxCallbackMilliseconds.load(std::memory_order_relaxed))
            {
                MaxCallbackMilliseconds.store(elapsedMilliseconds, std::memory_order_relaxed);
            }

            if (periodMilliseconds > 0.0)
            {
                const double load = elapsedMilliseconds / periodMilliseconds;

                if (load > WorstLoadFactor.load(std::memory_order_relaxed))
                {
                    WorstLoadFactor.store(load, std::memory_order_relaxed);
                }
            }
        }

        if (mmcssHandle != nullptr)
        {
            AvRevertMmThreadCharacteristics(mmcssHandle);
        }

        if (SUCCEEDED(comResult))
        {
            CoUninitialize();
        }
    }

    WasapiAudioSink::WasapiAudioSink()
        : m_impl(std::make_unique<Impl>())
    {
    }

    WasapiAudioSink::~WasapiAudioSink()
    {
        Stop();
    }

    _Use_decl_annotations_
    bool WasapiAudioSink::Open(bool lowLatency)
    {
        auto& impl = *m_impl;
        impl.LowLatency = lowLatency;

        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
            IID_PPV_ARGS(&impl.Enumerator));

        if (SUCCEEDED(hr))
        {
            hr = impl.Enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &impl.Device);
        }

        if (FAILED(hr))
        {
            return false;
        }

        {
            ComPtr<IPropertyStore> properties;

            if (SUCCEEDED(impl.Device->OpenPropertyStore(STGM_READ, &properties)))
            {
                PROPVARIANT name{};
                PropVariantInit(&name);

                if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &name)) && name.vt == VT_LPWSTR)
                {
                    impl.FriendlyName = name.pwszVal;
                }

                PropVariantClear(&name);
            }
        }

        hr = impl.Device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
            reinterpret_cast<void**>(&impl.AudioClient));

        if (SUCCEEDED(hr))
        {
            hr = impl.AudioClient->GetMixFormat(&impl.MixFormat);
        }

        if (FAILED(hr) || impl.MixFormat == nullptr)
        {
            return false;
        }

        impl.ChannelCount = impl.MixFormat->nChannels;
        impl.IsFloat = (impl.MixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
            (impl.MixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
             reinterpret_cast<WAVEFORMATEXTENSIBLE*>(impl.MixFormat)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);

        if (!impl.IsFloat && impl.MixFormat->wBitsPerSample != 16)
        {
            return false;
        }

        impl.BufferEvent.Value = CreateEventW(nullptr, FALSE, FALSE, nullptr);

        if (impl.BufferEvent.Value == nullptr)
        {
            return false;
        }

        bool initialized = false;

        {
            ComPtr<IAudioClient3> audioClient3;

            if (SUCCEEDED(impl.AudioClient.Pointer->QueryInterface(IID_PPV_ARGS(&audioClient3))))
            {
                UINT32 defaultPeriod = 0;
                UINT32 fundamentalPeriod = 0;
                UINT32 minimumPeriod = 0;
                UINT32 maximumPeriod = 0;

                if (SUCCEEDED(audioClient3->GetSharedModeEnginePeriod(impl.MixFormat,
                    &defaultPeriod, &fundamentalPeriod, &minimumPeriod, &maximumPeriod)))
                {
                    impl.DefaultPeriod = defaultPeriod;
                    impl.MinimumPeriod = minimumPeriod;

                    if (lowLatency && SUCCEEDED(audioClient3->InitializeSharedAudioStream(
                        AUDCLNT_STREAMFLAGS_EVENTCALLBACK, minimumPeriod, impl.MixFormat, nullptr)))
                    {
                        impl.PeriodFrameCount = minimumPeriod;
                        initialized = true;
                    }
                }
            }
        }

        if (!initialized)
        {
            impl.LowLatency = false;

            hr = impl.AudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, impl.MixFormat, nullptr);

            if (FAILED(hr))
            {
                return false;
            }

            REFERENCE_TIME devicePeriod = 0;

            if (SUCCEEDED(impl.AudioClient->GetDevicePeriod(&devicePeriod, nullptr)))
            {
                impl.PeriodFrameCount = static_cast<uint32_t>(
                    devicePeriod * impl.MixFormat->nSamplesPerSec / 10000000);
            }
        }

        if (FAILED(impl.AudioClient->SetEventHandle(impl.BufferEvent.Value)))
        {
            return false;
        }

        UINT32 bufferFrames = 0;

        if (FAILED(impl.AudioClient->GetBufferSize(&bufferFrames)))
        {
            return false;
        }

        impl.BufferFrameCount = bufferFrames;

        if (impl.PeriodFrameCount == 0)
        {
            impl.PeriodFrameCount = bufferFrames;
        }

        if (FAILED(impl.AudioClient->GetService(IID_PPV_ARGS(&impl.RenderClient))))
        {
            return false;
        }

        // Sized for a whole buffer so the render thread never allocates.
        impl.Staging.assign(static_cast<size_t>(bufferFrames) * 2, 0.0f);

        return true;
    }

    _Use_decl_annotations_
    bool WasapiAudioSink::Start(IAudioRenderSource* source)
    {
        auto& impl = *m_impl;

        if (!impl.AudioClient || impl.RenderThread.joinable())
        {
            return false;
        }

        impl.Source = source;
        impl.StopRequested.store(false, std::memory_order_relaxed);

        // Prime one period so the first event does not land on an empty buffer. Priming the whole
        // buffer would start the stream already a full buffer behind.
        {
            BYTE* buffer = nullptr;

            if (SUCCEEDED(impl.RenderClient->GetBuffer(impl.PeriodFrameCount, &buffer)))
            {
                impl.RenderClient->ReleaseBuffer(impl.PeriodFrameCount, AUDCLNT_BUFFERFLAGS_SILENT);
            }
        }

        if (FAILED(impl.AudioClient->Start()))
        {
            return false;
        }

        impl.RenderThread = std::thread([&impl] { impl.RenderLoop(); });

        return true;
    }

    void WasapiAudioSink::Stop()
    {
        auto& impl = *m_impl;

        impl.StopRequested.store(true, std::memory_order_relaxed);

        if (impl.BufferEvent.Value != nullptr)
        {
            SetEvent(impl.BufferEvent.Value);
        }

        if (impl.RenderThread.joinable())
        {
            impl.RenderThread.join();
        }

        if (impl.AudioClient)
        {
            impl.AudioClient->Stop();
        }

        impl.Source = nullptr;
    }

    uint32_t WasapiAudioSink::SampleRate() const noexcept
    {
        return (m_impl->MixFormat != nullptr) ? m_impl->MixFormat->nSamplesPerSec : 0;
    }

    uint32_t WasapiAudioSink::BufferFrames() const noexcept
    {
        return m_impl->BufferFrameCount;
    }

    double WasapiAudioSink::BufferMilliseconds() const noexcept
    {
        const uint32_t rate = SampleRate();
        return (rate > 0) ? 1000.0 * m_impl->BufferFrameCount / rate : 0.0;
    }

    uint32_t WasapiAudioSink::PeriodFrames() const noexcept
    {
        return m_impl->PeriodFrameCount;
    }

    double WasapiAudioSink::PeriodMilliseconds() const noexcept
    {
        const uint32_t rate = SampleRate();
        return (rate > 0) ? 1000.0 * m_impl->PeriodFrameCount / rate : 0.0;
    }

    bool WasapiAudioSink::UsingLowLatencyPath() const noexcept
    {
        return m_impl->LowLatency;
    }

    uint32_t WasapiAudioSink::DefaultPeriodFrames() const noexcept
    {
        return m_impl->DefaultPeriod;
    }

    uint32_t WasapiAudioSink::MinimumPeriodFrames() const noexcept
    {
        return m_impl->MinimumPeriod;
    }

    std::wstring WasapiAudioSink::DeviceName() const
    {
        return m_impl->FriendlyName;
    }

    AudioSinkStats WasapiAudioSink::Stats() const noexcept
    {
        AudioSinkStats stats;
        stats.CallbackCount = m_impl->CallbackCount.load(std::memory_order_relaxed);
        stats.GlitchCount = m_impl->GlitchCount.load(std::memory_order_relaxed);
        stats.LastCallbackMilliseconds = m_impl->LastCallbackMilliseconds.load(std::memory_order_relaxed);
        stats.MaxCallbackMilliseconds = m_impl->MaxCallbackMilliseconds.load(std::memory_order_relaxed);
        stats.WorstLoadFactor = m_impl->WorstLoadFactor.load(std::memory_order_relaxed);

        const uint32_t rate = SampleRate();

        if (rate > 0)
        {
            const uint64_t samples = m_impl->PaddingSamples.load(std::memory_order_relaxed);

            if (samples > 0)
            {
                const double averageFrames =
                    static_cast<double>(m_impl->PaddingSum.load(std::memory_order_relaxed)) / samples;
                stats.AverageQueuedMilliseconds = 1000.0 * averageFrames / rate;
            }

            stats.MaxQueuedMilliseconds =
                1000.0 * m_impl->MaxPadding.load(std::memory_order_relaxed) / rate;

            const uint32_t minimum = m_impl->MinPadding.load(std::memory_order_relaxed);

            if (minimum != UINT32_MAX)
            {
                stats.MinQueuedMilliseconds = 1000.0 * minimum / rate;
            }
        }

        return stats;
    }

    double WasapiAudioSink::StreamLatencyMilliseconds() const noexcept
    {
        if (!m_impl->AudioClient)
        {
            return 0.0;
        }

        REFERENCE_TIME latency = 0;

        if (FAILED(m_impl->AudioClient->GetStreamLatency(&latency)))
        {
            return 0.0;
        }

        return static_cast<double>(latency) / 10000.0;
    }

    bool WasapiAudioSink::DeviceLost() const noexcept
    {
        return m_impl->Lost.load(std::memory_order_relaxed);
    }
}
