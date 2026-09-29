// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Tests for the Bluetooth MIDI API in Windows.Devices.Midi2.dll, run through the service against
// real Bluetooth hardware. They need a Bluetooth LE radio and the Bluetooth MIDI transport, and most
// of them need a device, so they are not for CI. A test skips when something it needs is missing.
//
// Devices are named with TAEF runtime parameters. `midi bluetooth list` shows the ids.
//
//   /p:BleDeviceId=<id>        A device which is switched on, in range, and not connected to any
//                              other computer, phone or tablet. Most tests need this.
//   /p:BleAbsentDeviceId=<id>  A paired device which is switched off.
//   /p:BleInteractive=true     Runs the test which asks you to play the device.
//
// For example:
//
//   TE.exe Midi2.WinRTClient.Bluetooth.hardwaretests.dll /p:BleDeviceId=F1E2D3C4B5A6 /logOutput:High
//
// Each test puts the devices back the way it found them, connected or not. The peripheral test
// makes this PC advertise for a few seconds. Only the tests about saved devices write to the
// configuration file, and only an entry for an address no real device should have, which they
// remove again.
class MidiBluetoothHardwareTests
    : public WEX::TestClass<MidiBluetoothHardwareTests>
{
public:

    BEGIN_TEST_CLASS(MidiBluetoothHardwareTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Hardware")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.dll")
    END_TEST_CLASS()

    TEST_CLASS_SETUP(ClassSetup);
    TEST_CLASS_CLEANUP(ClassCleanup);

    TEST_METHOD_CLEANUP(TestCleanup);

    // The radio and the transport, no device
    TEST_METHOD(TestRadioCanConnectToDevices);
    TEST_METHOD(TestMalformedDeviceIdsAreRejected);
    TEST_METHOD(TestUnknownDeviceIdIsAcceptedAndCanBeCanceled);
    TEST_METHOD(TestConfiguredDevicesFollowSavedChanges);
    TEST_METHOD(TestSavingOfflineRetentionDoesNotSaveTheDevice);
    TEST_METHOD(TestPeripheralStartsAndStops);

    // BleDeviceId
    TEST_METHOD(TestDeviceIsDiscovered);
    TEST_METHOD(TestConnectCreatesEndpointAndDisconnectRemovesIt);
    TEST_METHOD(TestDeviceIdIsAcceptedInOtherForms);
    TEST_METHOD(TestSettingOfflineRetentionDoesNotConnect);
    TEST_METHOD(TestSentMessagesAreWrittenToTheDevice);

    // BleDeviceId and BleInteractive
    TEST_METHOD(TestMessagesFromTheDeviceArrive);

    // BleDeviceId and BleAbsentDeviceId
    TEST_METHOD(TestAbsentDeviceDoesNotHoldUpAnother);

private:
    // Each device a test changed, and whether it was wanted before
    std::vector<std::pair<std::wstring, bool>> m_devicesToRestore{};

    void RememberStateOf(_In_ std::wstring const& deviceId);
    void RestoreDevices();
};
