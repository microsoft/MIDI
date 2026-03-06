// Copyright (c) Microsoft Corporation. All rights reserved.
#include "stdafx.h"
#include "Midi2TransportTestsBase.h"
#include "Midi2SrvTransportTests.h"

void MidiSrvTransportTests::TestMidiSrvTransport_UMP()
{
    TestMidiTransport(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_UMP, FALSE);
}
void MidiSrvTransportTests::TestMidiSrvTransport_ByteStream()
{
    TestMidiTransport(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_ByteStream, FALSE);
}
void MidiSrvTransportTests::TestMidiSrvTransport_Any()
{
    TestMidiTransport(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_Any, FALSE);
}
void MidiSrvTransportTests::TestMidiSrvTransport_UMP_MidiOne()
{
    TestMidiTransport(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_UMP, TRUE);
}
void MidiSrvTransportTests::TestMidiSrvTransport_ByteStream_MidiOne()
{
    TestMidiTransport(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_ByteStream, TRUE);
}
void MidiSrvTransportTests::TestMidiSrvTransport_Any_MidiOne()
{
    TestMidiTransport(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_Any, TRUE);
}

void MidiSrvTransportTests::TestMidiSrvTransportCreationOrder_UMP()
{
    TestMidiTransportCreationOrder(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_UMP, FALSE);
}
void MidiSrvTransportTests::TestMidiSrvTransportCreationOrder_ByteStream()
{
    TestMidiTransportCreationOrder(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_ByteStream, FALSE);
}
void MidiSrvTransportTests::TestMidiSrvTransportCreationOrder_Any()
{
    TestMidiTransportCreationOrder(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_Any, FALSE);
}
void MidiSrvTransportTests::TestMidiSrvTransportCreationOrder_UMP_MidiOne()
{
    TestMidiTransportCreationOrder(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_UMP, TRUE);
}
void MidiSrvTransportTests::TestMidiSrvTransportCreationOrder_ByteStream_MidiOne()
{
    TestMidiTransportCreationOrder(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_ByteStream, TRUE);
}
void MidiSrvTransportTests::TestMidiSrvTransportCreationOrder_Any_MidiOne()
{
    TestMidiTransportCreationOrder(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_Any, TRUE);
}

void MidiSrvTransportTests::TestMidiSrvTransportBidi_UMP()
{
    TestMidiTransportBidi(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_UMP);
}
void MidiSrvTransportTests::TestMidiSrvTransportBidi_ByteStream()
{
    TestMidiTransportBidi(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_ByteStream);
}
void MidiSrvTransportTests::TestMidiSrvTransportBidi_Any()
{
    TestMidiTransportBidi(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_Any);
}

void MidiSrvTransportTests::TestMidiSrvIO_Latency_UMP()
{
    TestMidiIO_Latency(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_UMP, FALSE, MessageOptionFlags_None);
}
void MidiSrvTransportTests::TestMidiSrvIO_Latency_ByteStream()
{
    TestMidiIO_Latency(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_ByteStream, FALSE, MessageOptionFlags_None);
}

void MidiSrvTransportTests::TestMidiSrvIOSlowMessages_Latency_UMP()
{
    TestMidiIO_Latency(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_UMP, TRUE, MessageOptionFlags_None);
}
void MidiSrvTransportTests::TestMidiSrvIOSlowMessages_Latency_ByteStream()
{
    TestMidiIO_Latency(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_ByteStream, TRUE, MessageOptionFlags_None);
}

void MidiSrvTransportTests::TestMidiSrvIO_Latency_UMP_WaitForSendComplete()
{
    TestMidiIO_Latency(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_UMP, FALSE, MessageOptionFlags_WaitForSendComplete);
}
void MidiSrvTransportTests::TestMidiSrvIO_Latency_ByteStream_WaitForSendComplete()
{
    TestMidiIO_Latency(__uuidof(Midi2MidiSrvTransport), MidiDataFormats_ByteStream, FALSE, MessageOptionFlags_WaitForSendComplete);
}

UMP32 g_MidiTestData2_32 = {0x20A04321 };
UMP64 g_MidiTestData2_64 = { 0x40917000, 0x24000000 };
UMP96 g_MidiTestData2_96 = {0xb1C04321, 0xbaadf00d, 0xdeadbeef };
UMP128 g_MidiTestData2_128 = {0xF1D04321, 0xbaadf00d, 0xdeadbeef, 0xd000000d };

MIDI_MESSAGE g_MidiTestMessage2 = { 0x91, 0x70, 0x24 };

