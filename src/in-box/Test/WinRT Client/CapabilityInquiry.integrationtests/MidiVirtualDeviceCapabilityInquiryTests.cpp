// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

#include <algorithm>
#include <cwctype>

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::CapabilityInquiry;
using namespace winrt::Windows::Devices::Midi2::Enumeration;
using namespace winrt::Windows::Devices::Midi2::Transports::Virtual;

namespace json = winrt::Windows::Data::Json;

namespace
{
    // A virtual device and a client connected to it, which is the shape an application takes when
    // it wants to look like a device to everything else on the machine.
    struct VirtualDeviceUnderTest
    {
        MidiSession Session{ nullptr };
        MidiVirtualDevice Device{ nullptr };
        MidiEndpointConnection DeviceConnection{ nullptr };
        MidiEndpointConnection ClientConnection{ nullptr };

        ~VirtualDeviceUnderTest()
        {
            if (DeviceConnection != nullptr && Session != nullptr && Device != nullptr)
            {
                DeviceConnection.RemoveMessageProcessingPlugin(Device.PluginId());
            }

            if (Session != nullptr)
            {
                Session.Close();
            }
        }
    };

    std::wstring MakeUniqueProductInstanceId()
    {
        std::wstring value{ winrt::to_hstring(winrt::Windows::Foundation::GuidHelper::CreateNewGuid()) };

        value.erase(
            std::remove_if(value.begin(), value.end(), [](wchar_t c) { return !std::iswalnum(c); }),
            value.end());

        return value;
    }

    std::unique_ptr<VirtualDeviceUnderTest> CreateVirtualDevice(std::wstring const& name)
    {
        VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());

        auto result = std::make_unique<VirtualDeviceUnderTest>();

        MidiDeclaredEndpointInfo declaredEndpointInfo{};
        declaredEndpointInfo.Name(winrt::hstring{ name });
        declaredEndpointInfo.ProductInstanceId(winrt::hstring{ MakeUniqueProductInstanceId() });
        declaredEndpointInfo.SpecificationVersionMajor(1);
        declaredEndpointInfo.SpecificationVersionMinor(1);
        declaredEndpointInfo.SupportsMidi10Protocol(true);
        declaredEndpointInfo.SupportsMidi20Protocol(true);
        declaredEndpointInfo.HasStaticFunctionBlocks(true);

        // Microsoft's own identifier, three bytes because the first is zero. The responder declares
        // this in its capability inquiry replies, which is what keeps the two carriers in step.
        MidiDeclaredDeviceIdentity declaredDeviceIdentity(
            0x00, 0x00, 0x41,
            0x0B, 0x00,
            0x01, 0x00,
            0x01, 0x00, 0x00, 0x00);

        MidiEndpointUserSuppliedInfo userSuppliedInfo{};

        MidiVirtualDeviceCreationConfig config(
            winrt::hstring{ name },
            L"Capability inquiry integration test",
            L"Windows MIDI Services Test",
            declaredEndpointInfo,
            declaredDeviceIdentity,
            userSuppliedInfo);

        result->Session = MidiSession::Create(L"Capability inquiry integration tests");
        VERIFY_IS_NOT_NULL(result->Session);

        result->Device = MidiVirtualDeviceManager::CreateVirtualDevice(config);
        VERIFY_IS_NOT_NULL(result->Device);

        result->DeviceConnection = result->Session.CreateEndpointConnection(
            result->Device.DeviceEndpointDeviceId());
        VERIFY_IS_NOT_NULL(result->DeviceConnection);

        auto const pluginAddResult = result->DeviceConnection.AddMessageProcessingPlugin(result->Device);
        VERIFY_IS_TRUE(pluginAddResult == MidiMessageProcessingPluginAddResult::Succeeded);

        VERIFY_IS_TRUE(result->DeviceConnection.Open());

        // The client-visible endpoint is created once the device side opens, so there is nothing
        // to connect to until then.
        winrt::hstring clientEndpointDeviceId{};

        for (int i = 0; i < 100 && clientEndpointDeviceId.empty(); i++)
        {
            Sleep(100);

            clientEndpointDeviceId = MidiVirtualDeviceManager::GetAssociatedClientEndpointDeviceId(
                result->Device.AssociationId());
        }

        VERIFY_IS_FALSE(clientEndpointDeviceId.empty());

        result->ClientConnection = result->Session.CreateEndpointConnection(clientEndpointDeviceId);
        VERIFY_IS_NOT_NULL(result->ClientConnection);

