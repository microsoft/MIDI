// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#include "MidiSynth/ProgramList.h"

#include <windows.h>

#include "MidiCiProgramList.h"

namespace MidiSynth
{
    namespace
    {
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
    std::vector<char> BuildProgramListJson(const DlsCollection& collection, ProgramListKind kind)
    {
        namespace ci = WindowsMidiServicesCapabilityInquiry;

        const bool wantDrumKits = (kind == ProgramListKind::DrumKits);

        std::vector<std::string> titles;
        std::vector<ci::ProgramListEntry> entries;

        titles.reserve(collection.Instruments().size());
        entries.reserve(collection.Instruments().size());

        for (const auto& instrument : collection.Instruments())
        {
            if (instrument.IsDrumKit != wantDrumKits)
            {
                continue;
            }

            titles.push_back(ToUtf8(instrument.Name));
        }

        size_t titleIndex = 0;

        for (const auto& instrument : collection.Instruments())
        {
            if (instrument.IsDrumKit != wantDrumKits)
            {
                continue;
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
