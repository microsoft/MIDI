// Copyright (c) Microsoft Corporation. All rights reserved.
#include "stdafx.h"

#include "MidiAbstraction_i.c"
#include "MidiAbstraction.h"

#include "Midi2DriverTests.h"

#include "MidiDefs.h"
#include "MidiKsDef.h"
#include "MidiKsCommon.h"
#include "MidiKsEnum.h"
#include "MidiXProc.h"
#include "MidiKs.h"

using namespace WEX::Logging;

void Midi2DriverTests::TestMidiIO(BOOL Cyclic)
{
//    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    KSMidiDeviceEnum midiDeviceEnum;
    KSMidiOutDevice midiOutDevice;
    KSMidiInDevice midiInDevice;
    DWORD mmcssTaskId {0};
    wil::unique_event_nothrow allMessagesReceived;
    UINT32 expectedMessageCount = 4;

    LARGE_INTEGER position {0};

    UINT midiMessagesReceived = 0;

    m_MidiInCallback = [&](PVOID payload, UINT32 payloadSize, LONGLONG payloadPosition)
    {
        midiMessagesReceived++;

        PrintMidiMessage(payload, payloadSize, sizeof(UMP32), payloadPosition);

        if (midiMessagesReceived == expectedMessageCount)
        {
            allMessagesReceived.SetEvent();
        }
    };

    VERIFY_SUCCEEDED(allMessagesReceived.create());

    VERIFY_SUCCEEDED(midiDeviceEnum.EnumerateFilters());

    VERIFY_IS_TRUE(midiDeviceEnum.m_AvailableMidiInPinCount > 0);
    VERIFY_IS_TRUE(midiDeviceEnum.m_AvailableMidiOutPinCount > 0);

    if (Cyclic)
    {
        if (!midiDeviceEnum.m_AvailableMidiOutPins[0].UMP || 
            !midiDeviceEnum.m_AvailableMidiInPins[0].UMP)
        {
            Log::Result(WEX::Logging::TestResults::Skipped, L"Skipping test: Midi 1 device doesn't support cyclic.");
            return;
        }
    }

    LOG_OUTPUT(L"Initializing midi in");
    VERIFY_SUCCEEDED(midiInDevice.Initialize(midiDeviceEnum.m_AvailableMidiInPins[0].FilterName.get(), NULL, midiDeviceEnum.m_AvailableMidiInPins[0].PinId, Cyclic, midiDeviceEnum.m_AvailableMidiInPins[0].UMP, PAGE_SIZE, &mmcssTaskId, this));

    LOG_OUTPUT(L"Initializing midi out");
    VERIFY_SUCCEEDED(midiOutDevice.Initialize(midiDeviceEnum.m_AvailableMidiOutPins[0].FilterName.get(), NULL, midiDeviceEnum.m_AvailableMidiOutPins[0].PinId, Cyclic, midiDeviceEnum.m_AvailableMidiOutPins[0].UMP, PAGE_SIZE, &mmcssTaskId));

    LOG_OUTPUT(L"Writing midi data");

    QueryPerformanceCounter(&position);
    VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestData, sizeof(UMP32), position.QuadPart));
    QueryPerformanceCounter(&position);
    VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestData, sizeof(UMP64), position.QuadPart));
    QueryPerformanceCounter(&position);
    VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestData, sizeof(UMP96), position.QuadPart));
    QueryPerformanceCounter(&position);
    VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestData, sizeof(UMP128), position.QuadPart));

    // wait for up to 30 seconds for all the messages
    if(!allMessagesReceived.wait(30000))
    {
        LOG_OUTPUT(L"Failure waiting for messages, timed out.");
    }

    // wait to see if any additional messages come in (there shouldn't be any)
    Sleep(500);

    LOG_OUTPUT(L"%d messages expected, %d received", expectedMessageCount, midiMessagesReceived);
    VERIFY_IS_TRUE(midiMessagesReceived == expectedMessageCount);

    LOG_OUTPUT(L"Done, cleaning up");

    VERIFY_SUCCEEDED(midiOutDevice.Cleanup());
    VERIFY_SUCCEEDED(midiInDevice.Cleanup());
}

