// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

// The sample layouts an exclusive mode stream may need, and the conversion from the engine's
// interleaved stereo float into them. Exclusive mode skips the Windows mixer, so the device gets
// exactly the bytes written here, in whatever format it accepted.

#pragma once

#include <windows.h>
#include <mmreg.h>

#include <sal.h>

#include <cstdint>

namespace MidiSynth
{
    enum class AudioSampleKind : uint8_t
    {
        Float32,
        Int16,
        Int24Packed,
        Int32,
    };

    struct AudioSampleFormat
    {
        AudioSampleKind Kind{ AudioSampleKind::Float32 };
        uint16_t Channels{ 0 };
        uint16_t BytesPerSample{ 0 };
        uint32_t SampleRate{ 0 };
    };

    // False for any layout WriteAudioFrames cannot produce.
    _Success_(return)
    bool DescribeAudioSampleFormat(
        _In_opt_ const WAVEFORMATEX* format,
        _Out_ AudioSampleFormat& described) noexcept;

    WAVEFORMATEXTENSIBLE MakeExtensibleFormat(
        _In_ uint32_t sampleRate,
        _In_ uint16_t channels,
        _In_ DWORD channelMask,
        _In_ uint16_t containerBits,
        _In_ uint16_t validBits,
        _In_ bool isFloat) noexcept;

    // Left and right go to the first two channels and the rest stay silent, so a surround device
    // does not get the whole mix on every speaker. A mono device gets both, halved. Every value is
    // clamped to full scale, because nothing mixes or limits after an exclusive stream.
    void WriteAudioFrames(
        _In_reads_(frameCount * 2) const float* interleavedStereo,
        _In_ uint32_t frameCount,
        _In_ const AudioSampleFormat& format,
        _Out_writes_bytes_(frameCount * format.Channels * format.BytesPerSample) BYTE* destination) noexcept;
}
