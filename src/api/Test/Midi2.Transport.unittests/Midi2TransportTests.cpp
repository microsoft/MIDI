// Copyright (c) Microsoft Corporation. All rights reserved.
#include "stdafx.h"
#include "Midi2KSTransport.h"
#include "Midi2MidiSrvTransport.h"

#include "Midi2TransportTests.h"

#include <Devpkey.h>
#include "MidiSwEnum.h"
#include <initguid.h>
#include "MidiDefs.h"
#include "MidiKsDef.h"
#include "MidiKsCommon.h"
#include "MidiXProc.h"
#include <mmdeviceapi.h>
#include "KsHandleWrapper.h"
#include "MidiKsEnum.h"

#include <wil/resource.h>
#include <setupapi.h>
#include <cfgmgr32.h>

#define TEST_APPID {0xc24cc593, 0xbc6b, 0x4726,{ 0xb5, 0x52, 0xbe, 0xc8, 0x2d, 0xed, 0xb6, 0x8c}}

using unique_hdevinfo = wil::unique_any_handle_invalid<decltype(&::SetupDiDestroyDeviceInfoList), ::SetupDiDestroyDeviceInfoList>;

using namespace winrt::Windows::Devices::Enumeration;

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

BOOL GetBidiEndpoint(MidiDataFormats dataFormat, BOOL midiSrv, std::wstring& midiBidiInstanceId)
{
    std::vector<std::unique_ptr<MIDIU_DEVICE>> midiBidiDevices;
    VERIFY_SUCCEEDED(MidiSWDeviceEnum::EnumerateDevices(midiBidiDevices, [&](PMIDIU_DEVICE device)
    {
        // When testing directly against the KS transport, we're limited to endpoints created by
        // KS transport, and supporting the format we wish to test.
        // When testing against midisrv, we can be more flexible.
        if ((midiSrv || device->TransportLayer == (GUID) __uuidof(Midi2KSTransport)) &&
            device->Flow == MidiFlowBidirectional &&
            (std::wstring::npos != device->ParentDeviceInstanceId.find(L"MINMIDI") ||
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"VID_CAFE&PID_4001&MI_02") ||
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"VID_0582&PID_0168&MI_00")) &&  // Roland UM_ONE, easy to loop back to itself for testing.
            (midiSrv || (0 != ((DWORD) device->SupportedDataFormats & (DWORD) dataFormat))))
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

BOOL GetEndpoints(MidiDataFormats dataFormat, BOOL midiOneInterface, BOOL midiSrv, std::wstring& midiInInstanceId, std::wstring& midiOutInstanceId)
{
    std::vector<std::unique_ptr<MIDIU_DEVICE>> midiInDevices;
    std::vector<std::unique_ptr<MIDIU_DEVICE>> midiOutDevices;

    VERIFY_SUCCEEDED(MidiSWDeviceEnum::EnumerateDevices(midiInDevices, [&](PMIDIU_DEVICE device)
    {
        if ((midiSrv || device->TransportLayer == (GUID) __uuidof(Midi2KSTransport)) &&
            (device->Flow == MidiFlowIn || device->Flow == MidiFlowBidirectional) &&
            (std::wstring::npos != device->ParentDeviceInstanceId.find(L"MINMIDI") ||
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"VID_CAFE&PID_4001&MI_02") ||
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"VID_0582&PID_0168&MI_00")) &&
            (midiOneInterface == device->MidiOne) &&
            (midiSrv || (0 != ((DWORD) device->SupportedDataFormats & (DWORD) dataFormat))))
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
        if ((midiSrv || device->TransportLayer == (GUID) __uuidof(Midi2KSTransport)) &&
            (device->Flow == MidiFlowIn || device->Flow == MidiFlowBidirectional) &&
            (std::wstring::npos != device->ParentDeviceInstanceId.find(L"MINMIDI") ||
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"VID_CAFE&PID_4001&MI_02") ||
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"VID_0582&PID_0168&MI_00")) &&
            (midiOneInterface == device->MidiOne) &&
            (midiSrv || (0 != ((DWORD) device->SupportedDataFormats & (DWORD) dataFormat))))
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
void MidiTransportTests::TestMidiTransport(REFIID iid, MidiDataFormats dataFormat, BOOL midiOneInterface)
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

    if (!GetEndpoints(transportCreationParams.DataFormat, midiOneInterface, __uuidof(Midi2MidiSrvTransport) == iid, midiInInstanceId, midiOutInstanceId))
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

// NOTE: activation with a MidiOne interface does not apply to the KS transport
// layer, it only knows how to activate using information that is contained on the midi 2 swd.
// This is why _MidiOne tests are not performed on the KSTransport.
void MidiTransportTests::TestMidiKSTransport_UMP()
{
    // UMP endpoint activated via midi 2 swd
    TestMidiTransport(__uuidof(Midi2KSTransport), MidiDataFormats_UMP, FALSE);
}
void MidiTransportTests::TestMidiKSTransport_ByteStream()
{
    // ByteStream endpoint activated via midi 2 swd
    TestMidiTransport(__uuidof(Midi2KSTransport), MidiDataFormats_ByteStream, FALSE);
}
void MidiTransportTests::TestMidiKSTransport_Any()
{
    // ByteStream endpoint activated via midi 2 swd
    TestMidiTransport(__uuidof(Midi2KSTransport), MidiDataFormats_Any, FALSE);
}

void MidiTransportTests::TestMidiSrvTransport_UMP()
{
    TestMidiTransport(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_UMP, FALSE);
}
void MidiTransportTests::TestMidiSrvTransport_ByteStream()
{
    TestMidiTransport(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_ByteStream, FALSE);
}
void MidiTransportTests::TestMidiSrvTransport_Any()
{
    TestMidiTransport(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_Any, FALSE);
}
void MidiTransportTests::TestMidiSrvTransport_UMP_MidiOne()
{
    TestMidiTransport(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_UMP, TRUE);
}
void MidiTransportTests::TestMidiSrvTransport_ByteStream_MidiOne()
{
    TestMidiTransport(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_ByteStream, TRUE);
}
void MidiTransportTests::TestMidiSrvTransport_Any_MidiOne()
{
    TestMidiTransport(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_Any, TRUE);
}

_Use_decl_annotations_
void MidiTransportTests::TestMidiTransportCreationOrder(REFIID iid, _In_ MidiDataFormats dataFormat, BOOL midiOneInterface)
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

    if (!GetEndpoints(transportCreationParams.DataFormat, midiOneInterface, __uuidof(Midi2MidiSrvTransport) == iid, midiInInstanceId, midiOutInstanceId))
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

void MidiTransportTests::TestMidiKSTransportCreationOrder_UMP()
{
    TestMidiTransportCreationOrder(__uuidof(Midi2KSTransport), MidiDataFormats_UMP, FALSE);
}
void MidiTransportTests::TestMidiKSTransportCreationOrder_ByteStream()
{
    TestMidiTransportCreationOrder(__uuidof(Midi2KSTransport), MidiDataFormats_ByteStream, FALSE);
}
void MidiTransportTests::TestMidiKSTransportCreationOrder_Any()
{
    TestMidiTransportCreationOrder(__uuidof(Midi2KSTransport), MidiDataFormats_Any, FALSE);
}


void MidiTransportTests::TestMidiSrvTransportCreationOrder_UMP()
{
    TestMidiTransportCreationOrder(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_UMP, FALSE);
}
void MidiTransportTests::TestMidiSrvTransportCreationOrder_ByteStream()
{
    TestMidiTransportCreationOrder(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_ByteStream, FALSE);
}
void MidiTransportTests::TestMidiSrvTransportCreationOrder_Any()
{
    TestMidiTransportCreationOrder(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_Any, FALSE);
}
void MidiTransportTests::TestMidiSrvTransportCreationOrder_UMP_MidiOne()
{
    TestMidiTransportCreationOrder(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_UMP, TRUE);
}
void MidiTransportTests::TestMidiSrvTransportCreationOrder_ByteStream_MidiOne()
{
    TestMidiTransportCreationOrder(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_ByteStream, TRUE);
}
void MidiTransportTests::TestMidiSrvTransportCreationOrder_Any_MidiOne()
{
    TestMidiTransportCreationOrder(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_Any, TRUE);
}

_Use_decl_annotations_
void MidiTransportTests::TestMidiTransportBidi(REFIID iid, MidiDataFormats dataFormat)
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

    if (!GetBidiEndpoint(transportCreationParams.DataFormat, __uuidof(Midi2MidiSrvTransport) == iid, midiBidirectionalInstanceId))
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

void MidiTransportTests::TestMidiKSTransportBidi_UMP()
{
    TestMidiTransportBidi(__uuidof(Midi2KSTransport), MidiDataFormats_UMP);
}
void MidiTransportTests::TestMidiKSTransportBidi_ByteStream()
{
    TestMidiTransportBidi(__uuidof(Midi2KSTransport), MidiDataFormats_ByteStream);
}
void MidiTransportTests::TestMidiKSTransportBidi_Any()
{
    TestMidiTransportBidi(__uuidof(Midi2KSTransport), MidiDataFormats_Any);
}

void MidiTransportTests::TestMidiSrvTransportBidi_UMP()
{
    TestMidiTransportBidi(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_UMP);
}
void MidiTransportTests::TestMidiSrvTransportBidi_ByteStream()
{
    TestMidiTransportBidi(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_ByteStream);
}
void MidiTransportTests::TestMidiSrvTransportBidi_Any()
{
    TestMidiTransportBidi(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_Any);
}

_Use_decl_annotations_
void MidiTransportTests::TestMidiIO_Latency(REFIID iid, MidiDataFormats dataFormat, BOOL delayedMessages, MessageOptionFlags messageOptions)
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    DWORD messageDelay{ 10 };

    wil::com_ptr_nothrow<IMidiTransport> midiTransport;
    wil::com_ptr_nothrow<IMidiBidirectional> midiBidiDevice;
    wil::com_ptr_nothrow<IMidiSessionTracker> midiSessionTracker;
    DWORD mmcssTaskId{0};
    wil::unique_event_nothrow allMessagesReceived;
    UINT expectedMessageCount = delayedMessages ? (3000 / messageDelay) : 100000;

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
        // the timestamp on the message that was just recieved relative
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

    if (!GetBidiEndpoint(transportCreationParams.DataFormat, __uuidof(Midi2MidiSrvTransport) == iid, midiBidirectionalInstanceId))
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
                LOG_OUTPUT(L"%d messages recieved so far, still waiting...", lastReceivedMessageCount);
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

void MidiTransportTests::TestMidiKSIO_Latency_UMP()
{
    TestMidiIO_Latency(__uuidof(Midi2KSTransport), MidiDataFormats_UMP, FALSE, MessageOptionFlags_None);
}
void MidiTransportTests::TestMidiKSIO_Latency_ByteStream()
{
    TestMidiIO_Latency(__uuidof(Midi2KSTransport), MidiDataFormats_ByteStream, FALSE, MessageOptionFlags_None);
}

void MidiTransportTests::TestMidiSrvIO_Latency_UMP()
{
    TestMidiIO_Latency(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_UMP, FALSE, MessageOptionFlags_None);
}
void MidiTransportTests::TestMidiSrvIO_Latency_ByteStream()
{
    TestMidiIO_Latency(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_ByteStream, FALSE, MessageOptionFlags_None);
}

void MidiTransportTests::TestMidiKSIOSlowMessages_Latency_UMP()
{
    TestMidiIO_Latency(__uuidof(Midi2KSTransport), MidiDataFormats_UMP, TRUE, MessageOptionFlags_None);
}
void MidiTransportTests::TestMidiKSIOSlowMessages_Latency_ByteStream()
{
    TestMidiIO_Latency(__uuidof(Midi2KSTransport), MidiDataFormats_ByteStream, TRUE, MessageOptionFlags_None);
}

void MidiTransportTests::TestMidiSrvIOSlowMessages_Latency_UMP()
{
    TestMidiIO_Latency(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_UMP, TRUE, MessageOptionFlags_None);
}
void MidiTransportTests::TestMidiSrvIOSlowMessages_Latency_ByteStream()
{
    TestMidiIO_Latency(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_ByteStream, TRUE, MessageOptionFlags_None);
}

void MidiTransportTests::TestMidiKSIO_Latency_UMP_WaitForSendComplete()
{
    TestMidiIO_Latency(__uuidof(Midi2KSTransport), MidiDataFormats_UMP, FALSE, MessageOptionFlags_WaitForSendComplete);
}
void MidiTransportTests::TestMidiKSIO_Latency_ByteStream_WaitForSendComplete()
{
    TestMidiIO_Latency(__uuidof(Midi2KSTransport), MidiDataFormats_ByteStream, FALSE, MessageOptionFlags_WaitForSendComplete);
}

void MidiTransportTests::TestMidiSrvIO_Latency_UMP_WaitForSendComplete()
{
    TestMidiIO_Latency(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_UMP, FALSE, MessageOptionFlags_WaitForSendComplete);
}
void MidiTransportTests::TestMidiSrvIO_Latency_ByteStream_WaitForSendComplete()
{
    TestMidiIO_Latency(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_ByteStream, FALSE, MessageOptionFlags_WaitForSendComplete);
}

UMP32 g_MidiTestData2_32 = {0x20A04321 };
UMP64 g_MidiTestData2_64 = { 0x40917000, 0x24000000 };
UMP96 g_MidiTestData2_96 = {0xb1C04321, 0xbaadf00d, 0xdeadbeef };
UMP128 g_MidiTestData2_128 = {0xF1D04321, 0xbaadf00d, 0xdeadbeef, 0xd000000d };

MIDI_MESSAGE g_MidiTestMessage2 = { 0x91, 0x70, 0x24 };

_Use_decl_annotations_
void MidiTransportTests::TestMidiSrvMultiClient(MidiDataFormats dataFormat1, MidiDataFormats dataFormat2, BOOL midiOneInterface)
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    wil::com_ptr_nothrow<IMidiTransport> midiTransport;
    wil::com_ptr_nothrow<IMidiSessionTracker> midiSessionTracker;
    wil::com_ptr_nothrow<IMidiIn> midiInDevice1;
    wil::com_ptr_nothrow<IMidiOut> midiOutDevice1;
    wil::com_ptr_nothrow<IMidiIn> midiInDevice2;
    wil::com_ptr_nothrow<IMidiOut> midiOutDevice2;
    DWORD mmcssTaskId{ 0 };
    wil::unique_event_nothrow allMessagesReceived;

    // We're sending 4 messages on MidiOut1, and 4 messages on
    // MidiOut2. MidiIn1 will recieve both sets, so 8 messages.
    // MidiIn2 will also receive both sets, 8 messages. So we
    // have a total of 8 messages.
    UINT32 expectedMessageCount = 16;
    UINT midiMessagesReceived = 0;
    UINT midiMessageReceived1 = 0;
    UINT midiMessageReceived2 = 0;

    // as there are two clients using the same callback, intentionally,
    // we need to ensure that only 1 is processed at a time.
    wil::critical_section callbackLock;

    TRANSPORTCREATIONPARAMS transportCreationParams1 { MessageOptionFlags_None, dataFormat1, TEST_APPID };
    TRANSPORTCREATIONPARAMS transportCreationParams2 { MessageOptionFlags_None, dataFormat2, TEST_APPID };

    std::wstring midiInInstanceId;
    std::wstring midiOutInstanceId;

    VERIFY_SUCCEEDED(CoCreateInstance(__uuidof(Midi2MidiSrvTransport), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiTransport)));

    auto cleanupOnFailure = wil::scope_exit([&]() {
        if (midiOutDevice1.get())
        {
            midiOutDevice1->Shutdown();
        }
        if (midiOutDevice2.get())
        {
            midiOutDevice2->Shutdown();
        }
        if (midiInDevice1.get())
        {
            midiInDevice1->Shutdown();
        }
        if (midiInDevice2.get())
        {
            midiInDevice2->Shutdown();
        }

        if (midiSessionTracker.get() != nullptr)
        {
            midiSessionTracker->RemoveClientSession(m_SessionId);
        }
    });

    LONGLONG context1 = 1;
    LONGLONG context2 = 2;

    // may fail, depending on transport layer support, currently only midisrv transport supports
    // the session tracker.
    midiTransport->Activate(__uuidof(IMidiSessionTracker), (void **) &midiSessionTracker);
    if (midiSessionTracker)
    {
        VERIFY_SUCCEEDED(midiSessionTracker->Initialize());
    }

    VERIFY_SUCCEEDED(midiTransport->Activate(__uuidof(IMidiIn), (void**)&midiInDevice1));
    VERIFY_SUCCEEDED(midiTransport->Activate(__uuidof(IMidiIn), (void**)&midiInDevice2));
    VERIFY_SUCCEEDED(midiTransport->Activate(__uuidof(IMidiOut), (void**)&midiOutDevice1));
    VERIFY_SUCCEEDED(midiTransport->Activate(__uuidof(IMidiOut), (void**)&midiOutDevice2));

    m_MidiInCallback = [&](PVOID payload, UINT32 payloadSize, LONGLONG payloadPosition, LONGLONG context)
    {
        auto lock = callbackLock.lock();

        LOG_OUTPUT(L"Message %I64d on client %I64d", payloadPosition, context);

        PrintMidiMessage(payload, payloadSize, sizeof(MIDI_MESSAGE), payloadPosition);

        midiMessagesReceived++;

        if (context == context1)
        {
            midiMessageReceived1++;
        }
        else if (context == context2)
        {
            midiMessageReceived2++;
        }

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
        VERIFY_SUCCEEDED(midiSessionTracker->AddClientSession(m_SessionId, L"TestMidiSrvMultiClient"));
    }

    if (!GetEndpoints(dataFormat1, midiOneInterface, TRUE, midiInInstanceId, midiOutInstanceId))
    {
        return;
    }

    UMP32 midiTestData_32 = g_MidiTestData_32;
    UMP64 midiTestData_64 = g_MidiTestData_64;
    UMP96 midiTestData_96 = g_MidiTestData_96;
    UMP128 midiTestData_128 = g_MidiTestData_128;

    UMP32 midiTestData2_32 = g_MidiTestData2_32;
    UMP64 midiTestData2_64 = g_MidiTestData2_64;
    UMP96 midiTestData2_96 = g_MidiTestData2_96;
    UMP128 midiTestData2_128 = g_MidiTestData2_128;

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
        midiTestData2_32.data1 |= midiInGroupIndex;
        midiTestData2_64.data1 |= midiInGroupIndex;
        midiTestData2_96.data1 |= midiInGroupIndex;
        midiTestData2_128.data1 |= midiInGroupIndex;
    }

    LOG_OUTPUT(L"Initializing midi in 1");
    VERIFY_SUCCEEDED(midiInDevice1->Initialize(midiInInstanceId.c_str(), &transportCreationParams1, &mmcssTaskId, this, context1, m_SessionId));

    LOG_OUTPUT(L"Initializing midi in 2");
    VERIFY_SUCCEEDED(midiInDevice2->Initialize(midiInInstanceId.c_str(), &transportCreationParams2, &mmcssTaskId, this, context2, m_SessionId));

    LOG_OUTPUT(L"Initializing midi out 1");
    VERIFY_SUCCEEDED(midiOutDevice1->Initialize(midiOutInstanceId.c_str(), &transportCreationParams1, &mmcssTaskId, m_SessionId));

    LOG_OUTPUT(L"Initializing midi out 2");
    VERIFY_SUCCEEDED(midiOutDevice2->Initialize(midiOutInstanceId.c_str(), &transportCreationParams2, &mmcssTaskId, m_SessionId));

    VERIFY_IS_TRUE(transportCreationParams1.DataFormat == MidiDataFormats_UMP || transportCreationParams1.DataFormat == MidiDataFormats_ByteStream);
    VERIFY_IS_TRUE(transportCreationParams2.DataFormat == MidiDataFormats_UMP || transportCreationParams2.DataFormat == MidiDataFormats_ByteStream);

    BYTE nativeInDataFormat {0};
    BYTE nativeOutDataFormat {0};
    VERIFY_SUCCEEDED(GetEndpointNativeDataFormat(midiInInstanceId.c_str(), nativeInDataFormat));
    VERIFY_SUCCEEDED(GetEndpointNativeDataFormat(midiOutInstanceId.c_str(), nativeOutDataFormat));

    LOG_OUTPUT(L"Writing midi data");

    if (transportCreationParams1.DataFormat == MidiDataFormats_UMP)
    {
        // if the peripheral native data format is bytestream, limit to 32 bit messages
        // that will roundtrip, the others will get dropped.
        if (nativeInDataFormat == MidiDataFormats_ByteStream || 
            nativeOutDataFormat == MidiDataFormats_ByteStream)
        {
            VERIFY_SUCCEEDED(midiOutDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&midiTestData_32, sizeof(UMP32), 29));
            VERIFY_SUCCEEDED(midiOutDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&midiTestData_32, sizeof(UMP32), 30));
            VERIFY_SUCCEEDED(midiOutDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&midiTestData_32, sizeof(UMP32), 31));
            VERIFY_SUCCEEDED(midiOutDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&midiTestData_32, sizeof(UMP32), 32));
        }
        else
        {
            VERIFY_SUCCEEDED(midiOutDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&midiTestData_32, sizeof(UMP32), 11));
            VERIFY_SUCCEEDED(midiOutDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&midiTestData_64, sizeof(UMP64), 12));
            if (transportCreationParams2.DataFormat == MidiDataFormats_UMP)
            {
                // if communicating UMP to UMP, we can send 96 and 128 messages
                VERIFY_SUCCEEDED(midiOutDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&midiTestData_96, sizeof(UMP96), 13));
                VERIFY_SUCCEEDED(midiOutDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&midiTestData_128, sizeof(UMP128), 14));
            }
            else
            {
                // if communicating to bytestream, UMP 96 and 128 don't translate, so resend the note on message
                VERIFY_SUCCEEDED(midiOutDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&midiTestData_64, sizeof(UMP64), 13));
                VERIFY_SUCCEEDED(midiOutDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&midiTestData_64, sizeof(UMP64), 14));
            }
        }
    }
    else
    {
        VERIFY_SUCCEEDED(midiOutDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestMessage, sizeof(MIDI_MESSAGE), 15));
        VERIFY_SUCCEEDED(midiOutDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestMessage, sizeof(MIDI_MESSAGE), 16));
        VERIFY_SUCCEEDED(midiOutDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestMessage, sizeof(MIDI_MESSAGE), 17));
        VERIFY_SUCCEEDED(midiOutDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestMessage, sizeof(MIDI_MESSAGE), 18));
    }

    if (transportCreationParams2.DataFormat == MidiDataFormats_UMP)
    {
        // if the peripheral native data format is bytestream, limit to 32 bit messages
        // that will roundtrip, the others will get dropped.
        if (nativeInDataFormat == MidiDataFormats_ByteStream || 
            nativeOutDataFormat == MidiDataFormats_ByteStream)
        {
            VERIFY_SUCCEEDED(midiOutDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&midiTestData2_32, sizeof(UMP32), 33));
            VERIFY_SUCCEEDED(midiOutDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&midiTestData2_32, sizeof(UMP32), 34));
            VERIFY_SUCCEEDED(midiOutDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&midiTestData2_32, sizeof(UMP32), 35));
            VERIFY_SUCCEEDED(midiOutDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&midiTestData2_32, sizeof(UMP32), 36));
        }
        else
        {
            VERIFY_SUCCEEDED(midiOutDevice2->SendMidiMessage(MessageOptionFlags_None, (void*)&midiTestData2_32, sizeof(UMP32), 21));
            VERIFY_SUCCEEDED(midiOutDevice2->SendMidiMessage(MessageOptionFlags_None, (void*)&midiTestData2_64, sizeof(UMP64), 22));
            if (transportCreationParams1.DataFormat == MidiDataFormats_UMP)
            {
                VERIFY_SUCCEEDED(midiOutDevice2->SendMidiMessage(MessageOptionFlags_None, (void*)&midiTestData2_96, sizeof(UMP96), 23));
                VERIFY_SUCCEEDED(midiOutDevice2->SendMidiMessage(MessageOptionFlags_None, (void*)&midiTestData2_128, sizeof(UMP128), 24));
            }
            else
            {
                VERIFY_SUCCEEDED(midiOutDevice2->SendMidiMessage(MessageOptionFlags_None, (void*)&midiTestData2_64, sizeof(UMP64), 23));
                VERIFY_SUCCEEDED(midiOutDevice2->SendMidiMessage(MessageOptionFlags_None, (void*)&midiTestData2_64, sizeof(UMP64), 24));
            }
        }
    }
    else
    {
        VERIFY_SUCCEEDED(midiOutDevice2->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestMessage2, sizeof(MIDI_MESSAGE), 25));
        VERIFY_SUCCEEDED(midiOutDevice2->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestMessage2, sizeof(MIDI_MESSAGE), 26));
        VERIFY_SUCCEEDED(midiOutDevice2->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestMessage2, sizeof(MIDI_MESSAGE), 27));
        VERIFY_SUCCEEDED(midiOutDevice2->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestMessage2, sizeof(MIDI_MESSAGE), 28));
    }

    // wait for up to 30 seconds for all the messages
    if (!allMessagesReceived.wait(30000))
    {
        LOG_OUTPUT(L"Failure waiting for messages, timed out.");
    }

    LOG_OUTPUT(L"%d messages expected, %d received", expectedMessageCount, midiMessagesReceived);
    LOG_OUTPUT(L"%d messages on client 1, %d message on client 2", midiMessageReceived1, midiMessageReceived2);
    VERIFY_IS_TRUE(midiMessagesReceived == expectedMessageCount);
    VERIFY_IS_TRUE((midiMessageReceived1 + midiMessageReceived2) == expectedMessageCount);

    LOG_OUTPUT(L"Done, cleaning up");

    cleanupOnFailure.release();
    VERIFY_SUCCEEDED(midiOutDevice1->Shutdown());
    VERIFY_SUCCEEDED(midiOutDevice2->Shutdown());
    VERIFY_SUCCEEDED(midiInDevice1->Shutdown());
    VERIFY_SUCCEEDED(midiInDevice2->Shutdown());

    if (midiSessionTracker.get() != nullptr)
    {
        VERIFY_SUCCEEDED(midiSessionTracker->RemoveClientSession(m_SessionId));
    }
}