void Midi2DriverTests::TestCyclicMidiIO()
{
    TestMidiIO(TRUE);
}

void Midi2DriverTests::TestStandardMidiIO()
{
    TestMidiIO(FALSE);
}

void Midi2DriverTests::TestMidiIO_ManyMessages(BOOL Cyclic)
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    KSMidiDeviceEnum midiDeviceEnum;
    KSMidiOutDevice midiOutDevice;
    KSMidiInDevice midiInDevice;
    DWORD mmcssTaskId {0};
    wil::unique_event_nothrow allMessagesReceived;
    UINT expectedMessageCount = 100000;

    LARGE_INTEGER position {0};
    LARGE_INTEGER previousPosition {0};

    // start position
    QueryPerformanceCounter(&previousPosition);

    UINT midiMessagesReceived = 0;
    m_MidiInCallback = [&](PVOID payload, UINT32 payloadSize, LONGLONG payloadPosition)
    {
        midiMessagesReceived++;

        if (0 != memcmp(payload, &g_MidiTestData, min(payloadSize, sizeof(g_MidiTestData))))
        {
            PrintMidiMessage(payload, payloadSize, sizeof(UMP32), payloadPosition);
        }

        if (midiMessagesReceived == expectedMessageCount)
        {
            allMessagesReceived.SetEvent();
        }
    };

    VERIFY_SUCCEEDED(allMessagesReceived.create());

    VERIFY_SUCCEEDED(midiDeviceEnum.EnumerateFilters());

    VERIFY_IS_TRUE(midiDeviceEnum.m_AvailableMidiInPinCount > 0);
    VERIFY_IS_TRUE(midiDeviceEnum.m_AvailableMidiOutPinCount > 0);

    if (Cyclic)
    {
        if (!midiDeviceEnum.m_AvailableMidiOutPins[0].UMP || 
            !midiDeviceEnum.m_AvailableMidiInPins[0].UMP)
        {
            Log::Result(WEX::Logging::TestResults::Skipped, L"Skipping test: Midi 1 device doesn't support cyclic.");
            return;
        }
    }

    LOG_OUTPUT(L"Initializing midi in");
    VERIFY_SUCCEEDED(midiInDevice.Initialize(midiDeviceEnum.m_AvailableMidiInPins[0].FilterName.get(), NULL, midiDeviceEnum.m_AvailableMidiInPins[0].PinId, Cyclic, midiDeviceEnum.m_AvailableMidiInPins[0].UMP, PAGE_SIZE, &mmcssTaskId, this));

    LOG_OUTPUT(L"Initializing midi out");
    VERIFY_SUCCEEDED(midiOutDevice.Initialize(midiDeviceEnum.m_AvailableMidiOutPins[0].FilterName.get(), NULL, midiDeviceEnum.m_AvailableMidiOutPins[0].PinId, Cyclic, midiDeviceEnum.m_AvailableMidiOutPins[0].UMP, PAGE_SIZE, &mmcssTaskId));

    LOG_OUTPUT(L"Writing midi data");

    for (UINT i = 0; i < expectedMessageCount; i++)
    {
        QueryPerformanceCounter(&position);
        VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestData, sizeof(UMP128), position.QuadPart));
    }

    // wait for up to 30 seconds for all the messages
    if(!allMessagesReceived.wait(30000))
    {
        LOG_OUTPUT(L"Failure waiting for messages, timed out.");
    }

    // wait to see if any additional messages come in (there shouldn't be any)
    Sleep(500);

    LOG_OUTPUT(L"%d messages expected, %d received", expectedMessageCount, midiMessagesReceived);
    VERIFY_IS_TRUE(midiMessagesReceived == expectedMessageCount);

    LOG_OUTPUT(L"Done, cleaning up");

    VERIFY_SUCCEEDED(midiOutDevice.Cleanup());
    VERIFY_SUCCEEDED(midiInDevice.Cleanup());
}

