// Copyright (c) Microsoft Corporation. All rights reserved.
#include "stdafx.h"

#include "MidiAbstraction_i.c"
#include "MidiAbstraction.h"

#include <Devpkey.h>
#include "MidiDefs.h"
#include "MidiKsDef.h"
#include "MidiKsCommon.h"
#include "MidiKsEnum.h"
#include "MidiXProc.h"
#include "MidiKs.h"

#include "Midi2DriverTests.h"

using namespace WEX::Logging;

BOOL GetPins(MidiTransport Transport, KSMidiDeviceEnum & MidiDeviceEnum, UINT & OutPinIndex, UINT & InPinIndex)
{
    // Transport is required to be one of these three, any other value is invlid.
    VERIFY_IS_TRUE(Transport == MidiTransport_StandardByteStream || Transport == MidiTransport_CyclicByteStream || Transport == MidiTransport_CyclicUMP);

    // Find a pair of pins which support this transport
    for (UINT i = 0; i < MidiDeviceEnum.m_AvailableMidiInPinCount; i++)
    {
        if (0 != ((DWORD) MidiDeviceEnum.m_AvailableMidiInPins[i].TransportCapability & (DWORD) Transport))
        {
            for (UINT j = 0; j < MidiDeviceEnum.m_AvailableMidiOutPinCount; j++)
            {
                // pins need to be on the same filter for testing.
                if (0 == _wcsicmp(MidiDeviceEnum.m_AvailableMidiInPins[i].FilterName.get(), MidiDeviceEnum.m_AvailableMidiOutPins[j].FilterName.get()))
                {
                    if (0 != ((DWORD) MidiDeviceEnum.m_AvailableMidiOutPins[j].TransportCapability & (DWORD) Transport))
                    {
                        InPinIndex = i;
                        OutPinIndex = i;
                        return TRUE;                    
                    }
                }
            }
        }
    }

    Log::Result(WEX::Logging::TestResults::Skipped, L"Skipping test: Midi device doesn't support requested transport.");

    return FALSE;
}

void Midi2DriverTests::TestMidiIO(MidiTransport Transport)
{
//    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    KSMidiDeviceEnum midiDeviceEnum;
    UINT inPinIndex {0};
    UINT outPinIndex{0};

    KSMidiOutDevice midiOutDevice;
    KSMidiInDevice midiInDevice;
    DWORD mmcssTaskId {0};
    wil::unique_event_nothrow allMessagesReceived;
    UINT32 expectedMessageCount = 4;

    LARGE_INTEGER position {0};

    UINT midiMessagesReceived = 0;

    m_MidiInCallback = [&](PVOID payload, UINT32 payloadSize, LONGLONG payloadPosition, LONGLONG)
    {
        midiMessagesReceived++;

        PrintMidiMessage(payload, payloadSize, (Transport == MidiTransport_CyclicUMP)?sizeof(UMP32):sizeof(MIDI_MESSAGE), payloadPosition);

        if (midiMessagesReceived == expectedMessageCount)
        {
            allMessagesReceived.SetEvent();
        }
    };

    VERIFY_SUCCEEDED(allMessagesReceived.create());

    VERIFY_SUCCEEDED(midiDeviceEnum.EnumerateFilters());

    if (!GetPins(Transport, midiDeviceEnum, outPinIndex, inPinIndex))
    {
        return;
    }

    ULONG requestedBufferSize = PAGE_SIZE;
    VERIFY_SUCCEEDED(GetRequiredBufferSize(requestedBufferSize));

    LOG_OUTPUT(L"Initializing midi in");
    VERIFY_SUCCEEDED(midiInDevice.Initialize(midiDeviceEnum.m_AvailableMidiInPins[inPinIndex].FilterName.get(), NULL, midiDeviceEnum.m_AvailableMidiInPins[inPinIndex].PinId, Transport, requestedBufferSize, &mmcssTaskId, this, 0));

    LOG_OUTPUT(L"Initializing midi out");
    VERIFY_SUCCEEDED(midiOutDevice.Initialize(midiDeviceEnum.m_AvailableMidiOutPins[outPinIndex].FilterName.get(), NULL, midiDeviceEnum.m_AvailableMidiOutPins[outPinIndex].PinId, Transport, requestedBufferSize, &mmcssTaskId));

    LOG_OUTPUT(L"Writing midi data");

    if (Transport == MidiTransport_CyclicUMP)
    {
        QueryPerformanceCounter(&position);
        VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestData_32, sizeof(UMP32), position.QuadPart));
        QueryPerformanceCounter(&position);
        VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestData_64, sizeof(UMP64), position.QuadPart));
        QueryPerformanceCounter(&position);
        VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestData_96, sizeof(UMP96), position.QuadPart));
        QueryPerformanceCounter(&position);
        VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestData_128, sizeof(UMP128), position.QuadPart));
    }
    else
    {
        QueryPerformanceCounter(&position);
        VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestMessage, sizeof(MIDI_MESSAGE), position.QuadPart));
        QueryPerformanceCounter(&position);
        VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestMessage, sizeof(MIDI_MESSAGE), position.QuadPart));
        QueryPerformanceCounter(&position);
        VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestMessage, sizeof(MIDI_MESSAGE), position.QuadPart));
        QueryPerformanceCounter(&position);
        VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestMessage, sizeof(MIDI_MESSAGE), position.QuadPart));
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

