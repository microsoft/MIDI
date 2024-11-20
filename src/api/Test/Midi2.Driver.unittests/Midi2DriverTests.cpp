// Copyright (c) Microsoft Corporation. All rights reserved.
#include "stdafx.h"

#include "WindowsMidiServices_i.c"
#include "WindowsMidiServices.h"

#include <devioctl.h>
#include <ks.h>
#include <ksmedia.h>
#include <avrt.h>

#include <Devpkey.h>
#include "MidiDefs.h"
#include "MidiKsDef.h"
#include "MidiKsCommon.h"
#include "MidiKsEnum.h"
#include "MidiXProc.h"
#include "MidiKs.h"

#include "Midi2DriverTests.h"

using namespace WEX::Logging;

BOOL GetPins(MidiTransport transport, KSMidiDeviceEnum & midiDeviceEnum, UINT & outPinIndex, UINT & inPinIndex)
{
    // Transport is required to be one of these three, any other value is invlid.
    VERIFY_IS_TRUE(transport == MidiTransport_StandardByteStream || transport == MidiTransport_CyclicByteStream || transport == MidiTransport_CyclicUMP);

    // Find a pair of pins which support this transport
    for (UINT i = 0; i < midiDeviceEnum.m_AvailableMidiInPinCount; i++)
    {
        if (0 != ((DWORD) midiDeviceEnum.m_AvailableMidiInPins[i].TransportCapability & (DWORD) transport))
        {
            for (UINT j = 0; j < midiDeviceEnum.m_AvailableMidiOutPinCount; j++)
            {
                // pins need to be on the same filter for testing.
                if (0 == _wcsicmp(midiDeviceEnum.m_AvailableMidiInPins[i].FilterName.get(), midiDeviceEnum.m_AvailableMidiOutPins[j].FilterName.get()))
                {
                    if (0 != ((DWORD) midiDeviceEnum.m_AvailableMidiOutPins[j].TransportCapability & (DWORD) transport))
                    {
                        inPinIndex = i;
                        outPinIndex = i;
                        return TRUE;                    
                    }
                }
            }
        }
    }

    Log::Result(WEX::Logging::TestResults::Skipped, L"Skipping test: Midi device doesn't support requested transport.");

    return FALSE;
}

void Midi2DriverTests::TestMidiIO(MidiTransport transport)
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

        PrintMidiMessage(payload, payloadSize, (transport == MidiTransport_CyclicUMP)?sizeof(UMP32):sizeof(MIDI_MESSAGE), payloadPosition);

        if (midiMessagesReceived == expectedMessageCount)
        {
            allMessagesReceived.SetEvent();
        }
    };

    VERIFY_SUCCEEDED(allMessagesReceived.create());

    VERIFY_SUCCEEDED(midiDeviceEnum.EnumerateFilters());

    if (!GetPins(transport, midiDeviceEnum, outPinIndex, inPinIndex))
    {
        return;
    }

    ULONG requestedBufferSize = PAGE_SIZE;
    VERIFY_SUCCEEDED(GetRequiredBufferSize(requestedBufferSize));

    LOG_OUTPUT(L"Initializing midi in");
    VERIFY_SUCCEEDED(midiInDevice.Initialize(midiDeviceEnum.m_AvailableMidiInPins[inPinIndex].FilterName.get(), NULL, midiDeviceEnum.m_AvailableMidiInPins[inPinIndex].PinId, transport, requestedBufferSize, &mmcssTaskId, this, 0));

    LOG_OUTPUT(L"Initializing midi out");
    VERIFY_SUCCEEDED(midiOutDevice.Initialize(midiDeviceEnum.m_AvailableMidiOutPins[outPinIndex].FilterName.get(), NULL, midiDeviceEnum.m_AvailableMidiOutPins[outPinIndex].PinId, transport, requestedBufferSize, &mmcssTaskId));

    LOG_OUTPUT(L"Writing midi data");

    if (transport == MidiTransport_CyclicUMP)
    {
        QueryPerformanceCounter(&position);
        VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestData_32, sizeof(UMP32), position.QuadPart));
        QueryPerformanceCounter(&position);
        VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestData_32, sizeof(UMP32), position.QuadPart));
        QueryPerformanceCounter(&position);
        VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestData_32, sizeof(UMP32), position.QuadPart));
        QueryPerformanceCounter(&position);
        VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestData_32, sizeof(UMP32), position.QuadPart));