        VERIFY_IS_TRUE(result->ClientConnection.Open());

        return result;
    }
}


void MidiCapabilityInquirySessionTests::TestVirtualDeviceAnswersDiscoveryAndResources()
{
    auto const device = CreateVirtualDevice(L"CI Virtual Device");

    auto responder = device->Device.CapabilityInquiry();
    VERIFY_IS_NOT_NULL(responder);

    // Nothing published yet, so there is nothing to declare.
    VERIFY_ARE_EQUAL(
        (int)responder.SupportedCategories(),
        (int)MidiCapabilityInquiryCategories::None);

    MidiDeviceInfo deviceInfo{};
    deviceInfo.Manufacturer(L"Contoso");
    deviceInfo.Family(L"Test Family");
    deviceInfo.Model(L"CI Virtual Device");
    responder.DeviceInfo(deviceInfo);

    MidiProgramList programList{};

    for (uint32_t i = 0; i < 300; i++)
    {
        MidiProgramListEntry entry{};
        entry.Title(winrt::hstring{ L"Program " + std::to_wstring(i) });
        entry.BankMsb((uint8_t)0);
        entry.BankLsb((uint8_t)0);
        entry.ProgramChange((uint8_t)(i % 128));

        programList.Entries().Append(entry);
    }

    responder.SetProgramList(L"main", programList);

    // Publishing something is what makes the device declare property exchange.
    VERIFY_IS_TRUE(
        ((int)responder.SupportedCategories() &
         (int)MidiCapabilityInquiryCategories::PropertyExchange) != 0);

    responder.IsEnabled(true);

    auto session = MidiCapabilityInquirySession::Create(device->ClientConnection);
    session.ResponseTimeoutMilliseconds(4000);

    auto const found = session.DiscoverAsync().get();

    VERIFY_ARE_EQUAL(found.Size(), (uint32_t)1);

    auto const muid = found.GetAt(0).Muid();

    VERIFY_IS_TRUE(found.GetAt(0).SupportsPropertyExchange());
    VERIFY_ARE_EQUAL(muid.AsCombined28BitValue(), responder.GetMuid(0).AsCombined28BitValue());

    // The identity in the capability inquiry reply has to be the one the device declares
    // everywhere else, or the two carriers would disagree about who this is.
    VERIFY_ARE_EQUAL(found.GetAt(0).Identity().SystemExclusiveId().at(2), (uint8_t)0x41);

    // The resource list is built from what was published, so a device does not describe itself
    // twice.
    auto const resources = session.GetResourceListAsync(muid).get();

    VERIFY_IS_NOT_NULL(resources);
    VERIFY_IS_TRUE(resources.SupportsResource(L"ResourceList"));
    VERIFY_IS_TRUE(resources.SupportsResource(L"DeviceInfo"));
    VERIFY_IS_TRUE(resources.SupportsResource(L"ProgramList"));
    VERIFY_IS_FALSE(resources.SupportsResource(L"ChannelList"));

    auto const readBack = session.GetDeviceInfoAsync(muid).get();

    VERIFY_IS_NOT_NULL(readBack);
    VERIFY_ARE_EQUAL(readBack.Manufacturer(), winrt::hstring{ L"Contoso" });
    VERIFY_ARE_EQUAL(readBack.Model(), winrt::hstring{ L"CI Virtual Device" });

    // Three hundred programs cannot fit in one message, so this exercises the responder's own
    // chunking and paging as well as the session's reassembly.
    auto const programs = session.GetProgramListAsync(muid, L"main").get();

    VERIFY_IS_NOT_NULL(programs);
    VERIFY_ARE_EQUAL(programs.Entries().Size(), (uint32_t)300);
    VERIFY_ARE_EQUAL(programs.Entries().GetAt(299).Title(), winrt::hstring{ L"Program 299" });

    session.Close();
}

void MidiCapabilityInquirySessionTests::TestVirtualDeviceSaysNothingUntilEnabled()
{
    auto const device = CreateVirtualDevice(L"CI Quiet Device");

    auto responder = device->Device.CapabilityInquiry();

    MidiDeviceInfo deviceInfo{};
    deviceInfo.Manufacturer(L"Contoso");
    responder.DeviceInfo(deviceInfo);

    // Deliberately not enabled. Answering Discovery is a device saying it implements capability
    // inquiry, and it should only say that when an application has asked it to.
    auto session = MidiCapabilityInquirySession::Create(device->ClientConnection);
    session.ResponseTimeoutMilliseconds(600);

    VERIFY_ARE_EQUAL(session.DiscoverAsync().get().Size(), (uint32_t)0);

    responder.IsEnabled(true);

    VERIFY_ARE_EQUAL(session.DiscoverAsync().get().Size(), (uint32_t)1);

    session.Close();
}

