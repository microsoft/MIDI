// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#ifndef MIDI_BLE_UTILITIES_H
#define MIDI_BLE_UTILITIES_H

// The parts which are decided purely from a string, a number or a json value live here, so they
// can be unit tested without the radio, the service or COM.
#include "midi_ble_validation.h"

namespace MidiBleProtocol
{
    inline constexpr wchar_t MidiServiceUuid[] = L"{03B80E5A-EDE8-4B33-A751-6CE34EC4C700}";
    inline constexpr wchar_t Midi1DataIoCharacteristicUuid[] = L"{7772E5DB-3868-4112-A1A9-F2669D106BF3}";
    inline constexpr wchar_t Midi2UmpCharacteristicUuid[] = L"{C3B10ECF-88F5-4F7D-BFFA-8AD2C91FBAFE}";

    // A device is dropped from the available list when it has not advertised for this long.
    inline constexpr uint64_t DeviceStaleAfterMilliseconds = 60000;

    struct DiscoveredDevice
    {
        uint64_t BluetoothAddress{ 0 };
        winrt::hstring Id{ };                       // the 12 hex digit address, and the key for every command
        winrt::hstring Name{ };
        winrt::hstring GattServiceDeviceId{ };      // known only once the system has enumerated the GATT service
        Protocol SelectedProtocol{ Protocol::Unknown };
        NativeDataFormat NativeDataFormat{ NativeDataFormat::Unknown };
        bool IsPaired{ false };
        bool IsConnected{ false };
        int16_t LastSignalStrengthDbm{ 0 };
        uint64_t LastSeenTimestamp{ 0 };
        winrt::hstring EndpointDeviceId{ };

        // The interval the link negotiated, in units of 1.25 ms. Zero when not connected.
        uint16_t ConnectionIntervalUnits{ 0 };

        // Deterministic, so a customization can name a device before its endpoint exists.
        winrt::hstring EndpointDeviceInstanceId{ };

        // Connecting happens on a background worker long after the command returns, so the last
        // failure is kept here. Without it a failed connect is completely silent.
        int32_t LastConnectErrorHresult{ 0 };
        winrt::hstring LastConnectErrorDetail{ };

        // Several distinct causes share one HRESULT, so the cause is recorded separately rather
        // than being inferred from it later.
        uint32_t LastConnectErrorCode{ 0 };

        // filled in from the live connection when one exists
        uint64_t MessagesReceived{ 0 };
        uint64_t MessagesSent{ 0 };

        // Set when the device has refused an operation until the link is authenticated. Retrying
        // cannot succeed until the customer pairs, and every attempt raises another prompt.
        bool RequiresPairing{ false };

        // Some devices ask for security over SMP rather than failing a GATT call, which never
        // reaches this transport as an error. All that is visible is a link which keeps dropping
        // moments after it comes up, so repeated quick drops while unpaired are counted here.
        uint32_t UnpairedEarlyDropCount{ 0 };

        // When the current link came up, so a drop can be told from a link which lasted.
        uint64_t ConnectedSinceTimestamp{ 0 };

        // Counted before decoding, so these move even when nothing decodes.
        uint64_t PacketsReceived{ 0 };
        uint64_t PacketsSent{ 0 };

        // Only knowable from received traffic, so it stays Unknown until the device has sent
        // enough for the transport to judge its clock.
        TimestampSource IncomingTimestampSource{ TimestampSource::Unknown };

        // Connecting is asynchronous, so this distinguishes an attempt under way from a device
        // which is wanted but has not appeared yet.
        ConnectionState ConnectionState{ ConnectionState::NotConnected };
        int32_t LastSendErrorHresult{ 0 };

        // computed when the list is taken, because both are relative to now
        uint64_t LastSeenAgoMilliseconds{ 0 };
        bool IsPresent{ false };

        // False when the radio has never heard this device, which is how a paired device the
        // system remembers is told apart from one which was heard a long time ago.
        bool HasBeenSeen{ false };

