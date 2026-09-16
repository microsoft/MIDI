// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

// Surveys a folder of Standard MIDI Files and reports what is actually in them, so that decisions
// about which manufacturer system exclusive messages to support are driven by evidence rather than
// by the full surface of a specification.
//
// Reads files in place. Nothing is copied, and no file content is written anywhere.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <algorithm>

namespace
{
    struct ByteReader
    {
        const uint8_t* Data{ nullptr };
        size_t Size{ 0 };
        size_t Position{ 0 };

        bool Need(size_t count) const noexcept { return Position + count <= Size; }

        uint8_t U8() noexcept { return Data[Position++]; }

        uint16_t U16() noexcept
        {
            const auto value = static_cast<uint16_t>((Data[Position] << 8) | Data[Position + 1]);
            Position += 2;
            return value;
        }

        uint32_t U32() noexcept
        {
            uint32_t value = 0;
            for (int i = 0; i < 4; i++) { value = (value << 8) | Data[Position + i]; }
            Position += 4;
            return value;
        }

        // Variable length quantity, at most four bytes.
        bool Vlq(uint32_t& value) noexcept
        {
            value = 0;

            for (int i = 0; i < 4; i++)
            {
                if (!Need(1)) { return false; }

                const uint8_t byte = U8();
                value = (value << 7) | (byte & 0x7F);

                if ((byte & 0x80) == 0) { return true; }
            }

            return false;
        }
    };

    struct Survey
    {
        size_t FilesSeen{ 0 };
        size_t FilesParsed{ 0 };
        size_t FilesFailed{ 0 };
        size_t FilesWithSysEx{ 0 };
        size_t FilesWithBankSelect{ 0 };

        std::map<uint16_t, size_t> Formats;
        std::map<std::string, size_t> UniversalMessages;
        std::map<std::string, size_t> Manufacturers;
        std::map<std::string, size_t> RolandAddresses;
        std::map<uint8_t, size_t> BankSelectMsb;
        std::map<uint8_t, size_t> BankSelectLsb;

        size_t TotalSysExMessages{ 0 };
        size_t TruncatedTracks{ 0 };

        // When set, payloads starting with these bytes are collected verbatim so an unfamiliar
        // manufacturer can be identified from what it actually sends.
        std::vector<uint8_t> DumpPrefix;
        std::map<std::string, std::pair<size_t, std::string>> DumpSamples;
        std::string CurrentFile;
    };

    std::string Hex(uint8_t value)
    {
        char text[8]{};
        snprintf(text, sizeof(text), "%02X", value);
        return text;
    }

    const char* UniversalName(uint8_t realTime, uint8_t subId1, uint8_t subId2)
    {
        if (realTime == 0x7E)
        {
            if (subId1 == 0x06 && subId2 == 0x01) { return "Identity Request"; }
            if (subId1 == 0x06 && subId2 == 0x02) { return "Identity Reply"; }
            if (subId1 == 0x09 && subId2 == 0x01) { return "GM1 System On"; }
            if (subId1 == 0x09 && subId2 == 0x02) { return "GM System Off"; }
            if (subId1 == 0x09 && subId2 == 0x03) { return "GM2 System On"; }
            if (subId1 == 0x08) { return "MIDI Tuning"; }
            if (subId1 == 0x0D) { return "MIDI-CI"; }
        }
        else
        {
            if (subId1 == 0x04 && subId2 == 0x01) { return "Master Volume"; }
            if (subId1 == 0x04 && subId2 == 0x02) { return "Master Balance"; }
            if (subId1 == 0x04 && subId2 == 0x03) { return "Master Fine Tuning"; }
            if (subId1 == 0x04 && subId2 == 0x04) { return "Master Coarse Tuning"; }
            if (subId1 == 0x04 && subId2 == 0x05) { return "Global Parameter Control"; }
            if (subId1 == 0x09 && subId2 == 0x01) { return "Channel Pressure (GM2)"; }
            if (subId1 == 0x0A) { return "Key Based Instrument Control"; }
        }

        return "other";
    }

