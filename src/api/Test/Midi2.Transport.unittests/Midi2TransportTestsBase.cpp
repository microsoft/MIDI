// Copyright (c) Microsoft Corporation. All rights reserved.
#include "stdafx.h"

#include "Midi2TransportTestsBase.h"

using namespace winrt::Windows::Devices::Enumeration;

MODULE_SETUP(ModuleSetup);
bool ModuleSetup()
{
    winrt::init_apartment();
    wil::SetResultTelemetryFallback(ErrorHandler);
    return true;
}

void ErrorHandler(bool, wil::FailureInfo const& failure) WI_NOEXCEPT
{
    // This callback enables us to have wil fallback errors logged into the test output.
    WCHAR failureString[2048];
    wil::GetFailureLogString(failureString, _countof(failureString), failure);
    LOG_OUTPUT( L"WIL error: %s", failureString);
}

HRESULT
GetEndpointNativeDataFormat(_In_ std::wstring midiDevice, _Inout_ BYTE& nativeDataFormat)
{
    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
    additionalProperties.Append(STRING_PKEY_MIDI_NativeDataFormat);
    auto deviceInfo = DeviceInformation::CreateFromIdAsync(midiDevice, additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

    auto prop = deviceInfo.Properties().Lookup(STRING_PKEY_MIDI_NativeDataFormat);
    if (prop)
    {
        try
        {
            // If a group index is provided by this device, it must be valid.
            nativeDataFormat = (BYTE) winrt::unbox_value<uint8_t>(prop);

            // minmidi doesn't specify a native data format, for the purposes of this test it's UMP.
            if (nativeDataFormat == 0)
            {
                nativeDataFormat = MidiDataFormats_UMP;
            }

            RETURN_HR_IF(E_UNEXPECTED, nativeDataFormat != MidiDataFormats_ByteStream && nativeDataFormat != MidiDataFormats_UMP);
        }
        CATCH_LOG();
    }

    return S_OK;
}

HRESULT
GetEndpointGroupIndex(_In_ std::wstring midiDevice, _Inout_ DWORD& groupIndex)
{
    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
    additionalProperties.Append(STRING_PKEY_MIDI_PortAssignedGroupIndex);
    auto deviceInfo = DeviceInformation::CreateFromIdAsync(midiDevice, additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

    auto prop = deviceInfo.Properties().Lookup(STRING_PKEY_MIDI_PortAssignedGroupIndex);
    if (prop)
    {
        try
        {
            // If a group index is provided by this device, it must be valid.
            groupIndex = (DWORD) winrt::unbox_value<UINT32>(prop);
            RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INDEX_OUT_OF_BOUNDS), groupIndex > 15);
        }
        CATCH_LOG();
    }

    return S_OK;
}

BOOL
GetBidiEndpoint(MidiDataFormats dataFormat, REFIID requestedTransport, std::wstring& midiBidiInstanceId)
{
    std::vector<std::unique_ptr<MIDIU_DEVICE>> midiBidiDevices;
    VERIFY_SUCCEEDED(MidiSWDeviceEnum::EnumerateDevices(midiBidiDevices, [&](PMIDIU_DEVICE device)
    {
        // When testing directly against the KS transport, we're limited to endpoints created by
        // KS transport, and supporting the format we wish to test.
        // When testing against midisrv, we can be more flexible.
        if ((requestedTransport == __uuidof(Midi2MidiSrvTransport) || device->TransportLayer == requestedTransport) &&
            device->Flow == MidiFlowBidirectional &&
            (std::wstring::npos != device->ParentDeviceInstanceId.find(L"MINMIDI") ||
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"VID_CAFE&PID_4001&MI_02") ||
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"VID_0582&PID_0168&MI_00")) &&  // Roland UM_ONE, easy to loop back to itself for testing.
            (requestedTransport == __uuidof(Midi2MidiSrvTransport) || (0 != ((DWORD) device->SupportedDataFormats & (DWORD) dataFormat))))
        {
            return true;
        }
        else
        {
            return false;
        }
    }));

    if (midiBidiDevices.size() == 0)
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"Test requires at least 1 midi bidi endpoint with the requested data format.");
        return FALSE;
    }

    midiBidiInstanceId = midiBidiDevices[0]->DeviceId;

    return TRUE;
}

