// Copyright (c) Microsoft Corporation. All rights reserved.
#include "stdafx.h"
#include "Midi2KSabstraction.h"
#include "Midi2MidiSrvAbstraction.h"

#include "Midi2AbstractionTests.h"

#include <Devpkey.h>
#include "MidiKsCommon.h"
#include "MidiSwEnum.h"
#include <initguid.h>
#include "MidiDefs.h"
#include "MidiXProc.h"
#include <mmdeviceapi.h>

_Use_decl_annotations_
void MidiAbstractionTests::TestMidiAbstraction(REFIID Iid, BOOL IncludeMidiOne)
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

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

    std::vector<std::unique_ptr<MIDIU_DEVICE>> midiInDevices;
    VERIFY_SUCCEEDED(MidiSWDeviceEnum::EnumerateDevices(midiInDevices, [&](PMIDIU_DEVICE device)
    {
        if (device->AbstractionLayer == (GUID) __uuidof(Midi2KSAbstraction) &&
            device->Flow == MidiFlowIn &&
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"MinMidi") &&
            (IncludeMidiOne || !device->MidiOne))
        {
            return true;
        }
        else
        {
            return false;
        }
    }));

    std::vector<std::unique_ptr<MIDIU_DEVICE>> midiOutDevices;
    VERIFY_SUCCEEDED(MidiSWDeviceEnum::EnumerateDevices(midiOutDevices, [&](PMIDIU_DEVICE device)
    {
        if (device->AbstractionLayer == (GUID) __uuidof(Midi2KSAbstraction) &&
            device->Flow == MidiFlowOut &&
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"MinMidi") &&
            (IncludeMidiOne || !device->MidiOne))
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
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"Test requires at least 1 midi in and 1 midi out endpoint.");
        return;
    }

    std::wstring midiInInstanceId = midiInDevices[0]->DeviceId;
    std::wstring midiOutInstanceId = midiOutDevices[0]->DeviceId;

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
    TestMidiAbstraction(__uuidof(Midi2KSAbstraction), FALSE);
}

void MidiAbstractionTests::TestMidiSrvAbstraction()
{
    TestMidiAbstraction(__uuidof(Midi2MidiSrvAbstraction), TRUE);
}

_Use_decl_annotations_
void MidiAbstractionTests::TestMidiAbstractionCreationOrder(REFIID Iid, BOOL IncludeMidiOne)
{
//    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

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

    std::vector<std::unique_ptr<MIDIU_DEVICE>> midiInDevices;
    VERIFY_SUCCEEDED(MidiSWDeviceEnum::EnumerateDevices(midiInDevices, [&](PMIDIU_DEVICE device)
    {
        if (device->AbstractionLayer == (GUID) __uuidof(Midi2KSAbstraction) &&
            device->Flow == MidiFlowIn &&
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"MinMidi") &&
            (IncludeMidiOne || !device->MidiOne))
        {
            return true;
        }
        else
        {
            return false;
        }
    }));

    std::vector<std::unique_ptr<MIDIU_DEVICE>> midiOutDevices;
    VERIFY_SUCCEEDED(MidiSWDeviceEnum::EnumerateDevices(midiOutDevices, [&](PMIDIU_DEVICE device)
    {
        if (device->AbstractionLayer == (GUID) __uuidof(Midi2KSAbstraction) &&
            device->Flow == MidiFlowOut &&
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"MinMidi") &&
            (IncludeMidiOne || !device->MidiOne))
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
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"Test requires at least 1 midi in and 1 midi out endpoint.");
        return;
    }

    std::wstring midiInInstanceId = midiInDevices[0]->DeviceId;
    std::wstring midiOutInstanceId = midiOutDevices[0]->DeviceId;

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
    TestMidiAbstractionCreationOrder(__uuidof(Midi2KSAbstraction), FALSE);
}

void MidiAbstractionTests::TestMidiSrvAbstractionCreationOrder()
{
    TestMidiAbstractionCreationOrder(__uuidof(Midi2MidiSrvAbstraction), TRUE);
}

