// Copyright (c) Microsoft Corporation. All rights reserved.
#include "stdafx.h"

#include "Midi2TransportTestsBase.h"
#include "Midi2KSTransportTests.h"

// NOTE: activation with a MidiOne interface does not apply to the KS transport
// layer, it only knows how to activate using information that is contained on the midi 2 swd.
// This is why _MidiOne tests are not performed on the KSTransport.
void MidiKSTransportTests::TestMidiKSTransport_UMP()
{
    // UMP endpoint activated via midi 2 swd
    TestMidiTransport(__uuidof(Midi2KSTransport), MidiDataFormats_UMP, FALSE);
}
void MidiKSTransportTests::TestMidiKSTransport_Any()
{
    // ByteStream endpoint activated via midi 2 swd
    TestMidiTransport(__uuidof(Midi2KSTransport), MidiDataFormats_Any, FALSE);
}

void MidiKSTransportTests::TestMidiKSTransportCreationOrder_UMP()
{
    TestMidiTransportCreationOrder(__uuidof(Midi2KSTransport), MidiDataFormats_UMP, FALSE);
}
void MidiKSTransportTests::TestMidiKSTransportCreationOrder_Any()
{
    TestMidiTransportCreationOrder(__uuidof(Midi2KSTransport), MidiDataFormats_Any, FALSE);
}

void MidiKSTransportTests::TestMidiKSTransportBidi_UMP()
{
    TestMidiTransportBidi(__uuidof(Midi2KSTransport), MidiDataFormats_UMP);
}
void MidiKSTransportTests::TestMidiKSTransportBidi_Any()
{
    TestMidiTransportBidi(__uuidof(Midi2KSTransport), MidiDataFormats_Any);
}

void MidiKSTransportTests::TestMidiKSIO_Latency_UMP()
{
    TestMidiIO_Latency(__uuidof(Midi2KSTransport), MidiDataFormats_UMP, FALSE, MessageOptionFlags_None);
}

void MidiKSTransportTests::TestMidiKSIOSlowMessages_Latency_UMP()
{
    TestMidiIO_Latency(__uuidof(Midi2KSTransport), MidiDataFormats_UMP, TRUE, MessageOptionFlags_None);
}

void MidiKSTransportTests::TestMidiKSIO_Latency_UMP_WaitForSendComplete()
{
    TestMidiIO_Latency(__uuidof(Midi2KSTransport), MidiDataFormats_UMP, FALSE, MessageOptionFlags_WaitForSendComplete);
}

void MidiKSTransportTests::TestKsHandleWrapperQueryRemove_FilterHandle()
{
    // Enumerate KS filters
    KSMidiDeviceEnum ksEnum;
    VERIFY_SUCCEEDED(ksEnum.EnumerateFilters());

    if (ksEnum.m_AvailableMidiOutPinCount == 0)
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"No MIDI OUT pins found.");
        return;
    }

    // Use the first available output filter
    auto filterName = ksEnum.m_AvailableMidiOutPins[0].FilterName.get();

    KsHandleWrapper filterWrapper(filterName);
    VERIFY_SUCCEEDED(filterWrapper.Open());

    std::wstring instanceId;
    HRESULT hr = GetInstanceIdFromFilterName(filterName, instanceId);
    VERIFY_SUCCEEDED(hr);
    VERIFY_IS_FALSE(instanceId.empty());

    if (!SetDeviceEnabled(instanceId.c_str(), false))
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"Unable to disable device (it may be virtual or protected).");
        return;
    }

    // If the query remove fails, the handle will remain open, indicating that something veto'd the removal.
    VERIFY_IS_TRUE(filterWrapper.GetHandle() == nullptr);

    if (!SetDeviceEnabled(instanceId.c_str(), true))
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"Unable to enable device (it may be virtual or protected).");
        return;
    }

    // Give the driver time to finish enabling.
    Sleep(3000);

    // The device should come back in a good state
    VERIFY_SUCCEEDED(filterWrapper.Open());
    filterWrapper.Close();
}