void MidiTransportTests::TestMidiSrvMultiClient_UMP_UMP()
{
    TestMidiSrvMultiClient(MidiDataFormats_UMP, MidiDataFormats_UMP, FALSE);
}
void MidiTransportTests::TestMidiSrvMultiClient_ByteStream_ByteStream()
{
    TestMidiSrvMultiClient(MidiDataFormats_ByteStream, MidiDataFormats_ByteStream, FALSE);
}
void MidiTransportTests::TestMidiSrvMultiClient_Any_Any()
{
    TestMidiSrvMultiClient(MidiDataFormats_Any, MidiDataFormats_Any, FALSE);
}
void MidiTransportTests::TestMidiSrvMultiClient_UMP_ByteStream()
{
    TestMidiSrvMultiClient(MidiDataFormats_UMP, MidiDataFormats_ByteStream, FALSE);
}
void MidiTransportTests::TestMidiSrvMultiClient_ByteStream_UMP()
{
    TestMidiSrvMultiClient(MidiDataFormats_ByteStream, MidiDataFormats_UMP, FALSE);
}
void MidiTransportTests::TestMidiSrvMultiClient_Any_ByteStream()
{
    TestMidiSrvMultiClient(MidiDataFormats_Any, MidiDataFormats_ByteStream, FALSE);
}
void MidiTransportTests::TestMidiSrvMultiClient_ByteStream_Any()
{
    TestMidiSrvMultiClient(MidiDataFormats_ByteStream, MidiDataFormats_Any, FALSE);
}
void MidiTransportTests::TestMidiSrvMultiClient_Any_UMP()
{
    TestMidiSrvMultiClient(MidiDataFormats_Any, MidiDataFormats_UMP, FALSE);
}
void MidiTransportTests::TestMidiSrvMultiClient_UMP_Any()
{
    TestMidiSrvMultiClient(MidiDataFormats_UMP, MidiDataFormats_Any, FALSE);
}
void MidiTransportTests::TestMidiSrvMultiClient_UMP_UMP_MidiOne()
{
    TestMidiSrvMultiClient(MidiDataFormats_UMP, MidiDataFormats_UMP, TRUE);
}
void MidiTransportTests::TestMidiSrvMultiClient_ByteStream_ByteStream_MidiOne()
{
    TestMidiSrvMultiClient(MidiDataFormats_ByteStream, MidiDataFormats_ByteStream, TRUE);
}
void MidiTransportTests::TestMidiSrvMultiClient_Any_Any_MidiOne()
{
    TestMidiSrvMultiClient(MidiDataFormats_Any, MidiDataFormats_Any, TRUE);
}
void MidiTransportTests::TestMidiSrvMultiClient_UMP_ByteStream_MidiOne()
{
    TestMidiSrvMultiClient(MidiDataFormats_UMP, MidiDataFormats_ByteStream, TRUE);
}
void MidiTransportTests::TestMidiSrvMultiClient_ByteStream_UMP_MidiOne()
{
    TestMidiSrvMultiClient(MidiDataFormats_ByteStream, MidiDataFormats_UMP, TRUE);
}
void MidiTransportTests::TestMidiSrvMultiClient_Any_ByteStream_MidiOne()
{
    TestMidiSrvMultiClient(MidiDataFormats_Any, MidiDataFormats_ByteStream, TRUE);
}
void MidiTransportTests::TestMidiSrvMultiClient_ByteStream_Any_MidiOne()
{
    TestMidiSrvMultiClient(MidiDataFormats_ByteStream, MidiDataFormats_Any, TRUE);
}
void MidiTransportTests::TestMidiSrvMultiClient_Any_UMP_MidiOne()
{
    TestMidiSrvMultiClient(MidiDataFormats_Any, MidiDataFormats_UMP, TRUE);
}
void MidiTransportTests::TestMidiSrvMultiClient_UMP_Any_MidiOne()
{
    TestMidiSrvMultiClient(MidiDataFormats_UMP, MidiDataFormats_Any, TRUE);
}

