// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "console_output.h"
#include "message_capture_writer.h"
#include "midi_formatting.h"
#include "strings.h"

namespace midi2console
{
    namespace
    {
        std::string ResolveDelimiter(_In_ std::string const& delimiterName)
        {
            if (EqualsIgnoreCase(delimiterName, "comma")) return ",";
            if (EqualsIgnoreCase(delimiterName, "tab"))   return "\t";
            if (EqualsIgnoreCase(delimiterName, "pipe"))  return "|";

            return " ";
        }

        std::string LocalTimeOfDay()
        {
            SYSTEMTIME now{};
            GetLocalTime(&now);

            return fmt::format("{:02}:{:02}:{:02}.{:03}",
                now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
        }

        // The shipping console writes the enum name here, not the friendly name, so a capture
        // stays greppable by message type.
        std::string MessageTypeName(_In_ uint32_t word0)
        {
            switch (midi2msg::MidiMessageHelper::GetMessageTypeFromMessageFirstWord(word0))
            {
            case midi2::MidiMessageType::UtilityMessage32:      return "UtilityMessage32";
            case midi2::MidiMessageType::SystemCommon32:        return "SystemCommon32";
            case midi2::MidiMessageType::Midi1ChannelVoice32:   return "Midi1ChannelVoice32";
            case midi2::MidiMessageType::DataMessage64:         return "DataMessage64";
            case midi2::MidiMessageType::Midi2ChannelVoice64:   return "Midi2ChannelVoice64";
            case midi2::MidiMessageType::DataMessage128:        return "DataMessage128";
            case midi2::MidiMessageType::FlexData128:           return "FlexData128";
            case midi2::MidiMessageType::Stream128:             return "Stream128";
            default:                                            return "Reserved";
            }
        }
    }

    _Use_decl_annotations_
    bool MessageCaptureWriter::IsValidDelimiterName(std::string const& delimiterName)
    {
        return EqualsIgnoreCase(delimiterName, "space") ||
            EqualsIgnoreCase(delimiterName, "comma") ||
            EqualsIgnoreCase(delimiterName, "tab") ||
            EqualsIgnoreCase(delimiterName, "pipe");
    }

    _Use_decl_annotations_
    bool MessageCaptureWriter::Open(
        std::string const& fileName,
        std::string const& delimiterName,
        bool annotate)
    {
        m_delimiter = ResolveDelimiter(delimiterName);
        m_annotate = annotate;

        auto const wideName = FromUtf8(fileName);

        std::wstring expanded;
        expanded.resize(MAX_PATH * 4);

        auto const expandedLength = ExpandEnvironmentStringsW(
            wideName.c_str(), expanded.data(), static_cast<DWORD>(expanded.size()));

        if (expandedLength > 0 && expandedLength <= expanded.size())
        {
            expanded.resize(expandedLength - 1);
        }
        else
        {
            expanded = wideName;
        }

        m_fileName = ToUtf8(expanded);

        // Appending matches the shipping console, so repeated runs build one capture.
        m_file.open(expanded, std::ios::out | std::ios::app);

        return m_file.is_open();
    }

    _Use_decl_annotations_
    void MessageCaptureWriter::Write(uint64_t timestamp, uint32_t wordCount, uint32_t const* words)
    {
        if (!m_file.is_open() || wordCount == 0)
        {
            return;
        }

        if (m_annotate)
        {
            // # ___time___ ___timestamp___ ___message type___ ___friendly name___
            m_file << "# " << LocalTimeOfDay() << m_delimiter
                << timestamp << m_delimiter
                << MessageTypeName(words[0]) << m_delimiter
                << ToUtf8(midi2msg::MidiMessageHelper::GetMessageDisplayNameFromFirstWord(words[0]))
                << "\n";
        }

        for (uint32_t index = 0; index < wordCount; index++)
        {
            if (index > 0)
            {
                m_file << m_delimiter;
            }

            m_file << fmt::format("0x{:08X}", words[index]);
        }

        m_file << "\n";

        m_messagesWritten++;
    }

    void MessageCaptureWriter::Flush()
    {
        if (m_file.is_open())
        {
            m_file.flush();
        }
    }
}