void Midi2DriverTests::TestCyclicUMPMidiIO()
{
    TestMidiIO(MidiTransport_CyclicUMP);
}

void Midi2DriverTests::TestCyclicByteStreamMidiIO()
{
    TestMidiIO(MidiTransport_CyclicByteStream);
}

void Midi2DriverTests::TestStandardMidiIO()
{
    TestMidiIO(MidiTransport_StandardByteStream);
}

void Midi2DriverTests::TestMidiIO_ManyMessages(MidiTransport Transport)
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    KSMidiDeviceEnum midiDeviceEnum;
    UINT inPinIndex {0};
    UINT outPinIndex{0};

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
    m_MidiInCallback = [&](PVOID payload, UINT32 payloadSize, LONGLONG payloadPosition, LONGLONG)
    {
        midiMessagesReceived++;

        if (Transport == MidiTransport_CyclicUMP)
        {
            if (0 != memcmp(payload, &g_MidiTestData_128, min(payloadSize, sizeof(UMP128))))
            {
                PrintMidiMessage(payload, payloadSize, sizeof(UMP128), payloadPosition);
            }
        }
        else
        {
            if (0 != memcmp(payload, &g_MidiTestMessage, min(payloadSize, sizeof(MIDI_MESSAGE))))
            {
                PrintMidiMessage(payload, payloadSize, sizeof(MIDI_MESSAGE), payloadPosition);
            }
        }

        if (midiMessagesReceived == expectedMessageCount)
        {
            allMessagesReceived.SetEvent();
        }
    };

    VERIFY_SUCCEEDED(allMessagesReceived.create());

    VERIFY_SUCCEEDED(midiDeviceEnum.EnumerateFilters());

    if (!GetPins(Transport, midiDeviceEnum, outPinIndex, inPinIndex))
    {
        return;
    }

    ULONG requestedBufferSize = PAGE_SIZE;
    VERIFY_SUCCEEDED(GetRequiredBufferSize(requestedBufferSize));

    LOG_OUTPUT(L"Initializing midi in");
    VERIFY_SUCCEEDED(midiInDevice.Initialize(midiDeviceEnum.m_AvailableMidiInPins[inPinIndex].FilterName.get(), NULL, midiDeviceEnum.m_AvailableMidiInPins[inPinIndex].PinId, Transport, requestedBufferSize, &mmcssTaskId, this, 0));

    LOG_OUTPUT(L"Initializing midi out");
    VERIFY_SUCCEEDED(midiOutDevice.Initialize(midiDeviceEnum.m_AvailableMidiOutPins[outPinIndex].FilterName.get(), NULL, midiDeviceEnum.m_AvailableMidiOutPins[outPinIndex].PinId, Transport, requestedBufferSize, &mmcssTaskId));

    LOG_OUTPUT(L"Writing midi data");

    for (UINT i = 0; i < expectedMessageCount; i++)
    {
        QueryPerformanceCounter(&position);

        if (Transport == MidiTransport_CyclicUMP)
        {
            QueryPerformanceCounter(&position);
            VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestData_128, sizeof(UMP128), position.QuadPart));
        }
        else
        {
            QueryPerformanceCounter(&position);
            VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestMessage, sizeof(MIDI_MESSAGE), position.QuadPart));
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

    LOG_OUTPUT(L"Done, cleaning up");

    VERIFY_SUCCEEDED(midiOutDevice.Cleanup());
    VERIFY_SUCCEEDED(midiInDevice.Cleanup());
}

void Midi2DriverTests::TestCyclicUMPMidiIO_ManyMessages()
{
    TestMidiIO_ManyMessages(MidiTransport_CyclicUMP);
}