_Use_decl_annotations_
void
MidiSrvTransportTests::TestMidiSrvMultiClient
(
    MidiDataFormats dataFormat1,
    MidiDataFormats dataFormat2,
    BOOL midiOneInterface
)
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

    if (!GetEndpoints(dataFormat1, midiOneInterface, __uuidof(Midi2MidiSrvTransport), midiInInstanceId, midiOutInstanceId))
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

void MidiSrvTransportTests::TestMidiSrvMultiClient_UMP_UMP()
{
    TestMidiSrvMultiClient(MidiDataFormats_UMP, MidiDataFormats_UMP, FALSE);
}
void MidiSrvTransportTests::TestMidiSrvMultiClient_ByteStream_ByteStream()
{
    TestMidiSrvMultiClient(MidiDataFormats_ByteStream, MidiDataFormats_ByteStream, FALSE);
}
void MidiSrvTransportTests::TestMidiSrvMultiClient_Any_Any()
{
    TestMidiSrvMultiClient(MidiDataFormats_Any, MidiDataFormats_Any, FALSE);
}
void MidiSrvTransportTests::TestMidiSrvMultiClient_UMP_ByteStream()
{
    TestMidiSrvMultiClient(MidiDataFormats_UMP, MidiDataFormats_ByteStream, FALSE);
}
void MidiSrvTransportTests::TestMidiSrvMultiClient_ByteStream_UMP()
{
    TestMidiSrvMultiClient(MidiDataFormats_ByteStream, MidiDataFormats_UMP, FALSE);
}
void MidiSrvTransportTests::TestMidiSrvMultiClient_Any_ByteStream()
{
    TestMidiSrvMultiClient(MidiDataFormats_Any, MidiDataFormats_ByteStream, FALSE);
}
void MidiSrvTransportTests::TestMidiSrvMultiClient_ByteStream_Any()
{
    TestMidiSrvMultiClient(MidiDataFormats_ByteStream, MidiDataFormats_Any, FALSE);
}
void MidiSrvTransportTests::TestMidiSrvMultiClient_Any_UMP()
{
    TestMidiSrvMultiClient(MidiDataFormats_Any, MidiDataFormats_UMP, FALSE);
}
void MidiSrvTransportTests::TestMidiSrvMultiClient_UMP_Any()
{
    TestMidiSrvMultiClient(MidiDataFormats_UMP, MidiDataFormats_Any, FALSE);
}
void MidiSrvTransportTests::TestMidiSrvMultiClient_UMP_UMP_MidiOne()
{
    TestMidiSrvMultiClient(MidiDataFormats_UMP, MidiDataFormats_UMP, TRUE);
}
void MidiSrvTransportTests::TestMidiSrvMultiClient_ByteStream_ByteStream_MidiOne()
{
    TestMidiSrvMultiClient(MidiDataFormats_ByteStream, MidiDataFormats_ByteStream, TRUE);
}
void MidiSrvTransportTests::TestMidiSrvMultiClient_Any_Any_MidiOne()
{
    TestMidiSrvMultiClient(MidiDataFormats_Any, MidiDataFormats_Any, TRUE);
}
void MidiSrvTransportTests::TestMidiSrvMultiClient_UMP_ByteStream_MidiOne()
{
    TestMidiSrvMultiClient(MidiDataFormats_UMP, MidiDataFormats_ByteStream, TRUE);
}
void MidiSrvTransportTests::TestMidiSrvMultiClient_ByteStream_UMP_MidiOne()
{
    TestMidiSrvMultiClient(MidiDataFormats_ByteStream, MidiDataFormats_UMP, TRUE);
}
void MidiSrvTransportTests::TestMidiSrvMultiClient_Any_ByteStream_MidiOne()
{
    TestMidiSrvMultiClient(MidiDataFormats_Any, MidiDataFormats_ByteStream, TRUE);
}
void MidiSrvTransportTests::TestMidiSrvMultiClient_ByteStream_Any_MidiOne()
{
    TestMidiSrvMultiClient(MidiDataFormats_ByteStream, MidiDataFormats_Any, TRUE);
}
void MidiSrvTransportTests::TestMidiSrvMultiClient_Any_UMP_MidiOne()
{
    TestMidiSrvMultiClient(MidiDataFormats_Any, MidiDataFormats_UMP, TRUE);
}
void MidiSrvTransportTests::TestMidiSrvMultiClient_UMP_Any_MidiOne()
{
    TestMidiSrvMultiClient(MidiDataFormats_UMP, MidiDataFormats_Any, TRUE);
}