void MidiKSTransportTests::TestKsHandleWrapperQueryRemove_PinHandle()
{
    // Enumerate KS filters
    KSMidiDeviceEnum ksEnum;
    VERIFY_SUCCEEDED(ksEnum.EnumerateFilters());

    if (ksEnum.m_AvailableMidiOutPinCount == 0)
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"No MIDI OUT pins found.");
        return;
    }

    auto& pinInfo = ksEnum.m_AvailableMidiOutPins[0];
    auto filterName = pinInfo.FilterName.get();
    UINT pinId = pinInfo.PinId;

    MidiTransport transport = (MidiTransport) (pinInfo.TransportCapability & MidiTransport_CyclicUMP);
    if (transport == MidiTransport_Invalid)
    {
        transport = (MidiTransport)(pinInfo.TransportCapability & MidiTransport_StandardByteStream);
    }

    // Open filter handle
    KsHandleWrapper parentFilterWrapper(filterName);
    VERIFY_SUCCEEDED(parentFilterWrapper.Open());

    // Get handle duplicate
    wil::unique_handle dupHandle(parentFilterWrapper.GetHandle());
    VERIFY_IS_NOT_NULL(dupHandle.get());

    // Create pin wrapper
    KsHandleWrapper pinWrapper(filterName, pinId, transport, dupHandle.get());

    // release the dup handle, because keeping it open outside of a wrapper will block
    // removal
    dupHandle.reset();

    // open the pin handle
    VERIFY_SUCCEEDED(pinWrapper.Open());

    std::wstring instanceId;
    HRESULT hr = GetInstanceIdFromFilterName(filterName, instanceId);
    VERIFY_SUCCEEDED(hr);
    VERIFY_IS_FALSE(instanceId.empty());

    LOG_OUTPUT(L"Disabling.");
    if (!SetDeviceEnabled(instanceId.c_str(), false))
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"Unable to disable device (it may be virtual or protected).");
        return;
    }

    // If the query remove fails, the handles will remain open, indicating that something veto'd the removal.
    VERIFY_IS_TRUE(pinWrapper.GetHandle() == nullptr);
    VERIFY_IS_TRUE(parentFilterWrapper.GetHandle() == nullptr);

    LOG_OUTPUT(L"Enabling.");
    if (!SetDeviceEnabled(instanceId.c_str(), true))
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"Unable to enable device (it may be virtual or protected).");
        return;
    }

    // Give the driver time to finish enabling.
    Sleep(3000);

    // The device should come back in a good state
    VERIFY_SUCCEEDED(pinWrapper.Open());
    pinWrapper.Close();
}

void MidiKSTransportTests::TestKsHandleWrapperSurpriseRemove_FilterHandle()
{
    // Enumerate KS filters
    KSMidiDeviceEnum ksEnum;
    VERIFY_SUCCEEDED(ksEnum.EnumerateFilters());

    UINT index {ksEnum.m_AvailableMidiOutPinCount};
    for (UINT i = 0; i < ksEnum.m_AvailableMidiOutPinCount; i++)
    {
        std::wstring filter = ksEnum.m_AvailableMidiOutPins[i].FilterName.get();
        if (std::wstring::npos != filter.find(L"MinMidi"))
        {
            index = i;
            break;
        }
    }

    if (index == ksEnum.m_AvailableMidiOutPinCount)
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"MinMidi pin not found");
        return;
    }

    auto filterName = ksEnum.m_AvailableMidiOutPins[index].FilterName.get();
    KsHandleWrapper filterWrapper(filterName);
    VERIFY_SUCCEEDED(filterWrapper.Open());

    std::wstring instanceId;
    HRESULT hr = GetInstanceIdFromFilterName(filterName, instanceId);
    VERIFY_SUCCEEDED(hr);
    VERIFY_IS_FALSE(instanceId.empty());

    VERIFY_SUCCEEDED(SendDriverCommand(KSPROPERTY_MINMIDICONTROL_SURPRISEREMOVESIMULATION, 1));

    if (!SetDeviceEnabled(instanceId.c_str(), false))
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"Unable to disable device (it may be virtual or protected).");
        return;
    }

    // If surprise removal fails, the handle will still be closed,
    // however we will fail to reactivate the device or fail to reopen
    // the filter because the driver will not reinitialize until all open handles
    // are closed.
    VERIFY_IS_TRUE(filterWrapper.GetHandle() == nullptr);

    if (!SetDeviceEnabled(instanceId.c_str(), true))
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"Unable to enable device (it may be virtual or protected).");
        return;
    }

    // Give the driver time to finish enabling.
    Sleep(3000);

    VERIFY_SUCCEEDED(filterWrapper.Open());
    filterWrapper.Close();
}