_Use_decl_annotations_
void MidiTransportTests::TestMidiSrvMultiClientBidi(MidiDataFormats dataFormat1, MidiDataFormats dataFormat2)
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    wil::com_ptr_nothrow<IMidiTransport> midiTransport;
    wil::com_ptr_nothrow<IMidiSessionTracker> midiSessionTracker;
    wil::com_ptr_nothrow<IMidiBidirectional> midiDevice1;
    wil::com_ptr_nothrow<IMidiBidirectional> midiDevice2;
    DWORD mmcssTaskId{ 0 };
    wil::unique_event_nothrow allMessagesReceived;

    // We're sending 4 messages on MidiOut1, and 4 messages on
    // MidiOut2. MidiIn1 will recieve both sets, so 8 messages.
    // MidiIn2 will also receive both sets, 8 messages. So we
    // have a total of 8 messages.
    UINT32 expectedMessageCount = 16;
    UINT midiMessagesReceived = 0;
    UINT midiMessageReceived1 = 0;
    UINT midiMessageReceived2 = 0;

    // as there are two clients using the same callback, intentionally,
    // we need to ensure that only 1 is processed at a time.
    wil::critical_section callbackLock;

    TRANSPORTCREATIONPARAMS transportCreationParams1 { MessageOptionFlags_None, dataFormat1, TEST_APPID };
    TRANSPORTCREATIONPARAMS transportCreationParams2 { MessageOptionFlags_None, dataFormat2, TEST_APPID };
    std::wstring midiInstanceId;

    VERIFY_SUCCEEDED(CoCreateInstance(__uuidof(Midi2MidiSrvTransport), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiTransport)));

    auto cleanupOnFailure = wil::scope_exit([&]() {
        if (midiDevice1.get())
        {
            midiDevice1->Shutdown();
        }
        if (midiDevice2.get())
        {
            midiDevice2->Shutdown();
        }

        if (midiSessionTracker.get() != nullptr)
        {
            midiSessionTracker->RemoveClientSession(m_SessionId);
        }
    });

    LONGLONG context1 = 1;
    LONGLONG context2 = 2;

    VERIFY_SUCCEEDED(midiTransport->Activate(__uuidof(IMidiBidirectional), (void**)&midiDevice1));
    VERIFY_SUCCEEDED(midiTransport->Activate(__uuidof(IMidiBidirectional), (void**)&midiDevice2));

    // may fail, depending on transport layer support, currently only midisrv transport supports
    // the session tracker.
    midiTransport->Activate(__uuidof(IMidiSessionTracker), (void **) &midiSessionTracker);
    if (midiSessionTracker)
    {
        VERIFY_SUCCEEDED(midiSessionTracker->Initialize());
    }

    m_MidiInCallback = [&](PVOID payload, UINT32 payloadSize, LONGLONG payloadPosition, LONGLONG context)
    {
        auto lock = callbackLock.lock();

        LOG_OUTPUT(L"Message %I64d on client %I64d", payloadPosition, context);

        PrintMidiMessage(payload, payloadSize, sizeof(MIDI_MESSAGE), payloadPosition);

        midiMessagesReceived++;

        if (context == context1)
        {
            midiMessageReceived1++;
        }
        else if (context == context2)
        {
            midiMessageReceived2++;
        }

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
        VERIFY_SUCCEEDED(midiSessionTracker->AddClientSession(m_SessionId, L"TestMidiSrvMultiClientBidi"));
    }

    if (!GetBidiEndpoint(dataFormat1, TRUE, midiInstanceId))
    {
        return;
    }

    LOG_OUTPUT(L"Initializing midi in 1");
    VERIFY_SUCCEEDED(midiDevice1->Initialize(midiInstanceId.c_str(), &transportCreationParams1, &mmcssTaskId, this, context1, m_SessionId));

    LOG_OUTPUT(L"Initializing midi in 2");
    VERIFY_SUCCEEDED(midiDevice2->Initialize(midiInstanceId.c_str(), &transportCreationParams2, &mmcssTaskId, this, context2, m_SessionId));

    VERIFY_IS_TRUE(transportCreationParams1.DataFormat == MidiDataFormats_UMP || transportCreationParams1.DataFormat == MidiDataFormats_ByteStream);
    VERIFY_IS_TRUE(transportCreationParams2.DataFormat == MidiDataFormats_UMP || transportCreationParams2.DataFormat == MidiDataFormats_ByteStream);

    BYTE nativeDataFormat {0};
    VERIFY_SUCCEEDED(GetEndpointNativeDataFormat(midiInstanceId.c_str(), nativeDataFormat));

    LOG_OUTPUT(L"Writing midi data");
    if (transportCreationParams1.DataFormat == MidiDataFormats_UMP)
    {
        // if the peripheral native data format is bytestream, limit to 32 bit messages
        // that will roundtrip, the others will get dropped.
        if (nativeDataFormat == MidiDataFormats_ByteStream)
        {
            VERIFY_SUCCEEDED(midiDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData_32, sizeof(UMP32), 33));
            VERIFY_SUCCEEDED(midiDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData_32, sizeof(UMP32), 34));
            VERIFY_SUCCEEDED(midiDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData_32, sizeof(UMP32), 35));
            VERIFY_SUCCEEDED(midiDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData_32, sizeof(UMP32), 36));
        }
        else
        {
            VERIFY_SUCCEEDED(midiDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData_32, sizeof(UMP32), 11));
            VERIFY_SUCCEEDED(midiDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData_64, sizeof(UMP64), 12));
            if (transportCreationParams2.DataFormat == MidiDataFormats_UMP)
            {
                VERIFY_SUCCEEDED(midiDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData_96, sizeof(UMP96), 13));
                VERIFY_SUCCEEDED(midiDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData_128, sizeof(UMP128), 14));
            }
            else
            {
                VERIFY_SUCCEEDED(midiDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData_64, sizeof(UMP64), 13));
                VERIFY_SUCCEEDED(midiDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData_64, sizeof(UMP64), 14));
            }
        }
    }
    else
    {
        VERIFY_SUCCEEDED(midiDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestMessage, sizeof(MIDI_MESSAGE), 15));
        VERIFY_SUCCEEDED(midiDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestMessage, sizeof(MIDI_MESSAGE), 16));
        VERIFY_SUCCEEDED(midiDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestMessage, sizeof(MIDI_MESSAGE), 17));
        VERIFY_SUCCEEDED(midiDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestMessage, sizeof(MIDI_MESSAGE), 18));
    }

    if (transportCreationParams2.DataFormat == MidiDataFormats_UMP)
    {
        // if the peripheral native data format is bytestream, limit to 32 bit messages
        // that will roundtrip, the others will get dropped.
        if (nativeDataFormat == MidiDataFormats_ByteStream)
        {
            VERIFY_SUCCEEDED(midiDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData2_32, sizeof(UMP32), 33));
            VERIFY_SUCCEEDED(midiDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData2_32, sizeof(UMP32), 34));
            VERIFY_SUCCEEDED(midiDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData2_32, sizeof(UMP32), 35));
            VERIFY_SUCCEEDED(midiDevice1->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData2_32, sizeof(UMP32), 36));
        }
        else
        {
            VERIFY_SUCCEEDED(midiDevice2->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData2_32, sizeof(UMP32), 21));
            VERIFY_SUCCEEDED(midiDevice2->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData2_64, sizeof(UMP64), 22));
            if (transportCreationParams1.DataFormat == MidiDataFormats_UMP)
            {
                VERIFY_SUCCEEDED(midiDevice2->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData2_96, sizeof(UMP96), 23));
                VERIFY_SUCCEEDED(midiDevice2->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData2_128, sizeof(UMP128), 24));
            }
            else
            {
                VERIFY_SUCCEEDED(midiDevice2->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData2_64, sizeof(UMP64), 23));
                VERIFY_SUCCEEDED(midiDevice2->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestData2_64, sizeof(UMP64), 24));
            }
        }
    }
    else
    {
        VERIFY_SUCCEEDED(midiDevice2->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestMessage2, sizeof(MIDI_MESSAGE), 25));
        VERIFY_SUCCEEDED(midiDevice2->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestMessage2, sizeof(MIDI_MESSAGE), 26));
        VERIFY_SUCCEEDED(midiDevice2->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestMessage2, sizeof(MIDI_MESSAGE), 27));
        VERIFY_SUCCEEDED(midiDevice2->SendMidiMessage(MessageOptionFlags_None, (void*)&g_MidiTestMessage2, sizeof(MIDI_MESSAGE), 28));
    }

    // wait for up to 30 seconds for all the messages
    if (!allMessagesReceived.wait(30000))
    {
        LOG_OUTPUT(L"Failure waiting for messages, timed out.");
    }

    LOG_OUTPUT(L"%d messages expected, %d received", expectedMessageCount, midiMessagesReceived);
    LOG_OUTPUT(L"%d messages on client 1, %d message on client 2", midiMessageReceived1, midiMessageReceived2);
    VERIFY_IS_TRUE(midiMessagesReceived == expectedMessageCount);
    VERIFY_IS_TRUE((midiMessageReceived1 + midiMessageReceived2) == expectedMessageCount);

    LOG_OUTPUT(L"Done, cleaning up");

    cleanupOnFailure.release();
    VERIFY_SUCCEEDED(midiDevice1->Shutdown());
    VERIFY_SUCCEEDED(midiDevice2->Shutdown());

    if (midiSessionTracker.get() != nullptr)
    {
        VERIFY_SUCCEEDED(midiSessionTracker->RemoveClientSession(m_SessionId));
    }
}