    void RecordSysEx(Survey& survey, const std::vector<uint8_t>& payload)
    {
        survey.TotalSysExMessages++;

        if (payload.empty()) { return; }

        if (!survey.DumpPrefix.empty() &&
            payload.size() >= survey.DumpPrefix.size() &&
            std::equal(survey.DumpPrefix.begin(), survey.DumpPrefix.end(), payload.begin()))
        {
            std::string hex;
            std::string text;

            for (size_t i = 0; i < payload.size() && i < 48; i++)
            {
                hex += Hex(payload[i]);
                hex += ' ';
                text += (payload[i] >= 0x20 && payload[i] < 0x7F) ? (char)payload[i] : '.';
            }

            auto& entry = survey.DumpSamples[hex + "  |" + text + "|"];
            entry.first++;
            if (entry.second.empty()) { entry.second = survey.CurrentFile; }
        }

        const uint8_t first = payload[0];

        if (first == 0x7E || first == 0x7F)
        {
            const uint8_t subId1 = payload.size() > 2 ? payload[2] : 0xFF;
            const uint8_t subId2 = payload.size() > 3 ? payload[3] : 0xFF;

            std::string key =
                std::string(first == 0x7E ? "non-real-time " : "real-time ") +
                Hex(subId1) + " " + Hex(subId2) + "  " +
                UniversalName(first, subId1, subId2);

            survey.UniversalMessages[key]++;
            survey.Manufacturers[first == 0x7E ? "universal non-real-time" : "universal real-time"]++;
            return;
        }

        if (first == 0x00 && payload.size() >= 3)
        {
            survey.Manufacturers["extended " + Hex(payload[0]) + " " + Hex(payload[1]) + " " + Hex(payload[2])]++;
            return;
        }

        survey.Manufacturers["single byte " + Hex(first)]++;

        // Roland data set, 41 <device> <model> 12 <address x3>
        if (first == 0x41 && payload.size() >= 7 && payload[3] == 0x12)
        {
            survey.RolandAddresses[
                "model " + Hex(payload[2]) + "  addr " +
                Hex(payload[4]) + " " + Hex(payload[5]) + " " + Hex(payload[6])]++;
        }
    }

    bool ParseTrack(ByteReader& reader, size_t trackEnd, Survey& survey, bool& sawSysEx, bool& sawBankSelect)
    {
        uint8_t runningStatus{ 0 };

        while (reader.Position < trackEnd)
        {
            uint32_t delta{ 0 };

            if (!reader.Vlq(delta)) { return false; }
            if (!reader.Need(1)) { return false; }

            uint8_t status = reader.Data[reader.Position];

            if (status < 0x80)
            {
                if (runningStatus == 0) { return false; }
                status = runningStatus;
            }
            else
            {
                reader.Position++;

                // System messages cancel running status.
                if (status < 0xF0) { runningStatus = status; } else { runningStatus = 0; }
            }

            if (status == 0xFF)
            {
                if (!reader.Need(1)) { return false; }

                reader.U8();

                uint32_t length{ 0 };
                if (!reader.Vlq(length)) { return false; }
                if (!reader.Need(length)) { return false; }

                reader.Position += length;
                continue;
            }

            if (status == 0xF0 || status == 0xF7)
            {
                uint32_t length{ 0 };
                if (!reader.Vlq(length)) { return false; }
                if (!reader.Need(length)) { return false; }

                std::vector<uint8_t> payload;
                payload.reserve(length);

                for (uint32_t i = 0; i < length; i++)
                {
                    const uint8_t byte = reader.Data[reader.Position + i];
                    if (byte == 0xF7) { break; }
                    payload.push_back(byte);
                }

                reader.Position += length;

                if (status == 0xF0)
                {
                    RecordSysEx(survey, payload);
                    sawSysEx = true;
                }

                continue;
            }

            const uint8_t high = status & 0xF0;

            if (high == 0xC0 || high == 0xD0)
            {
                if (!reader.Need(1)) { return false; }
                reader.Position += 1;
                continue;
            }

            if (high == 0x80 || high == 0x90 || high == 0xA0 || high == 0xB0 || high == 0xE0)
            {
                if (!reader.Need(2)) { return false; }

                const uint8_t data1 = reader.Data[reader.Position];
                const uint8_t data2 = reader.Data[reader.Position + 1];

                reader.Position += 2;

                if (high == 0xB0)
                {
                    if (data1 == 0) { survey.BankSelectMsb[data2 & 0x7F]++; sawBankSelect = true; }
                    else if (data1 == 32) { survey.BankSelectLsb[data2 & 0x7F]++; sawBankSelect = true; }
                }

                continue;
            }

            // A system common message inside a file is unusual; stop rather than desynchronize.
            return false;
        }

        return true;
    }

    void ParseFile(const std::filesystem::path& path, Survey& survey)
    {
        survey.FilesSeen++;

        if (!survey.DumpPrefix.empty())
        {
            // path::string() throws when the name will not convert, and real collections are full
            // of names that will not.
            survey.CurrentFile.clear();

            for (const wchar_t character : path.filename().wstring())
            {
                survey.CurrentFile += (character > 0 && character < 0x80) ? static_cast<char>(character) : '?';
            }
        }

        std::ifstream stream(path, std::ios::binary);
        if (!stream) { survey.FilesFailed++; return; }

        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

        if (bytes.size() < 14) { survey.FilesFailed++; return; }

        ByteReader reader{ bytes.data(), bytes.size(), 0 };

        if (memcmp(bytes.data(), "MThd", 4) != 0) { survey.FilesFailed++; return; }

        reader.Position = 4;

        const uint32_t headerLength = reader.U32();
        if (!reader.Need(headerLength) || headerLength < 6) { survey.FilesFailed++; return; }

        const uint16_t format = reader.U16();
        const uint16_t trackCount = reader.U16();
        reader.U16();

        reader.Position = 8 + headerLength;

        survey.Formats[format]++;

        bool sawSysEx{ false };
        bool sawBankSelect{ false };
        bool anyTrackParsed{ false };

        for (uint16_t track = 0; track < trackCount; track++)
        {
            if (!reader.Need(8)) { break; }

            if (memcmp(bytes.data() + reader.Position, "MTrk", 4) != 0)
            {
                reader.Position += 4;
                const uint32_t skip = reader.Need(4) ? reader.U32() : 0;
                if (!reader.Need(skip)) { break; }
                reader.Position += skip;
                continue;
            }

            reader.Position += 4;

            const uint32_t trackLength = reader.U32();
            if (!reader.Need(trackLength)) { break; }

            const size_t trackEnd = reader.Position + trackLength;

            if (!ParseTrack(reader, trackEnd, survey, sawSysEx, sawBankSelect))
            {
                survey.TruncatedTracks++;
            }

            reader.Position = trackEnd;
            anyTrackParsed = true;
        }

        if (anyTrackParsed) { survey.FilesParsed++; } else { survey.FilesFailed++; }
        if (sawSysEx) { survey.FilesWithSysEx++; }
        if (sawBankSelect) { survey.FilesWithBankSelect++; }
    }