_Use_decl_annotations_
void
MidiSrvTransportTests::TestMidiSrvMultiClientBidi
(
    MidiDataFormats dataFormat1,
    MidiDataFormats dataFormat2
)
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

    if (!GetBidiEndpoint(dataFormat1, __uuidof(Midi2MidiSrvTransport), midiInstanceId))
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

void MidiSrvTransportTests::TestMidiSrvMultiClientBidi_UMP_UMP()
{
    TestMidiSrvMultiClientBidi(MidiDataFormats_UMP, MidiDataFormats_UMP);
}
void MidiSrvTransportTests::TestMidiSrvMultiClientBidi_ByteStream_ByteStream()
{
    TestMidiSrvMultiClientBidi(MidiDataFormats_ByteStream, MidiDataFormats_ByteStream);
}
void MidiSrvTransportTests::TestMidiSrvMultiClientBidi_Any_Any()
{
    TestMidiSrvMultiClientBidi(MidiDataFormats_Any, MidiDataFormats_Any);
}
void MidiSrvTransportTests::TestMidiSrvMultiClientBidi_UMP_ByteStream()
{
    TestMidiSrvMultiClientBidi(MidiDataFormats_UMP, MidiDataFormats_ByteStream);
}
void MidiSrvTransportTests::TestMidiSrvMultiClientBidi_ByteStream_UMP()
{
    TestMidiSrvMultiClientBidi(MidiDataFormats_ByteStream, MidiDataFormats_UMP);
}
void MidiSrvTransportTests::TestMidiSrvMultiClientBidi_Any_ByteStream()
{
    TestMidiSrvMultiClientBidi(MidiDataFormats_Any, MidiDataFormats_ByteStream);
}
void MidiSrvTransportTests::TestMidiSrvMultiClientBidi_ByteStream_Any()
{
    TestMidiSrvMultiClientBidi(MidiDataFormats_ByteStream, MidiDataFormats_Any);
}
void MidiSrvTransportTests::TestMidiSrvMultiClientBidi_Any_UMP()
{
    TestMidiSrvMultiClientBidi(MidiDataFormats_Any, MidiDataFormats_UMP);
}
void MidiSrvTransportTests::TestMidiSrvMultiClientBidi_UMP_Any()
{
    TestMidiSrvMultiClientBidi(MidiDataFormats_UMP, MidiDataFormats_Any);
}

struct MultiThreadedMidiTestConfig
{
    std::wstring SessionName;
    MidiDataFormats DataFormat;
    MessageOptionFlags MessageOptions;
    UMP32 MidiTestData_32;
    MIDI_MESSAGE MidiTestMessage;
};

class MultiThreadedMidiTest : public IMidiCallback
{
public:

    MultiThreadedMidiTest()
    {
        UuidCreate(&m_SessionId);
    }

    STDMETHOD(Callback)
    (
        _In_ MessageOptionFlags,
        _In_ PVOID data,
        _In_ UINT size,
        _In_ LONGLONG position,
        _In_ LONGLONG context
    )
    {
        if (m_MidiInCallback)
        {
            m_MidiInCallback(data, size, position, context);
        }
        return S_OK;
    }