void MidiTransportTests::TestMidiSrvMultiClientBidi_UMP_UMP()
{
    TestMidiSrvMultiClientBidi(MidiDataFormats_UMP, MidiDataFormats_UMP);
}
void MidiTransportTests::TestMidiSrvMultiClientBidi_ByteStream_ByteStream()
{
    TestMidiSrvMultiClientBidi(MidiDataFormats_ByteStream, MidiDataFormats_ByteStream);
}
void MidiTransportTests::TestMidiSrvMultiClientBidi_Any_Any()
{
    TestMidiSrvMultiClientBidi(MidiDataFormats_Any, MidiDataFormats_Any);
}
void MidiTransportTests::TestMidiSrvMultiClientBidi_UMP_ByteStream()
{
    TestMidiSrvMultiClientBidi(MidiDataFormats_UMP, MidiDataFormats_ByteStream);
}
void MidiTransportTests::TestMidiSrvMultiClientBidi_ByteStream_UMP()
{
    TestMidiSrvMultiClientBidi(MidiDataFormats_ByteStream, MidiDataFormats_UMP);
}
void MidiTransportTests::TestMidiSrvMultiClientBidi_Any_ByteStream()
{
    TestMidiSrvMultiClientBidi(MidiDataFormats_Any, MidiDataFormats_ByteStream);
}
void MidiTransportTests::TestMidiSrvMultiClientBidi_ByteStream_Any()
{
    TestMidiSrvMultiClientBidi(MidiDataFormats_ByteStream, MidiDataFormats_Any);
}
void MidiTransportTests::TestMidiSrvMultiClientBidi_Any_UMP()
{
    TestMidiSrvMultiClientBidi(MidiDataFormats_Any, MidiDataFormats_UMP);
}
void MidiTransportTests::TestMidiSrvMultiClientBidi_UMP_Any()
{
    TestMidiSrvMultiClientBidi(MidiDataFormats_UMP, MidiDataFormats_Any);
}

