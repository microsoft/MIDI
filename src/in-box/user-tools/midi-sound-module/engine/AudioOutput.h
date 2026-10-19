// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace SoundFontSynth
{
    enum class AudioShareMode : uint8_t
    {
        Shared = 0,

        // The device belongs to this app alone while it plays. Lower latency, and nothing else can
        // make a sound through it in the meantime.
        Exclusive = 1,
    };

    enum class AudioOutputStatus : uint8_t
    {
        Ok = 0,
        NoDevice,
        DeviceInUse,
        ExclusiveNotAllowed,
        FormatNotSupported,
        Failed,
    };

    struct AudioDeviceInfo
    {
        std::wstring Id;
        std::wstring Name;
    };

    struct AudioOutputSettings
    {
        // Empty follows the Windows default output, including when the default changes.
        std::wstring DeviceId;

        AudioShareMode ShareMode{ AudioShareMode::Shared };

        // Exclusive mode only. Shared mode uses the smallest period the audio engine offers.
        uint32_t ExclusiveBufferMilliseconds{ 10 };

        static constexpr uint32_t MinimumExclusiveBufferMilliseconds = 2;
        static constexpr uint32_t MaximumExclusiveBufferMilliseconds = 100;
    };

    class IAudioRenderCallback
    {
    public:
        // Called on the audio thread. Must not block or allocate.
        virtual void RenderAudio(_Out_writes_(frameCount * 2) float* interleavedStereo, _In_ uint32_t frameCount) noexcept = 0;

    protected:
        ~IAudioRenderCallback() = default;
    };

    // One WASAPI render stream, shared or exclusive, fed with interleaved stereo float. Open, Start,
    // Stop and Close belong to one thread, which must have a COM apartment.
    class AudioOutput final
    {
    public:
        AudioOutput();
        ~AudioOutput();

        AudioOutput(AudioOutput const&) = delete;
        AudioOutput& operator=(AudioOutput const&) = delete;

        static std::vector<AudioDeviceInfo> EnumerateDevices() noexcept;

        AudioOutputStatus Open(_In_ AudioOutputSettings const& settings) noexcept;
        bool Start(_In_ IAudioRenderCallback* callback) noexcept;
        void Stop() noexcept;
        void Close() noexcept;

        bool IsOpen() const noexcept;
        bool IsRunning() const noexcept;

        // The stream died, or it follows the default output and the default changed, or the
        // chosen device came back while another one stood in for it.
        bool NeedsReopen() const noexcept;

        // Everything below describes the open stream and belongs to the thread that opened it.
        uint32_t SampleRate() const noexcept;
        uint32_t PeriodFrames() const noexcept;
        double PeriodMilliseconds() const noexcept;
        AudioShareMode ShareMode() const noexcept;
        uint16_t ChannelCount() const noexcept;
        uint16_t BitsPerSample() const noexcept;
        bool IsFloat() const noexcept;
        bool UsingFallbackDevice() const noexcept;
        std::wstring DeviceName() const;
        std::wstring DeviceId() const;

        // Any thread.
        uint64_t GlitchCount() const noexcept;

        // Largest block the callback is ever asked for. Bigger device buffers are split.
        static constexpr uint32_t MaximumCallbackFrames = 4096;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