_Use_decl_annotations_
void MidiAbstractionTests::TestMidiAbstractionBiDi(REFIID Iid)
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

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

    std::vector<std::unique_ptr<MIDIU_DEVICE>> midiBiDiDevices;
    VERIFY_SUCCEEDED(MidiSWDeviceEnum::EnumerateDevices(midiBiDiDevices, [&](PMIDIU_DEVICE device)
    {
        if (device->AbstractionLayer == (GUID) __uuidof(Midi2KSAbstraction) &&
            device->Flow == MidiFlowBidirectional &&
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"MinMidi") &&
            !device->MidiOne)
        {
            return true;
        }
        else
        {
            return false;
        }
    }));

    if (midiBiDiDevices.size() == 0)
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"Test requires at least 1 midi bidi endpoint.");
        return;
    }
    
    std::wstring midiBiDirectionalInstanceId = midiBiDiDevices[0]->DeviceId;
    
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
void MidiAbstractionTests::TestMidiIO_Latency(REFIID Iid, BOOL DelayedMessages)
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    DWORD messageDelay{ 10 };

    wil::com_ptr_nothrow<IMidiAbstraction> midiAbstraction;
    wil::com_ptr_nothrow<IMidiBiDi> midiBiDiDevice;
    DWORD mmcssTaskId{0};
    wil::unique_event_nothrow allMessagesReceived;
    UINT expectedMessageCount = DelayedMessages ? (3000 / messageDelay) : 100000;

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

    std::vector<std::unique_ptr<MIDIU_DEVICE>> midiBiDiDevices;
    VERIFY_SUCCEEDED(MidiSWDeviceEnum::EnumerateDevices(midiBiDiDevices, [&](PMIDIU_DEVICE device)
    {
        if (device->AbstractionLayer == (GUID) __uuidof(Midi2KSAbstraction) &&
            device->Flow == MidiFlowBidirectional &&
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"MinMidi") &&
            !device->MidiOne)
        {
            return true;
        }
        else
        {
            return false;
        }
    }));

    if (midiBiDiDevices.size() == 0)
    {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"Test requires at least 1 midi bidi endpoint.");
        return;
    }

    std::wstring midiBiDirectionalInstanceId = midiBiDiDevices[0]->DeviceId;

    LOG_OUTPUT(L"Initializing midi BiDi");
    VERIFY_SUCCEEDED(midiBiDiDevice->Initialize(midiBiDirectionalInstanceId.c_str(), &mmcssTaskId, this));

    LOG_OUTPUT(L"Writing midi data");

    LONGLONG firstSendLatency{ 0 };
    LONGLONG lastSendLatency{ 0 };
    long double avgSendLatency{ 0 };
    long double stddevSendLatency{ 0 };
    LONGLONG minSendLatency{ 0xffffffff };
    LONGLONG maxSendLatency{ 0 };

    for (UINT i = 0; i < expectedMessageCount; i++)
    {
        if (DelayedMessages)
        {
            Sleep(messageDelay);
        }

        LARGE_INTEGER qpcBefore{0};
        LARGE_INTEGER qpcAfter{0};
        LONGLONG sendLatency{0};

        QueryPerformanceCounter(&qpcBefore);
        VERIFY_SUCCEEDED(midiBiDiDevice->SendMidiMessage((void*)&g_MidiTestData, sizeof(UMP32), qpcBefore.QuadPart));
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

    // wait for up to 30 seconds for all the messages
    if (!allMessagesReceived.wait(5000))
    {
        LOG_OUTPUT(L"Failure waiting for messages, timed out.");
    }

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
    LOG_OUTPUT(L"Largest message receive %g micro seconds", minRLatency);
    LOG_OUTPUT(L"Smallest message receive %g micro seconds", maxRLatency);
    LOG_OUTPUT(L"Standard deviation %g micro seconds", stddevRLatency);
    LOG_OUTPUT(L" ");
    LOG_OUTPUT(L"****************************************************************************");

    LOG_OUTPUT(L"Done, cleaning up");

    VERIFY_SUCCEEDED(midiBiDiDevice->Cleanup());
}

void MidiAbstractionTests::TestMidiKSIO_Latency()
{
    TestMidiIO_Latency(__uuidof(Midi2KSAbstraction), FALSE);
}

void MidiAbstractionTests::TestMidiSrvIO_Latency()
{
    TestMidiIO_Latency(__uuidof(Midi2MidiSrvAbstraction), FALSE);
}

void MidiAbstractionTests::TestMidiKSIOSlowMessages_Latency()
{
    TestMidiIO_Latency(__uuidof(Midi2KSAbstraction), TRUE);
}

void MidiAbstractionTests::TestMidiSrvIOSlowMessages_Latency()
{
    TestMidiIO_Latency(__uuidof(Midi2MidiSrvAbstraction), TRUE);
}

UMP128 g_MidiTestData2 = { 0x00, 0x00, 0x5678, 0xbaadf00d, 0xdeadbeef, 0xd000000d };

