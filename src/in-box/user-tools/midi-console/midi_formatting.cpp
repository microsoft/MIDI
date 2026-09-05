// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include <algorithm>
#include <cctype>

#include "console_output.h"
#include "midi_formatting.h"
#include "strings.h"

namespace midi2console
{
    std::string ToLowerCopy(_In_ std::string_view value)
    {
        std::string result{ value };

        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        return result;
    }

    std::string TrimCopy(_In_ std::string_view value)
    {
        auto const first = value.find_first_not_of(" \t\r\n");

        if (first == std::string_view::npos)
        {
            return {};
        }

        auto const last = value.find_last_not_of(" \t\r\n");

        return std::string{ value.substr(first, last - first + 1) };
    }

    bool EqualsIgnoreCase(_In_ std::string_view left, _In_ std::string_view right)
    {
        return ToLowerCopy(left) == ToLowerCopy(right);
    }

    std::string FormatBoolean(_In_ bool value)
    {
        return value ? ResourceString(IDS_LABEL_YES) : ResourceString(IDS_LABEL_NO);
    }

    fmt::text_style BooleanStyle(_In_ bool value)
    {
        return value ? booleanTrueTextStyle : booleanFalseTextStyle;
    }

    std::string FormatGuid(_In_ winrt::guid const& value)
    {
        return ToUtf8(winrt::to_hstring(value));
    }

    winrt::guid ParseGuid(_In_ std::string_view value, _Out_ bool& succeeded)
    {
        succeeded = false;

        auto const trimmed = TrimCopy(value);

        if (trimmed.empty())
        {
            return {};
        }

        try
        {
            // winrt::guid accepts braced, parenthesized and bare forms.
            auto const parsed = winrt::guid{ FromUtf8(trimmed) };

            succeeded = true;

            return parsed;
        }
        catch (...)
        {
            return {};
        }
    }

    std::string FormatNumberWithSeparators(_In_ uint64_t value)
    {
        auto const digits = fmt::format("{}", value);

        std::string result;
        result.reserve(digits.size() + digits.size() / 3);

        auto const leading = digits.size() % 3;

        for (size_t i = 0; i < digits.size(); i++)
        {
            if (i > 0 && (i % 3) == leading)
            {
                result.push_back(',');
            }

            result.push_back(digits[i]);
        }

        return result;
    }

    std::string FormatDecimal(_In_ double value, _In_ int decimalPlaces)
    {
        return fmt::format("{:.{}f}", value, decimalPlaces);
    }