BOOL GetEndpoints(MidiDataFormats dataFormat, BOOL midiOneInterface, REFIID requestedTransport, std::wstring& midiInInstanceId, std::wstring& midiOutInstanceId)
{
    std::vector<std::unique_ptr<MIDIU_DEVICE>> midiInDevices;
    std::vector<std::unique_ptr<MIDIU_DEVICE>> midiOutDevices;

    VERIFY_SUCCEEDED(MidiSWDeviceEnum::EnumerateDevices(midiInDevices, [&](PMIDIU_DEVICE device)
    {
        if ((requestedTransport == __uuidof(Midi2MidiSrvTransport) || device->TransportLayer == requestedTransport) &&
            (device->Flow == MidiFlowIn || device->Flow == MidiFlowBidirectional) &&
            (std::wstring::npos != device->ParentDeviceInstanceId.find(L"MINMIDI") ||
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"VID_CAFE&PID_4001&MI_02") ||
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"VID_0582&PID_0168&MI_00")) &&
            (midiOneInterface == device->MidiOne) &&
            (requestedTransport == __uuidof(Midi2MidiSrvTransport) || (0 != ((DWORD) device->SupportedDataFormats & (DWORD) dataFormat))))
        {
            return true;
        }
        else
        {
            return false;
        }
    }));

    VERIFY_SUCCEEDED(MidiSWDeviceEnum::EnumerateDevices(midiOutDevices, [&](PMIDIU_DEVICE device)
    {
        if ((requestedTransport == __uuidof(Midi2MidiSrvTransport) || device->TransportLayer == requestedTransport) &&
            (device->Flow == MidiFlowIn || device->Flow == MidiFlowBidirectional) &&
            (std::wstring::npos != device->ParentDeviceInstanceId.find(L"MINMIDI") ||
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"VID_CAFE&PID_4001&MI_02") ||
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"VID_0582&PID_0168&MI_00")) &&
            (midiOneInterface == device->MidiOne) &&
            (requestedTransport == __uuidof(Midi2MidiSrvTransport) || (0 != ((DWORD) device->SupportedDataFormats & (DWORD) dataFormat))))
        {
            return true;
        }
        else
        {
            return false;
        }
    }));

    if (midiInDevices.size() == 0 || midiOutDevices.size() == 0)
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"Test requires at least 1 midi in and 1 midi out endpoint with the requested data format.");
        return FALSE;
    }

    midiInInstanceId = midiInDevices[0]->DeviceId;
    midiOutInstanceId = midiOutDevices[0]->DeviceId;

    return TRUE;
}

