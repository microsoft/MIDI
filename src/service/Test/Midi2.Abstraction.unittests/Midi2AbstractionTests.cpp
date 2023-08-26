// Copyright (c) Microsoft Corporation. All rights reserved.
#include "stdafx.h"
#include "midi2KSabstraction.h"
#include "Midi2MidiSrvAbstraction.h"

#include "Midi2AbstractionTests.h"

#include <Devpkey.h>
#include "MidiKsCommon.h"
#include "MidiSwEnum.h"
#include "MidiDefs.h"
#include "MidiXProc.h"

#include <initguid.h>
#include <mmdeviceapi.h>

_Use_decl_annotations_
void MidiAbstractionTests::TestMidiAbstraction(REFIID Iid)
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    MidiSWDeviceEnum midiDeviceEnum;
    wil::com_ptr_nothrow<IMidiAbstraction> midiAbstraction;
    wil::com_ptr_nothrow<IMidiIn> midiInDevice;
    wil::com_ptr_nothrow<IMidiOut> midiOutDevice;
    DWORD mmcssTaskId {0};
    wil::unique_event_nothrow allMessagesReceived;
    UINT32 expectedMessageCount = 4;
    UINT midiMessagesReceived = 0;

    VERIFY_SUCCEEDED(CoCreateInstance(Iid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiAbstraction)));

    VERIFY_SUCCEEDED(midiAbstraction->Activate(__uuidof(IMidiIn), (void **) &midiInDevice));
    VERIFY_SUCCEEDED(midiAbstraction->Activate(__uuidof(IMidiOut), (void **) &midiOutDevice));

    m_MidiInCallback = [&](PVOID payload, UINT32 payloadSize, LONGLONG payloadPosition)
    {
        PrintMidiMessage(payload, payloadSize, sizeof(UMP32), payloadPosition);

        midiMessagesReceived++;
        if (midiMessagesReceived == expectedMessageCount)
        {
            allMessagesReceived.SetEvent();
        }
    };

    VERIFY_SUCCEEDED(allMessagesReceived.create());

    VERIFY_SUCCEEDED(midiDeviceEnum.EnumerateDevices());

    UINT numMidiIn = midiDeviceEnum.GetNumMidiDevices(MidiFlowIn);
    UINT numMidiOut = midiDeviceEnum.GetNumMidiDevices(MidiFlowOut);

    VERIFY_IS_TRUE(numMidiIn > 0);
    VERIFY_IS_TRUE(numMidiOut > 0);

    std::wstring midiInInstanceId = midiDeviceEnum.GetMidiInstanceId(0, MidiFlowIn);
    std::wstring midiOutInstanceId = midiDeviceEnum.GetMidiInstanceId(0, MidiFlowOut);

    LOG_OUTPUT(L"Initializing midi in");
    VERIFY_SUCCEEDED(midiInDevice->Initialize(midiInInstanceId.c_str(), &mmcssTaskId, this));

    LOG_OUTPUT(L"Initializing midi out");
    VERIFY_SUCCEEDED(midiOutDevice->Initialize(midiOutInstanceId.c_str(), &mmcssTaskId));

    LOG_OUTPUT(L"Writing midi data");
    VERIFY_SUCCEEDED(midiOutDevice->SendMidiMessage((void *) &g_MidiTestData, sizeof(UMP32), 0));
    VERIFY_SUCCEEDED(midiOutDevice->SendMidiMessage((void *) &g_MidiTestData, sizeof(UMP64), 0));
    VERIFY_SUCCEEDED(midiOutDevice->SendMidiMessage((void *) &g_MidiTestData, sizeof(UMP96), 0));
    VERIFY_SUCCEEDED(midiOutDevice->SendMidiMessage((void *) &g_MidiTestData, sizeof(UMP128), 0));

    // wait for up to 30 seconds for all the messages
    if(!allMessagesReceived.wait(30000))
    {
        LOG_OUTPUT(L"Failure waiting for messages, timed out.");
    }

    LOG_OUTPUT(L"%d messages expected, %d received", expectedMessageCount, midiMessagesReceived);
    VERIFY_IS_TRUE(midiMessagesReceived == expectedMessageCount);

    LOG_OUTPUT(L"Done, cleaning up");

    VERIFY_SUCCEEDED(midiOutDevice->Cleanup());
    VERIFY_SUCCEEDED(midiInDevice->Cleanup());
}

