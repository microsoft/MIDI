// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code
//
// FOCUS OF THIS SAMPLE: being a device that answers MIDI Capability Inquiry.
//
// A virtual device already answers endpoint discovery on your behalf. This
// shows the same thing for capability inquiry: you hand it the lists you want
// to publish and it answers Discovery, property exchange capabilities, get
// requests and profile inquiries itself.
//
// What it declares it can do follows from what you gave it. Publish a resource
// and it declares property exchange; set a profile and it declares profile
// configuration. Declaring a category and then answering nothing is worse than
// not declaring it, so there is no flag for you to get wrong.
//
// Run this, then run the capability-inquiry-browse sample against the endpoint
// name printed below, or open any application that reads patch lists.
//
// See also the virtual-device-app-winui sample, which covers creating a virtual
// device and responding to stream messages.

#include <iostream>
#include <string>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.CapabilityInquiry.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.h>
#include <winrt/Windows.Devices.Midi2.Transports.Virtual.h>

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::CapabilityInquiry;
using namespace winrt::Windows::Devices::Midi2::Enumeration;
using namespace winrt::Windows::Devices::Midi2::Transports::Virtual;


const winrt::hstring DeviceName = L"Capability Inquiry Sample Device";

// Programs this device says it has. A real instrument would build these from
// whatever it actually holds.
const wchar_t* const ProgramTitles[]
{
    L"Grand Piano", L"Bright Piano", L"Electric Piano", L"Harpsichord",
    L"Celesta", L"Glockenspiel", L"Music Box", L"Vibraphone",
    L"Church Organ", L"Drawbar Organ", L"Nylon Guitar", L"Steel Guitar",
    L"Fretless Bass", L"Violin", L"Cello", L"String Ensemble"
};


MidiProgramList BuildProgramList()
{
    MidiProgramList programs{};

    uint8_t programChange{ 0 };

    for (auto const title : ProgramTitles)
    {
        MidiProgramListEntry entry{};

        entry.Title(title);

        // Zero based, and sent on the wire exactly as they appear here.
        entry.BankMsb(0);
        entry.BankLsb(0);
        entry.ProgramChange(programChange++);

        // A sound set commonly gives a program and its bank variation the same
        // title, so tags are often the only thing telling them apart.
        entry.Tags().Append(L"Sample");

        programs.Entries().Append(entry);
    }

    return programs;
}

MidiChannelList BuildChannelList()
{
    MidiChannelList channels{};

    for (uint16_t channel = 1; channel <= 16; channel++)
    {
        MidiChannelListEntry entry{};

        // One based, and the numbering runs across all sixteen groups rather
        // than restarting, which is why this is not a MidiChannel.
        entry.Channel(channel);
        entry.Title(winrt::hstring{ L"Part " + std::to_wstring(channel) });
        entry.ProgramTitle(ProgramTitles[0]);

        // The route from a channel to the programs it can select. Without this
        // an application has no way to know which list applies where.
        MidiResourceLink link{};

        link.Resource(L"ProgramList");
        link.ResourceId(L"main");
        link.Title(L"Sample Programs");

        entry.Links().Append(link);

        channels.Entries().Append(entry);
    }

    return channels;
}