    template <typename K>
    void PrintTop(const char* title, const std::map<K, size_t>& values, size_t limit)
    {
        printf("\n%s\n", title);

        std::vector<std::pair<K, size_t>> sorted(values.begin(), values.end());
        std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        size_t shown = 0;

        for (const auto& entry : sorted)
        {
            if (shown++ >= limit) { break; }

            if constexpr (std::is_same_v<K, std::string>)
            {
                printf("  %-52s %zu\n", entry.first.c_str(), entry.second);
            }
            else
            {
                printf("  %-52d %zu\n", (int)entry.first, entry.second);
            }
        }

        if (sorted.empty()) { printf("  (none)\n"); }
    }
}

int wmain(int argc, wchar_t** argv)
{
    if (argc < 2)
    {
        printf("usage: synthspike-smfsurvey <folder> [--dump <hex bytes, for example 002024>]\n");
        return 1;
    }

    const std::filesystem::path root{ argv[1] };

    if (!std::filesystem::exists(root))
    {
        printf("folder not found\n");
        return 1;
    }

    Survey survey{};

    for (int i = 2; i < argc; i++)
    {
        if (wcscmp(argv[i], L"--dump") == 0 && i + 1 < argc)
        {
            const std::wstring hex{ argv[++i] };

            for (size_t c = 0; c + 1 < hex.size(); c += 2)
            {
                const auto value = wcstoul(hex.substr(c, 2).c_str(), nullptr, 16);
                survey.DumpPrefix.push_back(static_cast<uint8_t>(value));
            }
        }
    }

    std::error_code error;

    for (auto iterator = std::filesystem::recursive_directory_iterator(
            root, std::filesystem::directory_options::skip_permission_denied, error);
        iterator != std::filesystem::recursive_directory_iterator();
        iterator.increment(error))
    {
        if (error) { break; }
        if (!iterator->is_regular_file(error)) { continue; }

        auto extension = iterator->path().extension().wstring();
        std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);

        if (extension != L".mid" && extension != L".midi") { continue; }

        ParseFile(iterator->path(), survey);

        if ((survey.FilesSeen % 500) == 0)
        {
            printf("  ... %zu files\r", survey.FilesSeen);
            fflush(stdout);
        }
    }

    printf("\nFiles\n");
    printf("  seen              %zu\n", survey.FilesSeen);
    printf("  parsed            %zu\n", survey.FilesParsed);
    printf("  failed            %zu\n", survey.FilesFailed);
    printf("  with system exclusive %zu\n", survey.FilesWithSysEx);
    printf("  with bank select      %zu\n", survey.FilesWithBankSelect);
    printf("  tracks ending early   %zu\n", survey.TruncatedTracks);
    printf("  system exclusive messages %zu\n", survey.TotalSysExMessages);

    PrintTop("Format", survey.Formats, 8);
    PrintTop("Manufacturer", survey.Manufacturers, 20);
    PrintTop("Universal system exclusive", survey.UniversalMessages, 25);
    PrintTop("Roland data set addresses", survey.RolandAddresses, 30);
    PrintTop("Bank select MSB value", survey.BankSelectMsb, 16);
    PrintTop("Bank select LSB value", survey.BankSelectLsb, 16);

    if (!survey.DumpPrefix.empty())
    {
        printf("\nDistinct payloads matching the requested prefix (%zu distinct)\n", survey.DumpSamples.size());

        std::vector<std::pair<std::string, std::pair<size_t, std::string>>> sorted(
            survey.DumpSamples.begin(), survey.DumpSamples.end());

        std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.second.first > b.second.first; });

        size_t shown = 0;

        for (const auto& entry : sorted)
        {
            if (shown++ >= 25) { break; }

            printf("  %6zu  %s\n", entry.second.first, entry.first.c_str());
            printf("          first seen in %s\n", entry.second.second.c_str());
        }
    }

    return 0;
}