HRESULT
GetInstanceIdFromFilterName(PCWSTR filterName, std::wstring& instanceIdOut)
{
    instanceIdOut.clear();

    // Use KSCATEGORY_AUDIO_DEVICE_INTERFACE GUID
    static const GUID InterfaceCategory = {0x6994ad04, 0x93ef, 0x11d0, {0xa3, 0xcc, 0x0, 0xa0, 0xc9, 0x22, 0x31, 0x96}};

    auto devInfo = unique_hdevinfo{SetupDiGetClassDevs(&InterfaceCategory, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE)};
    RETURN_HR_IF(HRESULT_FROM_WIN32(GetLastError()), !devInfo);

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData = {};
    deviceInterfaceData.cbSize = sizeof(deviceInterfaceData);

    for (DWORD index = 0; SetupDiEnumDeviceInterfaces(devInfo.get(), nullptr, &InterfaceCategory, index, &deviceInterfaceData); ++index)
    {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(devInfo.get(), &deviceInterfaceData, nullptr, 0, &requiredSize, nullptr);

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        {
            continue;
        }

        std::unique_ptr<BYTE[]> detailBuffer = std::make_unique<BYTE[]>(requiredSize);
        auto* detailData = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(detailBuffer.get());
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        SP_DEVINFO_DATA deviceInfoData = {};
        deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        if (!SetupDiGetDeviceInterfaceDetail(devInfo.get(), &deviceInterfaceData, detailData, requiredSize, nullptr, &deviceInfoData))
        {
            continue;
        }

        // Match to the filter name we are looking for
        if (_wcsicmp(detailData->DevicePath, filterName) == 0)
        {
            WCHAR instanceId[MAX_DEVICE_ID_LEN] = {};
            if (SetupDiGetDeviceInstanceId(devInfo.get(), &deviceInfoData, instanceId, ARRAYSIZE(instanceId), nullptr))
            {
                instanceIdOut = instanceId;
                return S_OK;
            }
            else
            {
                return HRESULT_FROM_WIN32(GetLastError());
            }
        }
    }

    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}