/*
        QueryPerformanceCounter(&position);
        VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestData_64, sizeof(UMP64), position.QuadPart));
        QueryPerformanceCounter(&position);
        VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestData_96, sizeof(UMP96), position.QuadPart));
        QueryPerformanceCounter(&position);
        VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestData_128, sizeof(UMP128), position.QuadPart));
*/
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

    VERIFY_SUCCEEDED(midiOutDevice.Shutdown());
    VERIFY_SUCCEEDED(midiInDevice.Shutdown());
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

void Midi2DriverTests::TestMidiIO_ManyMessages(MidiTransport transport, ULONG bufferSize)
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

        if (transport == MidiTransport_CyclicUMP)
        {
            if (0 != memcmp(payload, &g_MidiTestData_32, min(payloadSize, sizeof(UMP32))))
            {
                PrintMidiMessage(payload, payloadSize, sizeof(UMP32), payloadPosition);
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

    if (!GetPins(transport, midiDeviceEnum, outPinIndex, inPinIndex))
    {
        return;
    }

    ULONG requestedBufferSize = bufferSize;
    VERIFY_SUCCEEDED(GetRequiredBufferSize(requestedBufferSize));

    LOG_OUTPUT(L"Initializing midi in");
    VERIFY_SUCCEEDED(midiInDevice.Initialize(midiDeviceEnum.m_AvailableMidiInPins[inPinIndex].FilterName.get(), NULL, midiDeviceEnum.m_AvailableMidiInPins[inPinIndex].PinId, transport, requestedBufferSize, &mmcssTaskId, this, 0));

    LOG_OUTPUT(L"Initializing midi out");
    VERIFY_SUCCEEDED(midiOutDevice.Initialize(midiDeviceEnum.m_AvailableMidiOutPins[outPinIndex].FilterName.get(), NULL, midiDeviceEnum.m_AvailableMidiOutPins[outPinIndex].PinId, transport, requestedBufferSize, &mmcssTaskId));

    LOG_OUTPUT(L"Writing midi data");

    for (UINT i = 0; i < expectedMessageCount; i++)
    {
        QueryPerformanceCounter(&position);

        if (transport == MidiTransport_CyclicUMP)
        {
            QueryPerformanceCounter(&position);
            VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestData_32, sizeof(UMP32), position.QuadPart));
        }
        else
        {
            QueryPerformanceCounter(&position);
            VERIFY_SUCCEEDED(midiOutDevice.SendMidiMessage((void *) &g_MidiTestMessage, sizeof(MIDI_MESSAGE), position.QuadPart));
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
    Sleep(500);

    LOG_OUTPUT(L"%d messages expected, %d received", expectedMessageCount, midiMessagesReceived);
    VERIFY_IS_TRUE(midiMessagesReceived == expectedMessageCount);

    LOG_OUTPUT(L"Done, cleaning up");

    VERIFY_SUCCEEDED(midiOutDevice.Shutdown());
    VERIFY_SUCCEEDED(midiInDevice.Shutdown());
}

void Midi2DriverTests::TestCyclicUMPMidiIO_ManyMessages()
{
    TestMidiIO_ManyMessages(MidiTransport_CyclicUMP, PAGE_SIZE);
}

void Midi2DriverTests::TestCyclicByteStreamMidiIO_ManyMessages()
{
    TestMidiIO_ManyMessages(MidiTransport_CyclicByteStream, PAGE_SIZE);
}

void Midi2DriverTests::TestStandardMidiIO_ManyMessages()
{
    TestMidiIO_ManyMessages(MidiTransport_StandardByteStream, PAGE_SIZE);
}

void Midi2DriverTests::TestCyclicUMPMidiIO_ManyMessages_BufferSizes()
{
    // Valid, but corner case buffer sizes
    ULONG bufferSizes[] = {
                            1,
                            PAGE_SIZE - 1,
                            PAGE_SIZE + 1,
                            PAGE_SIZE + 2,
                            MAXIMUM_LOOPED_BUFFER_SIZE - 1,
                            MAXIMUM_LOOPED_BUFFER_SIZE - 2,
                            MAXIMUM_LOOPED_BUFFER_SIZE - PAGE_SIZE
                            };

    for (UINT i = 0; i < _countof(bufferSizes); i++)
    {
        LOG_OUTPUT(L"Verifying a buffer size of %lu", bufferSizes[i]);

        TestMidiIO_ManyMessages(MidiTransport_CyclicUMP, bufferSizes[i]);
    }
}

void Midi2DriverTests::TestMidiIO_Latency(MidiTransport transport, BOOL delayedMessages)
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

    if (!GetPins(transport, midiDeviceEnum, outPinIndex, inPinIndex))
    {
        return;
    }

    ULONG requestedBufferSize = PAGE_SIZE;
    VERIFY_SUCCEEDED(GetRequiredBufferSize(requestedBufferSize));

    LOG_OUTPUT(L"Initializing midi in");
    VERIFY_SUCCEEDED(midiInDevice.Initialize(midiDeviceEnum.m_AvailableMidiInPins[inPinIndex].FilterName.get(), NULL, midiDeviceEnum.m_AvailableMidiInPins[inPinIndex].PinId, transport, requestedBufferSize, &mmcssTaskId, this, 0));

    LOG_OUTPUT(L"Initializing midi out");
    VERIFY_SUCCEEDED(midiOutDevice.Initialize(midiDeviceEnum.m_AvailableMidiOutPins[outPinIndex].FilterName.get(), NULL, midiDeviceEnum.m_AvailableMidiOutPins[outPinIndex].PinId, transport, requestedBufferSize, &mmcssTaskId));

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

        LARGE_INTEGER qpcBefore {0};
        LARGE_INTEGER qpcAfter {0};
        LONGLONG sendLatency {0};

        QueryPerformanceCounter(&qpcBefore);
        if (transport == MidiTransport_CyclicUMP)
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

    VERIFY_SUCCEEDED(midiOutDevice.Shutdown());
    VERIFY_SUCCEEDED(midiInDevice.Shutdown());
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

void Midi2DriverTests::TestBufferAllocationLimits()
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    KSMidiDeviceEnum midiDeviceEnum;
    UINT inPinIndex {0};
    UINT outPinIndex{0};

    KSMidiOutDevice midiOutDevice;
    KSMidiInDevice midiInDevice;
    DWORD mmcssTaskId {0};


    m_MidiInCallback = [&](PVOID, UINT32, LONGLONG, LONGLONG)
    {
    };

    VERIFY_SUCCEEDED(midiDeviceEnum.EnumerateFilters());

    if (!GetPins(MidiTransport_CyclicUMP, midiDeviceEnum, outPinIndex, inPinIndex))
    {
        return;
    }

    // invalid corner case buffer sizes
    ULONG bufferSizes[] = {
                            0,
                            MAXIMUM_LOOPED_BUFFER_SIZE + 1,
                            MAXIMUM_LOOPED_BUFFER_SIZE + 2,
                            MAXIMUM_LOOPED_BUFFER_SIZE + PAGE_SIZE,
                            ULONG_MAX,
                            ULONG_MAX - 1,
                            ULONG_MAX - 2,
                            ULONG_MAX - PAGE_SIZE,
                            ULONG_MAX - PAGE_SIZE - 1,
                            ULONG_MAX - PAGE_SIZE - 2,
                            ULONG_MAX/2 + 1,
                            };

    for (UINT i = 0; i < _countof(bufferSizes); i++)
    {
        ULONG requestedBufferSize = bufferSizes[i];

        LOG_OUTPUT(L"Verifying a buffer size of %lu", bufferSizes[i]);

        VERIFY_FAILED(GetRequiredBufferSize(requestedBufferSize));

        // driver initialization with all buffer sizes, for both midi in and out, should fail.
        requestedBufferSize = bufferSizes[i];
        VERIFY_FAILED(midiInDevice.Initialize(midiDeviceEnum.m_AvailableMidiInPins[inPinIndex].FilterName.get(), NULL, midiDeviceEnum.m_AvailableMidiInPins[inPinIndex].PinId, MidiTransport_CyclicUMP, requestedBufferSize, &mmcssTaskId, this, 0));

        requestedBufferSize = bufferSizes[i];
        VERIFY_FAILED(midiOutDevice.Initialize(midiDeviceEnum.m_AvailableMidiOutPins[outPinIndex].FilterName.get(), NULL, midiDeviceEnum.m_AvailableMidiOutPins[outPinIndex].PinId, MidiTransport_CyclicUMP, requestedBufferSize, &mmcssTaskId));
    }
}