    HRESULT
    TestMultiThreadedMidiIO_Latency
    (
        _In_ const MultiThreadedMidiTestConfig& testConfig
    )
    {
        WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    
        wil::com_ptr_nothrow<IMidiTransport> midiTransport;
        wil::com_ptr_nothrow<IMidiBidirectional> midiBidiDevice;
        wil::com_ptr_nothrow<IMidiSessionTracker> midiSessionTracker;
        DWORD mmcssTaskId{0};
        wil::unique_event_nothrow allMessagesReceived;
        UINT expectedMessageCount = 80000;
        expectedMessageCount = (testConfig.MessageOptions == MessageOptionFlags_WaitForSendComplete) ? 1000 : expectedMessageCount;
       
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
        TRANSPORTCREATIONPARAMS transportCreationParams { testConfig.MessageOptions, testConfig.DataFormat, TEST_APPID };
        std::wstring midiBidirectionalInstanceId;

        QueryPerformanceFrequency(&performanceFrequency);

        qpcPerMs = (performanceFrequency.QuadPart / 1000.);

        UINT midiMessagesReceived = 0;

        RETURN_IF_FAILED(CoCreateInstance(__uuidof(Midi2MidiSrvTransport), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiTransport)));

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
            RETURN_IF_FAILED(midiSessionTracker->Initialize());
        }

        RETURN_IF_FAILED(midiTransport->Activate(__uuidof(IMidiBidirectional), (void**)&midiBidiDevice));

        m_MidiInCallback = [&](PVOID payload, UINT32 , LONGLONG payloadPosition, LONGLONG)
        {
            LARGE_INTEGER qpc{0};
            LONGLONG roundTripLatency{0};

            // filter the callback to only look at the messages sent by this thread
            if (((transportCreationParams.DataFormat == MidiDataFormats_UMP) && 0 == memcmp(payload, &(testConfig.MidiTestData_32), sizeof(testConfig.MidiTestData_32))) ||
                ((transportCreationParams.DataFormat == MidiDataFormats_ByteStream) && 0 == memcmp(payload, &(testConfig.MidiTestMessage), sizeof(testConfig.MidiTestMessage))))
            {
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
            }
        };
    
        RETURN_IF_FAILED(allMessagesReceived.create());
    
        if (midiSessionTracker.get() != nullptr)
        {
            // create the client session on the service before calling GetEndpoints, which will kickstart
            // the service if it's not already running.
            RETURN_IF_FAILED(midiSessionTracker->AddClientSession(m_SessionId, testConfig.SessionName.c_str()));
        }
    
        if (!GetBidiEndpoint(transportCreationParams.DataFormat, __uuidof(Midi2MidiSrvTransport), midiBidirectionalInstanceId))
        {
            return E_ABORT;
        }
    
        LOG_OUTPUT(L"%s - Initializing midi Bidi", testConfig.SessionName.c_str());
        RETURN_IF_FAILED(midiBidiDevice->Initialize(midiBidirectionalInstanceId.c_str(), &transportCreationParams, &mmcssTaskId, this, 0, m_SessionId));
    
        RETURN_HR_IF(E_FAIL, !(transportCreationParams.DataFormat == MidiDataFormats_UMP || transportCreationParams.DataFormat == MidiDataFormats_ByteStream));
    
        LOG_OUTPUT(L"%s - Delaying for all threads to initialize", testConfig.SessionName.c_str());
        // allow all threads to initialize before starting to send
        Sleep(100);

        LOG_OUTPUT(L"%s - Writing midi data", testConfig.SessionName.c_str());

        LONGLONG firstSendLatency{ 0 };
        LONGLONG lastSendLatency{ 0 };
        long double avgSendLatency{ 0 };
        long double stddevSendLatency{ 0 };
        LONGLONG minSendLatency{ 0xffffffff };
        LONGLONG maxSendLatency{ 0 };
    
        for (UINT i = 0; i < expectedMessageCount; i++)
        {
            LARGE_INTEGER qpcBefore{0};
            LARGE_INTEGER qpcAfter{0};
            LONGLONG sendLatency{0};
    
            QueryPerformanceCounter(&qpcBefore);
            if (transportCreationParams.DataFormat == MidiDataFormats_UMP)
            {
                HRESULT hr;
                do
                {
                    hr = midiBidiDevice->SendMidiMessage(testConfig.MessageOptions, (void*)&(testConfig.MidiTestData_32), sizeof(UMP32), qpcBefore.QuadPart);
                }while(HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) == hr);
                LOG_IF_FAILED(hr);
            }
            else
            {
                HRESULT hr;
                do
                {
                    hr = midiBidiDevice->SendMidiMessage(testConfig.MessageOptions, (void*)&(testConfig.MidiTestMessage), sizeof(MIDI_MESSAGE), qpcBefore.QuadPart);
                }while(HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) == hr);
                LOG_IF_FAILED(hr);
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
    
        LOG_OUTPUT(L"%s - Waiting For Messages...", testConfig.SessionName.c_str());
    
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
                    LOG_OUTPUT(L"%s - %d messages received so far, still waiting...", testConfig.SessionName.c_str(), lastReceivedMessageCount);
                }
            }
        } while(continueWaiting);
    
        // wait to see if any additional messages come in (there shouldn't be any)
        Sleep(100);
    
        LOG_OUTPUT(L"%s - %d messages expected, %d received", testConfig.SessionName.c_str(), expectedMessageCount, midiMessagesReceived);
    
        m_elapsedMs = (lastReceive.QuadPart - firstSend.QuadPart) / qpcPerMs;
        m_messagesPerSecond = ((long double)midiMessagesReceived) / (m_elapsedMs / 1000.);
    
        m_rtLatency = 1000. * (avgRoundTripLatency / qpcPerMs);
        m_firstRtLatency = 1000. * (firstRoundTripLatency / qpcPerMs);
        m_lastRtLatency = 1000. * (lastRoundTripLatency / qpcPerMs);
        m_minRtLatency = 1000. * (minRoundTripLatency / qpcPerMs);
        m_maxRtLatency = 1000. * (maxRoundTripLatency / qpcPerMs);
        m_stddevRtLatency = 1000. * (sqrt(stdevRoundTripLatency / (long double)midiMessagesReceived) / qpcPerMs);
    
        m_avgSLatency = 1000. * (avgSendLatency / qpcPerMs);
        m_firstSLatency = 1000. * (firstSendLatency / qpcPerMs);
        m_lastSLatency = 1000. * (lastSendLatency / qpcPerMs);
        m_minSLatency = 1000. * (minSendLatency / qpcPerMs);
        m_maxSLatency = 1000. * (maxSendLatency / qpcPerMs);
        m_stddevSLatency = 1000. * (sqrt(stddevSendLatency / (long double)midiMessagesReceived) / qpcPerMs);
    
        m_avgRLatency = 1000. * (avgReceiveLatency / qpcPerMs);
        m_maxRLatency = 1000. * (maxReceiveLatency / qpcPerMs);
        m_minRLatency = 1000. * (minReceiveLatency / qpcPerMs);
        m_stddevRLatency = 1000. * (sqrt(stdevReceiveLatency / ((long double)midiMessagesReceived - 1.)) / qpcPerMs);

        LOG_OUTPUT(L"%s - Done, cleaning up", testConfig.SessionName.c_str());
    
        cleanupOnFailure.release();
        RETURN_IF_FAILED(midiBidiDevice->Shutdown());
    
        if (midiSessionTracker.get() != nullptr)
        {
            RETURN_IF_FAILED(midiSessionTracker->RemoveClientSession(m_SessionId));
        }

        RETURN_HR_IF(E_FAIL, midiMessagesReceived != expectedMessageCount);

        return S_OK;
    }

    void
    operator()
    (
        _In_ const MultiThreadedMidiTestConfig& testConfig,
        _Out_ HRESULT &outResult
    )
    {
        outResult = TestMultiThreadedMidiIO_Latency(testConfig);
    }

    // This test is not refcounted, stubbed.
    STDMETHODIMP QueryInterface(REFIID, void**) { return S_OK; }
    STDMETHODIMP_(ULONG) AddRef() { return 1; }
    STDMETHODIMP_(ULONG) Release() { return 1; }

    void LogResults()
    {
        LOG_OUTPUT(L"Elapsed time from start of send to final receive %g ms", m_elapsedMs);
        LOG_OUTPUT(L"Messages per second %.9g", m_messagesPerSecond);
        LOG_OUTPUT(L" ");
        LOG_OUTPUT(L"RoundTrip latencies");
        LOG_OUTPUT(L"Average round trip latency %g micro seconds", m_rtLatency);
        LOG_OUTPUT(L"First message round trip latency %g micro seconds", m_firstRtLatency);
        LOG_OUTPUT(L"Last message round trip latency %g micro seconds", m_lastRtLatency);
        LOG_OUTPUT(L"Smallest round trip latency %g micro seconds", m_minRtLatency);
        LOG_OUTPUT(L"Largest round trip latency %g micro seconds", m_maxRtLatency);
        LOG_OUTPUT(L"Standard deviation %g micro seconds", m_stddevRtLatency);
        LOG_OUTPUT(L" ");
        LOG_OUTPUT(L"Message send jitter");
        LOG_OUTPUT(L"Average message send %g micro seconds", m_avgSLatency);
        LOG_OUTPUT(L"First message send %g micro seconds", m_firstSLatency);
        LOG_OUTPUT(L"Last message send %g micro seconds", m_lastSLatency);
        LOG_OUTPUT(L"Smallest message send %g micro seconds", m_minSLatency);
        LOG_OUTPUT(L"Largest message send %g micro seconds", m_maxSLatency);
        LOG_OUTPUT(L"Standard deviation %g micro seconds", m_stddevSLatency);
        LOG_OUTPUT(L" ");
        LOG_OUTPUT(L"Message receive jitter");
        LOG_OUTPUT(L"Average message receive %g micro seconds", m_avgRLatency);
        LOG_OUTPUT(L"Smallest message receive %g micro seconds", m_minRLatency);
        LOG_OUTPUT(L"Largest message receive %g micro seconds", m_maxRLatency);
        LOG_OUTPUT(L"Standard deviation %g micro seconds", m_stddevRLatency);
        LOG_OUTPUT(L" ");
    }