bool SetDeviceEnabled(_In_ PCWSTR deviceInstanceId, _In_ bool enable)
{
    bool result = false;
    HDEVINFO devInfo = SetupDiCreateDeviceInfoList(nullptr, nullptr);
    if (devInfo == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    SP_DEVINFO_DATA devInfoData = {};
    devInfoData.cbSize = sizeof(devInfoData);
    if (SetupDiOpenDeviceInfo(devInfo, deviceInstanceId, nullptr, 0, &devInfoData))
    {
        SP_PROPCHANGE_PARAMS propChangeParams = {};
        propChangeParams.ClassInstallHeader.cbSize = sizeof(propChangeParams.ClassInstallHeader);
        propChangeParams.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
        propChangeParams.Scope = DICS_FLAG_GLOBAL;
        propChangeParams.StateChange = enable ? DICS_ENABLE : DICS_DISABLE;

        auto cleanupOnFailure = wil::scope_exit([&]() {

            // if request was to disable and we failed, restore to enabled state
            // so it isn't applied after the next reboot, interfering with other tests.
            if (false == enable)
            {
                propChangeParams.StateChange = DICS_ENABLE;
                SetupDiSetClassInstallParams(devInfo, &devInfoData, &propChangeParams.ClassInstallHeader, sizeof(propChangeParams));
                SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, devInfo, &devInfoData);
            }
        });

        if (SetupDiSetClassInstallParams(devInfo, &devInfoData, &propChangeParams.ClassInstallHeader, sizeof(propChangeParams)))
        {
            if (SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, devInfo, &devInfoData))
            {
                result = true;
                cleanupOnFailure.release();
            }
        }
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return result;
}

