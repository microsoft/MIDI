// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: asking a device what it can do, and reading the lists
// it publishes, using MIDI Capability Inquiry.
//
// Capability Inquiry is how a device tells you its manufacturer and model, what
// its channels are set to, and what patches it has, without you needing to know
// anything about that make of device. A patch browser in a sequencer is built
// on exactly this.
//
// The session does the awkward parts: it draws an identifier which must not
// survive a restart, numbers its own requests, puts chunked replies back
// together, asks for further pages of a long list until there are no more, and
// gives up on a device that is not going to answer. What is left here is the
// question you wanted to ask.
//
// Run it against the General MIDI synthesizer, which ships with Windows MIDI
// Services and answers all of this, or point it at a MIDI 2.0 instrument.

#include <iostream>
#include <iomanip>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.CapabilityInquiry.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.h>

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::CapabilityInquiry;
using namespace winrt::Windows::Devices::Midi2::Enumeration;


// The in-box General MIDI synthesizer. Replace this with any endpoint id, or
// leave it and run the sample as it stands.
const winrt::hstring EndpointId =
    L"\\\\?\\swd#midisrv#midiu_gmsynth_gm1#{e7cce071-3c03-423f-88d3-f1045d02552b}";


void PrintDeviceInfo(MidiDeviceInfo const& info)
{
    if (info == nullptr)
    {
        std::wcout << L"  The device did not publish DeviceInfo." << std::endl;
        return;
    }

    std::wcout << L"  Manufacturer : " << info.Manufacturer().c_str() << std::endl;
    std::wcout << L"  Family       : " << info.Family().c_str() << std::endl;
    std::wcout << L"  Model        : " << info.Model().c_str() << std::endl;
    std::wcout << L"  Version      : " << info.Version().c_str() << std::endl;
}

void PrintResourceList(MidiResourceList const& resources)
{
    if (resources == nullptr)
    {
        std::wcout << L"  The device did not publish a ResourceList." << std::endl;
        return;
    }

    for (auto const& entry : resources.Entries())
    {
        std::wcout << L"  " << std::left << std::setw(20) << entry.Resource().c_str();

        // CanSet is a string rather than a flag because the set is open: the
        // specification defines three values and a device may declare its own.
        if (entry.CanSet() != MidiResourceListEntry::CanSetNone())
        {
            std::wcout << L" writable(" << entry.CanSet().c_str() << L")";
        }

        if (entry.CanPaginate())
        {
            std::wcout << L" paged";
        }

        if (entry.CanSubscribe())
        {
            std::wcout << L" subscribable";
        }

        std::wcout << std::endl;
    }
}

void PrintChannelList(MidiChannelList const& channels)
{
    if (channels == nullptr)
    {
        std::wcout << L"  The device did not publish a ChannelList." << std::endl;
        return;
    }

    for (auto const& entry : channels.Entries())
    {
        // Channel numbers here run from 1 to 256, not 1 to 16: the
        // specification counts from the first channel of the first group and
        // runs across all sixteen groups.
        std::wcout << L"  Channel " << std::setw(3) << entry.Channel() << L"  "
            << std::left << std::setw(24) << entry.Title().c_str()
            << L"  " << entry.ProgramTitle().c_str() << std::endl;
    }
}

void PrintProgramList(MidiProgramList const& programs)
{
    if (programs == nullptr)
    {
        std::wcout << L"  The device did not publish a ProgramList." << std::endl;
        return;
    }

    std::wcout << L"  " << programs.Entries().Size() << L" programs" << std::endl;

    uint32_t shown{ 0 };

    for (auto const& entry : programs.Entries())
    {
        if (shown++ >= 20)
        {
            std::wcout << L"  ..." << std::endl;
            break;
        }

        // All three values go on the wire exactly as they arrive here. They are
        // zero based, which the specification's normative text requires even
        // though its own worked example is not.
        std::wcout << L"  "
            << std::setw(3) << (int)entry.BankMsb() << L" "
            << std::setw(3) << (int)entry.BankLsb() << L" "
            << std::setw(3) << (int)entry.ProgramChange() << L"  "
            << entry.Title().c_str() << std::endl;
    }
}


