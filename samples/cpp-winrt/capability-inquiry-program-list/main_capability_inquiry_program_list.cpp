// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: reading the channel list and the program lists a device
// publishes through MIDI Capability Inquiry, and using an entry from a program
// list to choose the sound on a channel.
//
// The General MIDI synthesizer that comes with Windows MIDI Services publishes
// both. Its channel list says what each channel is playing, and which program
// list that channel chooses from: one list of instruments, and one of drum kits
// for channel 10. A workstation with factory, user and expansion banks has the
// same shape, so code written against the synthesizer carries over.
//
// The capability-inquiry-browse sample walks everything a device publishes.
// This one goes further into these two lists. It sends one program change,
// which makes no sound, and puts the channel back afterward.

#include <io.h>
#include <fcntl.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <algorithm>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.CapabilityInquiry.h>
#include <winrt/Windows.Devices.Midi2.Transports.Synth.h>
#include <winrt/Windows.Devices.Midi2.Utilities.Messages.h>

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::CapabilityInquiry;
using namespace winrt::Windows::Devices::Midi2::Transports::Synth;
using namespace winrt::Windows::Devices::Midi2::Utilities::Messages;


// The endpoint to ask. Leave empty to use the General MIDI synthesizer.
const winrt::hstring EndpointId = L"";

// How many program list entries to ask for at a time.
constexpr uint32_t ProgramListPageSize = 64;

// The channel to choose a sound for, and the category to choose it from.
// Channel numbers in a channel list start at 1.
constexpr uint16_t ChannelToChange = 1;
constexpr std::wstring_view CategoryToChoose{ L"Organ" };


// Bank select MSB, bank select LSB and program change, exactly as they are sent.
template <typename Entry>
std::wstring ProgramNumbers(Entry const& entry)
{
    std::wostringstream text;

    text << L"[" << std::setw(3) << static_cast<int>(entry.BankMsb())
        << L" " << std::setw(3) << static_cast<int>(entry.BankLsb())
        << L" " << std::setw(3) << static_cast<int>(entry.ProgramChange()) << L"]";

    return text.str();
}

// A sound set often gives a program and its variations the same title. The
// tags are what tell them apart.
std::wstring DisplayName(MidiProgramListEntry const& entry)
{
    std::wstring name{ entry.Title() };

    auto const tags = entry.Tags();

    for (uint32_t index = 0; index < tags.Size(); index++)
    {
        name += index == 0 ? L" (" : L", ";
        name += tags.GetAt(index);
    }

    return tags.Size() > 0 ? name + L")" : name;
}

// The program list a channel chooses from, or null when it names none.
MidiResourceLink ProgramListLinkFor(MidiChannelListEntry const& entry)
{
    for (auto const& link : entry.Links())
    {
        if (link.Resource() == L"ProgramList")
        {
            return link;
        }
    }

    return nullptr;
}

// Reads a whole program list a page at a time. GetProgramListAsync runs this
// same loop for you. Page it yourself when you want to show the first entries
// while the rest are still on their way.
std::vector<MidiProgramListEntry> ReadProgramList(
    MidiCapabilityInquirySession const& capabilityInquiry,
    MidiUniqueId const& responder,
    winrt::hstring const& resourceId)
{
    std::vector<MidiProgramListEntry> entries;

    uint32_t offset{ 0 };

    while (true)
    {
        auto const page = capabilityInquiry.GetProgramListPageAsync(responder, resourceId, offset, ProgramListPageSize).get();

        // Null when the device did not answer, or turned the request down.
        if (page == nullptr || page.Entries().Size() == 0)
        {
            break;
        }

        for (auto const& entry : page.Entries())
        {
            entries.push_back(entry);
        }

        std::wcout << L"    " << entries.size() << L" of " << page.TotalCount() << std::endl;

        // A device that does not page sends the whole list at once, and says
        // there is no more.
        if (!page.HasMoreEntries())
        {
            break;
        }

        offset = page.NextOffset();
    }

    return entries;
}