        // An endpoint can exist for a device which has gone away. Keeping these separate is what
        // makes a silent disconnect visible instead of leaving apps holding a dead endpoint.
        bool HasEndpoint{ false };
    };

    // A remembered approve or deny decision for a Central which connects to this PC. The address
    // is the match key; the name is carried only so the configuration file is readable.
    struct PeripheralClientIdentity
    {
        std::wstring Address{ };
        std::wstring Name{ };
    };

    // A Central which has subscribed and is waiting on a decision. Every identity WinRT offers is
    // kept, because a device which is not bonded rotates its address and cannot be remembered.
    struct PendingPeripheralClient
    {
        std::wstring BluetoothDeviceId{ };
        std::wstring Name{ };
        std::wstring Address{ };
        std::wstring AddressType{ };
        bool IsPaired{ false };
        bool HasGenericName{ false };

        // False when the address rotates, which means "always" cannot be honored for this device
        // and the caller should not offer it.
        bool IsRememberable{ false };

        // FILETIME UTC, so a person deciding later can see how long something has been asking
        uint64_t RequestedFileTime{ 0 };
    };
}

namespace MidiBleUtilities
{
    using namespace ::winrt::Windows::Devices::Bluetooth;
    using namespace ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;

    // A GATT call against a device which has gone to sleep or out of range blocks for the full
    // Bluetooth timeout. Service shutdown joins the threads which make these calls, so an
    // unbounded wait here keeps the whole midisrv process alive long after the service stopped.
    inline constexpr uint32_t BleOperationTimeoutMilliseconds = 5000;

    // Establishing a link is much slower than talking over one which already exists: the radio may
    // have to wait several advertising intervals, and a device which demands pairing adds a whole
    // security exchange. Five seconds gave up on devices which were about to succeed.
    inline constexpr uint32_t BleConnectOperationTimeoutMilliseconds = 12000;

    inline constexpr uint32_t BleDataOperationTimeoutMilliseconds = 2000;
    inline constexpr uint32_t BleTeardownOperationTimeoutMilliseconds = 1000;

    // Blocking waits are taken in slices this long so shutdown does not have to sit out a whole
    // Bluetooth timeout, which for a connect attempt is several of them back to back.
    inline constexpr uint32_t AwaitPollSliceMilliseconds = 200;

    // Set while the transport is tearing down. Service shutdown joins the threads which make these
    // calls, so a wait in progress abandons rather than running to its full timeout.
    inline std::atomic<bool> g_shuttingDown{ false };

    inline bool IsShuttingDown() noexcept { return g_shuttingDown.load(std::memory_order_relaxed); }
    inline void SetShuttingDown(_In_ bool const value) noexcept { g_shuttingDown.store(value, std::memory_order_relaxed); }

    struct AwaitOutcome
    {
        // A wait which ran out of time and a call the system failed outright both come back empty,
        // but only one of them means the device stopped answering.
        bool TimedOut{ false };
        bool Failed{ false };
        HRESULT ErrorCode{ S_OK };
    };