void Midi2DriverTests::TestCyclicByteStreamMidiIO_ManyMessages()
{
    TestMidiIO_ManyMessages(MidiTransport_CyclicByteStream);
}

void Midi2DriverTests::TestStandardMidiIO_ManyMessages()
{
    TestMidiIO_ManyMessages(MidiTransport_StandardByteStream);
}

void Midi2DriverTests::TestMidiIO_Latency(MidiTransport Transport, BOOL DelayedMessages)
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    DWORD messageDelay {10};

    KSMidiDeviceEnum midiDeviceEnum;
    UINT inPinIndex {0};
    UINT outPinIndex{0};

    KSMidiOutDevice midiOutDevice;
    KSMidiInDevice midiInDevice;
    DWORD mmcssTaskId {0};
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
    m_MidiInCallback = [&](PVOID, UINT32, LONGLONG payloadPosition, LONGLONG)
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

            long double prevAvgReceiveLatency = avgReceiveLatency;

            // running average for the average recieve latency/jitter
            avgReceiveLatency = (prevAvgReceiveLatency + (((long double) receiveLatency - prevAvgReceiveLatency) / ((long double)midiMessagesReceived)));

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
        avgRoundTripLatency = (prevAvgRoundTripLatency + (((long double)roundTripLatency - prevAvgRoundTripLatency) / (long double)midiMessagesReceived));

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

    VERIFY_SUCCEEDED(midiDeviceEnum.EnumerateFilters());

    if (!GetPins(Transport, midiDeviceEnum, outPinIndex, inPinIndex))
    {
        return;
    }

    ULONG requestedBufferSize = PAGE_SIZE;
    VERIFY_SUCCEEDED(GetRequiredBufferSize(requestedBufferSize));

    LOG_OUTPUT(L"Initializing midi in");
    VERIFY_SUCCEEDED(midiInDevice.Initialize(midiDeviceEnum.m_AvailableMidiInPins[inPinIndex].FilterName.get(), NULL, midiDeviceEnum.m_AvailableMidiInPins[inPinIndex].PinId, Transport, requestedBufferSize, &mmcssTaskId, this, 0));

    LOG_OUTPUT(L"Initializing midi out");
    VERIFY_SUCCEEDED(midiOutDevice.Initialize(midiDeviceEnum.m_AvailableMidiOutPins[outPinIndex].FilterName.get(), NULL, midiDeviceEnum.m_AvailableMidiOutPins[outPinIndex].PinId, Transport, requestedBufferSize, &mmcssTaskId));

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

        LARGE_INTEGER qpcBefore {0};
        LARGE_INTEGER qpcAfter {0};
        LONGLONG sendLatency {0};

        QueryPerformanceCounter(&qpcBefore);
        if (Transport == MidiTransport_CyclicUMP)
        {
            VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void*) &g_MidiTestData_32, sizeof(UMP32), qpcBefore.QuadPart));
        }
        else
        {
            VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestMessage, sizeof(MIDI_MESSAGE), qpcBefore.QuadPart));
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
        else if ((expectedMessageCount -1) == i)
        {
            lastSendLatency = sendLatency;
        }
    }

    // wait for up to 30 seconds for all the messages
    if(!allMessagesReceived.wait(5000))
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

    VERIFY_SUCCEEDED(midiOutDevice.Cleanup());
    VERIFY_SUCCEEDED(midiInDevice.Cleanup());
}

void Midi2DriverTests::TestCyclicUMPMidiIO_Latency()
{
    TestMidiIO_Latency(MidiTransport_CyclicUMP, FALSE);
}

void Midi2DriverTests::TestCyclicByteStreamMidiIO_Latency()
{
    TestMidiIO_Latency(MidiTransport_CyclicByteStream, FALSE);
}

void Midi2DriverTests::TestStandardMidiIO_Latency()
{
    TestMidiIO_Latency(MidiTransport_StandardByteStream, FALSE);
}


void Midi2DriverTests::TestCyclicUMPMidiIOSlowMessages_Latency()
{
    TestMidiIO_Latency(MidiTransport_CyclicUMP, TRUE);
}

void Midi2DriverTests::TestCyclicByteStreamMidiIOSlowMessages_Latency()
{
    TestMidiIO_Latency(MidiTransport_CyclicByteStream, TRUE);
}

void Midi2DriverTests::TestStandardMidiIOSlowMessages_Latency()
{
    TestMidiIO_Latency(MidiTransport_StandardByteStream, TRUE);
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