class KSMidiTest : public KSMidiDevice
{
public:

    HRESULT
    Initialize(
        _In_ LPCWSTR device,
        _In_ UINT pinId,
        _In_ MidiTransport transport
    )
    {
        m_Transport = transport;

        m_FilterFilename = wil::make_cotaskmem_string_nothrow(device);
        RETURN_IF_NULL_ALLOC(m_FilterFilename);

        RETURN_IF_FAILED(FilterInstantiate(m_FilterFilename.get(), &m_Filter));
        m_PinID = pinId;

        RETURN_IF_FAILED(InstantiateMidiPin(m_Filter.get(), m_PinID, m_Transport, &m_Pin));

        return S_OK;
    }

    HRESULT
    InstantiatePinCall(
        _In_ HANDLE *CreatedPinHandle
    )
    {
        return InstantiateMidiPin(m_Filter.get(), m_PinID, m_Transport, CreatedPinHandle);
    }

    HRESULT
    SetStateCall(
        _In_ KSSTATE pinState
    )
    {
        KSPROPERTY property {0};
        ULONG propertySize {sizeof(property)};
    
        property.Set    = KSPROPSETID_Connection; 
        property.Id     = KSPROPERTY_CONNECTION_STATE;       
        property.Flags  = KSPROPERTY_TYPE_SET;
    
        RETURN_IF_FAILED(SyncIoctl(
            m_Pin.get(),
            IOCTL_KS_PROPERTY,
            &property,
            propertySize,
            &pinState,
            sizeof(KSSTATE),
            nullptr));
    
        return S_OK;
    }