    template<typename TResult>
    inline TResult AwaitWithTimeout(
        _In_ ::winrt::Windows::Foundation::IAsyncOperation<TResult> const& operation,
        _In_ uint32_t const timeoutMilliseconds,
        _In_ TResult const onTimeout,
        _Out_ AwaitOutcome& outcome)
    {
        outcome = AwaitOutcome{ };

        if (operation == nullptr)
        {
            outcome.Failed = true;
            outcome.ErrorCode = E_POINTER;

            return onTimeout;
        }

        try
        {
            // Completed may only ever be assigned once on a WinRT async operation, which rules out
            // calling wait_for in a loop: the second call throws and the wait collapses to a single
            // slice. Signaling an event from one handler and waiting on that instead is what lets
            // the wait be given up early without touching the operation again.
            //
            // Shared rather than captured by reference, because the handler can still run after
            // this function has abandoned the operation and returned.
            auto completed = std::make_shared<wil::slim_event_manual_reset>();

            operation.Completed([completed](auto&&, auto&&) { completed->SetEvent(); });

            uint32_t remaining = timeoutMilliseconds;

            while (remaining > 0)
            {
                uint32_t const slice = remaining < AwaitPollSliceMilliseconds ? remaining : AwaitPollSliceMilliseconds;

                if (completed->wait(slice))
                {
                    break;
                }

                remaining -= slice;

                // Service shutdown joins the threads making these calls, so a wait in progress is
                // abandoned rather than run to its full timeout.
                if (IsShuttingDown())
                {
                    break;
                }
            }

            auto const status = operation.Status();

            if (status == ::winrt::Windows::Foundation::AsyncStatus::Completed)
            {
                return operation.GetResults();
            }

            if (status == ::winrt::Windows::Foundation::AsyncStatus::Started)
            {
                outcome.TimedOut = true;
            }
            else
            {
                // Error and Canceled carry the HRESULT which names the real fault. Reporting these
                // as a timeout discards the only field a "it will not connect" report can act on.
                outcome.Failed = true;
                outcome.ErrorCode = operation.ErrorCode();
            }

            // Best effort. The operation keeps its own references and unwinds on its own, so
            // abandoning it is safe even when the cancel is ignored.
            operation.Cancel();
        }
        CATCH_LOG();

        if (!outcome.TimedOut && !outcome.Failed)
        {
            outcome.Failed = true;
        }

        return onTimeout;
    }

    template<typename TResult>
    inline TResult AwaitWithTimeout(
        _In_ ::winrt::Windows::Foundation::IAsyncOperation<TResult> const& operation,
        _In_ uint32_t const timeoutMilliseconds,
        _In_ TResult const onTimeout,
        _Out_ bool& timedOut)
    {
        AwaitOutcome outcome{ };

        auto result = AwaitWithTimeout(operation, timeoutMilliseconds, onTimeout, outcome);

        // Existing callers use this flag to mean "did not succeed"
        timedOut = outcome.TimedOut || outcome.Failed;

        return result;
    }

    template<typename TResult>
    inline TResult AwaitWithTimeout(
        _In_ ::winrt::Windows::Foundation::IAsyncOperation<TResult> const& operation,
        _In_ uint32_t const timeoutMilliseconds,
        _In_ TResult const onTimeout)
    {
        AwaitOutcome ignored{ };

        return AwaitWithTimeout(operation, timeoutMilliseconds, onTimeout, ignored);
    }

    inline winrt::hstring BluetoothAddressTypeToString(_In_ BluetoothAddressType const addressType)
    {
        switch (addressType)
        {
        case BluetoothAddressType::Public:      return L"public";
        case BluetoothAddressType::Random:      return L"random";
        default:                                return L"unspecified";
        }
    }

    // Returns null for SystemDefault, meaning make no request and leave the radio alone.
    inline BluetoothLEPreferredConnectionParameters GetPreferredConnectionParameters(
        _In_ MidiBleProtocol::ConnectionParameterPreference const preference)
    {
        switch (preference)
        {
        case MidiBleProtocol::ConnectionParameterPreference::ThroughputOptimized:
            return BluetoothLEPreferredConnectionParameters::ThroughputOptimized();

        case MidiBleProtocol::ConnectionParameterPreference::Balanced:
            return BluetoothLEPreferredConnectionParameters::Balanced();

        case MidiBleProtocol::ConnectionParameterPreference::PowerOptimized:
            return BluetoothLEPreferredConnectionParameters::PowerOptimized();

        default:
            return nullptr;
        }
    }

    // Windows names an unnamed BLE device "Bluetooth <address with colons>". That is not a name
    // the device supplied, and treating it as one hides the fact that it advertised none.
    inline bool IsSynthesizedBluetoothName(
        _In_ winrt::hstring const& name,
        _In_ uint64_t const address) noexcept
    {
        try
        {
            if (name.empty() || address == 0)
            {
                return false;
            }

            auto const bytes = std::format(
                L"Bluetooth {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                static_cast<uint8_t>((address >> 40) & 0xFF),
                static_cast<uint8_t>((address >> 32) & 0xFF),
                static_cast<uint8_t>((address >> 24) & 0xFF),
                static_cast<uint8_t>((address >> 16) & 0xFF),
                static_cast<uint8_t>((address >> 8) & 0xFF),
                static_cast<uint8_t>(address & 0xFF));

            return ::_wcsicmp(std::wstring{ name }.c_str(), bytes.c_str()) == 0;
        }
        catch (...)
        {
            return false;
        }
    }