void Midi2DriverTests::TestCyclicMidiIO_ManyMessages()
{
    TestMidiIO_ManyMessages(TRUE);
}

void Midi2DriverTests::TestStandardMidiIO_ManyMessages()
{
    TestMidiIO_ManyMessages(FALSE);
}

void Midi2DriverTests::TestMidiIO_Latency(BOOL Cyclic, BOOL DelayedMessages)
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    DWORD messageDelay {10};

    KSMidiDeviceEnum midiDeviceEnum;
    KSMidiOutDevice midiOutDevice;
    KSMidiInDevice midiInDevice;
    DWORD mmcssTaskId {0};
    wil::unique_event_nothrow allMessagesReceived;
    UINT expectedMessageCount = DelayedMessages?(30000/messageDelay):100000;

    LARGE_INTEGER performanceFrequency {0};
    LARGE_INTEGER firstSend {0};
    LARGE_INTEGER lastReceive {0};

    LONGLONG firstRoundTripLatency {0};
    LONGLONG lastRoundTripLatency {0};
    LONGLONG avgRoundTripLatency {0};
    LONGLONG minRoundTripLatency {0xffffffff};
    LONGLONG maxRoundTripLatency {0};

    LONGLONG avgReceiveLatency {0};
    LONGLONG minReceiveLatency {0xffffffff};
    LONGLONG maxReceiveLatency {0};

    LONGLONG previousReceive {0};

    double qpcPerMs = 0;

    QueryPerformanceFrequency(&performanceFrequency);

    qpcPerMs = (performanceFrequency.QuadPart / 1000.);

    UINT midiMessagesReceived = 0;
    m_MidiInCallback = [&](PVOID, UINT32, LONGLONG payloadPosition)
    {
        LARGE_INTEGER qpc {0};
        LONGLONG roundTripLatency {0};

        QueryPerformanceCounter(&qpc);

        // first, we calculate the jitter statistics for how often the
        // recieve function was called. Since the messages are sent at a
        // fixed cadence, they should also be received at a similar cadence.
        // We can only calculate this for the 2nd and on message, since we don't
        // have a previous recieve time for the first message.
        if (previousReceive > 0)
        {
            LONGLONG receiveLatency {0};

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
            avgReceiveLatency = (avgReceiveLatency + ((receiveLatency - avgReceiveLatency)/(midiMessagesReceived)));
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
        avgRoundTripLatency = (avgRoundTripLatency + ((roundTripLatency - avgRoundTripLatency)/midiMessagesReceived));

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

    VERIFY_SUCCEEDED(midiDeviceEnum.EnumerateFilters());

    VERIFY_IS_TRUE(midiDeviceEnum.m_AvailableMidiInPinCount > 0);
    VERIFY_IS_TRUE(midiDeviceEnum.m_AvailableMidiOutPinCount > 0);

    if (Cyclic)
    {
        if (!midiDeviceEnum.m_AvailableMidiOutPins[0].UMP || 
            !midiDeviceEnum.m_AvailableMidiInPins[0].UMP)
        {
            Log::Result(WEX::Logging::TestResults::Skipped, L"Skipping test: Midi 1 device doesn't support cyclic.");
            return;
        }
    }

    LOG_OUTPUT(L"Initializing midi in");
    VERIFY_SUCCEEDED(midiInDevice.Initialize(midiDeviceEnum.m_AvailableMidiInPins[0].FilterName.get(), NULL, midiDeviceEnum.m_AvailableMidiInPins[0].PinId, Cyclic, midiDeviceEnum.m_AvailableMidiInPins[0].UMP, PAGE_SIZE, &mmcssTaskId, this));

    LOG_OUTPUT(L"Initializing midi out");
    VERIFY_SUCCEEDED(midiOutDevice.Initialize(midiDeviceEnum.m_AvailableMidiOutPins[0].FilterName.get(), NULL, midiDeviceEnum.m_AvailableMidiOutPins[0].PinId, Cyclic, midiDeviceEnum.m_AvailableMidiOutPins[0].UMP, PAGE_SIZE, &mmcssTaskId));

    LOG_OUTPUT(L"Writing midi data");

    LONGLONG firstSendLatency {0};
    LONGLONG lastSendLatency {0};
    LONGLONG avgSendLatency {0};
    LONGLONG minSendLatency {0xffffffff};
    LONGLONG maxSendLatency {0};

    // stabilize the system before we start testing.
    Sleep(1000);

    for (UINT i = 0; i < expectedMessageCount; i++)
    {
        if (DelayedMessages)
        {
            Sleep(messageDelay);
        }

        LARGE_INTEGER qpcBefore {0};
        LARGE_INTEGER qpcAfter {0};
        LONGLONG sendLatency {0};

        QueryPerformanceCounter(&qpcBefore);
        VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestData, sizeof(UMP128), qpcBefore.QuadPart));
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

        avgSendLatency = (avgSendLatency + ((sendLatency - avgSendLatency)/(((LONGLONG)i)+1)));

        if (0 == i)
        {
            firstSend = qpcBefore;
            firstSendLatency = sendLatency;
        }
        else if ((expectedMessageCount -1) == i)
        {
            lastSendLatency = sendLatency;
        }
    }

    // wait for up to 30 seconds for all the messages
    if(!allMessagesReceived.wait(30000))
    {
        LOG_OUTPUT(L"Failure waiting for messages, timed out.");
    }

    // wait to see if any additional messages come in (there shouldn't be any)
    Sleep(500);

    LOG_OUTPUT(L"%d messages expected, %d received", expectedMessageCount, midiMessagesReceived);
    VERIFY_IS_TRUE(midiMessagesReceived == expectedMessageCount);

    double messagesPerSecond {0};
    double elapsedMs {0};

    elapsedMs = (lastReceive.QuadPart - firstSend.QuadPart) / qpcPerMs;
    messagesPerSecond = expectedMessageCount / (elapsedMs / 1000.);

    LOG_OUTPUT(L"****************************************************************************");
    LOG_OUTPUT(L"Elapsed time from start of send to final receive %g ms", elapsedMs);
    LOG_OUTPUT(L"Messages per second %.9g", messagesPerSecond);
    LOG_OUTPUT(L" ");
    LOG_OUTPUT(L"RoundTrip latencies");
    LOG_OUTPUT(L"Average round trip latency %g ms", avgRoundTripLatency / qpcPerMs);
    LOG_OUTPUT(L"First message round trip latency %g ms", firstRoundTripLatency / qpcPerMs );
    LOG_OUTPUT(L"Last message round trip latency %g ms", lastRoundTripLatency / qpcPerMs );
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

    VERIFY_SUCCEEDED(midiOutDevice.Cleanup());
    VERIFY_SUCCEEDED(midiInDevice.Cleanup());
}

void Midi2DriverTests::TestCyclicMidiIO_Latency()
{
    TestMidiIO_Latency(TRUE, FALSE);
}

void Midi2DriverTests::TestStandardMidiIO_Latency()
{
    TestMidiIO_Latency(FALSE, FALSE);
}

void Midi2DriverTests::TestCyclicMidiIOSlowMessages_Latency()
{
    TestMidiIO_Latency(TRUE, TRUE);
}

void Midi2DriverTests::TestStandardMidiIOSlowMessages_Latency()
{
    TestMidiIO_Latency(FALSE, TRUE);
}

bool Midi2DriverTests::TestSetup()
{
    m_MidiInCallback = nullptr;
    return true;
}

bool Midi2DriverTests::TestCleanup()
{
    return true;
}

bool Midi2DriverTests::ClassSetup()
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    
    return true;
}

bool Midi2DriverTests::ClassCleanup()
{
    return true;
}

