// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

#include "EndpointSettings.h"
#include "EndpointSettingsTests.h"

using namespace WEX::Logging;

namespace custom = ::midiapp::customizations;
namespace json = ::winrt::Windows::Data::Json;
namespace midi2enum = ::winrt::Windows::Devices::Midi2::Enumeration;
namespace midi2config = ::winrt::Windows::Devices::Midi2::ServiceConfig;

namespace
{
    winrt::hstring const EndpointA{ LR"(\\?\swd#midisrv#midiu_ksa_111#{e7cce071-3c03-423f-88d3-f1045d02552b})" };
    winrt::hstring const EndpointB{ LR"(\\?\swd#midisrv#midiu_ksa_222#{e7cce071-3c03-423f-88d3-f1045d02552b})" };

    // ids reach the app in different cases from different sources
    winrt::hstring const EndpointAUpperCase{ LR"(\\?\SWD#MIDISRV#MIDIU_KSA_111#{E7CCE071-3C03-423F-88D3-F1045D02552B})" };

    winrt::guid const TransportId{ 0x3a61d14c, 0x1aa4, 0x4b7f, { 0x9c, 0x2e, 0x4d, 0x6b, 0x8e, 0x52, 0x1f, 0x07 } };

    constexpr auto Source = midi2enum::Midi1PortFlow::MidiMessageSource;
    constexpr auto Destination = midi2enum::Midi1PortFlow::MidiMessageDestination;

    using PortList = std::initializer_list<std::pair<int, PCWSTR>>;

    // Group indexes as plain numbers, which in a map literal would narrow inside std::pair
    std::map<uint8_t, winrt::hstring> Ports(_In_ PortList const ports)
    {
        std::map<uint8_t, winrt::hstring> result{};

        for (auto const& port : ports)
        {
            result[static_cast<uint8_t>(port.first)] = winrt::hstring{ port.second };
        }

        return result;
    }

    custom::EndpointSettings SettingsFor(
        _In_ winrt::hstring const& endpointDeviceId,
        _In_ PortList const sources,
        _In_ PortList const destinations = {})
    {
        custom::EndpointSettings settings{};
        settings.EndpointDeviceId = endpointDeviceId;
        settings.SourcePortNames = Ports(sources);
        settings.DestinationPortNames = Ports(destinations);

        return settings;
    }

    // input 1 was "Synth A" and the customer emptied it, keeping input 2 as "Keys"
    void ClearInput1(_Inout_ custom::ClearedPortNames& list, _In_ winrt::hstring const& endpointDeviceId)
    {
        list.Saved(SettingsFor(endpointDeviceId, { { 1, L"Keys" } }), { { Source, 0, L"Synth A" } });
    }

    json::JsonObject CustomPropertiesOf(_In_ midi2config::MidiServiceEndpointCustomizationConfig const& config)
    {
        auto const root = config.ConfigJson();
        VERIFY_IS_TRUE(root != nullptr);

        auto const transports = root.GetNamedObject(L"endpointTransportPluginSettings");
        VERIFY_ARE_EQUAL(transports.Size(), (uint32_t)1);

        auto const update = transports.First().Current().Value().GetObject().GetNamedArray(L"update");
        VERIFY_ARE_EQUAL(update.Size(), (uint32_t)1);

        return update.GetObjectAt(0).GetNamedObject(L"customProperties");
    }

    void VerifyPort(
        _In_ json::JsonArray const& ports,
        _In_ uint32_t const index,
        _In_ double const groupIndex,
        _In_z_ wchar_t const* name)
    {
        auto const port = ports.GetObjectAt(index);

        VERIFY_ARE_EQUAL(port.GetNamedNumber(L"groupIndex"), groupIndex);
        VERIFY_ARE_EQUAL(port.GetNamedString(L"name"), winrt::hstring{ name });
    }
}


void EndpointSettingsTests::TestEveryNamedPortIsSentNotJustTheChangedOnes()
{
    // Input 1 was named at an earlier save and left alone, input 2 is named now. Sending only
    // input 2 replaced the stored list and lost input 1 at the next restart.
    std::vector<custom::PortNameRow> const rows{
        { 0, L"Synth A", L"Synth A" },
        { 1, L"Synth B", L"" },
        { 2, L"", L"" } };

    std::map<uint8_t, winrt::hstring> names{};
    std::vector<custom::ClearedPortName> cleared{};

    custom::CollectPortNames(rows, Source, names, cleared);

    VERIFY_ARE_EQUAL(names.size(), (size_t)2);
    VERIFY_ARE_EQUAL(names.at(0), winrt::hstring{ L"Synth A" });
    VERIFY_ARE_EQUAL(names.at(1), winrt::hstring{ L"Synth B" });
    VERIFY_ARE_EQUAL(cleared.size(), (size_t)0);
}