void MidiAbstractionTests::TestMidiKSAbstraction()
{
    TestMidiAbstraction(__uuidof(Midi2KSAbstraction));
}

void MidiAbstractionTests::TestMidiSrvAbstraction()
{
    TestMidiAbstraction(__uuidof(Midi2MidiSrvAbstraction));
}

_Use_decl_annotations_
void MidiAbstractionTests::TestMidiAbstractionCreationOrder(REFIID Iid)
{
//    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    MidiSWDeviceEnum midiDeviceEnum;
    wil::com_ptr_nothrow<IMidiAbstraction> midiAbstraction;
    wil::com_ptr_nothrow<IMidiIn> midiInDevice;
    wil::com_ptr_nothrow<IMidiOut> midiOutDevice;
    DWORD mmcssTaskId {0};
    DWORD mmcssTaskIdIn {0};
    DWORD mmcssTaskIdOut {0};
    unique_mmcss_handle mmcssHandle;

    VERIFY_SUCCEEDED(CoCreateInstance(Iid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiAbstraction)));

    VERIFY_SUCCEEDED(midiAbstraction->Activate(__uuidof(IMidiIn), (void **) &midiInDevice));
    VERIFY_SUCCEEDED(midiAbstraction->Activate(__uuidof(IMidiOut), (void **) &midiOutDevice));

    m_MidiInCallback = [&](PVOID, UINT32, LONGLONG)
    {
    };

    // enable mmcss for our test thread, to ensure that components only
    // manage mmcss for their own workers.
    VERIFY_SUCCEEDED(EnableMmcss(mmcssHandle, mmcssTaskId));

    VERIFY_SUCCEEDED(midiDeviceEnum.EnumerateDevices());

    UINT numMidiIn = midiDeviceEnum.GetNumMidiDevices(MidiFlowIn);
    UINT numMidiOut = midiDeviceEnum.GetNumMidiDevices(MidiFlowOut);

    VERIFY_IS_TRUE(numMidiIn > 0);
    VERIFY_IS_TRUE(numMidiOut > 0);

    std::wstring midiInInstanceId = midiDeviceEnum.GetMidiInstanceId(0, MidiFlowIn);
    std::wstring midiOutInstanceId = midiDeviceEnum.GetMidiInstanceId(0, MidiFlowOut);

    // initialize midi out and then midi in, reset the task id,
    // and then initialize midi in then out to ensure that order
    // doesn't matter.
    LOG_OUTPUT(L"Initializing midi out");
    mmcssTaskIdOut = mmcssTaskId;
    VERIFY_SUCCEEDED(midiOutDevice->Initialize(midiOutInstanceId.c_str(), &mmcssTaskIdOut));
    mmcssTaskIdIn = mmcssTaskIdOut;
    LOG_OUTPUT(L"Initializing midi in");
    VERIFY_SUCCEEDED(midiInDevice->Initialize(midiInInstanceId.c_str(), &mmcssTaskIdIn, this));
    VERIFY_IS_TRUE(mmcssTaskId == mmcssTaskIdOut);
    VERIFY_IS_TRUE(mmcssTaskIdIn == mmcssTaskIdOut);
    VERIFY_SUCCEEDED(midiOutDevice->Cleanup());
    VERIFY_SUCCEEDED(midiInDevice->Cleanup());
    LOG_OUTPUT(L"Zeroing task id");
    mmcssTaskIdIn = 0;
    mmcssTaskIdOut = 0;
    LOG_OUTPUT(L"Initializing midi in");
    mmcssTaskIdIn = mmcssTaskId;
    VERIFY_SUCCEEDED(midiInDevice->Initialize(midiInInstanceId.c_str(), &mmcssTaskIdIn, this));
    mmcssTaskIdOut = mmcssTaskIdIn;
    LOG_OUTPUT(L"Initializing midi out");
    VERIFY_SUCCEEDED(midiOutDevice->Initialize(midiOutInstanceId.c_str(), &mmcssTaskIdOut));
    VERIFY_IS_TRUE(mmcssTaskId == mmcssTaskIdOut);
    VERIFY_IS_TRUE(mmcssTaskIdIn == mmcssTaskIdOut);
    VERIFY_SUCCEEDED(midiOutDevice->Cleanup());
    VERIFY_SUCCEEDED(midiInDevice->Cleanup());

    mmcssHandle.reset();
    VERIFY_SUCCEEDED(EnableMmcss(mmcssHandle, mmcssTaskId));

    // reusing the same task id, which was cleaned up,
    // initialize again, but revers the cleanup order, which shouldn't matter,
    // then repeat again in the opposite order with reversed cleanup order.
    LOG_OUTPUT(L"Initializing midi out");
    mmcssTaskIdOut = mmcssTaskId;
    VERIFY_SUCCEEDED(midiOutDevice->Initialize(midiOutInstanceId.c_str(), &mmcssTaskIdOut));
    mmcssTaskIdIn = mmcssTaskIdOut;
    LOG_OUTPUT(L"Initializing midi in");
    VERIFY_SUCCEEDED(midiInDevice->Initialize(midiInInstanceId.c_str(), &mmcssTaskIdIn, this));
    VERIFY_IS_TRUE(mmcssTaskId == mmcssTaskIdOut);
    VERIFY_IS_TRUE(mmcssTaskIdIn == mmcssTaskIdOut);
    VERIFY_SUCCEEDED(midiInDevice->Cleanup());
    VERIFY_SUCCEEDED(midiOutDevice->Cleanup());
    LOG_OUTPUT(L"Initializing midi in");
    mmcssTaskIdIn = mmcssTaskId;
    VERIFY_SUCCEEDED(midiInDevice->Initialize(midiInInstanceId.c_str(), &mmcssTaskIdIn, this));
    mmcssTaskIdOut = mmcssTaskIdIn;
    LOG_OUTPUT(L"Initializing midi out");
    VERIFY_SUCCEEDED(midiOutDevice->Initialize(midiOutInstanceId.c_str(), &mmcssTaskIdOut));
    VERIFY_IS_TRUE(mmcssTaskId == mmcssTaskIdOut);
    VERIFY_IS_TRUE(mmcssTaskIdIn == mmcssTaskIdOut);
    VERIFY_SUCCEEDED(midiInDevice->Cleanup());
    VERIFY_SUCCEEDED(midiOutDevice->Cleanup());
}