    HRESULT
    LoopedBufferCall(
        _In_ ULONG& bufferSize
    )
    {
        KSMIDILOOPED_BUFFER_PROPERTY property {0};
        KSMIDILOOPED_BUFFER buffer{0};
        ULONG propertySize {sizeof(property)};

        property.Property.Set           = KSPROPSETID_MidiLoopedStreaming; 
        property.Property.Id            = KSPROPERTY_MIDILOOPEDSTREAMING_BUFFER;       
        property.Property.Flags         = KSPROPERTY_TYPE_GET;
        property.RequestedBufferSize    = bufferSize;

        RETURN_IF_FAILED(SyncIoctl(
            m_Pin.get(),
            IOCTL_KS_PROPERTY,
            &property,
            propertySize,
            &buffer,
            sizeof(buffer),
            nullptr));

        bufferSize = buffer.ActualBufferSize;

        return S_OK;
    }

    HRESULT
    LoopedRegisterCall(
        _In_ PULONG& ReadPosition,
        _In_ PULONG& WritePosition
    )
    {
        KSPROPERTY property {0};
        KSMIDILOOPED_REGISTERS registers {0};
        ULONG propertySize {sizeof(property)};

        property.Set    = KSPROPSETID_MidiLoopedStreaming; 
        property.Id     = KSPROPERTY_MIDILOOPEDSTREAMING_REGISTERS;       
        property.Flags  = KSPROPERTY_TYPE_GET;

        RETURN_IF_FAILED(SyncIoctl(
            m_Pin.get(),
            IOCTL_KS_PROPERTY,
            &property,
            propertySize,
            &registers,
            sizeof(registers),
            nullptr));

        ReadPosition = (PULONG) registers.ReadPosition;
        WritePosition = (PULONG) registers.WritePosition;

        return S_OK;
    }

    HRESULT
    LoopedEventCall(
        _In_ HANDLE WriteEvent,
        _In_ HANDLE ReadEvent
    )
    {
        KSPROPERTY property {0};
        ULONG propertySize {sizeof(property)};
        KSMIDILOOPED_EVENT2 LoopedEvent {0};

        LoopedEvent.WriteEvent = WriteEvent;
        LoopedEvent.ReadEvent = ReadEvent;

        property.Set    = KSPROPSETID_MidiLoopedStreaming; 
        property.Id     = KSPROPERTY_MIDILOOPEDSTREAMING_NOTIFICATION_EVENT;       
        property.Flags  = KSPROPERTY_TYPE_SET;

        RETURN_IF_FAILED(SyncIoctl(
            m_Pin.get(),
            IOCTL_KS_PROPERTY,
            &property,
            propertySize,
            &LoopedEvent,
            sizeof(LoopedEvent),
            nullptr));

        return S_OK;
    }
};