private:
    MessageOptionFlags m_Options { MessageOptionFlags_None };

    std::function<void(PVOID, UINT32, LONGLONG, LONGLONG)> m_MidiInCallback;

    long double m_elapsedMs {0};
    long double m_messagesPerSecond {0};
    long double m_rtLatency {0};
    long double m_firstRtLatency {0};
    long double m_lastRtLatency {0};
    long double m_minRtLatency {0};
    long double m_maxRtLatency {0};
    long double m_stddevRtLatency {0};
    long double m_avgSLatency {0};
    long double m_firstSLatency {0};
    long double m_lastSLatency {0};
    long double m_minSLatency {0};
    long double m_maxSLatency {0};
    long double m_stddevSLatency {0};
    long double m_avgRLatency {0};
    long double m_maxRLatency {0};
    long double m_minRLatency {0};
    long double m_stddevRLatency {0};

    GUID m_SessionId{};
};

#define NUM_TEST_THREADS 2

void MidiSrvTransportTests::MultiThreadedMidiSendTest()
{
    // four test threads, two threads waiting for send to complete, two not
    HRESULT results[NUM_TEST_THREADS];

    UMP32 midiTestData_32_0 = {0x20AA1234 };
    MIDI_MESSAGE midiTestMessage_0 = { 0xAA, 0x12, 0x34 };
    UMP32 midiTestData_32_1 = {0x20AB2234 };
    MIDI_MESSAGE midiTestMessage_1 = { 0xAB, 0x22, 0x34 };
    std::wstring midiBidirectionalInstanceId;

    // Confirm the required bidi endpoint is available for testing, otherwise skip
    if (!GetBidiEndpoint(MidiDataFormats_UMP, __uuidof(Midi2MidiSrvTransport), midiBidirectionalInstanceId))
    {
        return;
    }

    MultiThreadedMidiTest test[NUM_TEST_THREADS];
    MultiThreadedMidiTestConfig testConfig[NUM_TEST_THREADS] = {
            { L"Thread0Session", MidiDataFormats_ByteStream, MessageOptionFlags_WaitForSendComplete, midiTestData_32_0, midiTestMessage_0 },
            { L"Thread1Session", MidiDataFormats_UMP, MessageOptionFlags_None, midiTestData_32_1, midiTestMessage_1 }
        };

    std::vector<std::thread> threads;
    std::fill(std::begin(results), std::end(results), E_FAIL);

    LOG_OUTPUT(L"****************************************************************************");
    // Launch the threads
    for (int i = 0; i < NUM_TEST_THREADS; ++i)
    {
        LOG_OUTPUT(L"Launching thread %d", i);
        threads.emplace_back([&, i]() {
            test[i](testConfig[i], results[i]);
            });
    }
    LOG_OUTPUT(L"****************************************************************************");
    LOG_OUTPUT(L"Waiting for threads");
    // Wait for all threads to finish
    for (auto& t : threads)
    {
        t.join();
    }
    LOG_OUTPUT(L"****************************************************************************");
    LOG_OUTPUT(L"Logging Results");
    // Verify all threads succeeded
    for (int i = 0; i < NUM_TEST_THREADS; ++i)
    {
        LOG_OUTPUT(L"****************************************************************************");
        LOG_OUTPUT(L"Thread %d - 0x%08x", i, results[i]);
        test[i].LogResults();
        LOG_OUTPUT(L"****************************************************************************");
    }

    for (int i = 0; i < NUM_TEST_THREADS; ++i)
    {
        VERIFY_SUCCEEDED(results[i]);
    }
}