void EndpointSettingsTests::TestEmptiedPortIsSentAsCleared()
{
    std::vector<custom::PortNameRow> const rows{
        { 0, L"", L"Synth A" },
        { 1, L"Synth B", L"Synth B" } };

    std::map<uint8_t, winrt::hstring> names{};
    std::vector<custom::ClearedPortName> cleared{};

    custom::CollectPortNames(rows, Destination, names, cleared);

    VERIFY_ARE_EQUAL(names.size(), (size_t)1);
    VERIFY_ARE_EQUAL(names.at(1), winrt::hstring{ L"Synth B" });

    VERIFY_ARE_EQUAL(cleared.size(), (size_t)1);
    VERIFY_IS_TRUE(cleared[0].Flow == Destination);
    VERIFY_ARE_EQUAL((uint32_t)cleared[0].GroupIndex, (uint32_t)0);
    VERIFY_ARE_EQUAL(cleared[0].Name, winrt::hstring{ L"Synth A" });
}


void EndpointSettingsTests::TestUntouchedEmptyPortIsLeftOut()
{
    std::vector<custom::PortNameRow> const rows{ { 3, L"", L"" } };

    // filled beforehand, to show the names are replaced rather than added to
    auto names = Ports({ { 9, L"Old" } });
    std::vector<custom::ClearedPortName> cleared{};

    custom::CollectPortNames(rows, Source, names, cleared);

    VERIFY_ARE_EQUAL(names.size(), (size_t)0);
    VERIFY_ARE_EQUAL(cleared.size(), (size_t)0);
}


void EndpointSettingsTests::TestSpacesOnlyCountAsEmpty()
{
    std::vector<custom::PortNameRow> const rows{
        { 0, L"   ", L"Synth A" },
        { 1, L"  Keys  ", L"" } };

    std::map<uint8_t, winrt::hstring> names{};
    std::vector<custom::ClearedPortName> cleared{};

    custom::CollectPortNames(rows, Source, names, cleared);

    VERIFY_ARE_EQUAL(names.size(), (size_t)1);
    VERIFY_ARE_EQUAL(names.at(1), winrt::hstring{ L"Keys" });

    VERIFY_ARE_EQUAL(cleared.size(), (size_t)1);
    VERIFY_ARE_EQUAL((uint32_t)cleared[0].GroupIndex, (uint32_t)0);
}


void EndpointSettingsTests::TestClearedNameIsLeftOutWhileTheEndpointStillShowsIt()
{
    custom::ClearedPortNames list{};
    ClearInput1(list, EndpointA);

    // until the service restarts, the endpoint still reports the name the customer cleared
    auto settings = SettingsFor(EndpointAUpperCase, { { 0, L"Synth A" }, { 1, L"Keys" } });

    list.RemoveFrom(settings);

    VERIFY_ARE_EQUAL(settings.SourcePortNames.size(), (size_t)1);
    VERIFY_IS_TRUE(settings.SourcePortNames.find(0) == settings.SourcePortNames.end());
    VERIFY_ARE_EQUAL(settings.SourcePortNames.at(1), winrt::hstring{ L"Keys" });

    // still needed the next time the endpoint is read
    VERIFY_ARE_EQUAL(list.Count(), (size_t)1);
}


void EndpointSettingsTests::TestClearedNameIsForgottenOnceTheEndpointStopsShowingIt()
{
    custom::ClearedPortNames list{};
    ClearInput1(list, EndpointA);

    // after a restart the endpoint no longer has the name, so there is nothing left to hide
    auto restarted = SettingsFor(EndpointA, { { 1, L"Keys" } });
    list.RemoveFrom(restarted);

    VERIFY_ARE_EQUAL(list.Count(), (size_t)0);

    // the same name set again later, by any tool, is then shown
    auto renamed = SettingsFor(EndpointA, { { 0, L"Synth A" } });
    list.RemoveFrom(renamed);

    VERIFY_ARE_EQUAL(renamed.SourcePortNames.at(0), winrt::hstring{ L"Synth A" });

    // a port showing a different name has been renamed elsewhere, so it is forgotten too
    ClearInput1(list, EndpointA);

    auto renamedElsewhere = SettingsFor(EndpointA, { { 0, L"Lead" } });
    list.RemoveFrom(renamedElsewhere);

    VERIFY_ARE_EQUAL(renamedElsewhere.SourcePortNames.at(0), winrt::hstring{ L"Lead" });
    VERIFY_ARE_EQUAL(list.Count(), (size_t)0);
}