_Use_decl_annotations_
void MidiTransportTestsBase::TestMidiTransport(REFIID iid, MidiDataFormats dataFormat, BOOL midiOneInterface)
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    wil::com_ptr_nothrow<IMidiTransport> midiTransport;
    wil::com_ptr_nothrow<IMidiIn> midiInDevice;
    wil::com_ptr_nothrow<IMidiOut> midiOutDevice;
    wil::com_ptr_nothrow<IMidiSessionTracker> midiSessionTracker;
    
    DWORD mmcssTaskId {0};
    wil::unique_event_nothrow allMessagesReceived;
    UINT32 expectedMessageCount = 4;
    UINT midiMessagesReceived = 0;
    TRANSPORTCREATIONPARAMS transportCreationParams { MessageOptionFlags_None, dataFormat, TEST_APPID };
    std::wstring midiInInstanceId;
    std::wstring midiOutInstanceId;

    VERIFY_SUCCEEDED(CoCreateInstance(iid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiTransport)));

    auto cleanupOnFailure = wil::scope_exit([&]() {
        if (midiOutDevice.get())
        {
            midiOutDevice->Shutdown();
        }
        if (midiInDevice.get())
        {
            midiInDevice->Shutdown();
        }

        if (midiSessionTracker.get() != nullptr)
        {
            midiSessionTracker->RemoveClientSession(m_SessionId);
        }
    });

    VERIFY_SUCCEEDED(midiTransport->Activate(__uuidof(IMidiIn), (void **) &midiInDevice));
    VERIFY_SUCCEEDED(midiTransport->Activate(__uuidof(IMidiOut), (void **) &midiOutDevice));

    // may fail, depending on transport layer support, currently only midisrv transport supports
    // the session tracker.
    midiTransport->Activate(__uuidof(IMidiSessionTracker), (void **) &midiSessionTracker);
    if (midiSessionTracker)
    {
        VERIFY_SUCCEEDED(midiSessionTracker->Initialize());
    }

    m_MidiInCallback = [&](PVOID payload, UINT32 payloadSize, LONGLONG payloadPosition, LONGLONG)
    {
        PrintMidiMessage(payload, payloadSize, (transportCreationParams.DataFormat == MidiDataFormats_UMP)?sizeof(UMP32):sizeof(MIDI_MESSAGE), payloadPosition);

        midiMessagesReceived++;
        if (midiMessagesReceived == expectedMessageCount)
        {
            allMessagesReceived.SetEvent();
        }
    };

    VERIFY_SUCCEEDED(allMessagesReceived.create());

    if (midiSessionTracker.get() != nullptr)
    {
        // create the client session on the service before calling GetEndpoints, which will kickstart
        // the service if it's not already running.
        VERIFY_SUCCEEDED(midiSessionTracker->AddClientSession(m_SessionId, L"TestMidiTransport"));
    }

    if (!GetEndpoints(transportCreationParams.DataFormat, midiOneInterface, iid, midiInInstanceId, midiOutInstanceId))
    {
        return;
    }

    LOG_OUTPUT(L"Initializing midi in");
    VERIFY_SUCCEEDED(midiInDevice->Initialize(midiInInstanceId.c_str(), &transportCreationParams, &mmcssTaskId, this, 0, m_SessionId));

    LOG_OUTPUT(L"Initializing midi out");
    VERIFY_SUCCEEDED(midiOutDevice->Initialize(midiOutInstanceId.c_str(), &transportCreationParams, &mmcssTaskId, m_SessionId));

    VERIFY_IS_TRUE(transportCreationParams.DataFormat == MidiDataFormats_UMP || transportCreationParams.DataFormat == MidiDataFormats_ByteStream);

    UMP32 midiTestData_32 = g_MidiTestData_32;
    UMP64 midiTestData_64 = g_MidiTestData_64;
    UMP96 midiTestData_96 = g_MidiTestData_96;
    UMP128 midiTestData_128 = g_MidiTestData_128;

    // if we're using midi 1 interfaces, then we need to ensure that the group index
    // on the messages going out will not cause them to be filtered/mistargeted.
    if (midiOneInterface)
    {
        DWORD midiInGroupIndex = 0;
        DWORD midiOutGroupIndex = 0;

        VERIFY_SUCCEEDED(GetEndpointGroupIndex(midiInInstanceId, midiInGroupIndex));
        VERIFY_SUCCEEDED(GetEndpointGroupIndex(midiOutInstanceId, midiOutGroupIndex));
        VERIFY_IS_TRUE(midiInGroupIndex == midiOutGroupIndex);

        // Shift left 24 bits, so that it's in the correct field
        midiInGroupIndex = midiInGroupIndex << 24;

        midiTestData_32.data1 |= midiInGroupIndex;
        midiTestData_64.data1 |= midiInGroupIndex;
        midiTestData_96.data1 |= midiInGroupIndex;
        midiTestData_128.data1 |= midiInGroupIndex;
    }

    LOG_OUTPUT(L"Writing midi data");
    if (transportCreationParams.DataFormat == MidiDataFormats_UMP)
    {
        BYTE nativeInDataFormat {0};
        BYTE nativeOutDataFormat {0};
        VERIFY_SUCCEEDED(GetEndpointNativeDataFormat(midiInInstanceId.c_str(), nativeInDataFormat));
        VERIFY_SUCCEEDED(GetEndpointNativeDataFormat(midiOutInstanceId.c_str(), nativeOutDataFormat));

        // if the peripheral native data format is bytestream, limit to 32 bit messages
        // that will roundtrip, the others will get dropped.
        if (nativeInDataFormat == MidiDataFormats_ByteStream || 
            nativeOutDataFormat == MidiDataFormats_ByteStream)
        {
            VERIFY_SUCCEEDED(midiOutDevice->SendMidiMessage(MessageOptionFlags_None, (void *) &midiTestData_32, sizeof(UMP32), 0));
            VERIFY_SUCCEEDED(midiOutDevice->SendMidiMessage(MessageOptionFlags_None, (void *) &midiTestData_32, sizeof(UMP32), 0));
            VERIFY_SUCCEEDED(midiOutDevice->SendMidiMessage(MessageOptionFlags_None, (void *) &midiTestData_32, sizeof(UMP32), 0));
            VERIFY_SUCCEEDED(midiOutDevice->SendMidiMessage(MessageOptionFlags_None, (void *) &midiTestData_32, sizeof(UMP32), 0));
        }
        else
        {
            VERIFY_SUCCEEDED(midiOutDevice->SendMidiMessage(MessageOptionFlags_None, (void *) &midiTestData_32, sizeof(UMP32), 0));
            VERIFY_SUCCEEDED(midiOutDevice->SendMidiMessage(MessageOptionFlags_None, (void *) &midiTestData_64, sizeof(UMP64), 0));
            VERIFY_SUCCEEDED(midiOutDevice->SendMidiMessage(MessageOptionFlags_None, (void *) &midiTestData_96, sizeof(UMP96), 0));
            VERIFY_SUCCEEDED(midiOutDevice->SendMidiMessage(MessageOptionFlags_None, (void *) &midiTestData_128, sizeof(UMP128), 0));
        }
    }
    else
    {
        VERIFY_SUCCEEDED(midiOutDevice->SendMidiMessage(MessageOptionFlags_None, (void *) &g_MidiTestMessage, sizeof(MIDI_MESSAGE), 0));
        VERIFY_SUCCEEDED(midiOutDevice->SendMidiMessage(MessageOptionFlags_None, (void *) &g_MidiTestMessage, sizeof(MIDI_MESSAGE), 0));
        VERIFY_SUCCEEDED(midiOutDevice->SendMidiMessage(MessageOptionFlags_None, (void *) &g_MidiTestMessage, sizeof(MIDI_MESSAGE), 0));
        VERIFY_SUCCEEDED(midiOutDevice->SendMidiMessage(MessageOptionFlags_None, (void *) &g_MidiTestMessage, sizeof(MIDI_MESSAGE), 0));
    }

    // wait for up to 30 seconds for all the messages
    if(!allMessagesReceived.wait(30000))
    {
        LOG_OUTPUT(L"Failure waiting for messages, timed out.");
    }

    LOG_OUTPUT(L"%d messages expected, %d received", expectedMessageCount, midiMessagesReceived);
    VERIFY_IS_TRUE(midiMessagesReceived == expectedMessageCount);

    LOG_OUTPUT(L"Done, cleaning up");
    cleanupOnFailure.release();
    VERIFY_SUCCEEDED(midiOutDevice->Shutdown());
    VERIFY_SUCCEEDED(midiInDevice->Shutdown());

    if (midiSessionTracker.get() != nullptr)
    {
        VERIFY_SUCCEEDED(midiSessionTracker->RemoveClientSession(m_SessionId));
    }
}