// Categories are optional, and a device may use names of its own, so treat
// them as text to show, and be ready for entries that have none.
void PrintByCategory(std::vector<MidiProgramListEntry> const& entries)
{
    std::vector<std::pair<std::wstring, std::vector<MidiProgramListEntry>>> groups;

    for (auto const& entry : entries)
    {
        auto const categories = entry.Categories();

        std::wstring const category = categories.Size() > 0 ? std::wstring{ categories.GetAt(0) } : L"(none)";

        auto group = std::find_if(groups.begin(), groups.end(),
            [&category](auto const& existing) { return existing.first == category; });

        if (group == groups.end())
        {
            group = groups.insert(groups.end(), { category, {} });
        }

        group->second.push_back(entry);
    }

    for (auto const& [category, members] : groups)
    {
        std::wcout << L"    " << std::left << std::setw(22) << category << std::right
            << std::setw(4) << members.size() << L"  " << DisplayName(members[0]);

        if (members.size() > 1)
        {
            std::wcout << L", " << DisplayName(members[1]);
        }

        std::wcout << (members.size() > 2 ? L", ..." : L"") << std::endl;
    }
}

MidiProgramListEntry FindFirstInCategory(std::vector<MidiProgramListEntry> const& entries, std::wstring_view const category)
{
    for (auto const& entry : entries)
    {
        for (auto const& name : entry.Categories())
        {
            if (std::wstring_view{ name } == category)
            {
                return entry;
            }
        }
    }

    return nullptr;
}

// Bank select is two control changes, 0 for the high byte and 32 for the low,
// and it takes effect at the next program change. The values from a program
// list go on the wire exactly as they are. They are already zero based.
void SendProgram(
    MidiEndpointConnection const& connection,
    uint16_t const channelListNumber,
    uint8_t const bankMsb,
    uint8_t const bankLsb,
    uint8_t const program)
{
    // A channel list counts channels from 1 to 256, across all sixteen groups.
    // Split that into the group and the channel within it.
    MidiGroup const group{ static_cast<uint8_t>((channelListNumber - 1) / 16) };
    MidiChannel const channel{ static_cast<uint8_t>((channelListNumber - 1) % 16) };

    auto const now = MidiClock::TimestampConstantSendImmediately();

    connection.SendSingleMessagePacket(MidiMessageBuilder::BuildMidi1ChannelVoiceMessage(
        now, group, Midi1ChannelVoiceMessageStatus::ControlChange, channel, 0, bankMsb));

    connection.SendSingleMessagePacket(MidiMessageBuilder::BuildMidi1ChannelVoiceMessage(
        now, group, Midi1ChannelVoiceMessageStatus::ControlChange, channel, 32, bankLsb));

    connection.SendSingleMessagePacket(MidiMessageBuilder::BuildMidi1ChannelVoiceMessage(
        now, group, Midi1ChannelVoiceMessageStatus::ProgramChange, channel, program, 0));
}