int main()
{
    winrt::init_apartment();

    if (!MidiApi::EnsureServiceAvailable())
    {
        std::wcout << L"Could not demand-start the MIDI service." << std::endl;
        return 1;
    }

    auto session = MidiSession::Create(L"Capability Inquiry Browse Sample");

    auto connection = session.CreateEndpointConnection(EndpointId);

    if (connection == nullptr || !connection.Open())
    {
        std::wcout << L"Could not open a connection to " << EndpointId.c_str() << std::endl;
        return 1;
    }

    // The session does not take ownership of the connection and does not close
    // it. Several sessions may share one connection, each with its own
    // identifier, which is how you talk to more than one function block on the
    // same endpoint.
    auto capabilityInquiry = MidiCapabilityInquirySession::Create(connection);

    if (capabilityInquiry == nullptr)
    {
        std::wcout << L"Could not start a capability inquiry session." << std::endl;
        return 1;
    }

    // A device announcing a change sends it to everybody, so these arrive
    // whether or not anything was asked for.
    capabilityInquiry.ProfileStateChanged([](auto&&, MidiCapabilityInquiryMessageReceivedEventArgs const& args)
        {
            std::wcout << L"Profile state changed: "
                << args.Message().ProfileId().ToString().c_str() << std::endl;
        });

    std::wcout << L"Looking for devices..." << std::endl;

    // Unlike every other request this waits out the whole timeout, because
    // there is no way to know how many devices are out there until they have
    // all had a chance to answer.
    auto const responders = capabilityInquiry.DiscoverAsync().get();

    if (responders.Size() == 0)
    {
        std::wcout << L"Nothing answered. This endpoint does not implement capability inquiry."
            << std::endl;
        return 0;
    }

    for (auto const& responder : responders)
    {
        std::wcout << std::endl
            << L"Responder " << responder.Muid().ToString().c_str()
            << L" on function block " << (int)responder.FunctionBlockNumber() << std::endl;

        std::wcout << L"  Receivable system exclusive size: "
            << responder.ReceivableMaximumSystemExclusiveSize() << L" bytes" << std::endl;

        if (responder.SupportsProfiles())
        {
            // 0x7F addresses the whole function block. A channel is 0x00 to
            // 0x0F and a group is 0x7E.
            auto const profiles = capabilityInquiry.GetProfilesAsync(responder.Muid(), 0x7F).get();

            std::wcout << std::endl << L" Profiles" << std::endl;

            for (auto const& profileId : profiles.EnabledProfiles())
            {
                std::wcout << L"  " << profileId.ToString().c_str() << L"  enabled" << std::endl;
            }

            for (auto const& profileId : profiles.DisabledProfiles())
            {
                std::wcout << L"  " << profileId.ToString().c_str() << L"  available" << std::endl;
            }
        }

        if (!responder.SupportsPropertyExchange())
        {
            std::wcout << L"  This responder does not offer property exchange." << std::endl;
            continue;
        }

        std::wcout << std::endl << L" Resources" << std::endl;
        PrintResourceList(capabilityInquiry.GetResourceListAsync(responder.Muid()).get());

        std::wcout << std::endl << L" Device info" << std::endl;
        PrintDeviceInfo(capabilityInquiry.GetDeviceInfoAsync(responder.Muid()).get());

        std::wcout << std::endl << L" Channels" << std::endl;

        auto const channels = capabilityInquiry.GetChannelListAsync(responder.Muid()).get();
        PrintChannelList(channels);

        std::wcout << std::endl << L" Programs" << std::endl;

        // A device may publish several program lists and point each channel at
        // the one it uses. Following the links is how you reach all of them,
        // and the list de-duplicates itself so a sixteen channel device is not
        // asked for the same collection sixteen times.
        bool askedForAList{ false };

        if (channels != nullptr)
        {
            for (auto const& link : channels.GetProgramListLinks())
            {
                std::wcout << L"  From " << link.Title().c_str()
                    << L" (" << link.ResourceId().c_str() << L")" << std::endl;

                // This asks for pages until the device says there are no more,
                // so what comes back is the whole list however long it is.
                PrintProgramList(
                    capabilityInquiry.GetProgramListAsync(responder.Muid(), link.ResourceId()).get());

                askedForAList = true;
            }
        }

        if (!askedForAList)
        {
            // A device with a single list does not need a channel list to point
            // at it, so ask for it directly rather than deciding it has none.
            PrintProgramList(capabilityInquiry.GetProgramListAsync(responder.Muid(), L"").get());
        }
    }

    std::wcout << std::endl << L"Done." << std::endl;

    // Closing withdraws the identifier, which tells anything that was talking
    // to this session to stop.
    capabilityInquiry.Close();

    session.Close();

    return 0;
}