void MidiAbstractionTests::TestMidiKSAbstractionCreationOrder()
{
    TestMidiAbstractionCreationOrder(__uuidof(Midi2KSAbstraction));
}

void MidiAbstractionTests::TestMidiSrvAbstractionCreationOrder()
{
    TestMidiAbstractionCreationOrder(__uuidof(Midi2MidiSrvAbstraction));
}

_Use_decl_annotations_
void MidiAbstractionTests::TestMidiAbstractionBiDi(REFIID Iid)
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    MidiSWDeviceEnum midiDeviceEnum;
    wil::com_ptr_nothrow<IMidiAbstraction> midiAbstraction;
    wil::com_ptr_nothrow<IMidiBiDi> midiBiDiDevice;
    DWORD mmcssTaskId {0};
    wil::unique_event_nothrow allMessagesReceived;
    UINT32 expectedMessageCount = 4;
    UINT midiMessagesReceived = 0;

    VERIFY_SUCCEEDED(CoCreateInstance(Iid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiAbstraction)));
    VERIFY_SUCCEEDED(midiAbstraction->Activate(__uuidof(IMidiBiDi), (void **) &midiBiDiDevice));

    m_MidiInCallback = [&](PVOID payload, UINT32 payloadSize, LONGLONG payloadPosition)
    {
        PrintMidiMessage(payload, payloadSize, sizeof(UMP32), payloadPosition);

        midiMessagesReceived++;
        if (midiMessagesReceived == expectedMessageCount)
        {
            allMessagesReceived.SetEvent();
        }
    };

    VERIFY_SUCCEEDED(allMessagesReceived.create());

    VERIFY_SUCCEEDED(midiDeviceEnum.EnumerateDevices());

    UINT numMidiBiDirectional = midiDeviceEnum.GetNumMidiDevices(MidiFlowBidirectional);
    
    VERIFY_IS_TRUE(numMidiBiDirectional > 0);
    
    std::wstring midiBiDirectionalInstanceId = midiDeviceEnum.GetMidiInstanceId(0, MidiFlowBidirectional);
    
    LOG_OUTPUT(L"Initializing midi BiDi");
    VERIFY_SUCCEEDED(midiBiDiDevice->Initialize(midiBiDirectionalInstanceId.c_str(), &mmcssTaskId, this));

    LOG_OUTPUT(L"Writing midi data");
    VERIFY_SUCCEEDED(midiBiDiDevice->SendMidiMessage((void *) &g_MidiTestData, sizeof(UMP32), 0));
    VERIFY_SUCCEEDED(midiBiDiDevice->SendMidiMessage((void *) &g_MidiTestData, sizeof(UMP64), 0));
    VERIFY_SUCCEEDED(midiBiDiDevice->SendMidiMessage((void *) &g_MidiTestData, sizeof(UMP96), 0));
    VERIFY_SUCCEEDED(midiBiDiDevice->SendMidiMessage((void *) &g_MidiTestData, sizeof(UMP128), 0));

    // wait for up to 30 seconds for all the messages
    if(!allMessagesReceived.wait(30000))
    {
        LOG_OUTPUT(L"Failure waiting for messages, timed out.");
    }

    LOG_OUTPUT(L"%d messages expected, %d received", expectedMessageCount, midiMessagesReceived);
    VERIFY_IS_TRUE(midiMessagesReceived == expectedMessageCount);

    LOG_OUTPUT(L"Done, cleaning up");

    VERIFY_SUCCEEDED(midiBiDiDevice->Cleanup());
}