void MidiAbstractionTests::TestMidiSrv_MultiClient()
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    wil::com_ptr_nothrow<IMidiAbstraction> midiAbstraction;
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
    // as there are two clients using the same callback, intentionally,
    // we need to ensure that only 1 is processed at a time.
    wil::critical_section callbackLock;

    std::vector<std::unique_ptr<MIDIU_DEVICE>> midiInDevices;
    VERIFY_SUCCEEDED(MidiSWDeviceEnum::EnumerateDevices(midiInDevices, [&](PMIDIU_DEVICE device)
    {
        if (device->AbstractionLayer == (GUID) __uuidof(Midi2KSAbstraction) &&
            device->Flow == MidiFlowIn &&
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"MinMidi"))
        {
            return true;
        }
        else
        {
            return false;
        }
    }));

    std::vector<std::unique_ptr<MIDIU_DEVICE>> midiOutDevices;
    VERIFY_SUCCEEDED(MidiSWDeviceEnum::EnumerateDevices(midiOutDevices, [&](PMIDIU_DEVICE device)
    {
        if (device->AbstractionLayer == (GUID) __uuidof(Midi2KSAbstraction) &&
            device->Flow == MidiFlowOut &&
            std::wstring::npos != device->ParentDeviceInstanceId.find(L"MinMidi"))
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
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped, L"Test requires at least 1 midi in and 1 midi out endpoint.");
        return;
    }

    std::wstring midiInInstanceId = midiInDevices[0]->DeviceId;
    std::wstring midiOutInstanceId = midiOutDevices[0]->DeviceId;

    VERIFY_SUCCEEDED(CoCreateInstance(__uuidof(Midi2MidiSrvAbstraction), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&midiAbstraction)));
    VERIFY_SUCCEEDED(midiAbstraction->Activate(__uuidof(IMidiIn), (void**)&midiInDevice1));
    VERIFY_SUCCEEDED(midiAbstraction->Activate(__uuidof(IMidiIn), (void**)&midiInDevice2));
    VERIFY_SUCCEEDED(midiAbstraction->Activate(__uuidof(IMidiOut), (void**)&midiOutDevice1));
    VERIFY_SUCCEEDED(midiAbstraction->Activate(__uuidof(IMidiOut), (void**)&midiOutDevice2));

    m_MidiInCallback = [&](PVOID payload, UINT32 payloadSize, LONGLONG payloadPosition)
    {
        auto lock = callbackLock.lock();

        PrintMidiMessage(payload, payloadSize, sizeof(UMP32), payloadPosition);

        midiMessagesReceived++;
        if (midiMessagesReceived == expectedMessageCount)
        {
            allMessagesReceived.SetEvent();
        }
    };

    VERIFY_SUCCEEDED(allMessagesReceived.create());

    LOG_OUTPUT(L"Initializing midi in 1");
    VERIFY_SUCCEEDED(midiInDevice1->Initialize(midiInInstanceId.c_str(), &mmcssTaskId, this));

    LOG_OUTPUT(L"Initializing midi in 2");
    VERIFY_SUCCEEDED(midiInDevice2->Initialize(midiInInstanceId.c_str(), &mmcssTaskId, this));

    LOG_OUTPUT(L"Initializing midi out 1");
    VERIFY_SUCCEEDED(midiOutDevice1->Initialize(midiOutInstanceId.c_str(), &mmcssTaskId));

    LOG_OUTPUT(L"Initializing midi out 2");
    VERIFY_SUCCEEDED(midiOutDevice2->Initialize(midiOutInstanceId.c_str(), &mmcssTaskId));

    LOG_OUTPUT(L"Writing midi data");
    VERIFY_SUCCEEDED(midiOutDevice1->SendMidiMessage((void*)&g_MidiTestData, sizeof(UMP32), 0));
    //Sleep(0);
    VERIFY_SUCCEEDED(midiOutDevice2->SendMidiMessage((void*)&g_MidiTestData2, sizeof(UMP32), 0));
    //Sleep(0);
    VERIFY_SUCCEEDED(midiOutDevice1->SendMidiMessage((void*)&g_MidiTestData, sizeof(UMP64), 0));
    //Sleep(0);
    VERIFY_SUCCEEDED(midiOutDevice2->SendMidiMessage((void*)&g_MidiTestData2, sizeof(UMP64), 0));
    //Sleep(0);
    VERIFY_SUCCEEDED(midiOutDevice1->SendMidiMessage((void*)&g_MidiTestData, sizeof(UMP96), 0));
    //Sleep(0);
    VERIFY_SUCCEEDED(midiOutDevice2->SendMidiMessage((void*)&g_MidiTestData2, sizeof(UMP96), 0));
    //Sleep(0);
    VERIFY_SUCCEEDED(midiOutDevice1->SendMidiMessage((void*)&g_MidiTestData, sizeof(UMP128), 0));
    //Sleep(0);
    VERIFY_SUCCEEDED(midiOutDevice2->SendMidiMessage((void*)&g_MidiTestData2, sizeof(UMP128), 0));
    //Sleep(0);

    // wait for up to 30 seconds for all the messages
    if (!allMessagesReceived.wait(30000))
    {
        LOG_OUTPUT(L"Failure waiting for messages, timed out.");
    }

    LOG_OUTPUT(L"%d messages expected, %d received", expectedMessageCount, midiMessagesReceived);
    VERIFY_IS_TRUE(midiMessagesReceived == expectedMessageCount);

    LOG_OUTPUT(L"Done, cleaning up");

    VERIFY_SUCCEEDED(midiOutDevice1->Cleanup());
    VERIFY_SUCCEEDED(midiOutDevice2->Cleanup());
    VERIFY_SUCCEEDED(midiInDevice1->Cleanup());
    VERIFY_SUCCEEDED(midiInDevice2->Cleanup());
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
    
    return true;
}

bool MidiAbstractionTests::ClassCleanup()
{
    return true;
}

