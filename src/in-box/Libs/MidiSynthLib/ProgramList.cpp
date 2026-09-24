// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#include "MidiSynth/ProgramList.h"

#include <iterator>

#include <windows.h>

#include "MidiCiProgramList.h"

namespace MidiSynth
{
    namespace
    {
        // RP-003 Table 1 "Instrument Group", one per block of eight programs. These are the
        // categories this sound set actually has, so they are published verbatim rather than
        // translated into the vocabulary M2-107-UM suggests.
        constexpr char const* GeneralMidiInstrumentGroups[]
        {
            "Piano",
            "Chromatic Percussion",
            "Organ",
            "Guitar",
            "Bass",
            "Strings",
            "Ensemble",
            "Brass",
            "Reed",
            "Pipe",
            "Synth Lead",
            "Synth Pad",
            "Synth Effects",
            "Ethnic",
            "Percussive",
            "Sound Effects",
        };

        constexpr size_t GeneralMidiProgramsPerGroup = 8;

        static_assert(
            std::size(GeneralMidiInstrumentGroups) * GeneralMidiProgramsPerGroup == 128,
            "The instrument groups must cover every program number exactly once.");

        // Table 1 covers every channel but 10, so General MIDI names no group for a kit. A client
        // filtering on category would drop these entirely, so they get one of our own.
        constexpr char const* DrumKitGroup[]{ "Drum Kit" };

        std::string ToUtf8(_In_ const std::wstring& text)
        {
            if (text.empty())
            {
                return {};
            }

            const auto required = WideCharToMultiByte(
                CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);

            if (required <= 0)
            {
                return {};
            }

            std::string result(static_cast<size_t>(required), '\0');

            WideCharToMultiByte(
                CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                result.data(), required, nullptr, nullptr);

            return result;
        }
    }

    _Use_decl_annotations_
    size_t CountPrograms(const DlsCollection& collection, ProgramListKind kind) noexcept
    {
        const bool wantDrumKits = (kind == ProgramListKind::DrumKits);

        size_t count = 0;

        for (const auto& instrument : collection.Instruments())
        {
            if (instrument.IsDrumKit == wantDrumKits)
            {
                count++;
            }
        }

        return count;
    }

    _Use_decl_annotations_
    std::vector<char> BuildProgramListJson(const DlsCollection& collection, ProgramListKind kind)
    {
        return BuildProgramListPageJson(collection, kind, 0, SIZE_MAX);
    }

    _Use_decl_annotations_
    std::vector<char> BuildProgramListPageJson(
        const DlsCollection& collection,
        ProgramListKind kind,
        size_t offset,
        size_t limit)
    {
        namespace ci = WindowsMidiServicesCapabilityInquiry;

        const bool wantDrumKits = (kind == ProgramListKind::DrumKits);

        std::vector<std::string> titles;
        std::vector<ci::ProgramListEntry> entries;

        titles.reserve(collection.Instruments().size());
        entries.reserve(collection.Instruments().size());

        size_t position = 0;

        for (const auto& instrument : collection.Instruments())
        {
            if (instrument.IsDrumKit != wantDrumKits)
            {
                continue;
            }

            if (position++ < offset)
            {
                continue;
            }

            if (titles.size() >= limit)
            {
                break;
            }

            titles.push_back(ToUtf8(instrument.Name));
        }

        size_t titleIndex = 0;
        position = 0;

        for (const auto& instrument : collection.Instruments())
        {
            if (instrument.IsDrumKit != wantDrumKits)
            {
                continue;
            }

            if (position++ < offset)
            {
                continue;
            }

            if (titleIndex >= titles.size())
            {
                break;
            }

            ci::ProgramListEntry entry{};

            entry.Title = titles[titleIndex++].c_str();
            entry.BankMsb = static_cast<uint8_t>(instrument.BankMsb & 0x7F);
            entry.BankLsb = static_cast<uint8_t>(instrument.BankLsb & 0x7F);
            entry.Program = static_cast<uint8_t>(instrument.Program & 0x7F);

            // This sound set gives a program and its GS variation the same name, so without a tag
            // a client shows two identical entries and cannot tell which bank it is choosing.
            if (!wantDrumKits)
            {
                entry.Tag = (instrument.BankMsb == 0) ? "GM" : "GS variation";

                entry.Categories = &GeneralMidiInstrumentGroups[entry.Program / GeneralMidiProgramsPerGroup];
                entry.CategoryCount = 1;
            }
            else
            {
                entry.Categories = DrumKitGroup;
                entry.CategoryCount = std::size(DrumKitGroup);
            }

            entries.push_back(entry);
        }

        const auto required = ci::BuildProgramListJson(entries.data(), entries.size(), nullptr, 0);

        if (required == 0)
        {
            return {};
        }

        std::vector<char> json(required);

        const auto written = ci::BuildProgramListJson(entries.data(), entries.size(), json.data(), json.size());

        if (written != required)
        {
            return {};
        }

        return json;
    }
}