void MidiAbstractionTests::TestMidiKSAbstractionBiDi()
{
    TestMidiAbstractionBiDi(__uuidof(Midi2KSAbstraction));
}

void MidiAbstractionTests::TestMidiSrvAbstractionBiDi()
{
    TestMidiAbstractionBiDi(__uuidof(Midi2MidiSrvAbstraction));
}

_Use_decl_annotations_
void MidiAbstractionTests::TestMidiIO_Latency(REFIID Iid)
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    MidiSWDeviceEnum midiDeviceEnum;
    wil::com_ptr_nothrow<IMidiAbstraction> midiAbstraction;
    wil::com_ptr_nothrow<IMidiBiDi> midiBiDiDevice;
    DWORD mmcssTaskId{0};
    wil::unique_event_nothrow allMessagesReceived;
    UINT expectedMessageCount = 100000;

    LARGE_INTEGER performanceFrequency{0};
    LARGE_INTEGER firstSend{0};
    LARGE_INTEGER lastReceive{0};

    LONGLONG firstRoundTripLatency{0};
    LONGLONG lastRoundTripLatency{0};
    LONGLONG avgRoundTripLatency{0};
    LONGLONG minRoundTripLatency{0xffffffff};
    LONGLONG maxRoundTripLatency{0};

    LONGLONG avgReceiveLatency{0};
    LONGLONG minReceiveLatency{0xffffffff};
    LONGLONG maxReceiveLatency{0};

    LONGLONG previousReceive{0};

    double qpcPerMs = 0;

    QueryPerformanceFrequency(&performanceFrequency);

    qpcPerMs = (performanceFrequency.QuadPart / 1000.);

    UINT midiMessagesReceived = 0;

    VERIFY_SUCCEEDED(CoCreateInstance(Iid, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiAbstraction)));
    VERIFY_SUCCEEDED(midiAbstraction->Activate(__uuidof(IMidiBiDi), (void**)&midiBiDiDevice));

    m_MidiInCallback = [&](PVOID , UINT32 , LONGLONG payloadPosition)
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

            // running average for the average recieve latency/jitter
            avgReceiveLatency = (avgReceiveLatency + ((receiveLatency - avgReceiveLatency) / (midiMessagesReceived)));
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

        // running average for the round trip latency
        avgRoundTripLatency = (avgRoundTripLatency + ((roundTripLatency - avgRoundTripLatency) / midiMessagesReceived));

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

    VERIFY_SUCCEEDED(midiDeviceEnum.EnumerateDevices());

    UINT numMidiBiDirectional = midiDeviceEnum.GetNumMidiDevices(MidiFlowBidirectional);

    VERIFY_IS_TRUE(numMidiBiDirectional > 0);

    std::wstring midiBiDirectionalInstanceId = midiDeviceEnum.GetMidiInstanceId(0, MidiFlowBidirectional);

    LOG_OUTPUT(L"Initializing midi BiDi");
    VERIFY_SUCCEEDED(midiBiDiDevice->Initialize(midiBiDirectionalInstanceId.c_str(), &mmcssTaskId, this));

    LOG_OUTPUT(L"Writing midi data");

    LONGLONG firstSendLatency{0};
    LONGLONG lastSendLatency{0};
    LONGLONG avgSendLatency{0};
    LONGLONG minSendLatency{0xffffffff};
    LONGLONG maxSendLatency{0};

    // stabilize the system before we start testing.
    Sleep(1000);

    for (UINT i = 0; i < expectedMessageCount; i++)
    {
        LARGE_INTEGER qpcBefore{0};
        LARGE_INTEGER qpcAfter{0};
        LONGLONG sendLatency{0};

        QueryPerformanceCounter(&qpcBefore);
        VERIFY_SUCCEEDED(midiBiDiDevice->SendMidiMessage((void*)&g_MidiTestData, sizeof(UMP128), qpcBefore.QuadPart));
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

        avgSendLatency = (avgSendLatency + ((sendLatency - avgSendLatency) / (((LONGLONG)(i)) + 1)));

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

    // wait for up to 30 seconds for all the messages
    if (!allMessagesReceived.wait(30000))
    {
        LOG_OUTPUT(L"Failure waiting for messages, timed out.");
    }

    // wait to see if any additional messages come in (there shouldn't be any)
    Sleep(500);

    LOG_OUTPUT(L"%d messages expected, %d received", expectedMessageCount, midiMessagesReceived);
    VERIFY_IS_TRUE(midiMessagesReceived == expectedMessageCount);

    double messagesPerSecond{0};
    double elapsedMs{0};

    elapsedMs = (lastReceive.QuadPart - firstSend.QuadPart) / qpcPerMs;
    messagesPerSecond = expectedMessageCount / (elapsedMs / 1000.);

    LOG_OUTPUT(L"****************************************************************************");
    LOG_OUTPUT(L"Elapsed time from start of send to final receive %g ms", elapsedMs);
    LOG_OUTPUT(L"Messages per second %.9g", messagesPerSecond);
    LOG_OUTPUT(L" ");
    LOG_OUTPUT(L"RoundTrip latencies");
    LOG_OUTPUT(L"Average round trip latency %g ms", avgRoundTripLatency / qpcPerMs);
    LOG_OUTPUT(L"First message round trip latency %g ms", firstRoundTripLatency / qpcPerMs);
    LOG_OUTPUT(L"Last message round trip latency %g ms", lastRoundTripLatency / qpcPerMs);
    LOG_OUTPUT(L"Smallest round trip latency %g ms", minRoundTripLatency / qpcPerMs);
    LOG_OUTPUT(L"Largest round trip latency %g ms", maxRoundTripLatency / qpcPerMs);
    LOG_OUTPUT(L" ");
    LOG_OUTPUT(L"Message send jitter");
    LOG_OUTPUT(L"Average message send %g ms", avgSendLatency / qpcPerMs);
    LOG_OUTPUT(L"First message send %g ms", firstSendLatency / qpcPerMs);
    LOG_OUTPUT(L"Last message send %g ms", lastSendLatency / qpcPerMs);
    LOG_OUTPUT(L"Smallest message send %g ms", minSendLatency / qpcPerMs);
    LOG_OUTPUT(L"Largest message send %g ms", maxSendLatency / qpcPerMs);
    LOG_OUTPUT(L" ");
    LOG_OUTPUT(L"Message receive jitter");
    LOG_OUTPUT(L"Average message receive %g ms", avgReceiveLatency / qpcPerMs);
    LOG_OUTPUT(L"Largest message receive %g ms", maxReceiveLatency / qpcPerMs);
    LOG_OUTPUT(L"Smallest message receive %g ms", minReceiveLatency / qpcPerMs);
    LOG_OUTPUT(L" ");
    LOG_OUTPUT(L"****************************************************************************");

    LOG_OUTPUT(L"Done, cleaning up");

    LOG_OUTPUT(L"Done, cleaning up");

    VERIFY_SUCCEEDED(midiBiDiDevice->Cleanup());
}

void MidiAbstractionTests::TestMidiKSIO_Latency()
{
    TestMidiIO_Latency(__uuidof(Midi2KSAbstraction));
}

void MidiAbstractionTests::TestMidiSrvIO_Latency()
{
    TestMidiIO_Latency(__uuidof(Midi2MidiSrvAbstraction));
}

bool MidiAbstractionTests::TestSetup()
{
    m_MidiInCallback = nullptr;
    return true;
}

bool MidiAbstractionTests::TestCleanup()
{
    return true;
}

bool MidiAbstractionTests::ClassSetup()
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    VERIFY_SUCCEEDED(Windows::Foundation::Initialize(RO_INIT_MULTITHREADED));
    
    return true;
}

bool MidiAbstractionTests::ClassCleanup()
{
    Windows::Foundation::Uninitialize();
    return true;
}

