// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

#include "MidiSynthAudioFormatTests.h"

#include <MidiSynth/AudioSampleFormat.h>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace MidiSynth;

namespace
{
    WAVEFORMATEX PlainFormat(_In_ WORD tag, _In_ uint16_t channels, _In_ uint16_t bits)
    {
        WAVEFORMATEX format{};
        format.wFormatTag = tag;
        format.nChannels = channels;
        format.nSamplesPerSec = 96000;
        format.wBitsPerSample = bits;
        format.nBlockAlign = static_cast<WORD>(channels * bits / 8);
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
        return format;
    }

    // Left, right per frame. The third frame is outside full scale and must be clamped.
    constexpr float Stereo[]
    {
        1.0f, -1.0f,
        0.5f, -0.5f,
        1.5f, -2.0f,
        0.25f, 0.75f,
    };

    constexpr uint32_t StereoFrames = 4;
}

void MidiSynthAudioFormatTests::TestFormatsThatCanBeWritten()
{
    AudioSampleFormat described{};

    auto floatFormat = MakeExtensibleFormat(96000, 2, SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT, 32, 32, true);
    VERIFY_IS_TRUE(DescribeAudioSampleFormat(&floatFormat.Format, described));
    VERIFY_IS_TRUE(described.Kind == AudioSampleKind::Float32);
    VERIFY_ARE_EQUAL(static_cast<uint16_t>(2), described.Channels);
    VERIFY_ARE_EQUAL(static_cast<uint16_t>(4), described.BytesPerSample);
    VERIFY_ARE_EQUAL(static_cast<uint32_t>(96000), described.SampleRate);

    auto int32Format = MakeExtensibleFormat(48000, 2, 0, 32, 32, false);
    VERIFY_IS_TRUE(DescribeAudioSampleFormat(&int32Format.Format, described));
    VERIFY_IS_TRUE(described.Kind == AudioSampleKind::Int32);

    auto int24In32Format = MakeExtensibleFormat(48000, 2, 0, 32, 24, false);
    VERIFY_IS_TRUE(DescribeAudioSampleFormat(&int24In32Format.Format, described));
    VERIFY_IS_TRUE(described.Kind == AudioSampleKind::Int32, L"24 valid bits in a 32-bit container are written as 32-bit");

    auto packedFormat = MakeExtensibleFormat(44100, 8, 0, 24, 24, false);
    VERIFY_IS_TRUE(DescribeAudioSampleFormat(&packedFormat.Format, described));
    VERIFY_IS_TRUE(described.Kind == AudioSampleKind::Int24Packed);
    VERIFY_ARE_EQUAL(static_cast<uint16_t>(8), described.Channels);
    VERIFY_ARE_EQUAL(static_cast<uint16_t>(3), described.BytesPerSample);

    auto int16Format = MakeExtensibleFormat(44100, 1, SPEAKER_FRONT_CENTER, 16, 16, false);
    VERIFY_IS_TRUE(DescribeAudioSampleFormat(&int16Format.Format, described));
    VERIFY_IS_TRUE(described.Kind == AudioSampleKind::Int16);

    auto plainFloat = PlainFormat(WAVE_FORMAT_IEEE_FLOAT, 2, 32);
    VERIFY_IS_TRUE(DescribeAudioSampleFormat(&plainFloat, described));
    VERIFY_IS_TRUE(described.Kind == AudioSampleKind::Float32);

    auto plainPcm = PlainFormat(WAVE_FORMAT_PCM, 2, 16);
    VERIFY_IS_TRUE(DescribeAudioSampleFormat(&plainPcm, described));
    VERIFY_IS_TRUE(described.Kind == AudioSampleKind::Int16);
}

void MidiSynthAudioFormatTests::TestFormatsThatCannotBeWritten()
{
    AudioSampleFormat described{};

    VERIFY_IS_FALSE(DescribeAudioSampleFormat(nullptr, described));

    auto eightBit = PlainFormat(WAVE_FORMAT_PCM, 2, 8);
    VERIFY_IS_FALSE(DescribeAudioSampleFormat(&eightBit, described));

    auto doubleFloat = PlainFormat(WAVE_FORMAT_IEEE_FLOAT, 2, 64);
    VERIFY_IS_FALSE(DescribeAudioSampleFormat(&doubleFloat, described));

    auto noChannels = PlainFormat(WAVE_FORMAT_PCM, 0, 16);
    VERIFY_IS_FALSE(DescribeAudioSampleFormat(&noChannels, described));

    auto badAlign = PlainFormat(WAVE_FORMAT_PCM, 2, 16);
    badAlign.nBlockAlign = 6;
    VERIFY_IS_FALSE(DescribeAudioSampleFormat(&badAlign, described), L"a block size that disagrees with the sample size");

    auto otherSubFormat = MakeExtensibleFormat(48000, 2, 0, 16, 16, false);
    otherSubFormat.SubFormat = GUID{};
    VERIFY_IS_FALSE(DescribeAudioSampleFormat(&otherSubFormat.Format, described));

    auto shortExtensible = MakeExtensibleFormat(48000, 2, 0, 16, 16, false);
    shortExtensible.Format.cbSize = 0;
    VERIFY_IS_FALSE(DescribeAudioSampleFormat(&shortExtensible.Format, described), L"extensible tag without the extensible fields");
}