void MidiSrvTransportTests::TestDriverDeviceInterfaceIdIsPresent()
{
    // retrieve the number of midi in/out ports before running, to ensure the service is
    // started and winmm port information is fully initialized.
    UINT numOutDevices = midiOutGetNumDevs();
    UINT numInDevices = midiInGetNumDevs();

    std::vector<std::unique_ptr<MIDIU_DEVICE>> devices;
    VERIFY_SUCCEEDED(MidiSWDeviceEnum::EnumerateDevices(devices, [&](PMIDIU_DEVICE device)
        {
            if (device->MidiOne)
            {
                return true;
            }
            else
            {
                return false;
            }
        }));

    if (devices.size() == 0)
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"No Midi 1 endpoints found. Cannot validate interface id");
        return;
    }

    for (auto const& device: devices)
    {
        // DriverDeviceInterfaceId on the SWD should be valid
        VERIFY_IS_FALSE(device->DriverDeviceInterfaceId.empty());

        ULONG interfaceIdSize = 0;
        MMRESULT result;

        // retrieve the size of the interface id reported by winmm
        if (device->Flow == MidiFlowOut)
        {
            VERIFY_IS_TRUE(device->WinmmPortNumber <= numOutDevices);
            result = midiOutMessage((HMIDIOUT)(uintptr_t)device->WinmmPortNumber, DRV_QUERYDEVICEINTERFACESIZE, (DWORD_PTR) &interfaceIdSize, 0);
        }
        else
        {
            VERIFY_IS_TRUE(device->WinmmPortNumber <= numInDevices);
            result = midiInMessage((HMIDIIN)(uintptr_t)device->WinmmPortNumber, DRV_QUERYDEVICEINTERFACESIZE, (DWORD_PTR) &interfaceIdSize, 0);
        }

        // validate the size and allocate a storage buffer
        VERIFY_IS_TRUE(result == MMSYSERR_NOERROR);
        VERIFY_IS_TRUE(interfaceIdSize > 0);
        wil::unique_cotaskmem_string winmmDeviceInterfaceId(static_cast<PWSTR>(CoTaskMemAlloc(interfaceIdSize)));

        // retrieve the interface id from winmm
        if (device->Flow == MidiFlowOut)
        {
            result = midiOutMessage((HMIDIOUT)(uintptr_t)device->WinmmPortNumber, DRV_QUERYDEVICEINTERFACE, (DWORD_PTR) winmmDeviceInterfaceId.get(), interfaceIdSize);
        }
        else
        {
            result = midiInMessage((HMIDIIN)(uintptr_t)device->WinmmPortNumber, DRV_QUERYDEVICEINTERFACE, (DWORD_PTR) winmmDeviceInterfaceId.get(), interfaceIdSize);
        }

        // verify retrieval succeeded and that the string reported by winmm matches the string saved on the SWD
        VERIFY_IS_TRUE(result == MMSYSERR_NOERROR);
        LOG_OUTPUT(L"* Endpoint: %s", device->DeviceId.c_str());
        LOG_OUTPUT(L"  Driver Device Interface Id: %s", device->DriverDeviceInterfaceId.c_str());
        LOG_OUTPUT(L"  WinMM Driver Device Interface Id: %s", winmmDeviceInterfaceId.get());
        VERIFY_IS_TRUE(device->DriverDeviceInterfaceId == winmmDeviceInterfaceId.get());
    }
}