void MidiTransportTests::TestKsHandleWrapperQueryRemove_FilterHandle()
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

    filterWrapper.Close();

    if (!SetDeviceEnabled(instanceId.c_str(), false))
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"Unable to disable device (it may be virtual or protected).");
        return;
    }

    VERIFY_IS_TRUE(filterWrapper.GetHandle() == nullptr);

    if (!SetDeviceEnabled(instanceId.c_str(), true))
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"Unable to enable device (it may be virtual or protected).");
        return;
    }
}

void MidiTransportTests::TestKsHandleWrapperQueryRemove_PinHandle()
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

    // Open filter handle
    KsHandleWrapper parentFilterWrapper(filterName);
    VERIFY_SUCCEEDED(parentFilterWrapper.Open());

    // Get handle duplicate
    wil::unique_handle dupHandle(parentFilterWrapper.GetHandle());
    VERIFY_IS_NOT_NULL(dupHandle.get());

    // Create pin wrapper
    KsHandleWrapper pinWrapper(filterName, pinId, MidiTransport_CyclicUMP, dupHandle.get());
    VERIFY_SUCCEEDED(pinWrapper.Open());

    std::wstring instanceId;
    HRESULT hr = GetInstanceIdFromFilterName(filterName, instanceId);
    VERIFY_SUCCEEDED(hr);
    VERIFY_IS_FALSE(instanceId.empty());

    pinWrapper.Close();
    parentFilterWrapper.Close();

    if (!SetDeviceEnabled(instanceId.c_str(), false))
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"Unable to disable device (it may be virtual or protected).");
        return;
    }

    VERIFY_IS_TRUE(pinWrapper.GetHandle() == nullptr);

    if (!SetDeviceEnabled(instanceId.c_str(), true))
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"Unable to enable device (it may be virtual or protected).");
        return;
    }
}

