// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

// Defines the property keys and format GUIDs used here, in this file only.
#include <initguid.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <audioclient.h>
#include <avrt.h>
#include <mmreg.h>
#include <ksmedia.h>
#include <wrl/client.h>

#include <thread>

#include "AudioOutput.h"

#pragma comment(lib, "avrt.lib")

using Microsoft::WRL::ComPtr;

namespace SoundFontSynth
{
    namespace
    {
        enum class SampleKind : uint8_t
        {
            Float32,
            Int16,
            Int24Packed,
            Int32,
        };

        struct OutputFormat
        {
            SampleKind Kind{ SampleKind::Float32 };
            uint16_t Channels{ 0 };
            uint16_t BytesPerSample{ 0 };
            uint16_t ValidBits{ 0 };
            uint32_t SampleRate{ 0 };
        };

        _Success_(return)
        bool DescribeFormat(_In_opt_ WAVEFORMATEX const* format, _Out_ OutputFormat& described) noexcept
        {
            described = {};

            if (format == nullptr || format->nChannels == 0 || format->nSamplesPerSec == 0 || (format->wBitsPerSample % 8) != 0)
            {
                return false;
            }

            auto tag = format->wFormatTag;
            WORD validBits = format->wBitsPerSample;

            if (tag == WAVE_FORMAT_EXTENSIBLE)
            {
                if (format->cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX))
                {
                    return false;
                }

                auto const* const extensible = reinterpret_cast<WAVEFORMATEXTENSIBLE const*>(format);

                if (extensible->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
                {
                    tag = WAVE_FORMAT_IEEE_FLOAT;
                }
                else if (extensible->SubFormat == KSDATAFORMAT_SUBTYPE_PCM)
                {
                    tag = WAVE_FORMAT_PCM;
                }
                else
                {
                    return false;
                }

                if (extensible->Samples.wValidBitsPerSample != 0)
                {
                    validBits = extensible->Samples.wValidBitsPerSample;
                }
            }

            described.Channels = format->nChannels;
            described.SampleRate = format->nSamplesPerSec;
            described.BytesPerSample = static_cast<uint16_t>(format->wBitsPerSample / 8);
            described.ValidBits = validBits;

            if (format->nBlockAlign != described.Channels * described.BytesPerSample)
            {
                return false;
            }

            if (tag == WAVE_FORMAT_IEEE_FLOAT && format->wBitsPerSample == 32)
            {
                described.Kind = SampleKind::Float32;
            }
            else if (tag == WAVE_FORMAT_PCM && format->wBitsPerSample == 16)
            {
                described.Kind = SampleKind::Int16;
            }
            else if (tag == WAVE_FORMAT_PCM && format->wBitsPerSample == 24)
            {
                described.Kind = SampleKind::Int24Packed;
            }
            else if (tag == WAVE_FORMAT_PCM && format->wBitsPerSample == 32)
            {
                described.Kind = SampleKind::Int32;
            }
            else
            {
                return false;
            }

            return true;
        }

        WAVEFORMATEXTENSIBLE MakeFormat(
            _In_ uint32_t sampleRate,
            _In_ uint16_t channels,
            _In_ DWORD channelMask,
            _In_ uint16_t containerBits,
            _In_ uint16_t validBits,
            _In_ bool isFloat) noexcept
        {
            WAVEFORMATEXTENSIBLE format{};

            format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
            format.Format.nChannels = channels;
            format.Format.nSamplesPerSec = sampleRate;
            format.Format.wBitsPerSample = containerBits;
            format.Format.nBlockAlign = static_cast<WORD>(channels * containerBits / 8);
            format.Format.nAvgBytesPerSec = sampleRate * format.Format.nBlockAlign;
            format.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
            format.Samples.wValidBitsPerSample = validBits;
            format.dwChannelMask = channelMask;
            format.SubFormat = isFloat ? KSDATAFORMAT_SUBTYPE_IEEE_FLOAT : KSDATAFORMAT_SUBTYPE_PCM;

            return format;
        }

        // Left and right go to the first two channels and the rest stay silent, so a surround
        // device does not get the whole mix on every speaker. A mono device gets both, halved.
        inline float ChannelValue(_In_ float left, _In_ float right, _In_ uint16_t channel, _In_ uint16_t channels) noexcept
        {
            if (channels == 1)
            {
                return (left + right) * 0.5f;
            }

            return (channel == 0) ? left : (channel == 1) ? right : 0.0f;
        }