_Use_decl_annotations_
void MidiTransportTestsBase::TestMidiTransportCreationOrder(REFIID iid, _In_ MidiDataFormats dataFormat, BOOL midiOneInterface)
{
//    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    wil::com_ptr_nothrow<IMidiTransport> midiTransport;
    wil::com_ptr_nothrow<IMidiIn> midiInDevice;
    wil::com_ptr_nothrow<IMidiOut> midiOutDevice;
    wil::com_ptr_nothrow<IMidiSessionTracker> midiSessionTracker;
    DWORD mmcssTaskId {0};
    DWORD mmcssTaskIdIn {0};
    DWORD mmcssTaskIdOut {0};
    unique_mmcss_handle mmcssHandle;
    TRANSPORTCREATIONPARAMS transportCreationParams { MessageOptionFlags_None, dataFormat, TEST_APPID };
    std::wstring midiInInstanceId;
    std::wstring midiOutInstanceId;

    VERIFY_SUCCEEDED(CoCreateInstance(iid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiTransport)));

    auto cleanupOnFailure = wil::scope_exit([&]() {
        if (midiOutDevice.get())
        {
            midiOutDevice->Shutdown();
        }
        if (midiInDevice.get())
        {
            midiInDevice->Shutdown();
        }

        if (midiSessionTracker.get() != nullptr)
        {
            midiSessionTracker->RemoveClientSession(m_SessionId);
        }
    });

    // may fail, depending on transport layer support, currently only midisrv transport supports
    // the session tracker.
    midiTransport->Activate(__uuidof(IMidiSessionTracker), (void **) &midiSessionTracker);
    if (midiSessionTracker)
    {
        VERIFY_SUCCEEDED(midiSessionTracker->Initialize());
    }

    VERIFY_SUCCEEDED(midiTransport->Activate(__uuidof(IMidiIn), (void **) &midiInDevice));
    VERIFY_SUCCEEDED(midiTransport->Activate(__uuidof(IMidiOut), (void **) &midiOutDevice));

    m_MidiInCallback = [&](PVOID, UINT32, LONGLONG, LONGLONG)
    {
    };

    // enable mmcss for our test thread, to ensure that components only
    // manage mmcss for their own workers.
    VERIFY_SUCCEEDED(EnableMmcss(mmcssHandle, mmcssTaskId));

    if (midiSessionTracker.get() != nullptr)
    {
        // create the client session on the service before calling GetEndpoints, which will kickstart
        // the service if it's not already running.
        VERIFY_SUCCEEDED(midiSessionTracker->AddClientSession(m_SessionId, L"TestMidiTransportCreationOrder"));
    }

    if (!GetEndpoints(transportCreationParams.DataFormat, midiOneInterface, iid, midiInInstanceId, midiOutInstanceId))
    {
        return;
    }

    // initialize midi out and then midi in, reset the task id,
    // and then initialize midi in then out to ensure that order
    // doesn't matter.
    LOG_OUTPUT(L"Initializing midi out");
    mmcssTaskIdOut = mmcssTaskId;
    transportCreationParams.DataFormat = dataFormat;
    VERIFY_SUCCEEDED(midiOutDevice->Initialize(midiOutInstanceId.c_str(), &transportCreationParams, &mmcssTaskIdOut, m_SessionId));
    mmcssTaskIdIn = mmcssTaskIdOut;
    transportCreationParams.DataFormat = dataFormat;
    LOG_OUTPUT(L"Initializing midi in");
    VERIFY_SUCCEEDED(midiInDevice->Initialize(midiInInstanceId.c_str(), &transportCreationParams, &mmcssTaskIdIn, this, 0, m_SessionId));
    VERIFY_IS_TRUE(mmcssTaskId == mmcssTaskIdOut);
    VERIFY_IS_TRUE(mmcssTaskIdIn == mmcssTaskIdOut);
    VERIFY_SUCCEEDED(midiOutDevice->Shutdown());
    VERIFY_SUCCEEDED(midiInDevice->Shutdown());
    LOG_OUTPUT(L"Zeroing task id");
    mmcssTaskIdIn = 0;
    mmcssTaskIdOut = 0;
    LOG_OUTPUT(L"Initializing midi in");
    mmcssTaskIdIn = mmcssTaskId;
    transportCreationParams.DataFormat = dataFormat;
    VERIFY_SUCCEEDED(midiInDevice->Initialize(midiInInstanceId.c_str(), &transportCreationParams, &mmcssTaskIdIn, this, 0, m_SessionId));
    mmcssTaskIdOut = mmcssTaskIdIn;
    transportCreationParams.DataFormat = dataFormat;
    LOG_OUTPUT(L"Initializing midi out");
    VERIFY_SUCCEEDED(midiOutDevice->Initialize(midiOutInstanceId.c_str(), &transportCreationParams, &mmcssTaskIdOut, m_SessionId));
    VERIFY_IS_TRUE(mmcssTaskId == mmcssTaskIdOut);
    VERIFY_IS_TRUE(mmcssTaskIdIn == mmcssTaskIdOut);
    VERIFY_SUCCEEDED(midiOutDevice->Shutdown());
    VERIFY_SUCCEEDED(midiInDevice->Shutdown());

    mmcssHandle.reset();
    VERIFY_SUCCEEDED(EnableMmcss(mmcssHandle, mmcssTaskId));

    // reusing the same task id, which was cleaned up,
    // initialize again, but revers the cleanup order, which shouldn't matter,
    // then repeat again in the opposite order with reversed cleanup order.
    LOG_OUTPUT(L"Initializing midi out");
    mmcssTaskIdOut = mmcssTaskId;
    transportCreationParams.DataFormat = dataFormat;
    VERIFY_SUCCEEDED(midiOutDevice->Initialize(midiOutInstanceId.c_str(), &transportCreationParams, &mmcssTaskIdOut, m_SessionId));
    mmcssTaskIdIn = mmcssTaskIdOut;
    transportCreationParams.DataFormat = dataFormat;
    LOG_OUTPUT(L"Initializing midi in");
    VERIFY_SUCCEEDED(midiInDevice->Initialize(midiInInstanceId.c_str(), &transportCreationParams, &mmcssTaskIdIn, this, 0, m_SessionId));
    VERIFY_IS_TRUE(mmcssTaskId == mmcssTaskIdOut);
    VERIFY_IS_TRUE(mmcssTaskIdIn == mmcssTaskIdOut);
    VERIFY_SUCCEEDED(midiInDevice->Shutdown());
    VERIFY_SUCCEEDED(midiOutDevice->Shutdown());
    LOG_OUTPUT(L"Initializing midi in");
    mmcssTaskIdIn = mmcssTaskId;
    transportCreationParams.DataFormat = dataFormat;
    VERIFY_SUCCEEDED(midiInDevice->Initialize(midiInInstanceId.c_str(), &transportCreationParams, &mmcssTaskIdIn, this, 0, m_SessionId));
    mmcssTaskIdOut = mmcssTaskIdIn;
    transportCreationParams.DataFormat = dataFormat;
    LOG_OUTPUT(L"Initializing midi out");
    VERIFY_SUCCEEDED(midiOutDevice->Initialize(midiOutInstanceId.c_str(), &transportCreationParams, &mmcssTaskIdOut, m_SessionId));
    VERIFY_IS_TRUE(mmcssTaskId == mmcssTaskIdOut);
    VERIFY_IS_TRUE(mmcssTaskIdIn == mmcssTaskIdOut);

    if (midiSessionTracker.get() != nullptr)
    {
        VERIFY_SUCCEEDED(midiSessionTracker->RemoveClientSession(m_SessionId));
    }

    cleanupOnFailure.release();
    VERIFY_SUCCEEDED(midiInDevice->Shutdown());
    VERIFY_SUCCEEDED(midiOutDevice->Shutdown());
}

