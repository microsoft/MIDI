// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midi2console
{
    struct BluetoothDeviceOptions
    {
        std::string BluetoothDeviceId;
        bool Temporary{ false };
        bool Forget{ false };
    };

    struct BluetoothCustomizeOptions
    {
        std::string BluetoothDeviceId;
        std::string Name;
        std::string Description;
        std::string Image;
        std::string KeepWhenOffline;
        bool HasName{ false };
        bool HasDescription{ false };
        bool HasImage{ false };
        bool Clear{ false };
        bool Temporary{ false };
    };

    struct BluetoothPeripheralStartOptions
    {
        std::string Protocol;
        bool Temporary{ false };
    };

    struct BluetoothPeripheralStopOptions
    {
        bool Temporary{ false };
    };

    struct BluetoothPeripheralClientOptions
    {
        std::string BluetoothAddress;
        std::string Scope;
    };

    int RunBluetoothListCommand();
    int RunBluetoothConnectCommand(_In_ BluetoothDeviceOptions const& options);
    int RunBluetoothDisconnectCommand(_In_ BluetoothDeviceOptions const& options);
    int RunBluetoothCustomizeCommand(_In_ BluetoothCustomizeOptions const& options);

    int RunBluetoothPeripheralStartCommand(_In_ BluetoothPeripheralStartOptions const& options);
    int RunBluetoothPeripheralStopCommand(_In_ BluetoothPeripheralStopOptions const& options);
    int RunBluetoothStatusCommand();
    int RunBluetoothPeripheralStatusCommand();
    int RunBluetoothPeripheralCustomizeCommand(_In_ BluetoothCustomizeOptions const& options);
    int RunBluetoothPeripheralApproveCommand(_In_ BluetoothPeripheralClientOptions const& options);
    int RunBluetoothPeripheralDenyCommand(_In_ BluetoothPeripheralClientOptions const& options);
    int RunBluetoothPeripheralForgetCommand(_In_ BluetoothPeripheralClientOptions const& options);
}
