// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#include "MidiSynth/AudioSampleFormat.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace MidiSynth
{
    _Use_decl_annotations_
    bool DescribeAudioSampleFormat(const WAVEFORMATEX* format, AudioSampleFormat& described) noexcept
    {
        described = {};

        if (format == nullptr || format->nChannels == 0 || format->nSamplesPerSec == 0 || (format->wBitsPerSample % 8) != 0)
        {
            return false;
        }

        WORD tag = format->wFormatTag;

        if (tag == WAVE_FORMAT_EXTENSIBLE)
        {
            if (format->cbSize < sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX))
            {
                return false;
            }

            auto const* const extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);

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
        }

        const auto bytesPerSample = static_cast<uint16_t>(format->wBitsPerSample / 8);

        if (format->nBlockAlign != static_cast<uint32_t>(format->nChannels) * bytesPerSample)
        {
            return false;
        }

        AudioSampleKind kind{ AudioSampleKind::Float32 };

        if (tag == WAVE_FORMAT_IEEE_FLOAT && format->wBitsPerSample == 32)
        {
            kind = AudioSampleKind::Float32;
        }
        else if (tag == WAVE_FORMAT_PCM && format->wBitsPerSample == 16)
        {
            kind = AudioSampleKind::Int16;
        }
        else if (tag == WAVE_FORMAT_PCM && format->wBitsPerSample == 24)
        {
            kind = AudioSampleKind::Int24Packed;
        }
        else if (tag == WAVE_FORMAT_PCM && format->wBitsPerSample == 32)
        {
            kind = AudioSampleKind::Int32;
        }
        else
        {
            return false;
        }

        described.Kind = kind;
        described.Channels = format->nChannels;
        described.BytesPerSample = bytesPerSample;
        described.SampleRate = format->nSamplesPerSec;

        return true;
    }

    _Use_decl_annotations_
    WAVEFORMATEXTENSIBLE MakeExtensibleFormat(
        uint32_t sampleRate,
        uint16_t channels,
        DWORD channelMask,
        uint16_t containerBits,
        uint16_t validBits,
        bool isFloat) noexcept
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

    _Use_decl_annotations_
    void WriteAudioFrames(
        const float* interleavedStereo,
        uint32_t frameCount,
        const AudioSampleFormat& format,
        BYTE* destination) noexcept
    {
        const uint16_t channels = format.Channels;

        for (uint32_t frame = 0; frame < frameCount; frame++)
        {
            const float left = interleavedStereo[static_cast<size_t>(frame) * 2];
            const float right = interleavedStereo[static_cast<size_t>(frame) * 2 + 1];

            for (uint16_t channel = 0; channel < channels; channel++)
            {
                float value = (channels == 1) ? (left + right) * 0.5f
                    : (channel == 0) ? left
                    : (channel == 1) ? right
                    : 0.0f;

                value = (std::clamp)(value, -1.0f, 1.0f);

                const size_t index = static_cast<size_t>(frame) * channels + channel;

                switch (format.Kind)
                {
                case AudioSampleKind::Float32:
                    reinterpret_cast<float*>(destination)[index] = value;
                    break;

                case AudioSampleKind::Int16:
                    reinterpret_cast<int16_t*>(destination)[index] = static_cast<int16_t>(std::lrint(value * 32767.0f));
                    break;

                case AudioSampleKind::Int24Packed:
                {
                    const auto sample = static_cast<int32_t>(std::lrint(static_cast<double>(value) * 8388607.0));
                    BYTE* const bytes = destination + index * 3;

                    bytes[0] = static_cast<BYTE>(sample & 0xFF);
                    bytes[1] = static_cast<BYTE>((sample >> 8) & 0xFF);
                    bytes[2] = static_cast<BYTE>((sample >> 16) & 0xFF);
                    break;
                }

                case AudioSampleKind::Int32:
                default:
                    // A 24-bit device in a 32-bit container reads the top 24 bits, so full scale 32-bit suits both.
                    reinterpret_cast<int32_t*>(destination)[index] =
                        static_cast<int32_t>(std::lrint(static_cast<double>(value) * 2147483647.0));
                    break;
                }
            }
        }
    }
}