void MidiCapabilityInquirySessionTests::TestVirtualDeviceAnswersProfilesAndUnknownResources()
{
    auto const device = CreateVirtualDevice(L"CI Profile Device");

    auto responder = device->Device.CapabilityInquiry();

    auto const enabledProfile = MidiProfileId::CreateStandardDefined(0x01, 0x01, 0x01, 0x01);
    auto const disabledProfile = MidiProfileId::CreateStandardDefined(0x01, 0x02, 0x01, 0x01);

    auto enabled = winrt::single_threaded_vector<MidiProfileId>();
    enabled.Append(enabledProfile);

    auto disabled = winrt::single_threaded_vector<MidiProfileId>();
    disabled.Append(disabledProfile);

    responder.SetProfiles(0x7F, enabled, disabled);

    // A resource this API has no type for is published as its JSON text and answered the same way
    // as the ones it does.
    responder.SetResource(L"StateList", L"", L"[{\"title\":\"Scene 1\",\"resId\":\"scene1\"}]");

    responder.IsEnabled(true);

    VERIFY_IS_TRUE(
        ((int)responder.SupportedCategories() &
         (int)MidiCapabilityInquiryCategories::ProfileConfiguration) != 0);

    auto session = MidiCapabilityInquirySession::Create(device->ClientConnection);
    session.ResponseTimeoutMilliseconds(3000);

    auto const found = session.DiscoverAsync().get();
    VERIFY_ARE_EQUAL(found.Size(), (uint32_t)1);

    auto const muid = found.GetAt(0).Muid();

    VERIFY_IS_TRUE(found.GetAt(0).SupportsProfiles());

    auto const profiles = session.GetProfilesAsync(muid, 0x7F).get();

    VERIFY_ARE_EQUAL((int)profiles.Status(), (int)MidiCapabilityInquiryStatus::Success);
    VERIFY_ARE_EQUAL(profiles.EnabledProfiles().Size(), (uint32_t)1);
    VERIFY_IS_TRUE(profiles.EnabledProfiles().GetAt(0).IsSameProfileAs(enabledProfile));
    VERIFY_ARE_EQUAL(profiles.DisabledProfiles().Size(), (uint32_t)1);

    // A device with no profiles at the address asked about still answers, with both lists empty.
    // That is a real answer, and not the same as saying nothing.
    auto const channelProfiles = session.GetProfilesAsync(muid, 0x00).get();

    VERIFY_ARE_EQUAL((int)channelProfiles.Status(), (int)MidiCapabilityInquiryStatus::Success);
    VERIFY_ARE_EQUAL(channelProfiles.EnabledProfiles().Size(), (uint32_t)0);
    VERIFY_ARE_EQUAL(channelProfiles.DisabledProfiles().Size(), (uint32_t)0);

    json::JsonObject header{};
    header.SetNamedValue(L"resource", json::JsonValue::CreateStringValue(L"StateList"));

    auto const stateList = session.GetPropertyDataAsync(muid, header).get();

    VERIFY_ARE_EQUAL((int)stateList.Status(), (int)MidiCapabilityInquiryStatus::Success);
    VERIFY_IS_NOT_NULL(stateList.BodyAsJson());
    VERIFY_ARE_EQUAL(
        stateList.BodyAsJson().GetArray().GetObjectAt(0).GetNamedString(L"title"),
        winrt::hstring{ L"Scene 1" });

    // Asking for something the device does not publish is answered, not ignored: 404 in the reply
    // header is how a device says it does not have that resource.
    json::JsonObject missingHeader{};
    missingHeader.SetNamedValue(L"resource", json::JsonValue::CreateStringValue(L"NoSuchResource"));

    auto const missing = session.GetPropertyDataAsync(muid, missingHeader).get();

    VERIFY_ARE_EQUAL((int)missing.Status(), (int)MidiCapabilityInquiryStatus::NegativeAcknowledgment);
    VERIFY_ARE_EQUAL(missing.ResourceStatus(), 404);

    session.Close();
}