int main()
{
    winrt::init_apartment();

    if (!MidiApi::EnsureServiceAvailable())
    {
        std::wcout << L"Could not demand-start the MIDI service." << std::endl;
        return 1;
    }

    MidiDeclaredEndpointInfo declaredEndpointInfo{};

    declaredEndpointInfo.Name(DeviceName);
    declaredEndpointInfo.ProductInstanceId(L"CISAMPLE0001");
    declaredEndpointInfo.SpecificationVersionMajor(1);
    declaredEndpointInfo.SpecificationVersionMinor(1);
    declaredEndpointInfo.SupportsMidi10Protocol(true);
    declaredEndpointInfo.SupportsMidi20Protocol(true);
    declaredEndpointInfo.HasStaticFunctionBlocks(true);

    // A device states who it is through several different carriers and they are
    // required to agree. The responder declares this same identity in its
    // capability inquiry replies, so there is nothing to keep in step by hand.
    MidiDeclaredDeviceIdentity declaredDeviceIdentity(
        0x00, 0x00, 0x41,       // manufacturer system exclusive id
        0x0B, 0x00,             // device family
        0x01, 0x00,             // model
        0x01, 0x00, 0x00, 0x00  // software revision
    );

    MidiEndpointUserSuppliedInfo userSuppliedInfo{};

    MidiVirtualDeviceCreationConfig config(
        DeviceName,
        L"Answers capability inquiry",
        L"Windows MIDI Services Samples",
        declaredEndpointInfo,
        declaredDeviceIdentity,
        userSuppliedInfo);

    auto session = MidiSession::Create(L"Capability Inquiry Virtual Device Sample");

    auto device = MidiVirtualDeviceManager::CreateVirtualDevice(config);

    if (device == nullptr)
    {
        std::wcout << L"Could not create the virtual device." << std::endl;
        return 1;
    }

    auto connection = session.CreateEndpointConnection(device.DeviceEndpointDeviceId());

    if (connection == nullptr)
    {
        std::wcout << L"Could not create the device-side connection." << std::endl;
        return 1;
    }

    if (connection.AddMessageProcessingPlugin(device) != MidiMessageProcessingPluginAddResult::Succeeded)
    {
        std::wcout << L"Could not attach the virtual device to the connection." << std::endl;
        return 1;
    }


    // ---------------------------------------------------------------------
    // Everything this device will answer capability inquiry with.

    auto responder = device.CapabilityInquiry();

    MidiDeviceInfo deviceInfo(declaredDeviceIdentity, L"Microsoft", L"Windows MIDI Services", DeviceName);

    deviceInfo.Version(L"1.0");

    responder.DeviceInfo(deviceInfo);
    responder.ChannelList(BuildChannelList());

    // A device may publish more than one program list, told apart by resource
    // id. The channel list above points at this one.
    responder.SetProgramList(L"main", BuildProgramList());

    // A profile this device supports but has not turned on. An initiator sees
    // it in the disabled list and can offer to enable it.
    auto const pianoProfile = MidiProfileId::CreateStandardDefined(0x01, 0x01, 0x01, 0x01);

    auto disabledProfiles = winrt::single_threaded_vector<MidiProfileId>();
    disabledProfiles.Append(pianoProfile);

    responder.SetProfiles(0x7F, nullptr, disabledProfiles);

    // Anything the API has no type for is published as the JSON text of the
    // resource, and answered the same way as the ones it does have.
    responder.SetResource(L"StateList", L"",
        LR"([{"title":"Performance A","resId":"a"},{"title":"Performance B","resId":"b"}])");

    // Anything the responder did not answer itself arrives here with its
    // payload intact, which is where you implement what the API does not.
    responder.MessageReceived([](auto&&, MidiCapabilityInquiryMessageReceivedEventArgs const& args)
        {
            std::wcout << L"Unhandled capability inquiry message, type 0x"
                << std::hex << (int)args.Message().MessageType() << std::dec
                << L", " << args.Message().Data().Size() << L" bytes" << std::endl;
        });

    // Nothing above has been said out loud yet. Answering Discovery is this
    // device saying it implements capability inquiry, so it only says it once
    // there is something to answer with.
    responder.IsEnabled(true);


    if (!connection.Open())
    {
        std::wcout << L"Could not open the device-side connection." << std::endl;
        return 1;
    }

    std::wcout << L"Virtual device running." << std::endl;
    std::wcout << L"  Endpoint name : " << DeviceName.c_str() << std::endl;
    std::wcout << L"  Identifier    : " << responder.GetMuid(0).ToString().c_str() << std::endl;
    std::wcout << std::endl << L"Press Enter to stop." << std::endl;

    std::wstring line{};
    std::getline(std::wcin, line);

    connection.RemoveMessageProcessingPlugin(device.PluginId());
    session.Close();

    return 0;
}