void MidiTransportTests::TestKsHandleWrapperSurpriseRemove_FilterHandle()
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

    // TODO: Simulate surprise remove
    filterWrapper.OnDeviceRemove();

    VERIFY_IS_TRUE(filterWrapper.GetHandle() == nullptr);
}

void MidiTransportTests::TestKsHandleWrapperSurpriseRemove_PinHandle()
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

    // Open filter handle
    KsHandleWrapper parentFilterWrapper(filterName);
    VERIFY_SUCCEEDED(parentFilterWrapper.Open());

    // Get handle duplicate
    wil::unique_handle dupHandle(parentFilterWrapper.GetHandle());
    VERIFY_IS_NOT_NULL(dupHandle.get());

    // Create pin wrapper
    KsHandleWrapper pinWrapper(filterName, pinId, MidiTransport_CyclicUMP, dupHandle.get());
    VERIFY_SUCCEEDED(pinWrapper.Open());

    // TODO: Simulate surprise remove
    pinWrapper.OnDeviceRemove();

    VERIFY_IS_TRUE(pinWrapper.GetHandle() == nullptr);
}

bool MidiTransportTests::TestSetup()
{
    m_MidiInCallback = nullptr;
    return true;
}

bool MidiTransportTests::TestCleanup()
{
    return true;
}

bool MidiTransportTests::ClassSetup()
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    
    return true;
}

bool MidiTransportTests::ClassCleanup()
{
    return true;
}