void
RunDriverTest(_In_ std::function<void(KSMidiTest *)>&& predicate)
{
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    MidiFlow testFlows[] = { MidiFlowIn, MidiFlowOut };

    for (UINT i = 0; i < _countof(testFlows); i++)
    {
        LOG_OUTPUT(L"Testing flow %d.", testFlows[i]);

        KSMidiDeviceEnum midiDeviceEnum;
        UINT inPinIndex {0};
        UINT outPinIndex{0};

        KSMidiTest midiTestDevice;

        LPCWSTR selectedDevice {nullptr};
        UINT selectedPinId {0};
        MidiTransport selectedTransport {MidiTransport_CyclicUMP};

        VERIFY_SUCCEEDED(midiDeviceEnum.EnumerateFilters());

        if (!GetPins(selectedTransport, midiDeviceEnum, outPinIndex, inPinIndex))
        {
            return;
        }

        if (testFlows[i] == MidiFlowIn)
        {
            selectedDevice = midiDeviceEnum.m_AvailableMidiInPins[inPinIndex].FilterName.get();
            selectedPinId = midiDeviceEnum.m_AvailableMidiInPins[inPinIndex].PinId;
        }
        else
        {
            selectedDevice = midiDeviceEnum.m_AvailableMidiOutPins[outPinIndex].FilterName.get();
            selectedPinId = midiDeviceEnum.m_AvailableMidiOutPins[outPinIndex].PinId;
        }

        VERIFY_SUCCEEDED(midiTestDevice.Initialize(selectedDevice, selectedPinId, selectedTransport));

        predicate(&midiTestDevice);

        VERIFY_SUCCEEDED(midiTestDevice.Shutdown());
    }
}

void Midi2DriverTests::TestExtraPinCreation()
{
    RunDriverTest([&](KSMidiTest * midiTestDevice){
            wil::unique_handle createdPin;

            LOG_OUTPUT(L"Verifying extra pin creation fails.");

            // Creating a second pin should fail.
            VERIFY_FAILED(midiTestDevice->InstantiatePinCall(&createdPin));
        });
}

void Midi2DriverTests::TestLoopedBufferInitialization()
{
    RunDriverTest([&](KSMidiTest * midiTestDevice){
            ULONG selectedBufferSize {PAGE_SIZE};

            LOG_OUTPUT(L"Creating looped buffer.");

            // The first call to create the looped buffer should succeed.
            VERIFY_SUCCEEDED(midiTestDevice->LoopedBufferCall(selectedBufferSize));

            LOG_OUTPUT(L"Verifying second looped buffer fails.");

            // Subsequent calls should fail.
            selectedBufferSize = PAGE_SIZE;
            VERIFY_FAILED(midiTestDevice->LoopedBufferCall(selectedBufferSize));
        });
}

void Midi2DriverTests::TestLoopedRegisterInitialization()
{
    RunDriverTest([&](KSMidiTest * midiTestDevice){
            PULONG readPosition {nullptr};
            PULONG writePosition {nullptr};
            PULONG readPosition2 {nullptr};
            PULONG writePosition2 {nullptr};
            ULONG selectedBufferSize {PAGE_SIZE};

            LOG_OUTPUT(L"Verifying looped register without a buffer first fails.");

            // creating the looped register prior to creating the looped buffer should fail.
            VERIFY_FAILED(midiTestDevice->LoopedRegisterCall(readPosition, writePosition));

            LOG_OUTPUT(L"Creating looped buffer and registers.");

            // create the looped buffer and looped register
            VERIFY_SUCCEEDED(midiTestDevice->LoopedBufferCall(selectedBufferSize));
            VERIFY_SUCCEEDED(midiTestDevice->LoopedRegisterCall(readPosition, writePosition));

            LOG_OUTPUT(L"Verifying second looped register fails.");

            // A second call to create the looped register should fail.
            VERIFY_FAILED(midiTestDevice->LoopedRegisterCall(readPosition2, writePosition2));
        });
}