void EndpointSettingsTests::TestNamingAClearedPortAgainForgetsIt()
{
    custom::ClearedPortNames list{};
    ClearInput1(list, EndpointA);

    // the customer types the same name back in and saves before the service restarts
    list.Saved(SettingsFor(EndpointA, { { 0, L"Synth A" }, { 1, L"Keys" } }), {});

    VERIFY_ARE_EQUAL(list.Count(), (size_t)0);

    auto settings = SettingsFor(EndpointA, { { 0, L"Synth A" }, { 1, L"Keys" } });
    list.RemoveFrom(settings);

    VERIFY_ARE_EQUAL(settings.SourcePortNames.at(0), winrt::hstring{ L"Synth A" });
}


void EndpointSettingsTests::TestOtherEndpointsAndDirectionsAreLeftAlone()
{
    custom::ClearedPortNames list{};
    ClearInput1(list, EndpointA);

    // the same group and name on another endpoint
    auto other = SettingsFor(EndpointB, { { 0, L"Synth A" } });
    list.RemoveFrom(other);

    VERIFY_ARE_EQUAL(other.SourcePortNames.at(0), winrt::hstring{ L"Synth A" });

    // and on an output of the same endpoint, while its input still shows the cleared name
    auto sameEndpoint = SettingsFor(EndpointA, { { 0, L"Synth A" } }, { { 0, L"Synth A" } });
    list.RemoveFrom(sameEndpoint);

    VERIFY_IS_TRUE(sameEndpoint.SourcePortNames.empty());
    VERIFY_ARE_EQUAL(sameEndpoint.DestinationPortNames.at(0), winrt::hstring{ L"Synth A" });
}


void EndpointSettingsTests::TestClearedNamesSurviveARoundTrip()
{
    custom::ClearedPortNames original{};

    // a tab inside a name is fine, because the name is the last field
    original.Saved(SettingsFor(EndpointA, {}), {
        { Source, 0, L"Synth A" },
        { Destination, 15, L"Tab\tName" },
        { Source, 5, L"Line\nBreak" } });

    original.Saved(SettingsFor(EndpointB, {}), { { Source, 3, L"Pads" } });

    custom::ClearedPortNames loaded{};
    loaded.Load(original.Serialize());

    // a name holding a line break would break the format, so it is only kept in memory
    VERIFY_ARE_EQUAL(loaded.Count(), (size_t)3);

    auto a = SettingsFor(EndpointA, { { 0, L"Synth A" } }, { { 15, L"Tab\tName" } });
    loaded.RemoveFrom(a);

    VERIFY_IS_TRUE(a.SourcePortNames.empty());
    VERIFY_IS_TRUE(a.DestinationPortNames.empty());

    auto b = SettingsFor(EndpointB, { { 3, L"Pads" } });
    loaded.RemoveFrom(b);

    VERIFY_IS_TRUE(b.SourcePortNames.empty());
}


void EndpointSettingsTests::TestMalformedStoredTextIsIgnored()
{
    // the stored text is in the customer's own registry, so anything in it has to be survivable
    custom::ClearedPortNames list{};

    list.Load(
        L"not a line\n"
        L"S\t99\tid\tname\n"
        L"S\t16\tid\tname\n"
        L"X\t1\tid\tname\n"
        L"S\t1a\tid\tname\n"
        L"S\t\tid\tname\n"
        L"S\t1\t\tname\n"
        L"S\t1\tid\t\n"
        L"S\t1\tid\n"
        L"\n"
        L"D\t2\tid\tGood\n");

    VERIFY_ARE_EQUAL(list.Count(), (size_t)1);

    auto settings = SettingsFor(L"id", {}, { { 2, L"Good" } });
    list.RemoveFrom(settings);

    VERIFY_IS_TRUE(settings.DestinationPortNames.empty());

    list.Load(L"");
    VERIFY_ARE_EQUAL(list.Count(), (size_t)0);
}


