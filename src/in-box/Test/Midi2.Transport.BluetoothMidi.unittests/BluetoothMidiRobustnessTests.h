// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <WexTestClass.h>

// The parts of the review fixes which can be checked without a radio: the worker wakeup that
// shutdown depends on, the check on writes to the peripheral, and the cap on the discovered
// device table.
class BluetoothMidiRobustnessTests
    : public WEX::TestClass<BluetoothMidiRobustnessTests>
{
public:
    BEGIN_TEST_CLASS(BluetoothMidiRobustnessTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
    END_TEST_CLASS()

    // A stop raised after the worker last looked for one, but before it reset its event, used to
    // be lost, and the thread join in shutdown then never returned.
    TEST_METHOD(TestStopRaisedBeforeSleepIsNotLost);
    TEST_METHOD(TestStopRaisedAfterResetWakesTheSleeper);
    TEST_METHOD(TestStopSignalWaitsForTheQueueLock);
    TEST_METHOD(TestWorkerStopsWhenStopLandsBetweenCheckAndReset);
    TEST_METHOD(TestWorkerAlwaysStopsUnderRacingShutdown);
    TEST_METHOD(TestQueuedWorkIsNotLostToTheStopCheck);

    // Any Central in range can write to the peripheral's characteristic
    TEST_METHOD(TestPeripheralWriteFromAttachedClientIsAccepted);
    TEST_METHOD(TestPeripheralWriteIgnoresCase);
    TEST_METHOD(TestPeripheralWriteFromAnotherClientIsRejected);
    TEST_METHOD(TestPeripheralWriteWithNoAttachedClientIsRejected);
    TEST_METHOD(TestPeripheralWriteWithNoWriterIdIsRejected);

    // A radio advertising from a stream of random addresses must not grow the table forever
    TEST_METHOD(TestEvictionLeavesATableWithRoomAlone);
    TEST_METHOD(TestEvictionDropsStaleDevicesEvenWithRoom);
    TEST_METHOD(TestEvictionNeverDropsAKeptDevice);
    TEST_METHOD(TestEvictionDropsTheLongestUnheardWhenFull);
    TEST_METHOD(TestEvictionTurnsAwayWhenFullOfKeptDevices);
    TEST_METHOD(TestEvictionBoundsTheTableUnderAnAddressFlood);

    // A disconnect waited for the writer's in-flight Bluetooth call to run to its full timeout
    TEST_METHOD(TestAwaitReturnsTheResultWhenTheOperationCompletes);
    TEST_METHOD(TestAwaitGivesUpSoonAfterShutdownStarts);
    TEST_METHOD(TestAwaitGivesUpAtOnceWhenAlreadyShuttingDown);
    TEST_METHOD(TestAwaitRunsToItsTimeoutWhenNotShuttingDown);
};