void Midi2DriverTests::TestLoopedEventInitialization()
{
    RunDriverTest([&](KSMidiTest * midiTestDevice){
            PULONG readPosition {nullptr};
            PULONG writePosition {nullptr};
            ULONG selectedBufferSize {PAGE_SIZE};

            wil::unique_event readEvent;
            wil::unique_event writeEvent;

            readEvent.create(wil::EventOptions::ManualReset);
            writeEvent.create(wil::EventOptions::ManualReset);

            LOG_OUTPUT(L"Verifying invalid looped register parameters fail.");

            // invalid events should fail if no looped buffer exists
            VERIFY_FAILED(midiTestDevice->LoopedEventCall(NULL, NULL));
            VERIFY_FAILED(midiTestDevice->LoopedEventCall(readEvent.get(), NULL));            
            VERIFY_FAILED(midiTestDevice->LoopedEventCall(NULL, writeEvent.get()));

            LOG_OUTPUT(L"Verifying looped events without a buffer first fails.");

            // initializing the looped event should fail if the looped buffer is not yet initialized.
            VERIFY_FAILED(midiTestDevice->LoopedEventCall(readEvent.get(), writeEvent.get()));

            LOG_OUTPUT(L"Creating looped buffer and registers.");

            // create the looped buffer and looped register
            VERIFY_SUCCEEDED(midiTestDevice->LoopedBufferCall(selectedBufferSize));
            VERIFY_SUCCEEDED(midiTestDevice->LoopedRegisterCall(readPosition, writePosition));

            LOG_OUTPUT(L"Verifying invalid looped register parameters fail.");

            // invalid events should still fail if looped buffer exists
            VERIFY_FAILED(midiTestDevice->LoopedEventCall(NULL, NULL));
            VERIFY_FAILED(midiTestDevice->LoopedEventCall(readEvent.get(), NULL));            
            VERIFY_FAILED(midiTestDevice->LoopedEventCall(NULL, writeEvent.get()));
        });
}

void Midi2DriverTests::TestSetState()
{
    RunDriverTest([&](KSMidiTest * midiTestDevice){
        PULONG readPosition {nullptr};
        PULONG writePosition {nullptr};
        ULONG selectedBufferSize {PAGE_SIZE};
        
        wil::unique_event readEvent;
        wil::unique_event writeEvent;
        
        readEvent.create(wil::EventOptions::ManualReset);
        writeEvent.create(wil::EventOptions::ManualReset);

        LOG_OUTPUT(L"Creating looped buffer and registers.");

        // create the looped buffer and looped register
        VERIFY_SUCCEEDED(midiTestDevice->LoopedBufferCall(selectedBufferSize));
        VERIFY_SUCCEEDED(midiTestDevice->LoopedRegisterCall(readPosition, writePosition));
        VERIFY_SUCCEEDED(midiTestDevice->LoopedEventCall(readEvent.get(), writeEvent.get()));

        
        LOG_OUTPUT(L"Moving to an invalid state should fail");
        // valid values are 0 through 3, for stop through run.
        VERIFY_FAILED(midiTestDevice->SetStateCall((KSSTATE)4));
        VERIFY_FAILED(midiTestDevice->SetStateCall((KSSTATE)5));
        VERIFY_FAILED(midiTestDevice->SetStateCall((KSSTATE)0xFF));
        VERIFY_FAILED(midiTestDevice->SetStateCall((KSSTATE)0xFFFF));
        VERIFY_FAILED(midiTestDevice->SetStateCall((KSSTATE)0xFFFFFFFF));

        // Test out the transitions from each state to every other state
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_STOP));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_STOP));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_ACQUIRE));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_STOP));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_PAUSE));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_STOP));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_RUN));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_STOP));

        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_ACQUIRE));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_ACQUIRE));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_STOP));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_ACQUIRE));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_PAUSE));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_ACQUIRE));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_RUN));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_ACQUIRE));

        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_PAUSE));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_PAUSE));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_STOP));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_PAUSE));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_ACQUIRE));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_PAUSE));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_RUN));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_PAUSE));

        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_RUN));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_RUN));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_PAUSE));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_RUN));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_ACQUIRE));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_RUN));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_STOP));
        VERIFY_SUCCEEDED(midiTestDevice->SetStateCall(KSSTATE_RUN));
    });
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

