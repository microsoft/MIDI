// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midi2console
{
    // Value formatters shared by every command so that a guid, a boolean or a timestamp looks
    // the same wherever it appears.

    std::string FormatBoolean(_In_ bool value);
    fmt::text_style BooleanStyle(_In_ bool value);

    std::string FormatGuid(_In_ winrt::guid const& value);
    winrt::guid ParseGuid(_In_ std::string_view value, _Out_ bool& succeeded);

    std::string FormatNumberWithSeparators(_In_ uint64_t value);
    std::string FormatDecimal(_In_ double value, _In_ int decimalPlaces);

    std::string FormatDateTime(_In_ foundation::DateTime const& value);

    std::string FormatEndpointPurpose(_In_ midi2enum::MidiEndpointDevicePurpose purpose);
    std::string FormatNativeDataFormat(_In_ midi2enum::MidiEndpointNativeDataFormat format);
    std::string FormatProtocol(_In_ midi2enum::MidiProtocol protocol);
    std::string FormatFunctionBlockDirection(_In_ midi2enum::MidiFunctionBlockDirection direction);    std::string FormatRepresentsMidi10Connection(_In_ midi2enum::MidiFunctionBlockRepresentsMidi10Connection value);    std::string FormatFunctionBlockUIHint(_In_ midi2enum::MidiFunctionBlockUIHint hint);
    std::string FormatGroupTerminalBlockDirection(_In_ midi2enum::MidiGroupTerminalBlockDirection direction);
    std::string FormatPortFlow(_In_ midi2enum::Midi1PortFlow flow);
    std::string FormatPortNamingApproach(_In_ midi2enum::Midi1PortNamingApproach approach);

    // "Group 1" style label, and "Groups 1-4" when the span is more than one.
    std::string FormatGroupSpan(_In_ uint8_t firstGroupIndex, _In_ uint8_t groupCount);

    std::string ToLowerCopy(_In_ std::string_view value);
    std::string TrimCopy(_In_ std::string_view value);
    bool EqualsIgnoreCase(_In_ std::string_view left, _In_ std::string_view right);

    // Decodes a raw PnP property value. The returned string may already contain styling escape
    // sequences (byte arrays colorize their printable characters), so pass it to WriteField with
    // an empty style rather than restyling it.
    std::string FormatPropertyValue(_In_ winrt::Windows::Foundation::IInspectable const& value);
}