void EndpointSettingsTests::TestClearedNamesAreCapped()
{
    custom::ClearedPortNames list{};

    for (uint32_t i = 0; i < 70; i++)
    {
        list.Saved(SettingsFor(winrt::hstring{ L"endpoint " + std::to_wstring(i) }, {}), { { Source, 0, L"Name" } });
    }

    VERIFY_ARE_EQUAL(list.Count(), (size_t)64);

    // the oldest went first
    auto oldest = SettingsFor(L"endpoint 0", { { 0, L"Name" } });
    list.RemoveFrom(oldest);

    VERIFY_ARE_EQUAL(oldest.SourcePortNames.size(), (size_t)1);

    auto newest = SettingsFor(L"endpoint 69", { { 0, L"Name" } });
    list.RemoveFrom(newest);

    VERIFY_IS_TRUE(newest.SourcePortNames.empty());
}


void EndpointSettingsTests::TestCopyIntoSendsEverySetting()
{
    custom::EndpointSettings settings{};
    settings.EndpointDeviceId = EndpointA;
    settings.Name = L"My Synth";
    settings.Description = L"Rack 2";
    settings.ImageFileName = L"ep-synth.png";
    settings.RequiresNoteOffTranslation = true;
    settings.SupportsMidiPolyphonicExpression = true;
    settings.RecommendedControlChangeIntervalMilliseconds = 20;
    settings.OutgoingLatencyTicks = 49152;
    settings.Midi1PortNamingApproach = midi2enum::Midi1PortNamingApproach::UseNewStyle;
    settings.SourcePortNames = Ports({ { 0, L"Synth A" }, { 1, L"Synth B" } });
    settings.DestinationPortNames = Ports({ { 2, L"Pads" } });

    midi2config::MidiServiceEndpointCustomizationConfig config{ TransportId };
    custom::CopyInto(settings, config);

    auto const props = CustomPropertiesOf(config);

    VERIFY_ARE_EQUAL(props.GetNamedString(L"name"), winrt::hstring{ L"My Synth" });
    VERIFY_ARE_EQUAL(props.GetNamedString(L"description"), winrt::hstring{ L"Rack 2" });
    VERIFY_ARE_EQUAL(props.GetNamedString(L"image"), winrt::hstring{ L"ep-synth.png" });

    VERIFY_IS_TRUE(props.GetNamedBoolean(L"requiresNoteOffTranslation"));
    VERIFY_IS_TRUE(props.GetNamedBoolean(L"supportsMidiPolyphonicExpression"));
    VERIFY_ARE_EQUAL(props.GetNamedNumber(L"recommendedControlChangeIntervalMilliseconds"), 20.0);

    // a measured latency and the naming style used to go back to zero and "default"
    VERIFY_ARE_EQUAL(props.GetNamedNumber(L"outgoingLatencyTicks"), 49152.0);

    auto const ports = props.GetNamedObject(L"midi1Ports");

    VERIFY_ARE_EQUAL(ports.GetNamedString(L"namingApproach"), winrt::hstring{ L"newStyle" });

    auto const sources = ports.GetNamedArray(L"sources");
    VERIFY_ARE_EQUAL(sources.Size(), (uint32_t)2);
    VerifyPort(sources, 0, 0.0, L"Synth A");
    VerifyPort(sources, 1, 1.0, L"Synth B");

    auto const destinations = ports.GetNamedArray(L"destinations");
    VERIFY_ARE_EQUAL(destinations.Size(), (uint32_t)1);
    VerifyPort(destinations, 0, 2.0, L"Pads");
}


void EndpointSettingsTests::TestCopyIntoLeavesEmptyDisplayValuesOut()
{
    // The port names dialog sends the endpoint's own name, description and picture back. Where
    // the endpoint shows none, sending an empty one would delete a stored value.
    custom::EndpointSettings settings{};
    settings.EndpointDeviceId = EndpointA;
    settings.SourcePortNames = Ports({ { 0, L"Synth A" } });

    midi2config::MidiServiceEndpointCustomizationConfig config{ TransportId };
    custom::CopyInto(settings, config);

    auto const props = CustomPropertiesOf(config);

    VERIFY_IS_FALSE(props.HasKey(L"name"));
    VERIFY_IS_FALSE(props.HasKey(L"description"));
    VERIFY_IS_FALSE(props.HasKey(L"image"));
}