_Use_decl_annotations_
void MidiTransportTestsBase::TestMidiTransportBidi(REFIID iid, MidiDataFormats dataFormat)
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    wil::com_ptr_nothrow<IMidiTransport> midiTransport;
    wil::com_ptr_nothrow<IMidiBidirectional> midiBidiDevice;
    wil::com_ptr_nothrow<IMidiSessionTracker> midiSessionTracker;
    DWORD mmcssTaskId {0};
    wil::unique_event_nothrow allMessagesReceived;
    UINT32 expectedMessageCount = 4;
    UINT midiMessagesReceived = 0;
    TRANSPORTCREATIONPARAMS transportCreationParams { MessageOptionFlags_None, dataFormat, TEST_APPID };
    std::wstring midiBidirectionalInstanceId;

    VERIFY_SUCCEEDED(CoCreateInstance(iid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiTransport)));

    auto cleanupOnFailure = wil::scope_exit([&]() {
        if (midiBidiDevice.get())
        {
            midiBidiDevice->Shutdown();
        }

        if (midiSessionTracker.get() != nullptr)
        {
            midiSessionTracker->RemoveClientSession(m_SessionId);
        }
    });

    // may fail, depending on transport layer support, currently only midisrv transport supports
    // the session tracker.
    midiTransport->Activate(__uuidof(IMidiSessionTracker), (void **) &midiSessionTracker);
    if (midiSessionTracker)
    {
        VERIFY_SUCCEEDED(midiSessionTracker->Initialize());
    }

    VERIFY_SUCCEEDED(midiTransport->Activate(__uuidof(IMidiBidirectional), (void **) &midiBidiDevice));

    m_MidiInCallback = [&](PVOID payload, UINT32 payloadSize, LONGLONG payloadPosition, LONGLONG)
    {
        PrintMidiMessage(payload, payloadSize, (transportCreationParams.DataFormat == MidiDataFormats_UMP)?sizeof(UMP32):sizeof(MIDI_MESSAGE), payloadPosition);

        midiMessagesReceived++;
        if (midiMessagesReceived == expectedMessageCount)
        {
            allMessagesReceived.SetEvent();
        }
    };

    VERIFY_SUCCEEDED(allMessagesReceived.create());

    if (midiSessionTracker.get() != nullptr)
    {
        // create the client session on the service before calling GetEndpoints, which will kickstart
        // the service if it's not already running.
        VERIFY_SUCCEEDED(midiSessionTracker->AddClientSession(m_SessionId, L"TestMidiTransportBidi"));
    }

    if (!GetBidiEndpoint(transportCreationParams.DataFormat, iid, midiBidirectionalInstanceId))
    {
        return;
    }

    LOG_OUTPUT(L"Initializing midi Bidi");
    VERIFY_SUCCEEDED(midiBidiDevice->Initialize(midiBidirectionalInstanceId.c_str(), &transportCreationParams, &mmcssTaskId, this, 0, m_SessionId));

    VERIFY_IS_TRUE(transportCreationParams.DataFormat == MidiDataFormats_UMP || transportCreationParams.DataFormat == MidiDataFormats_ByteStream);

    LOG_OUTPUT(L"Writing midi data");
    if (transportCreationParams.DataFormat == MidiDataFormats_UMP)
    {
        BYTE nativeDataFormat {0};
        VERIFY_SUCCEEDED(GetEndpointNativeDataFormat(midiBidirectionalInstanceId.c_str(), nativeDataFormat));

        // if the peripheral native data format is bytestream, limit to 32 bit messages
        // that will roundtrip, the others will get dropped.
        if (nativeDataFormat == MidiDataFormats_ByteStream)
        {
            VERIFY_SUCCEEDED(midiBidiDevice->SendMidiMessage(MessageOptionFlags_None, (void *) &g_MidiTestData_32, sizeof(UMP32), 0));
            VERIFY_SUCCEEDED(midiBidiDevice->SendMidiMessage(MessageOptionFlags_None, (void *) &g_MidiTestData_32, sizeof(UMP32), 0));
            VERIFY_SUCCEEDED(midiBidiDevice->SendMidiMessage(MessageOptionFlags_None, (void *) &g_MidiTestData_32, sizeof(UMP32), 0));
            VERIFY_SUCCEEDED(midiBidiDevice->SendMidiMessage(MessageOptionFlags_None, (void *) &g_MidiTestData_32, sizeof(UMP32), 0));
        }
        else
        {
            VERIFY_SUCCEEDED(midiBidiDevice->SendMidiMessage(MessageOptionFlags_None, (void *) &g_MidiTestData_32, sizeof(UMP32), 0));
            VERIFY_SUCCEEDED(midiBidiDevice->SendMidiMessage(MessageOptionFlags_None, (void *) &g_MidiTestData_64, sizeof(UMP64), 0));
            VERIFY_SUCCEEDED(midiBidiDevice->SendMidiMessage(MessageOptionFlags_None, (void *) &g_MidiTestData_96, sizeof(UMP96), 0));
            VERIFY_SUCCEEDED(midiBidiDevice->SendMidiMessage(MessageOptionFlags_None, (void *) &g_MidiTestData_128, sizeof(UMP128), 0));
        }
    }
    else
    {
        VERIFY_SUCCEEDED(midiBidiDevice->SendMidiMessage(MessageOptionFlags_None, (void *) &g_MidiTestMessage, sizeof(MIDI_MESSAGE), 0));
        VERIFY_SUCCEEDED(midiBidiDevice->SendMidiMessage(MessageOptionFlags_None, (void *) &g_MidiTestMessage, sizeof(MIDI_MESSAGE), 0));
        VERIFY_SUCCEEDED(midiBidiDevice->SendMidiMessage(MessageOptionFlags_None, (void *) &g_MidiTestMessage, sizeof(MIDI_MESSAGE), 0));
        VERIFY_SUCCEEDED(midiBidiDevice->SendMidiMessage(MessageOptionFlags_None, (void *) &g_MidiTestMessage, sizeof(MIDI_MESSAGE), 0));
    }

    // wait for up to 30 seconds for all the messages
    if(!allMessagesReceived.wait(30000))
    {
        LOG_OUTPUT(L"Failure waiting for messages, timed out.");
    }

    LOG_OUTPUT(L"%d messages expected, %d received", expectedMessageCount, midiMessagesReceived);
    VERIFY_IS_TRUE(midiMessagesReceived == expectedMessageCount);

    LOG_OUTPUT(L"Done, cleaning up");

    cleanupOnFailure.release();
    VERIFY_SUCCEEDED(midiBidiDevice->Shutdown());

    if (midiSessionTracker.get() != nullptr)
    {
        VERIFY_SUCCEEDED(midiSessionTracker->RemoveClientSession(m_SessionId));
    }
}

