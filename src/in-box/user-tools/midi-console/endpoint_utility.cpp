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
#include "endpoint_utility.h"
#include "midi_formatting.h"

namespace midi2console
{
    std::string GetEndpointIcon(_In_ midi2enum::MidiEndpointDevicePurpose purpose)
    {
        // Every icon here must be an Emoji_Presentation code point so it occupies two terminal
        // cells. Single-cell pictographs such as U+2699 GEAR and U+1F6E0 HAMMER AND WRENCH break
        // the column alignment of the picker, which is why they are deliberately not used.
        // Code points newer than FTXUI's width tables have the same effect: U+1F6DC WIRELESS
        // (Unicode 15.0) measures as one cell but draws as two, shifting the row by one.
        switch (purpose)
        {
        case midi2enum::MidiEndpointDevicePurpose::DiagnosticPing:
            return "\U0001F4E1";                                // satellite antenna
        case midi2enum::MidiEndpointDevicePurpose::DiagnosticLoopback:
            return "\U0001F9EA";                                // test tube
        case midi2enum::MidiEndpointDevicePurpose::VirtualDeviceResponder:
            return "\U0001F4BB";                                // laptop
        case midi2enum::MidiEndpointDevicePurpose::InBoxGeneralMidiSynth:
            return "\U0001F50A";                                // speaker
        case midi2enum::MidiEndpointDevicePurpose::NormalMessageEndpoint:
            return "\U0001F3B9";                                // musical keyboard
        default:
            return "\U0001F3B5";                                // musical note
        }
    }

    std::string GetEndpointIcon(_In_ midi2enum::MidiEndpointDeviceInformation const& device)
    {
        auto const transportCode = ToLowerCopy(ToUtf8(device.GetTransportSuppliedInfo().TransportCode()));

        if (transportCode == "loop")     return "\U0001F504";   // counterclockwise arrows
        if (transportCode == "bloop")    return "\U0001F517";   // link
        if (transportCode == "net2udp")  return "\U0001F310";   // globe with meridians
        if (transportCode == "blemidi")  return "\U0001F4F6";   // antenna bars

        return GetEndpointIcon(device.EndpointPurpose());
    }

    midi2enum::MidiEndpointDeviceInformationFilters BuildEndpointFilters(
        _In_ bool includeDiagnosticLoopback,
        _In_ bool includeAll)
    {
        auto filters =
            midi2enum::MidiEndpointDeviceInformationFilters::StandardNativeMidi1ByteFormat |
            midi2enum::MidiEndpointDeviceInformationFilters::StandardNativeUniversalMidiPacketFormat;

        if (includeAll)
        {
            filters |=
                midi2enum::MidiEndpointDeviceInformationFilters::DiagnosticLoopback |
                midi2enum::MidiEndpointDeviceInformationFilters::DiagnosticPing |
                midi2enum::MidiEndpointDeviceInformationFilters::VirtualDeviceResponder;
        }
        else if (includeDiagnosticLoopback)
        {
            filters |= midi2enum::MidiEndpointDeviceInformationFilters::DiagnosticLoopback;
        }

        return filters;
    }

    collections::IVectorView<midi2enum::MidiEndpointDeviceInformation> EnumerateEndpoints(
        _In_ midi2enum::MidiEndpointDeviceInformationFilters filters)
    {
        return midi2enum::MidiEndpointDeviceInformation::FindAll(
            midi2enum::MidiEndpointDeviceInformationSortOrder::Name,
            filters);
    }

    std::string GetEndpointNameFromEndpointDeviceId(_In_ std::string const& endpointDeviceId)
    {
        try
        {
            auto const info = midi2enum::MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(
                winrt::hstring{ FromUtf8(endpointDeviceId) });

            if (info != nullptr)
            {
                return ToUtf8(info.Name());
            }
        }
        CATCH_LOG();

        return {};
    }

    FriendlyTimeUnit ConvertTicksToFriendlyTimeUnit(_In_ uint64_t ticks)
    {
        constexpr double secondsPerMinute = 60.0;
        constexpr double secondsPerHour = secondsPerMinute * 60.0;
        constexpr double secondsPerDay = secondsPerHour * 24.0;
        constexpr double secondsPerYear = secondsPerDay * 365.0;

        auto const frequency = static_cast<double>(midi2::MidiClock::TimestampFrequency());

        double const ticksPerSecond = frequency;
        double const ticksPerMinute = frequency * secondsPerMinute;
        double const ticksPerHour = frequency * secondsPerHour;
        double const ticksPerDay = frequency * secondsPerDay;
        double const ticksPerYear = frequency * secondsPerYear;
        double const ticksPerMillisecond = frequency / 1000.0;
        double const ticksPerMicrosecond = frequency / 1000000.0;
        double const nanosecondsPerTick = 1000000000.0 / frequency;

        auto const value = static_cast<double>(ticks);

        if (ticks == 0)                      return { 0.0, "--" };
        if (value > ticksPerYear)            return { value / ticksPerYear, "yy" };
        if (value > ticksPerDay)             return { value / ticksPerDay, "dd" };
        if (value > ticksPerHour)            return { value / ticksPerHour, "hr" };
        if (value > ticksPerMinute)          return { value / ticksPerMinute, "mn" };
        if (value > ticksPerSecond)          return { value / ticksPerSecond, "s" };
        if (value > ticksPerMillisecond)     return { value / ticksPerMillisecond, "ms" };
        if (value > ticksPerMicrosecond)     return { value / ticksPerMicrosecond, "\u03BCs" };

        return { value * nanosecondsPerTick, "ns" };
    }
}