        void WriteFrames(
            _In_reads_(frames * 2) float const* stereo,
            _In_ uint32_t frames,
            _In_ OutputFormat const& format,
            _Out_writes_bytes_(frames * format.Channels * format.BytesPerSample) BYTE* destination) noexcept
        {
            auto const channels = format.Channels;

            for (uint32_t frame = 0; frame < frames; frame++)
            {
                auto const left = stereo[static_cast<size_t>(frame) * 2];
                auto const right = stereo[static_cast<size_t>(frame) * 2 + 1];

                for (uint16_t channel = 0; channel < channels; channel++)
                {
                    auto const value = (std::clamp)(ChannelValue(left, right, channel, channels), -1.0f, 1.0f);
                    auto const index = static_cast<size_t>(frame) * channels + channel;

                    switch (format.Kind)
                    {
                    case SampleKind::Float32:
                        reinterpret_cast<float*>(destination)[index] = value;
                        break;

                    case SampleKind::Int16:
                        reinterpret_cast<int16_t*>(destination)[index] = static_cast<int16_t>(lrintf(value * 32767.0f));
                        break;

                    case SampleKind::Int24Packed:
                    {
                        auto const sample = static_cast<int32_t>(lrint(static_cast<double>(value) * 8388607.0));
                        auto* const bytes = destination + index * 3;

                        bytes[0] = static_cast<BYTE>(sample & 0xFF);
                        bytes[1] = static_cast<BYTE>((sample >> 8) & 0xFF);
                        bytes[2] = static_cast<BYTE>((sample >> 16) & 0xFF);
                        break;
                    }

                    case SampleKind::Int32:
                    default:
                        // A 24-bit device in a 32-bit container reads the top 24 bits, so a full
                        // scale 32-bit value suits both.
                        reinterpret_cast<int32_t*>(destination)[index] =
                            static_cast<int32_t>(lrint(static_cast<double>(value) * 2147483647.0));
                        break;
                    }
                }
            }
        }

        AudioOutputStatus StatusFromResult(_In_ HRESULT result) noexcept
        {
            switch (result)
            {
            case S_OK:
                return AudioOutputStatus::Ok;

            case AUDCLNT_E_DEVICE_IN_USE:
                return AudioOutputStatus::DeviceInUse;

            case AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED:
                return AudioOutputStatus::ExclusiveNotAllowed;

            case AUDCLNT_E_UNSUPPORTED_FORMAT:
                return AudioOutputStatus::FormatNotSupported;

            case AUDCLNT_E_DEVICE_INVALIDATED:
            case E_NOTFOUND:
                return AudioOutputStatus::NoDevice;

            default:
                return AudioOutputStatus::Failed;
            }
        }