int main()
{
    winrt::init_apartment();

    // Titles come from the device, in any language. Writing UTF-16 straight to
    // the console shows all of them, where the default narrow conversion stops
    // printing anything at the first character it cannot convert.
    (void)_setmode(_fileno(stdout), _O_U16TEXT);

    if (!MidiApi::EnsureServiceAvailable())
    {
        std::wcout << L"Could not demand-start the MIDI service." << std::endl;
        return 1;
    }

    // The synthesizer has exactly one endpoint, so it can be named without
    // enumerating. The id is empty when the synthesizer is switched off.
    auto const endpointId = EndpointId.empty() ? MidiSynthManager::EndpointDeviceId() : EndpointId;

    if (endpointId.empty())
    {
        std::wcout << L"The General MIDI synthesizer is not available. It may be switched off in MIDI Settings." << std::endl;
        return 1;
    }

    auto session = MidiSession::Create(L"Capability Inquiry Program List Sample");

    auto connection = session.CreateEndpointConnection(endpointId);

    if (connection == nullptr || !connection.Open())
    {
        std::wcout << L"Could not open a connection to " << endpointId.c_str() << std::endl;
        return 1;
    }

    auto capabilityInquiry = MidiCapabilityInquirySession::Create(connection);

    if (capabilityInquiry == nullptr)
    {
        std::wcout << L"Could not start a capability inquiry session." << std::endl;
        return 1;
    }

    std::wcout << L"Looking for devices..." << std::endl;

    MidiCapabilityInquiryResponder responder{ nullptr };

    for (auto const& found : capabilityInquiry.DiscoverAsync().get())
    {
        if (found.SupportsPropertyExchange())
        {
            responder = found;
            break;
        }
    }

    if (responder == nullptr)
    {
        std::wcout << L"Nothing on this endpoint publishes lists." << std::endl;
        return 0;
    }

    // Identifies the responder for as long as this session lasts. It changes
    // every time the device starts, so never store it.
    auto const muid = responder.Muid();

    // Checking the resource list first saves asking for something the device
    // can only refuse. A device that sends no resource list has not said it
    // lacks anything, so only rule a resource out when a list came back.
    auto const resources = capabilityInquiry.GetResourceListAsync(muid).get();

    if (resources != nullptr && !resources.SupportsResource(L"ChannelList"))
    {
        std::wcout << L"This device does not publish a channel list." << std::endl;
        return 0;
    }

    auto const channels = capabilityInquiry.GetChannelListAsync(muid).get();

    if (channels == nullptr)
    {
        std::wcout << L"The device did not send its channel list." << std::endl;
        return 0;
    }

    // A channel list is how things stood when it was sent. To hear about changes
    // someone else makes, such as a program change from a keyboard, subscribe to
    // it with SubscribeAsync when its resource list entry says CanSubscribe.
    std::wcout << std::endl << L" Channels" << std::endl;

    for (auto const& entry : channels.Entries())
    {
        auto const link = ProgramListLinkFor(entry);

        std::wcout << L"  " << std::setw(3) << entry.Channel() << L"  "
            << std::left << std::setw(16) << entry.ProgramTitle().c_str() << std::right
            << ProgramNumbers(entry)
            << L"  from " << (link == nullptr ? L"no list" : link.Title().c_str()) << std::endl;
    }

    // Each distinct list the channels point at, once, however many channels
    // share it.
    std::wcout << std::endl << L" Program lists" << std::endl;

    std::map<std::wstring, std::vector<MidiProgramListEntry>> programLists;

    for (auto const& link : channels.GetProgramListLinks())
    {
        std::wcout << L"  " << link.Title().c_str() << L" (" << link.ResourceId().c_str() << L")" << std::endl;

        auto entries = ReadProgramList(capabilityInquiry, muid, link.ResourceId());

        PrintByCategory(entries);

        programLists[std::wstring{ link.ResourceId() }] = std::move(entries);
    }

    // Choose a sound from the list the channel itself points at. A drum channel
    // points at drum kits, so this never offers it a piano.
    std::wcout << std::endl << L" Choosing a sound for channel " << ChannelToChange << std::endl;

    auto const before = channels.GetEntryForChannel(ChannelToChange);
    auto const link = before == nullptr ? nullptr : ProgramListLinkFor(before);

    if (link == nullptr)
    {
        std::wcout << L"  The device does not say which list this channel uses." << std::endl;
        return 0;
    }

    auto const& choices = programLists[std::wstring{ link.ResourceId() }];

    auto program = FindFirstInCategory(choices, CategoryToChoose);

    if (program == nullptr && !choices.empty())
    {
        program = choices.front();
    }

    if (program == nullptr)
    {
        std::wcout << L"  That list is empty." << std::endl;
        return 0;
    }

    std::wcout << L"  Channel " << ChannelToChange << L" plays " << before.ProgramTitle().c_str() << L" " << ProgramNumbers(before) << std::endl;
    std::wcout << L"  Sending " << DisplayName(program) << L" " << ProgramNumbers(program) << std::endl;

    SendProgram(connection, ChannelToChange, program.BankMsb(), program.BankLsb(), program.ProgramChange());

    // The device handles messages in the order they arrive, so a channel list
    // asked for after the program change already shows it.
    auto const updated = capabilityInquiry.GetChannelListAsync(muid).get();

    if (updated != nullptr)
    {
        if (auto const after = updated.GetEntryForChannel(ChannelToChange))
        {
            std::wcout << L"  Channel " << ChannelToChange << L" now plays " << after.ProgramTitle().c_str() << L" " << ProgramNumbers(after) << std::endl;
        }
    }

    // The synthesizer is shared by every application on the PC, so put the
    // channel back the way it was.
    SendProgram(connection, ChannelToChange, before.BankMsb(), before.BankLsb(), before.ProgramChange());

    std::wcout << L"  Put channel " << ChannelToChange << L" back to " << before.ProgramTitle().c_str() << std::endl;

    std::wcout << std::endl << L"Done." << std::endl;

    // Closing withdraws this session's identifier, which tells the device to
    // stop keeping track of it.
    capabilityInquiry.Close();
    session.Close();

    return 0;
}
