// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SmfTestFileBuilder.h"

namespace smftest
{
    namespace
    {
        void AppendUInt16(std::vector<uint8_t>& bytes, uint16_t value)
        {
            bytes.push_back(static_cast<uint8_t>(value >> 8));
            bytes.push_back(static_cast<uint8_t>(value & 0xFF));
        }

        void AppendUInt32(std::vector<uint8_t>& bytes, uint32_t value)
        {
            bytes.push_back(static_cast<uint8_t>(value >> 24));
            bytes.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
            bytes.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
            bytes.push_back(static_cast<uint8_t>(value & 0xFF));
        }

        void AppendId(std::vector<uint8_t>& bytes, char const* id)
        {
            for (int index = 0; index < 4; ++index)
            {
                bytes.push_back(static_cast<uint8_t>(id[index]));
            }
        }
    }

    TrackBuilder& TrackBuilder::VariableLength(uint32_t value)
    {
        uint8_t staging[4]{};
        int count = 0;

        staging[count++] = static_cast<uint8_t>(value & 0x7F);
        value >>= 7;

        while (value > 0 && count < 4)
        {
            staging[count++] = static_cast<uint8_t>((value & 0x7F) | 0x80);
            value >>= 7;
        }

        while (count > 0)
        {
            m_bytes.push_back(staging[--count]);
        }

        return *this;
    }

    TrackBuilder& TrackBuilder::Raw(std::initializer_list<uint8_t> bytes)
    {
        m_bytes.insert(m_bytes.end(), bytes.begin(), bytes.end());
        return *this;
    }

    TrackBuilder& TrackBuilder::RawBytes(std::vector<uint8_t> const& bytes)
    {
        m_bytes.insert(m_bytes.end(), bytes.begin(), bytes.end());
        return *this;
    }

    TrackBuilder& TrackBuilder::NoteOn(uint32_t delta, uint8_t channel, uint8_t note, uint8_t velocity)
    {
        VariableLength(delta);
        return Raw({ static_cast<uint8_t>(0x90 | (channel & 0x0F)), note, velocity });
    }

    TrackBuilder& TrackBuilder::NoteOff(uint32_t delta, uint8_t channel, uint8_t note, uint8_t velocity)
    {
        VariableLength(delta);
        return Raw({ static_cast<uint8_t>(0x80 | (channel & 0x0F)), note, velocity });
    }

    TrackBuilder& TrackBuilder::ControlChange(uint32_t delta, uint8_t channel, uint8_t controller, uint8_t value)
    {
        VariableLength(delta);
        return Raw({ static_cast<uint8_t>(0xB0 | (channel & 0x0F)), controller, value });
    }

    TrackBuilder& TrackBuilder::ProgramChange(uint32_t delta, uint8_t channel, uint8_t program)
    {
        VariableLength(delta);
        return Raw({ static_cast<uint8_t>(0xC0 | (channel & 0x0F)), program });
    }

    TrackBuilder& TrackBuilder::RunningStatusData(uint32_t delta, uint8_t data1, uint8_t data2)
    {
        VariableLength(delta);
        return Raw({ data1, data2 });
    }

    TrackBuilder& TrackBuilder::Tempo(uint32_t delta, uint32_t microsecondsPerQuarterNote)
    {
        VariableLength(delta);

        return Raw({
            0xFF, 0x51, 0x03,
            static_cast<uint8_t>((microsecondsPerQuarterNote >> 16) & 0xFF),
            static_cast<uint8_t>((microsecondsPerQuarterNote >> 8) & 0xFF),
            static_cast<uint8_t>(microsecondsPerQuarterNote & 0xFF) });
    }

    TrackBuilder& TrackBuilder::TimeSignature(uint32_t delta, uint8_t numerator, uint8_t denominatorPowerOfTwo)
    {
        VariableLength(delta);
        return Raw({ 0xFF, 0x58, 0x04, numerator, denominatorPowerOfTwo, 24, 8 });
    }

    TrackBuilder& TrackBuilder::MetaText(uint32_t delta, uint8_t metaType, std::string const& text)
    {
        VariableLength(delta);

        m_bytes.push_back(0xFF);
        m_bytes.push_back(metaType);

        VariableLength(static_cast<uint32_t>(text.size()));

        for (auto const character : text)
        {
            m_bytes.push_back(static_cast<uint8_t>(character));
        }

        return *this;
    }

    TrackBuilder& TrackBuilder::SystemExclusive(uint32_t delta, std::vector<uint8_t> const& payload)
    {
        VariableLength(delta);

        m_bytes.push_back(0xF0);

        VariableLength(static_cast<uint32_t>(payload.size()));

        return RawBytes(payload);
    }

    TrackBuilder& TrackBuilder::Escape(uint32_t delta, std::vector<uint8_t> const& payload)
    {
        VariableLength(delta);

        m_bytes.push_back(0xF7);

        VariableLength(static_cast<uint32_t>(payload.size()));

        return RawBytes(payload);
    }

    TrackBuilder& TrackBuilder::EndOfTrack(uint32_t delta)
    {
        VariableLength(delta);
        return Raw({ 0xFF, 0x2F, 0x00 });
    }

    std::vector<uint8_t> BuildFile(
        uint16_t format,
        uint16_t division,
        std::vector<TrackBuilder> const& tracks)
    {
        std::vector<uint8_t> file{};

        AppendId(file, "MThd");
        AppendUInt32(file, 6);
        AppendUInt16(file, format);
        AppendUInt16(file, static_cast<uint16_t>(tracks.size()));
        AppendUInt16(file, division);

        for (auto const& track : tracks)
        {
            AppendId(file, "MTrk");
            AppendUInt32(file, static_cast<uint32_t>(track.Bytes().size()));

            file.insert(file.end(), track.Bytes().begin(), track.Bytes().end());
        }

        return file;
    }

    std::vector<uint8_t> BuildFileWithTrackLength(
        uint16_t format,
        uint16_t division,
        TrackBuilder const& track,
        uint32_t declaredLength)
    {
        std::vector<uint8_t> file{};

        AppendId(file, "MThd");
        AppendUInt32(file, 6);
        AppendUInt16(file, format);
        AppendUInt16(file, 1);
        AppendUInt16(file, division);

        AppendId(file, "MTrk");
        AppendUInt32(file, declaredLength);

        file.insert(file.end(), track.Bytes().begin(), track.Bytes().end());

        return file;
    }

    std::vector<uint8_t> WrapInRiff(std::vector<uint8_t> const& midiFile)
    {
        std::vector<uint8_t> file{};

        AppendId(file, "RIFF");

        // little endian in a RIFF container, unlike everything inside the MIDI file
        auto const riffSize = static_cast<uint32_t>(midiFile.size() + 12);

        file.push_back(static_cast<uint8_t>(riffSize & 0xFF));
        file.push_back(static_cast<uint8_t>((riffSize >> 8) & 0xFF));
        file.push_back(static_cast<uint8_t>((riffSize >> 16) & 0xFF));
        file.push_back(static_cast<uint8_t>((riffSize >> 24) & 0xFF));

        AppendId(file, "RMID");
        AppendId(file, "data");

        auto const dataSize = static_cast<uint32_t>(midiFile.size());

        file.push_back(static_cast<uint8_t>(dataSize & 0xFF));
        file.push_back(static_cast<uint8_t>((dataSize >> 8) & 0xFF));
        file.push_back(static_cast<uint8_t>((dataSize >> 16) & 0xFF));
        file.push_back(static_cast<uint8_t>((dataSize >> 24) & 0xFF));

        file.insert(file.end(), midiFile.begin(), midiFile.end());

        return file;
    }
}