    inline bool AdvertisementCarriesMidiService(
        _In_ ::winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisement const& advertisement) noexcept
    {
        try
        {
            if (advertisement == nullptr)
            {
                return false;
            }

            winrt::guid const midiServiceUuid{ MidiBleProtocol::MidiServiceUuid };

            for (auto const& uuid : advertisement.ServiceUuids())
            {
                if (uuid == midiServiceUuid)
                {
                    return true;
                }
            }

            return false;
        }
        catch (...)
        {
            return false;
        }
    }

    inline BluetoothLEDevice GetBleDeviceFromEnumerationDeviceId(_In_ std::wstring deviceId)
    {
        return AwaitWithTimeout(
            BluetoothLEDevice::FromIdAsync(winrt::to_hstring(deviceId.c_str())),
            BleOperationTimeoutMilliseconds,
            BluetoothLEDevice{ nullptr });
    }

    // Never throws and never fails: a machine with no Bluetooth at all simply reports a radio
    // which cannot do anything, which is a state the transport runs in quite happily.
    inline MidiBleProtocol::RadioCapabilities ProbeRadioCapabilities() noexcept
    {
        MidiBleProtocol::RadioCapabilities capabilities{};

        try
        {
            auto adapter = AwaitWithTimeout(
                winrt::Windows::Devices::Bluetooth::BluetoothAdapter::GetDefaultAsync(),
                BleOperationTimeoutMilliseconds,
                winrt::Windows::Devices::Bluetooth::BluetoothAdapter{ nullptr });

            if (adapter == nullptr)
            {
                return capabilities;
            }

            capabilities.RadioPresent = true;
            capabilities.LowEnergySupported = adapter.IsLowEnergySupported();
            capabilities.CentralRoleSupported = adapter.IsCentralRoleSupported();
            capabilities.PeripheralRoleSupported = adapter.IsPeripheralRoleSupported();
            capabilities.MaxAdvertisementDataLength = adapter.MaxAdvertisementDataLength();
        }
        catch (...)
        {
            // a radio which throws on interrogation is treated exactly like one which is absent
        }

        return capabilities;
    }

    // The name a remote Central sees for this PC. The GATT service provider puts the system's
    // Bluetooth name in the advertisement and gives an application no way to override it, and
    // Windows takes that name from the computer name, so this is reported rather than configured.
    inline winrt::hstring GetLocalBluetoothName()
    {
        wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1]{ 0 };
        DWORD computerNameLength = ARRAYSIZE(computerName);

        if (GetComputerNameW(computerName, &computerNameLength))
        {
            return winrt::hstring{ computerName };
        }