    std::string FormatDateTime(_In_ foundation::DateTime const& value)
    {
        // The "empty" value for a WinRT DateTime is year 1601, which is not worth showing.
        auto const fileTime = winrt::clock::to_file_time(value);

        if (fileTime.value == 0)
        {
            return {};
        }

        FILETIME rawFileTime{};
        rawFileTime.dwLowDateTime = static_cast<DWORD>(fileTime.value & 0xFFFFFFFF);
        rawFileTime.dwHighDateTime = static_cast<DWORD>(fileTime.value >> 32);

        FILETIME localFileTime{};
        SYSTEMTIME systemTime{};

        if (!FileTimeToLocalFileTime(&rawFileTime, &localFileTime) ||
            !FileTimeToSystemTime(&localFileTime, &systemTime))
        {
            return {};
        }

        if (systemTime.wYear <= 1601)
        {
            return {};
        }

        return fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}",
            systemTime.wYear, systemTime.wMonth, systemTime.wDay,
            systemTime.wHour, systemTime.wMinute, systemTime.wSecond);
    }

    std::string FormatEndpointPurpose(_In_ midi2enum::MidiEndpointDevicePurpose purpose)
    {
        switch (purpose)
        {
        case midi2enum::MidiEndpointDevicePurpose::NormalMessageEndpoint:   return "Normal Message Endpoint";
        case midi2enum::MidiEndpointDevicePurpose::VirtualDeviceResponder:  return "Virtual Device Responder";
        case midi2enum::MidiEndpointDevicePurpose::InBoxGeneralMidiSynth:   return "In-Box General MIDI Synth";
        case midi2enum::MidiEndpointDevicePurpose::DiagnosticLoopback:      return "Diagnostic Loopback";
        case midi2enum::MidiEndpointDevicePurpose::DiagnosticPing:          return "Diagnostic Ping";
        default:                                                            return ResourceString(IDS_LABEL_UNKNOWN);
        }
    }

    std::string FormatNativeDataFormat(_In_ midi2enum::MidiEndpointNativeDataFormat format)
    {
        switch (format)
        {
        case midi2enum::MidiEndpointNativeDataFormat::Midi1ByteFormat:           return "MIDI 1.0 Byte Format";
        case midi2enum::MidiEndpointNativeDataFormat::UniversalMidiPacketFormat: return "Universal MIDI Packet Format";
        default:                                                                 return ResourceString(IDS_LABEL_UNKNOWN);
        }
    }

    std::string FormatProtocol(_In_ midi2enum::MidiProtocol protocol)
    {
        switch (protocol)
        {
        case midi2enum::MidiProtocol::Midi1: return "MIDI 1.0";
        case midi2enum::MidiProtocol::Midi2: return "MIDI 2.0";
        default:                             return "Default";
        }
    }

    std::string FormatFunctionBlockDirection(_In_ midi2enum::MidiFunctionBlockDirection direction)
    {
        switch (direction)
        {
        case midi2enum::MidiFunctionBlockDirection::BlockInput:    return ResourceString(IDS_LABEL_MESSAGE_SOURCE);
        case midi2enum::MidiFunctionBlockDirection::BlockOutput:   return ResourceString(IDS_LABEL_MESSAGE_DESTINATION);
        case midi2enum::MidiFunctionBlockDirection::Bidirectional: return ResourceString(IDS_LABEL_BIDIRECTIONAL);
        default:                                                   return ResourceString(IDS_LABEL_UNKNOWN);
        }
    }

    std::string FormatFunctionBlockUIHint(_In_ midi2enum::MidiFunctionBlockUIHint hint)
    {
        switch (hint)
        {
        case midi2enum::MidiFunctionBlockUIHint::Receiver:      return "Receiver";
        case midi2enum::MidiFunctionBlockUIHint::Sender:        return "Sender";
        case midi2enum::MidiFunctionBlockUIHint::Bidirectional: return ResourceString(IDS_LABEL_BIDIRECTIONAL);
        default:                                                return ResourceString(IDS_LABEL_UNKNOWN);
        }
    }

    std::string FormatGroupTerminalBlockDirection(_In_ midi2enum::MidiGroupTerminalBlockDirection direction)
    {
        switch (direction)
        {
        case midi2enum::MidiGroupTerminalBlockDirection::BlockInput:    return ResourceString(IDS_LABEL_MESSAGE_SOURCE);
        case midi2enum::MidiGroupTerminalBlockDirection::BlockOutput:   return ResourceString(IDS_LABEL_MESSAGE_DESTINATION);
        default:                                                        return ResourceString(IDS_LABEL_BIDIRECTIONAL);
        }
    }

    std::string FormatPortFlow(_In_ midi2enum::Midi1PortFlow flow)
    {
        return flow == midi2enum::Midi1PortFlow::MidiMessageSource
            ? ResourceString(IDS_LABEL_MESSAGE_SOURCE)
            : ResourceString(IDS_LABEL_MESSAGE_DESTINATION);
    }

    std::string FormatPortNamingApproach(_In_ midi2enum::Midi1PortNamingApproach approach)
    {
        switch (approach)
        {
        case midi2enum::Midi1PortNamingApproach::UseClassicCompatible: return "Classic Compatible";
        case midi2enum::Midi1PortNamingApproach::UseNewStyle:          return "New Style";
        default:                                                       return "Default";
        }
    }

    std::string FormatRepresentsMidi10Connection(_In_ midi2enum::MidiFunctionBlockRepresentsMidi10Connection value)
    {
        switch (value)
        {
        case midi2enum::MidiFunctionBlockRepresentsMidi10Connection::Not10:
            return ResourceString(IDS_FB_MIDI10_NOT_MIDI10);
        case midi2enum::MidiFunctionBlockRepresentsMidi10Connection::YesBandwidthUnrestricted:
            return ResourceString(IDS_FB_MIDI10_UNRESTRICTED);
        case midi2enum::MidiFunctionBlockRepresentsMidi10Connection::YesBandwidthRestricted:
            return ResourceString(IDS_FB_MIDI10_RESTRICTED);
        default:
            return ResourceString(IDS_LABEL_UNKNOWN);
        }
    }

    std::string FormatGroupSpan(_In_ uint8_t firstGroupIndex, _In_ uint8_t groupCount)
    {
        auto const first = static_cast<int>(firstGroupIndex) + 1;

        if (groupCount > 1)
        {
            return fmt::format("{} {}-{}", ResourceString(IDS_LABEL_GROUPS), first, first + groupCount - 1);
        }

        return fmt::format("{} {}", ResourceString(IDS_LABEL_GROUP), first);
    }

    std::string FormatPropertyValue(_In_ foundation::IInspectable const& value)
    {
        if (value == nullptr)
        {
            return fmt::format("{}", Styled(ResourceString(IDS_EP_RAW_NULL), booleanFalseTextStyle));
        }

        auto const propertyValue = value.try_as<foundation::IPropertyValue>();

        if (propertyValue == nullptr)
        {
            auto const stringable = value.try_as<foundation::IStringable>();

            return stringable == nullptr ? std::string{} : ToUtf8(stringable.ToString());
        }

        switch (propertyValue.Type())
        {
        case foundation::PropertyType::String:
            return ToUtf8(propertyValue.GetString());

        case foundation::PropertyType::Boolean:
            return fmt::format("{}", Styled(FormatBoolean(propertyValue.GetBoolean()),
                BooleanStyle(propertyValue.GetBoolean())));

        case foundation::PropertyType::UInt8:   return fmt::format("{}", propertyValue.GetUInt8());
        case foundation::PropertyType::Int16:   return fmt::format("{}", propertyValue.GetInt16());
        case foundation::PropertyType::UInt16:  return fmt::format("{}", propertyValue.GetUInt16());
        case foundation::PropertyType::Int32:   return fmt::format("{}", propertyValue.GetInt32());
        case foundation::PropertyType::UInt32:  return fmt::format("{}", propertyValue.GetUInt32());
        case foundation::PropertyType::Int64:   return fmt::format("{}", propertyValue.GetInt64());
        case foundation::PropertyType::UInt64:  return fmt::format("{}", propertyValue.GetUInt64());
        case foundation::PropertyType::Single:  return FormatDecimal(propertyValue.GetSingle(), 4);
        case foundation::PropertyType::Double:  return FormatDecimal(propertyValue.GetDouble(), 4);

        case foundation::PropertyType::Guid:
            return fmt::format("{}", Styled(FormatGuid(propertyValue.GetGuid()), guidTextStyle));

        case foundation::PropertyType::DateTime:
            return fmt::format("{}", Styled(FormatDateTime(propertyValue.GetDateTime()), timestampTextStyle));

        case foundation::PropertyType::UInt8Array:
        {
            winrt::com_array<uint8_t> bytes;

            propertyValue.GetUInt8Array(bytes);

            // Hex with the printable character beside it, which is how the shipping console
            // renders binary properties and is what makes embedded strings readable.
            std::string result;

            for (auto const byteValue : bytes)
            {
                auto const character = static_cast<char>(byteValue);

                if (byteValue < 0x80 && std::isalnum(static_cast<unsigned char>(character)))
                {
                    result += fmt::format("{}{} ",
                        Styled(fmt::format("{:02X}", byteValue), separatorTextStyle),
                        Styled(std::string(1, character), fmt::fg(fmt::color::cyan)));
                }
                else
                {
                    result += fmt::format("{}  ", Styled(fmt::format("{:02X}", byteValue), separatorTextStyle));
                }
            }

            result += fmt::format("{}",
                Styled(FormatResourceString(IDS_EP_RAW_BYTE_COUNT, bytes.size()), fmt::fg(fmt::color::cyan)));

            return result;
        }

        case foundation::PropertyType::StringArray:
        {
            winrt::com_array<winrt::hstring> values;

            propertyValue.GetStringArray(values);

            std::string result;

            for (auto const& entry : values)
            {
                if (!result.empty())
                {
                    result += ", ";
                }

                result += ToUtf8(entry);
            }

            return result;
        }

        default:
        {
            auto const stringable = value.try_as<foundation::IStringable>();

            return stringable == nullptr
                ? fmt::format("({})", static_cast<int>(propertyValue.Type()))
                : ToUtf8(stringable.ToString());
        }
        }
    }
}
