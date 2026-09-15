// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "CustomizationHelpers.h"
#include "StringResources.h"

namespace res = ::midisettings::resources;

namespace midiapp::customizations
{
    _Use_decl_annotations_
    midi2config::MidiServiceEndpointCustomization FindCustomizationForEndpoint(
        winrt::guid const& transportId,
        winrt::hstring const& endpointDeviceId) noexcept
    {
        try
        {
            for (auto const& customization :
                midi2config::MidiServiceTransportPluginConfigManager::GetEndpointCustomizations(transportId))
            {
                // endpoint interface ids differ only by case between callers
                if (::_wcsicmp(
                    std::wstring{ customization.ResolvedEndpointDeviceId() }.c_str(),
                    std::wstring{ endpointDeviceId }.c_str()) == 0)
                {
                    return customization;
                }
            }
        }
        CATCH_LOG()

        return nullptr;
    }


    _Use_decl_annotations_
    bool HasNonDisplayContent(midi2config::MidiServiceEndpointCustomization const& customization) noexcept
    {
        try
        {
            if (customization == nullptr)
            {
                return false;
            }

            return
                customization.RequiresNoteOffTranslation() ||
                customization.SupportsMidiPolyphonicExpression() ||
                customization.RecommendedControlChangeIntervalMilliseconds() != 0 ||
                customization.OutgoingLatencyTicks() != 0 ||
                customization.UseCustomOutgoingLatency() ||
                customization.Midi1PortNamingApproach() != midi2enum::Midi1PortNamingApproach::Default ||
                customization.Midi1SourcePortCustomNames().Size() > 0 ||
                customization.Midi1DestinationPortCustomNames().Size() > 0;
        }
        CATCH_LOG()

        return false;
    }


    _Use_decl_annotations_
    midi2config::MidiServiceEndpointCustomizationProvenance BuildProvenance(
        winrt::hstring const& endpointDeviceId,
        midi2config::MidiServiceEndpointCustomizationProvenance const& existing) noexcept
    {
        try
        {
            auto const endpoint = midi2enum::MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(endpointDeviceId);

            if (endpoint == nullptr)
            {
                return existing;
            }

            auto provenance = midi2config::MidiServiceEndpointCustomizationProvenance::CreateForEndpoint(endpoint);

            if (provenance == nullptr || existing == nullptr)
            {
                return provenance;
            }

            if (!existing.Created().empty())
            {
                provenance.Created(existing.Created());
            }

            if (existing.LatencySource() != midi2config::MidiCustomizationLatencySource::Unspecified)
            {
                provenance.LatencySource(existing.LatencySource());
                provenance.LatencyMeasured(existing.LatencyMeasured());
            }

            return provenance;
        }
        CATCH_LOG()

        return existing;
    }


    _Use_decl_annotations_
    winrt::hstring DescribeContent(midi2config::MidiServiceEndpointCustomization const& customization) noexcept
    {
        try
        {
            if (customization == nullptr)
            {
                return {};
            }

            std::vector<winrt::hstring> parts{};

            if (!customization.ImageFileName().empty())
            {
                parts.push_back(res::GetString(L"CustomizationContentImage"));
            }

            if (customization.OutgoingLatencyTicks() != 0)
            {
                auto const milliseconds =
                    static_cast<double>(customization.OutgoingLatencyTicks()) /
                    static_cast<double>(midi2::MidiClock::TimestampFrequency()) * 1000.0;

                auto const measured =
                    customization.Provenance() != nullptr &&
                    customization.Provenance().LatencySource() == midi2config::MidiCustomizationLatencySource::Measured;

                parts.push_back(res::FormatString(
                    measured ? L"CustomizationContentLatencyMeasuredFormat" : L"CustomizationContentLatencyFormat",
                    winrt::hstring{ std::format(L"{:.1f}", milliseconds) }));
            }

            auto const portNames =
                customization.Midi1SourcePortCustomNames().Size() +
                customization.Midi1DestinationPortCustomNames().Size();

            if (portNames > 0)
            {
                parts.push_back(res::FormatString(L"CustomizationContentPortNamesFormat",
                    winrt::hstring{ std::to_wstring(portNames) }));
            }

            if (customization.Midi1PortNamingApproach() != midi2enum::Midi1PortNamingApproach::Default)
            {
                parts.push_back(res::GetString(L"CustomizationContentPortNaming"));
            }

            if (customization.RequiresNoteOffTranslation())
            {
                parts.push_back(res::GetString(L"CustomizationContentNoteOff"));
            }

            if (customization.SupportsMidiPolyphonicExpression())
            {
                parts.push_back(res::GetString(L"CustomizationContentMpe"));
            }

            if (customization.RecommendedControlChangeIntervalMilliseconds() != 0)
            {
                parts.push_back(res::FormatString(L"CustomizationContentCcIntervalFormat",
                    winrt::hstring{ std::to_wstring(customization.RecommendedControlChangeIntervalMilliseconds()) }));
            }

            std::wstring result{};

            for (auto const& part : parts)
            {
                if (!result.empty())
                {
                    result += L"  ·  ";
                }

                result += part;
            }

            return winrt::hstring{ result };
        }
        CATCH_LOG()

        return {};
    }


    _Use_decl_annotations_
    void CopyInto(
        midi2config::MidiServiceEndpointCustomization const& source,
        midi2config::MidiServiceEndpointCustomizationConfig const& destination) noexcept
    {
        try
        {
            if (source == nullptr || destination == nullptr)
            {
                return;
            }

            destination.Name(source.Name());
            destination.Description(source.Description());
            destination.ImageFileName(source.ImageFileName());
            destination.ClearDisplayProperties(true);

            destination.RequiresNoteOffTranslation(source.RequiresNoteOffTranslation());
            destination.SupportsMidiPolyphonicExpression(source.SupportsMidiPolyphonicExpression());
            destination.RecommendedControlChangeIntervalMilliseconds(source.RecommendedControlChangeIntervalMilliseconds());
            destination.OutgoingLatencyTicks(source.OutgoingLatencyTicks());
            destination.UseCustomOutgoingLatency(source.UseCustomOutgoingLatency());
            destination.Midi1PortNamingApproach(source.Midi1PortNamingApproach());

            for (auto const& entry : source.Midi1SourcePortCustomNames())
            {
                destination.AddMidi1SourcePortCustomName(midi2::MidiGroup{ entry.Key() }, entry.Value());
            }

            for (auto const& entry : source.Midi1DestinationPortCustomNames())
            {
                destination.AddMidi1DestinationPortCustomName(midi2::MidiGroup{ entry.Key() }, entry.Value());
            }
        }
        CATCH_LOG()
    }
}