void
MidiSrvTransportTests::GetKSAMinMidiEndpoints
(
    std::vector<std::unique_ptr<MIDIU_DEVICE>> &midiInDevices,
    std::vector<std::unique_ptr<MIDIU_DEVICE>> &midiOutDevices
)
{

    VERIFY_SUCCEEDED(MidiSWDeviceEnum::EnumerateDevices(midiInDevices, [&](PMIDIU_DEVICE device)
    {
        if (device->TransportLayer == __uuidof(Midi2KSAggregateTransport) &&
            (device->Flow == MidiFlowIn || device->Flow == MidiFlowBidirectional) &&
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"MINMIDI"))
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
        if (device->TransportLayer == __uuidof(Midi2KSAggregateTransport) &&
            (device->Flow == MidiFlowOut || device->Flow == MidiFlowBidirectional) &&
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"MINMIDI"))
        {
            return true;
        }
        else
        {
            return false;
        }
    }));
}

#define KSA_PORT_TIMEOUT 120000
#define KSA_PORT_QUERY_DELAY 500

HRESULT
MidiSrvTransportTests::WaitForDeviceCount
(
    std::vector<std::unique_ptr<MIDIU_DEVICE>> &midiInDevices,
    std::vector<std::unique_ptr<MIDIU_DEVICE>> &midiOutDevices,
    UINT expectedInCount,
    UINT expectedOutCount
)
{
    UINT64 startTime = GetTickCount64();

    while ((midiInDevices.size() != expectedInCount || midiOutDevices.size() != expectedOutCount) && (GetTickCount64() - startTime < KSA_PORT_TIMEOUT))
    {
        midiInDevices.clear();
        midiOutDevices.clear();
        Sleep(KSA_PORT_QUERY_DELAY);
        GetKSAMinMidiEndpoints(midiInDevices, midiOutDevices);
    };

    if (midiInDevices.size() != expectedInCount || midiOutDevices.size() != expectedOutCount)
    {
        LOG_OUTPUT(L"Expect %d in ports and %d out ports", expectedInCount, expectedOutCount);
        LOG_OUTPUT(L"Actually %d in ports and %d out ports", midiInDevices.size(), midiOutDevices.size());
    }

    RETURN_HR_IF(E_FAIL, midiInDevices.size() != expectedInCount || midiOutDevices.size() != expectedOutCount);

    return S_OK;
}