        std::wstring ReadFriendlyName(_In_ IMMDevice* device)
        {
            std::wstring name;

            ComPtr<IPropertyStore> properties;

            if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &properties)))
            {
                PROPVARIANT value;
                PropVariantInit(&value);

                if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value)) && value.vt == VT_LPWSTR && value.pwszVal != nullptr)
                {
                    name = value.pwszVal;
                }

                (void)PropVariantClear(&value);
            }

            return name;
        }

        std::wstring ReadDeviceId(_In_ IMMDevice* device)
        {
            std::wstring id;
            LPWSTR raw{ nullptr };

            if (SUCCEEDED(device->GetId(&raw)) && raw != nullptr)
            {
                id = raw;
            }

            if (raw != nullptr)
            {
                CoTaskMemFree(raw);
            }

            return id;
        }

        // Tells the stream when the device it should be using has changed under it. Called on a
        // system thread; everything it reads is fixed at construction.
        class DeviceChangeListener final : public IMMNotificationClient
        {
        public:
            DeviceChangeListener(_In_ bool followDefault, _In_ std::wstring const& awaitedDeviceId) :
                m_followDefault(followDefault),
                m_awaitedDeviceId(awaitedDeviceId)
            {
            }

            bool Changed() const noexcept { return m_changed.load(std::memory_order_acquire); }

            ULONG STDMETHODCALLTYPE AddRef() override
            {
                return static_cast<ULONG>(InterlockedIncrement(&m_references));
            }

            ULONG STDMETHODCALLTYPE Release() override
            {
                auto const remaining = InterlockedDecrement(&m_references);

                if (remaining == 0)
                {
                    delete this;
                }

                return static_cast<ULONG>(remaining);
            }

            HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID iid, _COM_Outptr_ void** object) override
            {
                if (object == nullptr)
                {
                    return E_POINTER;
                }

                if (iid == __uuidof(IUnknown) || iid == __uuidof(IMMNotificationClient))
                {
                    *object = static_cast<IMMNotificationClient*>(this);
                    AddRef();
                    return S_OK;
                }

                *object = nullptr;
                return E_NOINTERFACE;
            }

            HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(_In_ EDataFlow flow, _In_ ERole role, _In_opt_ LPCWSTR) override
            {
                if (m_followDefault && flow == eRender && role == eConsole)
                {
                    m_changed.store(true, std::memory_order_release);
                }

                return S_OK;
            }

            HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(_In_opt_ LPCWSTR deviceId, _In_ DWORD newState) override
            {
                if (!m_awaitedDeviceId.empty() && newState == DEVICE_STATE_ACTIVE &&
                    deviceId != nullptr && _wcsicmp(deviceId, m_awaitedDeviceId.c_str()) == 0)
                {
                    m_changed.store(true, std::memory_order_release);
                }

                return S_OK;
            }

            HRESULT STDMETHODCALLTYPE OnDeviceAdded(_In_opt_ LPCWSTR) override { return S_OK; }
            HRESULT STDMETHODCALLTYPE OnDeviceRemoved(_In_opt_ LPCWSTR) override { return S_OK; }
            HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(_In_opt_ LPCWSTR, _In_ PROPERTYKEY const) override { return S_OK; }

        private:
            ~DeviceChangeListener() = default;

            LONG m_references{ 1 };
            bool const m_followDefault;
            std::wstring const m_awaitedDeviceId;
            std::atomic<bool> m_changed{ false };
        };

        struct EventHandle
        {
            HANDLE Value{ nullptr };

            ~EventHandle() { Reset(); }

            void Reset() noexcept
            {
                if (Value != nullptr)
                {
                    CloseHandle(Value);
                    Value = nullptr;
                }
            }
        };
    }

    struct AudioOutput::Impl
    {
        ComPtr<IMMDeviceEnumerator> Enumerator;
        ComPtr<IMMDevice> Device;
        ComPtr<IAudioClient> Client;
        ComPtr<IAudioRenderClient> RenderClient;

        DeviceChangeListener* Listener{ nullptr };

        EventHandle BufferEvent;

        OutputFormat Format{};
        AudioShareMode Mode{ AudioShareMode::Shared };
        uint32_t BufferFrames{ 0 };
        uint32_t PeriodFrames{ 0 };
        bool UsingFallback{ false };

        std::wstring Name;
        std::wstring Id;

        std::thread RenderThread;
        std::atomic<bool> StopRequested{ false };
        std::atomic<bool> Lost{ false };
        std::atomic<uint64_t> Glitches{ 0 };

        IAudioRenderCallback* Callback{ nullptr };
        std::vector<float> Staging;

        ~Impl()
        {
            ReleaseListener();
        }

        void ReleaseListener() noexcept
        {
            if (Listener != nullptr)
            {
                if (Enumerator)
                {
                    (void)Enumerator->UnregisterEndpointNotificationCallback(Listener);
                }

                Listener->Release();
                Listener = nullptr;
            }
        }

        HRESULT OpenShared() noexcept;
        HRESULT OpenExclusive(_In_ uint32_t bufferMilliseconds) noexcept;
        HRESULT FillFrames(_In_ uint32_t frames) noexcept;
        void RenderLoop() noexcept;
    };

    HRESULT AudioOutput::Impl::OpenShared() noexcept
    {
        WAVEFORMATEX* mixFormat{ nullptr };

        auto hr = Client->GetMixFormat(&mixFormat);

        if (FAILED(hr) || mixFormat == nullptr)
        {
            return FAILED(hr) ? hr : E_FAIL;
        }

        if (!DescribeFormat(mixFormat, Format))
        {
            CoTaskMemFree(mixFormat);
            return AUDCLNT_E_UNSUPPORTED_FORMAT;
        }

        bool initialized{ false };

        // The smallest period the engine offers, where the driver supports one below the default.
        ComPtr<IAudioClient3> client3;

        if (SUCCEEDED(Client.As(&client3)))
        {
            UINT32 defaultPeriod{ 0 };
            UINT32 fundamentalPeriod{ 0 };
            UINT32 minimumPeriod{ 0 };
            UINT32 maximumPeriod{ 0 };

            if (SUCCEEDED(client3->GetSharedModeEnginePeriod(mixFormat, &defaultPeriod, &fundamentalPeriod, &minimumPeriod, &maximumPeriod)) &&
                minimumPeriod > 0 &&
                SUCCEEDED(client3->InitializeSharedAudioStream(AUDCLNT_STREAMFLAGS_EVENTCALLBACK, minimumPeriod, mixFormat, nullptr)))
            {
                PeriodFrames = minimumPeriod;
                initialized = true;
            }
        }

        if (!initialized)
        {
            hr = Client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, mixFormat, nullptr);

            if (FAILED(hr))
            {
                CoTaskMemFree(mixFormat);
                return hr;
            }

            REFERENCE_TIME devicePeriod{ 0 };

            if (SUCCEEDED(Client->GetDevicePeriod(&devicePeriod, nullptr)))
            {
                PeriodFrames = static_cast<uint32_t>(devicePeriod * static_cast<REFERENCE_TIME>(mixFormat->nSamplesPerSec) / 10000000);
            }
        }

        CoTaskMemFree(mixFormat);

        return S_OK;
    }

    _Use_decl_annotations_
    HRESULT AudioOutput::Impl::OpenExclusive(uint32_t bufferMilliseconds) noexcept
    {
        WAVEFORMATEX* mixFormat{ nullptr };

        auto hr = Client->GetMixFormat(&mixFormat);

        if (FAILED(hr) || mixFormat == nullptr)
        {
            return FAILED(hr) ? hr : E_FAIL;
        }

        auto const mixRate = mixFormat->nSamplesPerSec;
        auto const mixChannels = mixFormat->nChannels;

        DWORD mixMask{ 0 };

        if (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE && mixFormat->cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX))
        {
            mixMask = reinterpret_cast<WAVEFORMATEXTENSIBLE const*>(mixFormat)->dwChannelMask;
        }

        CoTaskMemFree(mixFormat);

        struct Candidate
        {
            uint16_t ContainerBits;
            uint16_t ValidBits;
            bool IsFloat;
        };

        // Best first. Exclusive mode skips the Windows mixer, so the format has to be one the
        // driver takes as is.
        constexpr Candidate Candidates[]
        {
            { 32, 32, true },
            { 32, 32, false },
            { 32, 24, false },
            { 24, 24, false },
            { 16, 16, false },
        };

        uint32_t const rates[]{ mixRate, 48000, 44100, 96000 };

        WAVEFORMATEXTENSIBLE chosen{};
        bool found{ false };
        HRESULT lastRefusal{ AUDCLNT_E_UNSUPPORTED_FORMAT };

        for (auto const rate : rates)
        {
            for (int pass = 0; pass < 2 && !found; pass++)
            {
                // Stereo first. Some devices only take their full channel count exclusively.
                uint16_t const channels = (pass == 0) ? 2 : mixChannels;
                DWORD const mask = (pass == 0) ? (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT) : mixMask;

                if (pass == 1 && (mixChannels == 2 || mixChannels == 0))
                {
                    continue;
                }

                for (auto const& candidate : Candidates)
                {
                    auto const format = MakeFormat(rate, channels, mask, candidate.ContainerBits, candidate.ValidBits, candidate.IsFloat);
                    auto const supported = Client->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &format.Format, nullptr);

                    if (supported == S_OK)
                    {
                        chosen = format;
                        found = true;
                        break;
                    }

                    // These two say nothing about the format, and no other format will fare better.
                    if (supported == AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED || supported == AUDCLNT_E_DEVICE_IN_USE)
                    {
                        return supported;
                    }

                    lastRefusal = (supported == S_FALSE) ? AUDCLNT_E_UNSUPPORTED_FORMAT : supported;
                }
            }

            if (found)
            {
                break;
            }
        }

        if (!found || !DescribeFormat(&chosen.Format, Format))
        {
            return lastRefusal;
        }

        REFERENCE_TIME defaultPeriod{ 0 };
        REFERENCE_TIME minimumPeriod{ 0 };

        hr = Client->GetDevicePeriod(&defaultPeriod, &minimumPeriod);

        if (FAILED(hr))
        {
            return hr;
        }

        auto const clampedMilliseconds = (std::clamp)(
            bufferMilliseconds,
            AudioOutputSettings::MinimumExclusiveBufferMilliseconds,
            AudioOutputSettings::MaximumExclusiveBufferMilliseconds);

        auto period = (std::max)(minimumPeriod, static_cast<REFERENCE_TIME>(clampedMilliseconds) * 10000);

        hr = Client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, period, period, &chosen.Format, nullptr);

        if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
        {
            // The driver wants a whole number of its own blocks. It says how many frames that is,
            // and the stream has to be created again from scratch with the matching period.
            UINT32 alignedFrames{ 0 };

            hr = Client->GetBufferSize(&alignedFrames);

            if (FAILED(hr) || alignedFrames == 0)
            {
                return FAILED(hr) ? hr : E_FAIL;
            }

            period = static_cast<REFERENCE_TIME>(10000000.0 * alignedFrames / Format.SampleRate + 0.5);

            Client.Reset();

            hr = Device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(Client.GetAddressOf()));

            if (FAILED(hr))
            {
                return hr;
            }

            hr = Client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, period, period, &chosen.Format, nullptr);
        }

        return hr;
    }

    _Use_decl_annotations_
    HRESULT AudioOutput::Impl::FillFrames(uint32_t frames) noexcept
    {
        BYTE* buffer{ nullptr };

        auto hr = RenderClient->GetBuffer(frames, &buffer);

        if (FAILED(hr))
        {
            return hr;
        }

        auto const frameBytes = static_cast<size_t>(Format.Channels) * Format.BytesPerSample;
        uint32_t done{ 0 };

        while (done < frames)
        {
            auto const chunk = (std::min)(frames - done, MaximumCallbackFrames);

            if (Callback != nullptr)
            {
                Callback->RenderAudio(Staging.data(), chunk);
            }
            else
            {
                std::fill_n(Staging.begin(), static_cast<size_t>(chunk) * 2, 0.0f);
            }

            WriteFrames(Staging.data(), chunk, Format, buffer + static_cast<size_t>(done) * frameBytes);

            done += chunk;
        }

        return RenderClient->ReleaseBuffer(frames, 0);
    }

    void AudioOutput::Impl::RenderLoop() noexcept
    {
        auto const comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        (void)SetThreadDescription(GetCurrentThread(), L"SoundFont Synth Audio");

        DWORD taskIndex{ 0 };
        auto const mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

        while (!StopRequested.load(std::memory_order_acquire))
        {
            auto const waitResult = WaitForSingleObject(BufferEvent.Value, 2000);

            if (StopRequested.load(std::memory_order_acquire))
            {
                break;
            }

            if (waitResult != WAIT_OBJECT_0)
            {
                Glitches.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            uint32_t frames{ 0 };

            if (Mode == AudioShareMode::Exclusive)
            {
                // Event driven exclusive mode hands over the whole buffer every time.
                frames = BufferFrames;
            }
            else
            {
                UINT32 padding{ 0 };
                auto const hr = Client->GetCurrentPadding(&padding);

                if (hr == AUDCLNT_E_DEVICE_INVALIDATED)
                {
                    Lost.store(true, std::memory_order_release);
                    break;
                }

                if (FAILED(hr))
                {
                    Glitches.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }

                // One period at a time. Keeping the buffer full would keep a whole buffer of
                // latency queued in front of the player for nothing.
                frames = (std::min)(BufferFrames - (std::min)(padding, BufferFrames), PeriodFrames);
            }

            if (frames == 0)
            {
                continue;
            }

            auto const hr = FillFrames(frames);

            if (hr == AUDCLNT_E_DEVICE_INVALIDATED)
            {
                Lost.store(true, std::memory_order_release);
                break;
            }

            if (FAILED(hr))
            {
                Glitches.fetch_add(1, std::memory_order_relaxed);
            }
        }

        if (mmcss != nullptr)
        {
            (void)AvRevertMmThreadCharacteristics(mmcss);
        }

        if (SUCCEEDED(comResult))
        {
            CoUninitialize();
        }
    }

    AudioOutput::AudioOutput() :
        m_impl(std::make_unique<Impl>())
    {
    }

    AudioOutput::~AudioOutput()
    {
        Close();
    }

    std::vector<AudioDeviceInfo> AudioOutput::EnumerateDevices() noexcept
    {
        std::vector<AudioDeviceInfo> devices;

        try
        {
            ComPtr<IMMDeviceEnumerator> enumerator;

            if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator))))
            {
                return devices;
            }

            ComPtr<IMMDeviceCollection> collection;

            if (FAILED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection)))
            {
                return devices;
            }

            UINT count{ 0 };

            if (FAILED(collection->GetCount(&count)))
            {
                return devices;
            }

            for (UINT i = 0; i < count; i++)
            {
                ComPtr<IMMDevice> device;

                if (FAILED(collection->Item(i, &device)))
                {
                    continue;
                }

                AudioDeviceInfo info{};
                info.Id = ReadDeviceId(device.Get());
                info.Name = ReadFriendlyName(device.Get());

                if (!info.Id.empty())
                {
                    devices.push_back(std::move(info));
                }
            }
        }
        catch (...)
        {
            devices.clear();
        }

        return devices;
    }

    _Use_decl_annotations_
    AudioOutputStatus AudioOutput::Open(AudioOutputSettings const& settings) noexcept
    {
        Close();

        auto& impl = *m_impl;

        try
        {
            auto hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&impl.Enumerator));

            if (FAILED(hr))
            {
                Close();
                return AudioOutputStatus::Failed;
            }

            if (!settings.DeviceId.empty())
            {
                DWORD state{ 0 };

                if (FAILED(impl.Enumerator->GetDevice(settings.DeviceId.c_str(), &impl.Device)) ||
                    FAILED(impl.Device->GetState(&state)) ||
                    state != DEVICE_STATE_ACTIVE)
                {
                    // Unplugged or disabled. The default stands in until it comes back.
                    impl.Device.Reset();
                    impl.UsingFallback = true;
                }
            }

            if (!impl.Device && FAILED(impl.Enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &impl.Device)))
            {
                Close();
                return AudioOutputStatus::NoDevice;
            }

            impl.Name = ReadFriendlyName(impl.Device.Get());
            impl.Id = ReadDeviceId(impl.Device.Get());

            impl.Listener = new (std::nothrow) DeviceChangeListener(
                settings.DeviceId.empty() || impl.UsingFallback,
                impl.UsingFallback ? settings.DeviceId : std::wstring{});

            if (impl.Listener != nullptr && FAILED(impl.Enumerator->RegisterEndpointNotificationCallback(impl.Listener)))
            {
                impl.Listener->Release();
                impl.Listener = nullptr;
            }

            hr = impl.Device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(impl.Client.GetAddressOf()));

            if (SUCCEEDED(hr))
            {
                impl.Mode = settings.ShareMode;

                hr = (settings.ShareMode == AudioShareMode::Exclusive)
                    ? impl.OpenExclusive(settings.ExclusiveBufferMilliseconds)
                    : impl.OpenShared();
            }

            if (FAILED(hr))
            {
                Close();
                return StatusFromResult(hr);
            }

            impl.BufferEvent.Value = CreateEventW(nullptr, FALSE, FALSE, nullptr);

            UINT32 bufferFrames{ 0 };

            if (impl.BufferEvent.Value == nullptr ||
                FAILED(impl.Client->SetEventHandle(impl.BufferEvent.Value)) ||
                FAILED(impl.Client->GetBufferSize(&bufferFrames)) ||
                bufferFrames == 0 ||
                FAILED(impl.Client->GetService(IID_PPV_ARGS(&impl.RenderClient))))
            {
                Close();
                return AudioOutputStatus::Failed;
            }

            impl.BufferFrames = bufferFrames;

            if (impl.Mode == AudioShareMode::Exclusive || impl.PeriodFrames == 0 || impl.PeriodFrames > bufferFrames)
            {
                impl.PeriodFrames = bufferFrames;
            }

            // Sized once so the render thread never allocates.
            impl.Staging.assign(static_cast<size_t>(MaximumCallbackFrames) * 2, 0.0f);

            return AudioOutputStatus::Ok;
        }
        catch (...)
        {
            Close();
            return AudioOutputStatus::Failed;
        }
    }

    _Use_decl_annotations_
    bool AudioOutput::Start(IAudioRenderCallback* callback) noexcept
    {
        auto& impl = *m_impl;

        if (!impl.Client || !impl.RenderClient || impl.RenderThread.joinable())
        {
            return false;
        }

        impl.Callback = callback;
        impl.StopRequested.store(false, std::memory_order_release);
        impl.Lost.store(false, std::memory_order_release);

        // Silence ahead of the first event: the whole buffer in exclusive mode, which the device
        // expects to find full, and one period in shared mode, which keeps latency down.
        auto const preroll = (impl.Mode == AudioShareMode::Exclusive) ? impl.BufferFrames : impl.PeriodFrames;
        BYTE* buffer{ nullptr };

        if (SUCCEEDED(impl.RenderClient->GetBuffer(preroll, &buffer)))
        {
            (void)impl.RenderClient->ReleaseBuffer(preroll, AUDCLNT_BUFFERFLAGS_SILENT);
        }

        if (FAILED(impl.Client->Start()))
        {
            impl.Callback = nullptr;
            return false;
        }

        try
        {
            impl.RenderThread = std::thread([&impl] { impl.RenderLoop(); });
        }
        catch (...)
        {
            (void)impl.Client->Stop();
            impl.Callback = nullptr;
            return false;
        }

        return true;
    }

    void AudioOutput::Stop() noexcept
    {
        auto& impl = *m_impl;

        impl.StopRequested.store(true, std::memory_order_release);

        if (impl.BufferEvent.Value != nullptr)
        {
            (void)SetEvent(impl.BufferEvent.Value);
        }

        if (impl.RenderThread.joinable())
        {
            impl.RenderThread.join();
        }

        if (impl.Client)
        {
            (void)impl.Client->Stop();
            (void)impl.Client->Reset();
        }

        impl.Callback = nullptr;
    }

    void AudioOutput::Close() noexcept
    {
        Stop();

        auto& impl = *m_impl;

        impl.ReleaseListener();
        impl.RenderClient.Reset();
        impl.Client.Reset();
        impl.Device.Reset();
        impl.Enumerator.Reset();
        impl.BufferEvent.Reset();

        impl.Format = {};
        impl.BufferFrames = 0;
        impl.PeriodFrames = 0;
        impl.UsingFallback = false;
        impl.Name.clear();
        impl.Id.clear();
        impl.Lost.store(false, std::memory_order_release);
    }

    bool AudioOutput::IsOpen() const noexcept
    {
        return static_cast<bool>(m_impl->RenderClient);
    }

    bool AudioOutput::IsRunning() const noexcept
    {
        return m_impl->RenderThread.joinable();
    }

    bool AudioOutput::NeedsReopen() const noexcept
    {
        return m_impl->Lost.load(std::memory_order_acquire) ||
            (m_impl->Listener != nullptr && m_impl->Listener->Changed());
    }

    uint32_t AudioOutput::SampleRate() const noexcept
    {
        return m_impl->Format.SampleRate;
    }

    uint32_t AudioOutput::PeriodFrames() const noexcept
    {
        return m_impl->PeriodFrames;
    }

    double AudioOutput::PeriodMilliseconds() const noexcept
    {
        return (m_impl->Format.SampleRate > 0)
            ? 1000.0 * m_impl->PeriodFrames / m_impl->Format.SampleRate
            : 0.0;
    }

    AudioShareMode AudioOutput::ShareMode() const noexcept
    {
        return m_impl->Mode;
    }

    uint16_t AudioOutput::ChannelCount() const noexcept
    {
        return m_impl->Format.Channels;
    }

    uint16_t AudioOutput::BitsPerSample() const noexcept
    {
        return m_impl->Format.ValidBits;
    }

    bool AudioOutput::IsFloat() const noexcept
    {
        return m_impl->Format.Kind == SampleKind::Float32;
    }

    bool AudioOutput::UsingFallbackDevice() const noexcept
    {
        return m_impl->UsingFallback;
    }

    std::wstring AudioOutput::DeviceName() const
    {
        return m_impl->Name;
    }

    std::wstring AudioOutput::DeviceId() const
    {
        return m_impl->Id;
    }

    uint64_t AudioOutput::GlitchCount() const noexcept
    {
        return m_impl->Glitches.load(std::memory_order_relaxed);
    }
}