void MidiKSTransportTests::TestKsHandleWrapperSurpriseRemove_PinHandle()
{
    // Enumerate KS filters
    KSMidiDeviceEnum ksEnum;
    VERIFY_SUCCEEDED(ksEnum.EnumerateFilters());

    UINT index {ksEnum.m_AvailableMidiOutPinCount};
    for (UINT i = 0; i < ksEnum.m_AvailableMidiOutPinCount; i++)
    {
        std::wstring filter = ksEnum.m_AvailableMidiOutPins[i].FilterName.get();
        if (std::wstring::npos != filter.find(L"MinMidi"))
        {
            index = i;
            break;
        }
    }

    if (index == ksEnum.m_AvailableMidiOutPinCount)
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"MinMidi pin not found");
        return;
    }

    auto& pinInfo = ksEnum.m_AvailableMidiOutPins[index];
    auto filterName = pinInfo.FilterName.get();
    UINT pinId = pinInfo.PinId;

    MidiTransport transport = (MidiTransport) (pinInfo.TransportCapability & MidiTransport_CyclicUMP);
    if (transport == MidiTransport_Invalid)
    {
        transport = (MidiTransport)(pinInfo.TransportCapability & MidiTransport_StandardByteStream);
    }

    // Open filter handle
    KsHandleWrapper parentFilterWrapper(filterName);
    VERIFY_SUCCEEDED(parentFilterWrapper.Open());

    // Get handle duplicate
    wil::unique_handle dupHandle(parentFilterWrapper.GetHandle());
    VERIFY_IS_NOT_NULL(dupHandle.get());

    // Create pin wrapper
    KsHandleWrapper pinWrapper(filterName, pinId, transport, dupHandle.get());

    // release the dup handle, because keeping it open outside of a wrapper will block
    // removal
    dupHandle.reset();

    // open the pin handle
    VERIFY_SUCCEEDED(pinWrapper.Open());

    std::wstring instanceId;
    HRESULT hr = GetInstanceIdFromFilterName(filterName, instanceId);
    VERIFY_SUCCEEDED(hr);
    VERIFY_IS_FALSE(instanceId.empty());

    LOG_OUTPUT(L"Disabling.");
    if (!SetDeviceEnabled(instanceId.c_str(), false))
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"Unable to disable device (it may be virtual or protected).");
        return;
    }

    // If surprise removal fails, the handles will still be closed,
    // however we will fail to reactivate the device or fail to reopen
    // the filter or pin handles because the driver will not reinitialize until all open handles
    // are closed.
    VERIFY_IS_TRUE(pinWrapper.GetHandle() == nullptr);
    VERIFY_IS_TRUE(parentFilterWrapper.GetHandle() == nullptr);

    LOG_OUTPUT(L"Enabling.");
    if (!SetDeviceEnabled(instanceId.c_str(), true))
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"Unable to enable device (it may be virtual or protected).");
        return;
    }

    // Give the driver time to finish enabling.
    Sleep(3000);

    VERIFY_SUCCEEDED(pinWrapper.Open());
    pinWrapper.Close();
}


bool MidiKSTransportTests::TestSetup()
{
    m_MidiInCallback = nullptr;
    return true;
}

bool MidiKSTransportTests::TestCleanup()
{
    return true;
}

bool MidiKSTransportTests::ClassSetup()
{
    PrintStagingStates();

    // Midi discovery conflicts with usage of the KS transport outside of the service
    StopMIDIService();
    SetMidiDiscovery(false);
    StartMIDIService();

    return true;
}

bool MidiKSTransportTests::ClassCleanup()
{
    // Stop the service and restore midi discovery.
    StopMIDIService();
    SetMidiDiscovery(true);

    return true;
}