void EndpointSettingsTests::TestCopyIntoDoesNotChooseWhetherToUseTheLatency()
{
    // Left out, the saved choice stays as it was. The endpoint cannot say whether the choice was
    // made or worked out from the value, so sending one could overrule the customer.
    custom::EndpointSettings settings{};
    settings.EndpointDeviceId = EndpointA;
    settings.OutgoingLatencyTicks = 49152;

    midi2config::MidiServiceEndpointCustomizationConfig config{ TransportId };
    custom::CopyInto(settings, config);

    auto const props = CustomPropertiesOf(config);

    VERIFY_ARE_EQUAL(props.GetNamedNumber(L"outgoingLatencyTicks"), 49152.0);
    VERIFY_IS_FALSE(props.HasKey(L"useCustomOutgoingLatency"));
}


void EndpointSettingsTests::TestClearedPortIsSentEmpty()
{
    // the empty name is what makes the service clear the port rather than leave it as it was
    custom::EndpointSettings settings{};
    settings.EndpointDeviceId = EndpointA;
    settings.SourcePortNames = Ports({ { 1, L"Keys" } });

    midi2config::MidiServiceEndpointCustomizationConfig config{ TransportId };
    custom::CopyInto(settings, config);
    custom::AddClearedPortNames({ { Source, 0, L"Synth A" }, { Destination, 3, L"Pads" } }, config);

    auto const ports = CustomPropertiesOf(config).GetNamedObject(L"midi1Ports");

    auto const sources = ports.GetNamedArray(L"sources");
    VERIFY_ARE_EQUAL(sources.Size(), (uint32_t)2);
    VerifyPort(sources, 0, 0.0, L"");
    VerifyPort(sources, 1, 1.0, L"Keys");

    auto const destinations = ports.GetNamedArray(L"destinations");
    VERIFY_ARE_EQUAL(destinations.Size(), (uint32_t)1);
    VerifyPort(destinations, 0, 3.0, L"");

    // with no named port left, the list sent still replaces the stored one
    midi2config::MidiServiceEndpointCustomizationConfig lastPort{ TransportId };
    custom::CopyInto(custom::EndpointSettings{}, lastPort);
    custom::AddClearedPortNames({ { Source, 1, L"Keys" } }, lastPort);

    auto const remaining = CustomPropertiesOf(lastPort).GetNamedObject(L"midi1Ports").GetNamedArray(L"sources");
    VERIFY_ARE_EQUAL(remaining.Size(), (uint32_t)1);
    VerifyPort(remaining, 0, 1.0, L"");
}


void EndpointSettingsTests::TestEverythingButTheDisplayValuesCountsAsContent()
{
    custom::EndpointSettings displayOnly{};
    displayOnly.Name = L"My Synth";
    displayOnly.Description = L"Rack 2";
    displayOnly.ImageFileName = L"ep-synth.png";

    VERIFY_IS_FALSE(custom::HasNonDisplayContent(displayOnly));

    auto noteOff = displayOnly;
    noteOff.RequiresNoteOffTranslation = true;
    VERIFY_IS_TRUE(custom::HasNonDisplayContent(noteOff));

    auto mpe = displayOnly;
    mpe.SupportsMidiPolyphonicExpression = true;
    VERIFY_IS_TRUE(custom::HasNonDisplayContent(mpe));

    auto interval = displayOnly;
    interval.RecommendedControlChangeIntervalMilliseconds = 20;
    VERIFY_IS_TRUE(custom::HasNonDisplayContent(interval));

    // a device can run early once the devices around it are compensated
    auto latency = displayOnly;
    latency.OutgoingLatencyTicks = -100;
    VERIFY_IS_TRUE(custom::HasNonDisplayContent(latency));

    auto naming = displayOnly;
    naming.Midi1PortNamingApproach = midi2enum::Midi1PortNamingApproach::UseClassicCompatible;
    VERIFY_IS_TRUE(custom::HasNonDisplayContent(naming));

    auto input = displayOnly;
    input.SourcePortNames = Ports({ { 0, L"Synth A" } });
    VERIFY_IS_TRUE(custom::HasNonDisplayContent(input));

    auto output = displayOnly;
    output.DestinationPortNames = Ports({ { 0, L"Pads" } });
    VERIFY_IS_TRUE(custom::HasNonDisplayContent(output));
}
