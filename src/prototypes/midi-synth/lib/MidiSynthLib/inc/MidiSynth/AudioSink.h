// WASAPI shared mode render sink.
//
// The endpoint is opened with the mix format it already reports, so no device setting is ever
// modified. Modern mode should render at SampleRate() so the audio engine does no conversion.

#pragma once

#include <sal.h>

#include <cstdint>
#include <memory>
#include <string>

namespace MidiSynth
{
    struct IAudioRenderSource
    {
        virtual ~IAudioRenderSource() = default;

        // Called on the audio render thread. Must not block, allocate, or take a lock.
        virtual void RenderAudio(
            _Out_writes_(frameCount * 2) float* interleavedStereo,
            _In_ uint32_t frameCount) noexcept = 0;
    };

    struct AudioSinkStats
    {
        uint64_t CallbackCount{ 0 };

        // An API call failed or the wait timed out. Note that WASAPI shared mode does not report
        // engine side glitches to the client, so a clean count here is not proof of clean audio.
        uint64_t GlitchCount{ 0 };

        double LastCallbackMilliseconds{ 0.0 };
        double MaxCallbackMilliseconds{ 0.0 };

        // Fraction of the available period spent inside RenderAudio. Above 1.0 cannot keep up.
        double WorstLoadFactor{ 0.0 };

        // How much audio is sitting ahead of the device, which is the latency the player hears.
        // Filling the whole buffer every callback would pin this at the buffer size.
        //
        // The minimum reaching zero is expected, not a fault: the event fires after the engine has
        // consumed a period, so an arrangement tuned for low latency wakes with an empty buffer.
        double AverageQueuedMilliseconds{ 0.0 };
        double MaxQueuedMilliseconds{ 0.0 };
        double MinQueuedMilliseconds{ 0.0 };
    };

    class WasapiAudioSink
    {
    public:
        WasapiAudioSink();
        ~WasapiAudioSink();

        WasapiAudioSink(const WasapiAudioSink&) = delete;
        WasapiAudioSink& operator=(const WasapiAudioSink&) = delete;

        // When lowLatency is set, requests the smallest period the engine supports through
        // IAudioClient3 and falls back to the default period if that is unavailable.
        bool Open(_In_ bool lowLatency);

        bool Start(_In_ IAudioRenderSource* source);
        void Stop();

        uint32_t SampleRate() const noexcept;
        uint32_t BufferFrames() const noexcept;
        double BufferMilliseconds() const noexcept;
        uint32_t PeriodFrames() const noexcept;
        double PeriodMilliseconds() const noexcept;
        bool UsingLowLatencyPath() const noexcept;

        // Range IAudioClient3 reports for shared mode, in frames. Zero when unavailable.
        // A device whose minimum equals its default offers no low latency headroom at all.
        uint32_t DefaultPeriodFrames() const noexcept;
        uint32_t MinimumPeriodFrames() const noexcept;

        // What the audio engine reports for the stream itself, excluding anything we queue.
        double StreamLatencyMilliseconds() const noexcept;

        std::wstring DeviceName() const;
        AudioSinkStats Stats() const noexcept;

        // Set when the endpoint went away, for example the default device changed.
        bool DeviceLost() const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