void
MidiSrvTransportTests::TestKSAPortEnumeration()
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    wil::com_ptr_nothrow<IMidiTransport> midiTransport;
    wil::com_ptr_nothrow<IMidiSessionTracker> midiSessionTracker;
    std::vector<std::unique_ptr<MIDIU_DEVICE>> midiInDevices;
    std::vector<std::unique_ptr<MIDIU_DEVICE>> midiOutDevices;
    std::wstring minmidiInstanceId;

    VERIFY_SUCCEEDED(CoCreateInstance(__uuidof(Midi2MidiSrvTransport), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiTransport)));

    VERIFY_SUCCEEDED(midiTransport->Activate(__uuidof(IMidiSessionTracker), (void **) &midiSessionTracker));
    VERIFY_SUCCEEDED(midiSessionTracker->Initialize());
    VERIFY_SUCCEEDED(midiSessionTracker->AddClientSession(m_SessionId, L"TestKSAPortEnumeration"));

    GetKSAMinMidiEndpoints(midiInDevices, midiOutDevices);

    if (midiInDevices.size() == 0 || midiOutDevices.size() == 0)
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"Test requires MinMidi.");
        return;
    }

    minmidiInstanceId = midiInDevices[0]->ParentDeviceInstanceId;
    LOG_OUTPUT(L"Testing %s", minmidiInstanceId.c_str());

    auto cleanup = wil::scope_exit([&]()
    {
        // Simulate a surprise removal to restore the driver back to the baseline state in the event this
        // test fails.
        SendDriverCommand(KSPROPERTY_MINMIDICONTROL_SURPRISEREMOVESIMULATION, 1);
        SetDeviceEnabled(minmidiInstanceId.c_str(), false);
        SetDeviceEnabled(minmidiInstanceId.c_str(), true);
    });

    // remove all of the endpoints.
    LOG_OUTPUT(L"Removing all minmidi ports");
    while (SUCCEEDED(SendDriverCommand(KSPROPERTY_MINMIDICONTROL_REMOVEPORT, (DWORD) MidiDataFormats_ByteStream)));
    while (SUCCEEDED(SendDriverCommand(KSPROPERTY_MINMIDICONTROL_REMOVEPORT, (DWORD) MidiDataFormats_UMP)));

    // There should be 0
    LOG_OUTPUT(L"Confirming removed");
    VERIFY_SUCCEEDED(WaitForDeviceCount(midiInDevices, midiOutDevices, 0, 0));

    // now enable all available bytestream interfaces on the driver and confirm the ports show up
    UINT expectedPortcount = 0;
    while (SUCCEEDED(SendDriverCommand(KSPROPERTY_MINMIDICONTROL_ADDPORT, (DWORD) MidiDataFormats_ByteStream)))
    {
        expectedPortcount++;

        LOG_OUTPUT(L"Enabling port %d", expectedPortcount);

        // A midi in port, a midi out port, and a bidi port are added for the first call, and an in and out
        // port added from then on up until 16 ports, then for each 17th port an additional bidi is added.
        UINT expectedCount = expectedPortcount?(expectedPortcount+((INT)((expectedPortcount+15)/16))):0;
        VERIFY_SUCCEEDED(WaitForDeviceCount(midiInDevices, midiOutDevices, expectedCount, expectedCount));
    }

    // disable all the bytestream interfaces and confirm the ports go away
    while (SUCCEEDED(SendDriverCommand(KSPROPERTY_MINMIDICONTROL_REMOVEPORT, (DWORD) MidiDataFormats_ByteStream)))
    {
        expectedPortcount--;
   
        LOG_OUTPUT(L"Disabling port %d", expectedPortcount);
        UINT expectedCount = expectedPortcount?(expectedPortcount+((INT)((expectedPortcount+15)/16))):0;
        VERIFY_SUCCEEDED(WaitForDeviceCount(midiInDevices, midiOutDevices, expectedCount, expectedCount));
    }

    // There again should be be 0
    LOG_OUTPUT(L"Confirming all gone");
    VERIFY_SUCCEEDED(WaitForDeviceCount(midiInDevices, midiOutDevices, 0, 0));

}

bool MidiSrvTransportTests::TestSetup()
{
    m_MidiInCallback = nullptr;
    return true;
}

bool MidiSrvTransportTests::TestCleanup()
{
    return true;
}

bool MidiSrvTransportTests::ClassSetup()
{
    PrintStagingStates();

    // Ensure midi discovery is in the default state, in the event that it was modified prior to this
    // test running and that test failed to restore the value because it crashed.
    // No need to restart the service after setting & stopping it, it'll demand start.
    StopMIDIService();
    SetMidiDiscovery(true);

    return true;
}

bool MidiSrvTransportTests::ClassCleanup()
{
    return true;
}