_Use_decl_annotations_
void MidiTransportTestsBase::TestMidiIO_Latency(REFIID iid, MidiDataFormats dataFormat, BOOL delayedMessages, MessageOptionFlags messageOptions)
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    DWORD messageDelay{ 10 };

    wil::com_ptr_nothrow<IMidiTransport> midiTransport;
    wil::com_ptr_nothrow<IMidiBidirectional> midiBidiDevice;
    wil::com_ptr_nothrow<IMidiSessionTracker> midiSessionTracker;
    DWORD mmcssTaskId{0};
    wil::unique_event_nothrow allMessagesReceived;
    UINT expectedMessageCount = delayedMessages ? (3000 / messageDelay) : 100000;
    expectedMessageCount = (messageOptions == MessageOptionFlags_WaitForSendComplete) ? 300 : expectedMessageCount;

    LARGE_INTEGER performanceFrequency{ 0 };
    LARGE_INTEGER firstSend{ 0 };
    LARGE_INTEGER lastReceive{ 0 };

    LONGLONG firstRoundTripLatency{ 0 };
    LONGLONG lastRoundTripLatency{ 0 };
    long double avgRoundTripLatency{ 0 };
    long double stdevRoundTripLatency{ 0 };

    LONGLONG minRoundTripLatency{ 0xffffffff };
    LONGLONG maxRoundTripLatency{ 0 };

    long double avgReceiveLatency{ 0 };
    long double stdevReceiveLatency{ 0 };
    LONGLONG minReceiveLatency{ 0xffffffff };
    LONGLONG maxReceiveLatency{ 0 };

    LONGLONG previousReceive{ 0 };

    long double qpcPerMs = 0;
    TRANSPORTCREATIONPARAMS transportCreationParams { messageOptions, dataFormat, TEST_APPID };
    std::wstring midiBidirectionalInstanceId;

    QueryPerformanceFrequency(&performanceFrequency);

    qpcPerMs = (performanceFrequency.QuadPart / 1000.);

    UINT midiMessagesReceived = 0;

    VERIFY_SUCCEEDED(CoCreateInstance(iid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiTransport)));

    auto cleanupOnFailure = wil::scope_exit([&]() {
        if (midiBidiDevice.get())
        {
            midiBidiDevice->Shutdown();
        }

        if (midiSessionTracker.get() != nullptr)
        {
            midiSessionTracker->RemoveClientSession(m_SessionId);
        }
    });

    // may fail, depending on transport layer support, currently only midisrv transport supports
    // the session tracker.
    midiTransport->Activate(__uuidof(IMidiSessionTracker), (void **) &midiSessionTracker);
    if (midiSessionTracker)
    {
        VERIFY_SUCCEEDED(midiSessionTracker->Initialize());
    }

    VERIFY_SUCCEEDED(midiTransport->Activate(__uuidof(IMidiBidirectional), (void**)&midiBidiDevice));

    m_MidiInCallback = [&](PVOID , UINT32 , LONGLONG payloadPosition, LONGLONG)
    {
        LARGE_INTEGER qpc{0};
        LONGLONG roundTripLatency{0};

        QueryPerformanceCounter(&qpc);

        // first, we calculate the jitter statistics for how often the
        // recieve function was called. Since the messages are sent at a
        // fixed cadence, they should also be received at a similar cadence.
        // We can only calculate this for the 2nd and on message, since we don't
        // have a previous recieve time for the first message.
        if (previousReceive > 0)
        {
            LONGLONG receiveLatency{0};

            receiveLatency = qpc.QuadPart - previousReceive;

            if (receiveLatency < minReceiveLatency)
            {
                minReceiveLatency = receiveLatency;
            }
            if (receiveLatency > maxReceiveLatency)
            {
                maxReceiveLatency = receiveLatency;
            }

            long double prevAvgReceiveLatency = avgReceiveLatency;

            // running average for the average recieve latency/jitter
            avgReceiveLatency = (prevAvgReceiveLatency + (((long double)receiveLatency - prevAvgReceiveLatency) / ((long double)midiMessagesReceived)));

            stdevReceiveLatency = stdevReceiveLatency + ((receiveLatency - prevAvgReceiveLatency) * (receiveLatency - avgReceiveLatency));
        }
        previousReceive = qpc.QuadPart;

        midiMessagesReceived++;

        // now calculate the round trip statistics based upon
        // the timestamp on the message that was just received relative
        // to when it was sent.
        roundTripLatency = qpc.QuadPart - payloadPosition;

        if (roundTripLatency < minRoundTripLatency)
        {
            minRoundTripLatency = roundTripLatency;
        }
        if (roundTripLatency > maxRoundTripLatency)
        {
            maxRoundTripLatency = roundTripLatency;
        }

        long double prevAvgRoundTripLatency = avgRoundTripLatency;

        // running average for the round trip latency
        avgRoundTripLatency = (prevAvgRoundTripLatency + (((long double) roundTripLatency - prevAvgRoundTripLatency) / (long double) midiMessagesReceived));

        stdevRoundTripLatency = stdevRoundTripLatency + ((roundTripLatency - prevAvgRoundTripLatency) * (roundTripLatency - avgRoundTripLatency));

        // Save the latency for the very first message
        if (1 == midiMessagesReceived)
        {
            firstRoundTripLatency = roundTripLatency;
        }
        else if (midiMessagesReceived == expectedMessageCount)
        {
            lastReceive = qpc;
            lastRoundTripLatency = roundTripLatency;
            allMessagesReceived.SetEvent();
        }
    };

    VERIFY_SUCCEEDED(allMessagesReceived.create());

    if (midiSessionTracker.get() != nullptr)
    {
        // create the client session on the service before calling GetEndpoints, which will kickstart
        // the service if it's not already running.
        VERIFY_SUCCEEDED(midiSessionTracker->AddClientSession(m_SessionId, L"TestMidiIO_Latency"));
    }

    if (!GetBidiEndpoint(transportCreationParams.DataFormat, iid, midiBidirectionalInstanceId))
    {
        return;
    }

    LOG_OUTPUT(L"Initializing midi Bidi");
    VERIFY_SUCCEEDED(midiBidiDevice->Initialize(midiBidirectionalInstanceId.c_str(), &transportCreationParams, &mmcssTaskId, this, 0, m_SessionId));

    VERIFY_IS_TRUE(transportCreationParams.DataFormat == MidiDataFormats_UMP || transportCreationParams.DataFormat == MidiDataFormats_ByteStream);

    LOG_OUTPUT(L"Writing midi data");

    LONGLONG firstSendLatency{ 0 };
    LONGLONG lastSendLatency{ 0 };
    long double avgSendLatency{ 0 };
    long double stddevSendLatency{ 0 };
    LONGLONG minSendLatency{ 0xffffffff };
    LONGLONG maxSendLatency{ 0 };

    for (UINT i = 0; i < expectedMessageCount; i++)
    {
        if (delayedMessages)
        {
            Sleep(messageDelay);
        }

        LARGE_INTEGER qpcBefore{0};
        LARGE_INTEGER qpcAfter{0};
        LONGLONG sendLatency{0};

        QueryPerformanceCounter(&qpcBefore);
        if (transportCreationParams.DataFormat == MidiDataFormats_UMP)
        {
            VERIFY_SUCCEEDED(midiBidiDevice->SendMidiMessage(messageOptions, (void*)&g_MidiTestData_32, sizeof(UMP32), qpcBefore.QuadPart));
        }
        else
        {
            VERIFY_SUCCEEDED(midiBidiDevice->SendMidiMessage(messageOptions, (void*)&g_MidiTestMessage, sizeof(MIDI_MESSAGE), qpcBefore.QuadPart));
        }
        QueryPerformanceCounter(&qpcAfter);

        // track the min, max, and average time that the send call took,
        // this will give us the jitter for sending messages.
        sendLatency = qpcAfter.QuadPart - qpcBefore.QuadPart;
        if (sendLatency < minSendLatency)
        {
            minSendLatency = sendLatency;
        }
        else if (sendLatency > maxSendLatency)
        {
            maxSendLatency = sendLatency;
        }

        long double prevAvgSendLatency = avgSendLatency;

        avgSendLatency = (prevAvgSendLatency + (((long double)sendLatency - prevAvgSendLatency) / ((long double)i + 1)));

        stddevSendLatency = stddevSendLatency + ((sendLatency - prevAvgSendLatency) * (sendLatency - avgSendLatency));

        if (0 == i)
        {
            firstSend = qpcBefore;
            firstSendLatency = sendLatency;
        }
        else if ((expectedMessageCount - 1) == i)
        {
            lastSendLatency = sendLatency;
        }
    }

    LOG_OUTPUT(L"Waiting For Messages...");

    bool continueWaiting {false};
    UINT lastReceivedMessageCount {0};
    do
    {
        continueWaiting = false;
        if(!allMessagesReceived.wait(30000))
        {
            // timeout, see if we're still advancing
            if (lastReceivedMessageCount != midiMessagesReceived)
            {
                // we're advancing, so we can continue waiting.
                continueWaiting = true;
                lastReceivedMessageCount = midiMessagesReceived;
                LOG_OUTPUT(L"%d messages received so far, still waiting...", lastReceivedMessageCount);
            }
        }
    } while(continueWaiting);

    // wait to see if any additional messages come in (there shouldn't be any)
    Sleep(100);

    LOG_OUTPUT(L"%d messages expected, %d received", expectedMessageCount, midiMessagesReceived);
    VERIFY_IS_TRUE(midiMessagesReceived == expectedMessageCount);

    long double elapsedMs = (lastReceive.QuadPart - firstSend.QuadPart) / qpcPerMs;
    long double messagesPerSecond = ((long double)expectedMessageCount) / (elapsedMs / 1000.);

    long double rtLatency = 1000. * (avgRoundTripLatency / qpcPerMs);
    long double firstRtLatency = 1000. * (firstRoundTripLatency / qpcPerMs);
    long double lastRtLatency = 1000. * (lastRoundTripLatency / qpcPerMs);
    long double minRtLatency = 1000. * (minRoundTripLatency / qpcPerMs);
    long double maxRtLatency = 1000. * (maxRoundTripLatency / qpcPerMs);
    long double stddevRtLatency = 1000. * (sqrt(stdevRoundTripLatency / (long double)midiMessagesReceived) / qpcPerMs);

    long double avgSLatency = 1000. * (avgSendLatency / qpcPerMs);
    long double firstSLatency = 1000. * (firstSendLatency / qpcPerMs);
    long double lastSLatency = 1000. * (lastSendLatency / qpcPerMs);
    long double minSLatency = 1000. * (minSendLatency / qpcPerMs);
    long double maxSLatency = 1000. * (maxSendLatency / qpcPerMs);
    long double stddevSLatency = 1000. * (sqrt(stddevSendLatency / (long double)midiMessagesReceived) / qpcPerMs);

    long double avgRLatency = 1000. * (avgReceiveLatency / qpcPerMs);
    long double maxRLatency = 1000. * (maxReceiveLatency / qpcPerMs);
    long double minRLatency = 1000. * (minReceiveLatency / qpcPerMs);
    long double stddevRLatency = 1000. * (sqrt(stdevReceiveLatency / ((long double)midiMessagesReceived - 1.)) / qpcPerMs);

    LOG_OUTPUT(L"****************************************************************************");
    LOG_OUTPUT(L"Elapsed time from start of send to final receive %g ms", elapsedMs);
    LOG_OUTPUT(L"Messages per second %.9g", messagesPerSecond);
    LOG_OUTPUT(L" ");
    LOG_OUTPUT(L"RoundTrip latencies");
    LOG_OUTPUT(L"Average round trip latency %g micro seconds", rtLatency);
    LOG_OUTPUT(L"First message round trip latency %g micro seconds", firstRtLatency);
    LOG_OUTPUT(L"Last message round trip latency %g micro seconds", lastRtLatency);
    LOG_OUTPUT(L"Smallest round trip latency %g micro seconds", minRtLatency);
    LOG_OUTPUT(L"Largest round trip latency %g micro seconds", maxRtLatency);
    LOG_OUTPUT(L"Standard deviation %g micro seconds", stddevRtLatency);
    LOG_OUTPUT(L" ");
    LOG_OUTPUT(L"Message send jitter");
    LOG_OUTPUT(L"Average message send %g micro seconds", avgSLatency);
    LOG_OUTPUT(L"First message send %g micro seconds", firstSLatency);
    LOG_OUTPUT(L"Last message send %g micro seconds", lastSLatency);
    LOG_OUTPUT(L"Smallest message send %g micro seconds", minSLatency);
    LOG_OUTPUT(L"Largest message send %g micro seconds", maxSLatency);
    LOG_OUTPUT(L"Standard deviation %g micro seconds", stddevSLatency);
    LOG_OUTPUT(L" ");
    LOG_OUTPUT(L"Message receive jitter");
    LOG_OUTPUT(L"Average message receive %g micro seconds", avgRLatency);
    LOG_OUTPUT(L"Smallest message receive %g micro seconds", minRLatency);
    LOG_OUTPUT(L"Largest message receive %g micro seconds", maxRLatency);
    LOG_OUTPUT(L"Standard deviation %g micro seconds", stddevRLatency);
    LOG_OUTPUT(L" ");
    LOG_OUTPUT(L"****************************************************************************");

    LOG_OUTPUT(L"Done, cleaning up");

    cleanupOnFailure.release();
    VERIFY_SUCCEEDED(midiBidiDevice->Shutdown());

    if (midiSessionTracker.get() != nullptr)
    {
        VERIFY_SUCCEEDED(midiSessionTracker->RemoveClientSession(m_SessionId));
    }
}