void MidiSynthAudioFormatTests::TestStereoBytesForEveryKind()
{
    // Expected values worked out by hand: full scale times the value, rounded half to even, after
    // clamping to -1..1. Two's complement little endian for the integer kinds.
    {
        float output[StereoFrames * 2]{};
        WriteAudioFrames(Stereo, StereoFrames, { AudioSampleKind::Float32, 2, 4, 96000 }, reinterpret_cast<BYTE*>(output));

        const float expected[]{ 1.0f, -1.0f, 0.5f, -0.5f, 1.0f, -1.0f, 0.25f, 0.75f };

        for (size_t i = 0; i < std::size(expected); i++)
        {
            VERIFY_ARE_EQUAL(expected[i], output[i]);
        }
    }

    {
        int16_t output[StereoFrames * 2]{};
        WriteAudioFrames(Stereo, StereoFrames, { AudioSampleKind::Int16, 2, 2, 96000 }, reinterpret_cast<BYTE*>(output));

        const int16_t expected[]{ 32767, -32767, 16384, -16384, 32767, -32767, 8192, 24575 };

        for (size_t i = 0; i < std::size(expected); i++)
        {
            VERIFY_ARE_EQUAL(expected[i], output[i]);
        }
    }

    {
        uint8_t output[StereoFrames * 2 * 3]{};
        WriteAudioFrames(Stereo, StereoFrames, { AudioSampleKind::Int24Packed, 2, 3, 96000 }, output);

        const uint8_t expected[]
        {
            0xFF, 0xFF, 0x7F,   0x01, 0x00, 0x80,
            0x00, 0x00, 0x40,   0x00, 0x00, 0xC0,
            0xFF, 0xFF, 0x7F,   0x01, 0x00, 0x80,
            0x00, 0x00, 0x20,   0xFF, 0xFF, 0x5F,
        };

        for (size_t i = 0; i < std::size(expected); i++)
        {
            VERIFY_ARE_EQUAL(expected[i], output[i], String().Format(L"packed 24-bit byte %zu", i));
        }
    }

    {
        int32_t output[StereoFrames * 2]{};
        WriteAudioFrames(Stereo, StereoFrames, { AudioSampleKind::Int32, 2, 4, 96000 }, reinterpret_cast<BYTE*>(output));

        const int32_t expected[]
        {
            2147483647, -2147483647,
            1073741824, -1073741824,
            2147483647, -2147483647,
            536870912, 1610612735,
        };

        for (size_t i = 0; i < std::size(expected); i++)
        {
            VERIFY_ARE_EQUAL(expected[i], output[i]);
        }
    }
}

void MidiSynthAudioFormatTests::TestExtraChannelsAreSilentAndMonoIsBothSides()
{
    {
        float output[StereoFrames * 4];
        std::fill(std::begin(output), std::end(output), 9.0f);

        WriteAudioFrames(Stereo, StereoFrames, { AudioSampleKind::Float32, 4, 4, 96000 }, reinterpret_cast<BYTE*>(output));

        VERIFY_ARE_EQUAL(0.5f, output[4]);
        VERIFY_ARE_EQUAL(-0.5f, output[5]);

        for (uint32_t frame = 0; frame < StereoFrames; frame++)
        {
            VERIFY_ARE_EQUAL(0.0f, output[frame * 4 + 2]);
            VERIFY_ARE_EQUAL(0.0f, output[frame * 4 + 3]);
        }
    }

    {
        int16_t output[StereoFrames]{};
        WriteAudioFrames(Stereo, StereoFrames, { AudioSampleKind::Int16, 1, 2, 96000 }, reinterpret_cast<BYTE*>(output));

        // (left + right) / 2, then clamped: 0, 0, -0.25, 0.5.
        const int16_t expected[]{ 0, 0, -8192, 16384 };

        for (size_t i = 0; i < std::size(expected); i++)
        {
            VERIFY_ARE_EQUAL(expected[i], output[i]);
        }
    }
}