        return L"";
    }


    // The ATT errors which all mean "authenticate the link first". A BLE advertisement carries no
    // pairing requirement, so a rejected operation is the only signal a device needs one.
    inline bool IsPairingRequiredProtocolError(
        _In_ ::winrt::Windows::Foundation::IReference<uint8_t> const& protocolError) noexcept
    {
        try
        {
            if (protocolError == nullptr)
            {
                return false;
            }

            auto const value = protocolError.Value();

            return value == GattProtocolError::InsufficientAuthentication() ||
                value == GattProtocolError::InsufficientEncryption() ||
                value == GattProtocolError::InsufficientAuthorization() ||
                value == GattProtocolError::InsufficientEncryptionKeySize();
        }
        catch (...)
        {
            return false;
        }
    }

    // What a GATT service lookup actually did, rather than only what it returned. The distinction
    // matters because a null result and a positively unreachable device used to be reported the
    // same way, and a device dropping the link partway through looks like the former.
    struct BleMidiServiceLookup
    {
        GattDeviceService Service{ nullptr };

        GattCommunicationStatus Status{ GattCommunicationStatus::Unreachable };

        // False when nothing came back at all, so Status is a placeholder rather than an answer
        bool StatusKnown{ false };

        bool TimedOut{ false };
        bool RequiresPairing{ false };

        // The lookup call itself failed rather than running out of time
        bool Failed{ false };
        HRESULT ErrorCode{ S_OK };

        // Recovered by opening the enumerated service node instead of querying by address
        bool OpenedFromServiceNode{ false };

        bool HasProtocolError{ false };
        uint8_t ProtocolError{ 0 };
    };

    // Pairing state at the moment of a connection attempt. Read for the log, because whether a
    // device is bonded is the first question any "it will not connect" report raises.
    struct BlePairingState
    {
        bool Known{ false };
        bool IsPaired{ false };
        bool CanPair{ false };
        uint32_t ProtectionLevel{ 0 };
    };

    inline BlePairingState GetPairingState(_In_ BluetoothLEDevice const& bleDevice) noexcept
    {
        BlePairingState state{};

        try
        {
            if (bleDevice == nullptr)
            {
                return state;
            }

            auto const deviceInformation = bleDevice.DeviceInformation();

            if (deviceInformation == nullptr)
            {
                return state;
            }

            auto const pairing = deviceInformation.Pairing();

            if (pairing == nullptr)
            {
                return state;
            }

            state.Known = true;
            state.IsPaired = pairing.IsPaired();
            state.CanPair = pairing.CanPair();
            state.ProtectionLevel = static_cast<uint32_t>(pairing.ProtectionLevel());
        }
        catch (...)
        {
        }

        return state;
    }

    // Windows enumerates the MIDI service as its own PnP node once it has discovered it. Opening
    // that node does not go through the address-based device object, so it still works on devices
    // where querying services by address fails.
    inline BleMidiServiceLookup& OpenServiceNodeIfNeeded(
        _Inout_ BleMidiServiceLookup& lookup,
        _In_ winrt::hstring const& gattServiceDeviceId)
    {
        if (lookup.Service != nullptr || gattServiceDeviceId.empty())
        {
            return lookup;
        }

        AwaitOutcome outcome{ };

        auto service = AwaitWithTimeout(
            GattDeviceService::FromIdAsync(gattServiceDeviceId),
            BleConnectOperationTimeoutMilliseconds,
            GattDeviceService{ nullptr },
            outcome);

        if (service == nullptr)
        {
            return lookup;
        }

        lookup.Service = service;
        lookup.Status = GattCommunicationStatus::Success;
        lookup.StatusKnown = true;
        lookup.TimedOut = false;
        lookup.Failed = false;
        lookup.ErrorCode = S_OK;
        lookup.OpenedFromServiceNode = true;

        return lookup;
    }

    inline BleMidiServiceLookup LookupBleMidiService(
        _In_ BluetoothLEDevice const& bleDevice,
        _In_ winrt::hstring const& gattServiceDeviceId)
    {
        BleMidiServiceLookup lookup{};

        if (bleDevice == nullptr)
        {
            return lookup;
        }

        winrt::guid bleServiceUuid{ MidiBleProtocol::MidiServiceUuid };

        AwaitOutcome outcome{ };

        auto gattServicesResult = AwaitWithTimeout(
            bleDevice.GetGattServicesForUuidAsync(bleServiceUuid, BluetoothCacheMode::Uncached),
            BleConnectOperationTimeoutMilliseconds,
            GattDeviceServicesResult{ nullptr },
            outcome);

        lookup.TimedOut = outcome.TimedOut;
        lookup.Failed = outcome.Failed;
        lookup.ErrorCode = outcome.ErrorCode;

        if (gattServicesResult == nullptr)
        {
            return OpenServiceNodeIfNeeded(lookup, gattServiceDeviceId);
        }

        lookup.Status = gattServicesResult.Status();
        lookup.StatusKnown = true;

        auto const protocolError = gattServicesResult.ProtocolError();

        if (protocolError != nullptr)
        {
            lookup.HasProtocolError = true;
            lookup.ProtocolError = protocolError.Value();
        }

        lookup.RequiresPairing = IsPairingRequiredProtocolError(protocolError);

        if (lookup.Status == GattCommunicationStatus::Success)
        {
            auto const services = gattServicesResult.Services();

            for (uint32_t serviceIndex = 0; serviceIndex < services.Size(); serviceIndex++)
            {
                if (serviceIndex == 0)
                {
                    lookup.Service = services.GetAt(serviceIndex);
                }
                else
                {
                    // Each service handed back holds the device open until it is closed
                    services.GetAt(serviceIndex).Close();
                }
            }
        }

        return OpenServiceNodeIfNeeded(lookup, gattServiceDeviceId);
    }

    // BLE MIDI 1.0 carries a single MIDI 1.0 byte stream with no notion of groups, so the
    // transport declares one group terminal block in each direction on group 1. Without them the
    // service has nothing to build MIDI 1.0 ports from and falls back to sixteen unnamed ports in
    // each direction, which is useless to the older apps the ports exist for.
    //
    // The two buffers are owned by the caller because the DEVPROPERTY entries point into them and
    // must stay valid until the device manager call returns.
    inline HRESULT BuildMidi1PortProperties(
        _In_ std::wstring const& portName,
        _In_opt_ std::shared_ptr<WindowsMidiServicesPluginConfigurationLib::MidiEndpointCustomProperties> const customProperties,
        _Inout_ std::vector<std::byte>& groupTerminalBlockData,
        _Inout_ WindowsMidiServicesNamingLib::MidiEndpointNameTable& nameTable,
        _Inout_ std::vector<DEVPROPERTY>& properties) noexcept
    {
        std::vector<internal::GroupTerminalBlockInternal> blocks{ };

        // block numbers are 1-based, group indexes are 0-based
        internal::GroupTerminalBlockInternal destinationBlock{ };
        destinationBlock.Number = 1;
        destinationBlock.Direction = MIDI_GROUP_TERMINAL_BLOCK_INPUT;    // MIDI Out from the user's perspective
        destinationBlock.FirstGroupIndex = 0;
        destinationBlock.GroupCount = 1;
        destinationBlock.Protocol = 0x01;                                // MIDI_1_0_UP_TO_64_BITS
        destinationBlock.Name = portName;
        blocks.push_back(destinationBlock);

        internal::GroupTerminalBlockInternal sourceBlock{ };
        sourceBlock.Number = 2;
        sourceBlock.Direction = MIDI_GROUP_TERMINAL_BLOCK_OUTPUT;        // MIDI In from the user's perspective
        sourceBlock.FirstGroupIndex = 0;
        sourceBlock.GroupCount = 1;
        sourceBlock.Protocol = 0x01;
        sourceBlock.Name = portName;
        blocks.push_back(sourceBlock);

        groupTerminalBlockData.clear();
        RETURN_HR_IF(E_FAIL, !internal::WriteGroupTerminalBlocksToPropertyDataPointer(blocks, groupTerminalBlockData));

        properties.push_back({ { PKEY_MIDI_GroupTerminalBlocks, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BINARY, (ULONG)groupTerminalBlockData.size(), (PVOID)groupTerminalBlockData.data() });

        // The port name the service ends up using comes from this table, not from the blocks, so
        // both have to be written for the customer's name to reach WinMM.
        RETURN_IF_FAILED(nameTable.PopulateAllEntriesForMidi1DeviceUsingUmpDriver(portName, blocks));

        // A name given to an individual port outranks the endpoint name
        if (customProperties != nullptr)
        {
            for (auto const& source : customProperties->Midi1Sources)
            {
                nameTable.UpdateSourceEntryCustomName(source.second.GroupIndex, source.second.Name);
            }

            for (auto const& destination : customProperties->Midi1Destinations)
            {
                nameTable.UpdateDestinationEntryCustomName(destination.second.GroupIndex, destination.second.Name);
            }
        }

        RETURN_IF_FAILED(nameTable.WriteProperties(properties));

        return S_OK;
    }
}

#endif
