// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"
#include "MidiBluetoothHardwareTests.h"

using namespace WEX::Logging;
using namespace WEX::Common;
using namespace WEX::TestExecution;

namespace
{
    constexpr wchar_t PresentDeviceIdParameter[] = L"BleDeviceId";
    constexpr wchar_t AbsentDeviceIdParameter[] = L"BleAbsentDeviceId";
    constexpr wchar_t InteractiveParameter[] = L"BleInteractive";

    // An address no real device should have, easy to spot if a failed run leaves it saved
    constexpr wchar_t UnknownDeviceId[] = L"C0DEC0DE0001";
    constexpr wchar_t UnknownDeviceIdOtherForm[] = L"c0:de:c0:de:00:01";
    constexpr wchar_t SavedEntryComment[] = L"Windows MIDI Services hardware test";

    // Mirrors MIDI_BLE_CONNECT_OPERATION_TIMEOUT_MS, the longest one connection attempt may take
    constexpr std::chrono::milliseconds ConnectAttemptLimit{ 12000 };

    // A device may take several advertising intervals to answer
    constexpr std::chrono::milliseconds ConnectWait{ 30000 };
    constexpr std::chrono::milliseconds DisconnectWait{ 10000 };
    constexpr std::chrono::milliseconds DiscoveryWait{ 30000 };
    constexpr std::chrono::milliseconds CountersWait{ 5000 };
    constexpr std::chrono::milliseconds PlayWait{ 30000 };

    // Longer than two of the transport's reconnect sweeps, which run every three seconds
    constexpr std::chrono::milliseconds StaysDisconnectedWait{ 7000 };

    // Long enough for the transport to start on one request before the next arrives
    constexpr std::chrono::milliseconds AttemptStartDelay{ 500 };

    constexpr std::chrono::milliseconds PollInterval{ 250 };

    // Note Off for the lowest note on channel 16, which nothing sounds
    constexpr uint32_t HarmlessMessage{ 0x208F0000 };
    constexpr uint32_t MessagesToSend{ 10 };

    bool TransportIsAvailable()
    {
        try
        {
            return MidiBluetoothTransportManager::IsTransportAvailable();
        }
        catch (...)
        {
            return false;
        }
    }

    bool RadioCanConnect()
    {
        try
        {
            auto const radio = MidiBluetoothTransportManager::GetRadioInformation();

            return radio != nullptr && radio.IsPresent() && radio.IsLowEnergySupported() && radio.IsCentralRoleSupported();
        }
        catch (...)
        {
            return false;
        }
    }

    std::wstring RuntimeParameter(_In_ PCWSTR name)
    {
        String value;

        if (SUCCEEDED(RuntimeParameters::TryGetValue(name, value)) && !value.IsEmpty())
        {
            return std::wstring{ static_cast<PCWSTR>(value) };
        }

        return {};
    }

    bool IsInteractive()
    {
        auto const value = RuntimeParameter(InteractiveParameter);

        return _wcsicmp(value.c_str(), L"true") == 0 || value == L"1";
    }

    // 12 uppercase hex digits, the form the API reports, or empty for anything else
    std::wstring CanonicalDeviceId(_In_ std::wstring const& value)
    {
        std::wstring digits{};

        for (auto const ch : value)
        {
            if (ch == L':' || ch == L'-' || ch == L' ')
            {
                continue;
            }

            if (!iswxdigit(ch) || digits.size() == 12)
            {
                return {};
            }

            digits.push_back(static_cast<wchar_t>(towupper(ch)));
        }

        return digits.size() == 12 ? digits : std::wstring{};
    }

    // The same address the way people often write one
    std::wstring LowercaseWithColons(_In_ std::wstring const& deviceId)
    {
        std::wstring result{};

        for (size_t i = 0; i < deviceId.size(); i++)
        {
            if (i > 0 && i % 2 == 0)
            {
                result.push_back(L':');
            }

            result.push_back(static_cast<wchar_t>(towlower(deviceId[i])));
        }

        return result;
    }

    // Skips the test when the parameter was not given. A value which is not an address fails it,
    // because then someone meant to name a device and got it wrong.
    bool TryGetDeviceIdParameter(_In_ PCWSTR name, _Out_ std::wstring& deviceId)
    {
        deviceId.clear();

        auto const value = RuntimeParameter(name);

        if (value.empty())
        {
            Log::Result(TestResults::Skipped, String().Format(L"Needs a device. Run with /p:%s=<id>, using an id from 'midi bluetooth list'.", name));
            return false;
        }

        deviceId = CanonicalDeviceId(value);

        if (deviceId.empty())
        {
            Log::Error(String().Format(L"/p:%s=%s is not a Bluetooth device id. Use the 12 hex digit id from 'midi bluetooth list'.", name, value.c_str()));
            return false;
        }

        return true;
    }

    long long ElapsedMilliseconds(_In_ std::chrono::steady_clock::time_point const since)
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - since).count();
    }

    PCWSTR StateName(_In_ MidiBluetoothConnectionState const state)
    {
        switch (state)
        {
        case MidiBluetoothConnectionState::NotConnected:
            return L"NotConnected";
        case MidiBluetoothConnectionState::WaitingForDevice:
            return L"WaitingForDevice";
        case MidiBluetoothConnectionState::Connecting:
            return L"Connecting";
        case MidiBluetoothConnectionState::Connected:
            return L"Connected";
        default:
            return L"(unrecognized)";
        }
    }

    void LogDevice(_In_ PCWSTR label, _In_ MidiBluetoothDeviceInformation const& device)
    {
        if (device == nullptr)
        {
            Log::Comment(String().Format(L"%s: not listed", label));
            return;
        }

        Log::Comment(String().Format(
            L"%s: %s '%s' state=%s present=%d paired=%d needsPairing=%d protocol=%d endpoint=%d "
            L"received=%llu/%llu sent=%llu/%llu (messages/packets) lastError=0x%08X hr=0x%08X '%s'",
            label,
            device.BluetoothDeviceId().c_str(),
            device.Name().c_str(),
            StateName(device.ConnectionState()),
            device.IsPresent() ? 1 : 0,
            device.IsPaired() ? 1 : 0,
            device.RequiresPairing() ? 1 : 0,
            static_cast<int>(device.SelectedProtocol()),
            device.HasEndpoint() ? 1 : 0,
            device.MessagesReceived(),
            device.PacketsReceived(),
            device.MessagesSent(),
            device.PacketsSent(),
            static_cast<uint32_t>(device.LastConnectErrorCode()),
            static_cast<uint32_t>(device.LastConnectErrorHResult()),
            device.LastConnectError().c_str()));
    }

    void LogConnectResponse(_In_ MidiBluetoothDeviceConnectResponse const& response)
    {
        if (response == nullptr)
        {
            return;
        }

        Log::Comment(String().Format(
            L"Connect response: success=%d known=%d code=0x%08X hr=0x%08X '%s'",
            response.Success() ? 1 : 0,
            response.IsKnown() ? 1 : 0,
            static_cast<uint32_t>(response.ErrorCode()),
            static_cast<uint32_t>(response.ErrorHResult()),
            response.ErrorMessage().c_str()));
    }

    bool IsListedAndPresent(_In_ MidiBluetoothDeviceInformation const& device)
    {
        return device != nullptr && device.IsPresent();
    }

    bool IsConnectedWithEndpoint(_In_ MidiBluetoothDeviceInformation const& device)
    {
        return device != nullptr && device.ConnectionState() == MidiBluetoothConnectionState::Connected && device.HasEndpoint();
    }

    // A device which is not wanted may drop out of the list altogether, which counts
    bool IsFullyDisconnected(_In_ MidiBluetoothDeviceInformation const& device)
    {
        return device == nullptr || (device.ConnectionState() == MidiBluetoothConnectionState::NotConnected && !device.HasEndpoint());
    }

    // The device is looked up again on every poll, and is null while it is not listed
    bool WaitForDevice(
        _In_ std::wstring const& deviceId,
        _In_ std::chrono::milliseconds const timeout,
        _In_ std::function<bool(MidiBluetoothDeviceInformation const&)> const& condition,
        _Out_ MidiBluetoothDeviceInformation& device)
    {
        auto const started = std::chrono::steady_clock::now();

        for (;;)
        {
            device = MidiBluetoothTransportManager::GetDevice(deviceId);

            if (condition(device))
            {
                return true;
            }

            if (std::chrono::steady_clock::now() - started >= timeout)
            {
                return false;
            }

            std::this_thread::sleep_for(PollInterval);
        }
    }

    MidiBluetoothDeviceInformation ConnectAndWait(
        _In_ std::wstring const& deviceId,
        _In_ std::wstring const& requestedId,
        _In_ std::chrono::milliseconds const timeout)
    {
        MidiBluetoothDeviceInformation device{ nullptr };

        // A device which was just disconnected may not be advertising again yet
        VERIFY_IS_TRUE(WaitForDevice(deviceId, DiscoveryWait, IsListedAndPresent, device), L"the device is switched on and advertising");

        auto const response = MidiBluetoothTransportManager::ConnectDeviceAsync(MidiBluetoothDeviceConnectConfig(requestedId)).get();

        VERIFY_IS_TRUE(response != nullptr);
        LogConnectResponse(response);

        VERIFY_IS_TRUE(response.Success(), L"the connect request is accepted");
        VERIFY_IS_TRUE(response.IsKnown(), L"for a device this PC has found");

        auto const connected = WaitForDevice(deviceId, timeout, IsConnectedWithEndpoint, device);

        LogDevice(L"After connecting", device);

        VERIFY_IS_TRUE(connected, L"the device connects and gets an endpoint. One which needs pairing, or is connected to another host, does not.");

        return device;
    }

    void DisconnectAndWait(_In_ std::wstring const& deviceId, _In_ std::wstring const& requestedId)
    {
        auto const response = MidiBluetoothTransportManager::DisconnectDeviceAsync(MidiBluetoothDeviceDisconnectConfig(requestedId)).get();

        VERIFY_IS_TRUE(response != nullptr);

        if (!response.Success())
        {
            Log::Comment(String().Format(
                L"Disconnect response: code=0x%08X hr=0x%08X '%s'",
                static_cast<uint32_t>(response.ErrorCode()),
                static_cast<uint32_t>(response.ErrorHResult()),
                response.ErrorMessage().c_str()));
        }

        VERIFY_IS_TRUE(response.Success(), L"the disconnect request is accepted");

        MidiBluetoothDeviceInformation device{ nullptr };

        auto const disconnected = WaitForDevice(deviceId, DisconnectWait, IsFullyDisconnected, device);

        LogDevice(L"After disconnecting", device);

        VERIFY_IS_TRUE(disconnected, L"the device disconnects and its endpoint goes away");
    }

    // Tests which connect a device start from a known state
    void StartDisconnected(_In_ std::wstring const& deviceId)
    {
        if (!IsFullyDisconnected(MidiBluetoothTransportManager::GetDevice(deviceId)))
        {
            Log::Comment(String().Format(L"Disconnecting %s first, because it is connected or wanted.", deviceId.c_str()));
            DisconnectAndWait(deviceId, deviceId);
        }
    }

    MidiBluetoothDeviceInformation EnsureConnected(_In_ std::wstring const& deviceId)
    {
        auto const device = MidiBluetoothTransportManager::GetDevice(deviceId);

        if (IsConnectedWithEndpoint(device))
        {
            return device;
        }

        return ConnectAndWait(deviceId, deviceId, ConnectWait);
    }

    // Disconnecting is a decision, so nothing may bring the device back by itself
    void VerifyStaysDisconnected(_In_ std::wstring const& deviceId)
    {
        std::this_thread::sleep_for(StaysDisconnectedWait);

        auto const device = MidiBluetoothTransportManager::GetDevice(deviceId);

        LogDevice(L"A while after disconnecting", device);

        VERIFY_IS_TRUE(IsFullyDisconnected(device), L"a disconnected device is not reconnected by itself");
    }

    std::vector<MidiBluetoothSavedDevice> FindSavedDevices(_In_ std::wstring const& deviceId)
    {
        std::vector<MidiBluetoothSavedDevice> found{};

        for (auto const& device : MidiBluetoothTransportManager::GetSavedDevices())
        {
            if (std::wstring{ device.BluetoothDeviceId() } == deviceId)
            {
                found.push_back(device);
            }
        }

        return found;
    }

    void VerifySaved(_In_ MidiServiceConfigSaveResponse const& response, _In_ PCWSTR description)
    {
        VERIFY_IS_TRUE(response != nullptr);

        if (!response.Success())
        {
            Log::Comment(String().Format(
                L"Save failed: result=%d '%s' file='%s'",
                static_cast<int>(response.Result()),
                response.ErrorMessage().c_str(),
                response.ConfigFilePath().c_str()));
        }

        VERIFY_IS_TRUE(response.Success(), description);
    }

    // For cleanup, so it never throws. Unconditional, because an entry holding only a setting is not listed.
    void RemoveSavedTestEntry() noexcept
    {
        try
        {
            MidiServiceTransportPluginConfigManager::SaveUpdate(MidiBluetoothDeviceDisconnectConfig(UnknownDeviceId, true));
        }
        catch (...)
        {
        }
    }
}

#define SKIP_IF_NO_BLUETOOTH_TRANSPORT() \
    if (!TransportIsAvailable()) \
    { \
        Log::Result(TestResults::Skipped, L"The Bluetooth MIDI transport is not installed, or the service is not running."); \
        return; \
    }

#define SKIP_IF_RADIO_CANNOT_CONNECT() \
    if (!RadioCanConnect()) \
    { \
        Log::Result(TestResults::Skipped, L"This PC has no Bluetooth LE radio which can connect to devices."); \
        return; \
    }

#define REQUIRE_DEVICE_ID(variable, parameterName) \
    std::wstring variable{}; \
    if (!TryGetDeviceIdParameter(parameterName, variable)) \
    { \
        return; \
    }


bool MidiBluetoothHardwareTests::ClassSetup()
{
    if (!MidiApi::EnsureServiceAvailable())
    {
        Log::Comment(L"The MIDI service is not available, so every test will skip.");
    }

    Log::Comment(String().Format(
        L"%s='%s' %s='%s' %s='%s'",
        PresentDeviceIdParameter, RuntimeParameter(PresentDeviceIdParameter).c_str(),
        AbsentDeviceIdParameter, RuntimeParameter(AbsentDeviceIdParameter).c_str(),
        InteractiveParameter, RuntimeParameter(InteractiveParameter).c_str()));

    return true;
}

bool MidiBluetoothHardwareTests::ClassCleanup()
{
    RestoreDevices();

    return true;
}

bool MidiBluetoothHardwareTests::TestCleanup()
{
    RestoreDevices();

    return true;
}

_Use_decl_annotations_
void MidiBluetoothHardwareTests::RememberStateOf(std::wstring const& deviceId)
{
    for (auto const& entry : m_devicesToRestore)
    {
        if (entry.first == deviceId)
        {
            return;
        }
    }

    // Connected, being connected, or waited for all mean somebody asked for it
    auto const device = MidiBluetoothTransportManager::GetDevice(deviceId);
    auto const wasWanted = device != nullptr && device.ConnectionState() != MidiBluetoothConnectionState::NotConnected;

    LogDevice(wasWanted ? L"Before the test, wanted" : L"Before the test, not wanted", device);

    m_devicesToRestore.emplace_back(deviceId, wasWanted);
}

void MidiBluetoothHardwareTests::RestoreDevices()
{
    for (auto const& [deviceId, wasWanted] : m_devicesToRestore)
    {
        try
        {
            bool restored{ false };

            if (wasWanted)
            {
                auto const response = MidiBluetoothTransportManager::ConnectDeviceAsync(MidiBluetoothDeviceConnectConfig(deviceId)).get();
                restored = response != nullptr && response.Success();
            }
            else
            {
                auto const response = MidiBluetoothTransportManager::DisconnectDeviceAsync(MidiBluetoothDeviceDisconnectConfig(deviceId)).get();
                restored = response != nullptr && response.Success();
            }

            Log::Comment(String().Format(
                L"Put %s back to %s: %s",
                deviceId.c_str(),
                wasWanted ? L"connected" : L"not connected",
                restored ? L"done" : L"FAILED"));
        }
        catch (...)
        {
            Log::Comment(String().Format(L"Could not put %s back the way it was", deviceId.c_str()));
        }
    }

    m_devicesToRestore.clear();
}


void MidiBluetoothHardwareTests::TestRadioCanConnectToDevices()
{
    SKIP_IF_NO_BLUETOOTH_TRANSPORT();

    auto const radio = MidiBluetoothTransportManager::GetRadioInformation();

    VERIFY_IS_TRUE(radio != nullptr, L"the transport reports the radio");

    Log::Comment(String().Format(
        L"Radio: present=%d lowEnergy=%d central=%d peripheral=%d",
        radio.IsPresent() ? 1 : 0,
        radio.IsLowEnergySupported() ? 1 : 0,
        radio.IsCentralRoleSupported() ? 1 : 0,
        radio.IsPeripheralRoleSupported() ? 1 : 0));

    VERIFY_IS_TRUE(radio.IsPresent(), L"this PC has a Bluetooth radio");
    VERIFY_IS_TRUE(radio.IsLowEnergySupported(), L"which supports Bluetooth LE");
    VERIFY_IS_TRUE(radio.IsCentralRoleSupported(), L"and can connect to devices");
}

void MidiBluetoothHardwareTests::TestMalformedDeviceIdsAreRejected()
{
    SKIP_IF_NO_BLUETOOTH_TRANSPORT();

    // empty, too short, too long, not hex, and nothing but separators
    for (auto const id : { L"", L"C0DEC0DE00", L"C0DEC0DE000100", L"C0DEC0DE00G1", L"not a device", L"::::::" })
    {
        Log::Comment(String().Format(L"Trying '%s'", id));

        auto const connect = MidiBluetoothTransportManager::ConnectDeviceAsync(MidiBluetoothDeviceConnectConfig(id)).get();

        VERIFY_IS_TRUE(connect != nullptr);
        VERIFY_IS_FALSE(connect.Success(), L"connecting is refused");
        VERIFY_IS_TRUE(
            connect.ErrorCode() == MidiBluetoothDeviceConnectErrorCode::InvalidBluetoothDeviceId ||
            connect.ErrorCode() == MidiBluetoothDeviceConnectErrorCode::MissingBluetoothDeviceId,
            L"because the id is unusable");

        auto const disconnect = MidiBluetoothTransportManager::DisconnectDeviceAsync(MidiBluetoothDeviceDisconnectConfig(id)).get();

        VERIFY_IS_TRUE(disconnect != nullptr);
        VERIFY_IS_FALSE(disconnect.Success(), L"disconnecting is refused");
        VERIFY_IS_TRUE(
            disconnect.ErrorCode() == MidiBluetoothDeviceDisconnectErrorCode::InvalidBluetoothDeviceId ||
            disconnect.ErrorCode() == MidiBluetoothDeviceDisconnectErrorCode::MissingBluetoothDeviceId,
            L"because the id is unusable");

        VERIFY_IS_TRUE(MidiBluetoothTransportManager::GetDevice(id) == nullptr, L"and looking it up finds nothing");
    }

    VERIFY_IS_FALSE(MidiBluetoothTransportManager::ConnectDeviceAsync(nullptr).get().Success(), L"a missing connect config is refused");
    VERIFY_IS_FALSE(MidiBluetoothTransportManager::DisconnectDeviceAsync(nullptr).get().Success(), L"a missing disconnect config is refused");
}

void MidiBluetoothHardwareTests::TestUnknownDeviceIdIsAcceptedAndCanBeCanceled()
{
    SKIP_IF_NO_BLUETOOTH_TRANSPORT();
    SKIP_IF_RADIO_CANNOT_CONNECT();

    RememberStateOf(UnknownDeviceId);

    // A device which is asleep when it is asked for is connected when it next advertises, so an
    // address nothing has been heard from is accepted rather than refused
    auto const connect = MidiBluetoothTransportManager::ConnectDeviceAsync(MidiBluetoothDeviceConnectConfig(UnknownDeviceId)).get();

    VERIFY_IS_TRUE(connect != nullptr);
    LogConnectResponse(connect);

    VERIFY_IS_TRUE(connect.Success(), L"the request is accepted");
    VERIFY_IS_FALSE(connect.IsKnown(), L"and says the device has not been found");
    VERIFY_IS_TRUE(connect.Device() == nullptr, L"so there is nothing to describe");
    VERIFY_IS_TRUE(MidiBluetoothTransportManager::GetDevice(UnknownDeviceId) == nullptr, L"an address never found is not listed");

    auto const disconnect = MidiBluetoothTransportManager::DisconnectDeviceAsync(MidiBluetoothDeviceDisconnectConfig(UnknownDeviceId)).get();

    VERIFY_IS_TRUE(disconnect != nullptr);
    VERIFY_IS_TRUE(disconnect.Success(), L"and the request can be canceled");
}

void MidiBluetoothHardwareTests::TestSavedDevicesFollowSavedChanges()
{
    SKIP_IF_NO_BLUETOOTH_TRANSPORT();

    if (MidiServiceTransportPluginConfigManager::ConfigFilePath().empty())
    {
        Log::Result(TestResults::Skipped, L"No configuration file is registered on this PC, so nothing can be saved.");
        return;
    }

    // Only there if an earlier run could not clean up
    RemoveSavedTestEntry();
    VERIFY_IS_TRUE(FindSavedDevices(UnknownDeviceId).empty(), L"the test address is not saved to begin with");

    auto removeEntry = wil::scope_exit([] { RemoveSavedTestEntry(); });

    // Saved in one form and changed in the other, so the two must be treated as one device
    MidiBluetoothDeviceConnectConfig connect{ UnknownDeviceIdOtherForm };
    connect.Comment(SavedEntryComment);

    VerifySaved(MidiServiceTransportPluginConfigManager::SaveUpdate(connect), L"saving a device to connect works");

    auto entries = FindSavedDevices(UnknownDeviceId);

    VERIFY_IS_TRUE(entries.size() == 1, L"the saved device is listed once, in the form the API reports");
    VERIFY_IS_TRUE(entries[0].Comment() == SavedEntryComment, L"with its comment");
    VERIFY_IS_TRUE(entries[0].IsEnabled(), L"and set to connect");
    VERIFY_IS_TRUE(
        entries[0].OfflineRetentionSeconds() == static_cast<int32_t>(MidiBluetoothOfflineRetention::UseTransportDefault),
        L"with no offline retention of its own");

    VerifySaved(
        MidiServiceTransportPluginConfigManager::SaveUpdate(MidiBluetoothDeviceDisconnectConfig(UnknownDeviceId, false)),
        L"saving it switched off works");

    entries = FindSavedDevices(UnknownDeviceId);

    VERIFY_IS_TRUE(entries.size() == 1, L"a device switched off is still listed, still once");
    VERIFY_IS_FALSE(entries[0].IsEnabled(), L"as switched off");
    VERIFY_IS_TRUE(entries[0].Comment() == SavedEntryComment, L"and keeps its comment");

    VerifySaved(
        MidiServiceTransportPluginConfigManager::SaveUpdate(MidiBluetoothDeviceDisconnectConfig(UnknownDeviceIdOtherForm, true)),
        L"removing it works");

    VERIFY_IS_TRUE(FindSavedDevices(UnknownDeviceId).empty(), L"a removed device is no longer listed");
}

void MidiBluetoothHardwareTests::TestSavingOfflineRetentionDoesNotSaveTheDevice()
{
    SKIP_IF_NO_BLUETOOTH_TRANSPORT();

    if (MidiServiceTransportPluginConfigManager::ConfigFilePath().empty())
    {
        Log::Result(TestResults::Skipped, L"No configuration file is registered on this PC, so nothing can be saved.");
        return;
    }

    RemoveSavedTestEntry();
    VERIFY_IS_TRUE(FindSavedDevices(UnknownDeviceId).empty(), L"the test address is not saved to begin with");

    auto removeEntry = wil::scope_exit([] { RemoveSavedTestEntry(); });

    VerifySaved(
        MidiServiceTransportPluginConfigManager::SaveUpdate(MidiBluetoothOfflineRetentionConfig(UnknownDeviceId, 30)),
        L"saving a device's offline retention works");

    VERIFY_IS_TRUE(FindSavedDevices(UnknownDeviceId).empty(), L"and does not save it as a device to connect");

    MidiBluetoothDeviceConnectConfig connect{ UnknownDeviceId };
    connect.Comment(SavedEntryComment);

    VerifySaved(MidiServiceTransportPluginConfigManager::SaveUpdate(connect), L"saving it to connect afterwards works");

    auto const entries = FindSavedDevices(UnknownDeviceId);

    VERIFY_IS_TRUE(entries.size() == 1, L"after which it is listed once");
    VERIFY_IS_TRUE(entries[0].IsEnabled(), L"set to connect");
    VERIFY_IS_TRUE(entries[0].OfflineRetentionSeconds() == 30, L"and still has the retention saved before");
}

void MidiBluetoothHardwareTests::TestPeripheralStartsAndStops()
{
    SKIP_IF_NO_BLUETOOTH_TRANSPORT();

    auto const radio = MidiBluetoothTransportManager::GetRadioInformation();

    if (radio == nullptr || !radio.IsPresent() || !radio.IsPeripheralRoleSupported())
    {
        Log::Result(TestResults::Skipped, L"This PC's Bluetooth radio cannot act as a peripheral.");
        return;
    }

    auto const before = MidiBluetoothTransportManager::GetPeripheralStatus();

    VERIFY_IS_TRUE(before != nullptr);

    if (before.IsRunning())
    {
        Log::Result(TestResults::Skipped, L"The peripheral is already running, and stopping it would drop anything connected to it.");
        return;
    }

    for (auto const protocol : { MidiBluetoothProtocol::BluetoothLowEnergyMidi1, MidiBluetoothProtocol::BluetoothLowEnergyMidi2Ump })
    {
        Log::Comment(String().Format(L"Protocol %d", static_cast<int>(protocol)));

        auto stopPeripheral = wil::scope_exit([]
            {
                try
                {
                    MidiBluetoothTransportManager::StopPeripheralAsync().get();
                }
                catch (...)
                {
                }
            });

        auto const started = MidiBluetoothTransportManager::StartPeripheralAsync(MidiBluetoothPeripheralConfig(protocol)).get();

        VERIFY_IS_TRUE(started != nullptr);

        if (!started.Success())
        {
            Log::Comment(String().Format(
                L"Start failed: code=0x%08X hr=0x%08X '%s'",
                static_cast<uint32_t>(started.ErrorCode()),
                static_cast<uint32_t>(started.ErrorHResult()),
                started.ErrorMessage().c_str()));
        }

        VERIFY_IS_TRUE(started.Success(), L"the peripheral starts");
        VERIFY_IS_TRUE(started.Status() != nullptr && started.Status().IsRunning(), L"and says it is running");
        VERIFY_IS_TRUE(started.Status().Protocol() == protocol, L"with the protocol asked for");

        Log::Comment(String().Format(L"Advertised as '%s'", started.Status().AdvertisedName().c_str()));

        VERIFY_IS_TRUE(MidiBluetoothTransportManager::GetPeripheralStatus().IsRunning(), L"and the status agrees");

        stopPeripheral.release();

        auto const stopped = MidiBluetoothTransportManager::StopPeripheralAsync().get();

        VERIFY_IS_TRUE(stopped != nullptr);
        VERIFY_IS_TRUE(stopped.Success(), L"the peripheral stops");
        VERIFY_IS_FALSE(MidiBluetoothTransportManager::GetPeripheralStatus().IsRunning(), L"and says so");
    }
}

void MidiBluetoothHardwareTests::TestDeviceIsDiscovered()
{
    SKIP_IF_NO_BLUETOOTH_TRANSPORT();
    SKIP_IF_RADIO_CANNOT_CONNECT();
    REQUIRE_DEVICE_ID(deviceId, PresentDeviceIdParameter);

    // A connected device stops advertising, so being connected counts as present
    MidiBluetoothDeviceInformation device{ nullptr };

    auto const found = WaitForDevice(deviceId, DiscoveryWait, IsListedAndPresent, device);

    LogDevice(L"Device under test", device);

    VERIFY_IS_TRUE(found, L"the device is found. Is it switched on, in range, and not connected to another host?");
    VERIFY_IS_FALSE(device.Name().empty(), L"with a name");

    wchar_t address[13]{};
    swprintf_s(address, L"%012llX", device.BluetoothAddress());

    VERIFY_IS_TRUE(std::wstring{ device.BluetoothDeviceId() } == address, L"its id is its address as 12 hex digits");

    std::set<std::wstring> listedIds{};

    for (auto const& listed : MidiBluetoothTransportManager::GetAvailableDevices())
    {
        LogDevice(L"Listed", listed);

        auto const listedId = std::wstring{ listed.BluetoothDeviceId() };

        VERIFY_IS_TRUE(CanonicalDeviceId(listedId) == listedId, L"every listed id is 12 uppercase hex digits");
        VERIFY_IS_TRUE(listedIds.insert(listedId).second, L"and no device is listed twice");
    }

    VERIFY_IS_TRUE(listedIds.count(deviceId) == 1, L"the device under test is in the full list too");
}

void MidiBluetoothHardwareTests::TestConnectCreatesEndpointAndDisconnectRemovesIt()
{
    SKIP_IF_NO_BLUETOOTH_TRANSPORT();
    SKIP_IF_RADIO_CANNOT_CONNECT();
    REQUIRE_DEVICE_ID(deviceId, PresentDeviceIdParameter);

    RememberStateOf(deviceId);
    StartDisconnected(deviceId);

    auto const requested = std::chrono::steady_clock::now();
    auto const device = ConnectAndWait(deviceId, deviceId, ConnectWait);

    Log::Comment(String().Format(L"Connected %lld ms after it was asked for", ElapsedMilliseconds(requested)));

    VERIFY_IS_FALSE(device.EndpointDeviceId().empty(), L"the endpoint has an id");
    VERIFY_IS_TRUE(device.SelectedProtocol() != MidiBluetoothProtocol::Unknown, L"the protocol in use is known");
    VERIFY_IS_TRUE(device.LastConnectErrorCode() == MidiBluetoothDeviceConnectErrorCode::Success, L"and no error is left over from an earlier attempt");

    auto const endpoint = MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(device.EndpointDeviceId());

    VERIFY_IS_TRUE(endpoint != nullptr, L"the endpoint can be found by enumeration");
    VERIFY_IS_TRUE(endpoint.GetTransportSuppliedInfo().TransportId() == MidiBluetoothTransportManager::TransportId(), L"and belongs to the Bluetooth transport");

    Log::Comment(String().Format(L"Endpoint '%s' %s", endpoint.Name().c_str(), endpoint.EndpointDeviceId().c_str()));

    DisconnectAndWait(deviceId, deviceId);
    VerifyStaysDisconnected(deviceId);
}

void MidiBluetoothHardwareTests::TestDeviceIdIsAcceptedInOtherForms()
{
    SKIP_IF_NO_BLUETOOTH_TRANSPORT();
    SKIP_IF_RADIO_CANNOT_CONNECT();
    REQUIRE_DEVICE_ID(deviceId, PresentDeviceIdParameter);

    auto const otherForm = LowercaseWithColons(deviceId);

    Log::Comment(String().Format(L"Using '%s' for %s", otherForm.c_str(), deviceId.c_str()));

    RememberStateOf(deviceId);
    StartDisconnected(deviceId);

    MidiBluetoothDeviceInformation lookedUp{ nullptr };

    VERIFY_IS_TRUE(WaitForDevice(otherForm, DiscoveryWait, IsListedAndPresent, lookedUp), L"the device can be looked up");
    VERIFY_IS_TRUE(std::wstring{ lookedUp.BluetoothDeviceId() } == deviceId, L"and is reported in the usual form");

    // Every list the transport keeps uses the usual form, so a request in another one has to be
    // converted, or it is accepted and then matches nothing
    ConnectAndWait(deviceId, otherForm, ConnectWait);
    DisconnectAndWait(deviceId, otherForm);
    VerifyStaysDisconnected(deviceId);
}

void MidiBluetoothHardwareTests::TestSettingOfflineRetentionDoesNotConnect()
{
    SKIP_IF_NO_BLUETOOTH_TRANSPORT();
    SKIP_IF_RADIO_CANNOT_CONNECT();
    REQUIRE_DEVICE_ID(deviceId, PresentDeviceIdParameter);

    RememberStateOf(deviceId);
    StartDisconnected(deviceId);

    MidiBluetoothDeviceInformation device{ nullptr };

    VERIFY_IS_TRUE(WaitForDevice(deviceId, DiscoveryWait, IsListedAndPresent, device), L"the device is switched on and advertising");

    auto const previousRetention = device.OfflineRetentionSeconds();
    auto const retention = previousRetention == 30 ? 31 : 30;

    // Sent but never saved, so sending the old value back restores everything
    auto restoreRetention = wil::scope_exit([&deviceId, previousRetention]
        {
            try
            {
                MidiServiceTransportPluginConfigManager::SendUpdate(MidiBluetoothOfflineRetentionConfig(deviceId, previousRetention));
            }
            catch (...)
            {
            }
        });

    auto const response = MidiServiceTransportPluginConfigManager::SendUpdate(MidiBluetoothOfflineRetentionConfig(deviceId, retention));

    VERIFY_IS_TRUE(response != nullptr && response.Status() == MidiServiceConfigResponseStatus::Success, L"the setting is applied");

    MidiBluetoothDeviceInformation after{ nullptr };

    VERIFY_IS_TRUE(WaitForDevice(deviceId, CountersWait,
        [retention](MidiBluetoothDeviceInformation const& candidate)
        {
            return candidate != nullptr && candidate.OfflineRetentionSeconds() == retention;
        },
        after), L"the device reports the new setting");

    VerifyStaysDisconnected(deviceId);
}

void MidiBluetoothHardwareTests::TestSentMessagesAreWrittenToTheDevice()
{
    SKIP_IF_NO_BLUETOOTH_TRANSPORT();
    SKIP_IF_RADIO_CANNOT_CONNECT();
    REQUIRE_DEVICE_ID(deviceId, PresentDeviceIdParameter);

    RememberStateOf(deviceId);

    auto const device = EnsureConnected(deviceId);

    auto session = MidiSession::Create(L"Bluetooth hardware tests");

    VERIFY_IS_TRUE(session != nullptr);

    auto closeSession = wil::scope_exit([&session]
        {
            try
            {
                session.Close();
            }
            catch (...)
            {
            }
        });

    auto connection = session.CreateEndpointConnection(device.EndpointDeviceId());

    VERIFY_IS_TRUE(connection != nullptr);
    VERIFY_IS_TRUE(connection.Open(), L"the endpoint opens");

    auto const before = MidiBluetoothTransportManager::GetDevice(deviceId);

    VERIFY_IS_TRUE(before != nullptr);

    for (uint32_t i = 0; i < MessagesToSend; i++)
    {
        VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(
            connection.SendSingleMessageWords(MidiClock::TimestampConstantSendImmediately(), HarmlessMessage)),
            L"the message is accepted for sending");
    }

    // A write to a Bluetooth MIDI device is not acknowledged, so the counts are as far as this can see
    MidiBluetoothDeviceInformation after{ nullptr };

    auto const counted = WaitForDevice(deviceId, CountersWait,
        [&before](MidiBluetoothDeviceInformation const& candidate)
        {
            return candidate != nullptr &&
                candidate.MessagesSent() >= before.MessagesSent() + MessagesToSend &&
                candidate.PacketsSent() > before.PacketsSent();
        },
        after);

    LogDevice(L"After sending", after);

    VERIFY_IS_TRUE(counted, L"every message was written to the device");
    VERIFY_IS_TRUE(
        after.LastSendErrorHResult() == 0 || after.LastSendErrorHResult() == before.LastSendErrorHResult(),
        L"with no new send error");
}

void MidiBluetoothHardwareTests::TestMessagesFromTheDeviceArrive()
{
    SKIP_IF_NO_BLUETOOTH_TRANSPORT();
    SKIP_IF_RADIO_CANNOT_CONNECT();
    REQUIRE_DEVICE_ID(deviceId, PresentDeviceIdParameter);

    if (!IsInteractive())
    {
        Log::Result(TestResults::Skipped, String().Format(L"Someone has to play the device. Run with /p:%s=true.", InteractiveParameter));
        return;
    }

    RememberStateOf(deviceId);

    auto const device = EnsureConnected(deviceId);

    auto session = MidiSession::Create(L"Bluetooth hardware tests");

    VERIFY_IS_TRUE(session != nullptr);

    auto closeSession = wil::scope_exit([&session]
        {
            try
            {
                session.Close();
            }
            catch (...)
            {
            }
        });

    auto connection = session.CreateEndpointConnection(device.EndpointDeviceId());

    VERIFY_IS_TRUE(connection != nullptr);

    wil::unique_event_nothrow messageReceived;
    messageReceived.create();

    std::atomic<uint32_t> receivedCount{ 0 };
    std::atomic<uint32_t> firstWord{ 0 };

    auto const token = connection.MessageReceived([&](auto&&, MidiMessageReceivedEventArgs const& args)
        {
            if (receivedCount++ == 0)
            {
                firstWord = args.PeekFirstWord();
            }

            messageReceived.SetEvent();
        });

    auto removeHandler = wil::scope_exit([&connection, token]
        {
            try
            {
                connection.MessageReceived(token);
            }
            catch (...)
            {
            }
        });

    VERIFY_IS_TRUE(connection.Open(), L"the endpoint opens");

    auto const before = MidiBluetoothTransportManager::GetDevice(deviceId);

    VERIFY_IS_TRUE(before != nullptr);

    // Written to the console as well, so the prompt shows at any log level
    auto const prompt = std::wstring{ L">>> Play a note or move a control on '" } + std::wstring{ device.Name() } + L"' now.";

    Log::Comment(prompt.c_str());
    std::wcout << prompt << std::endl;

    VERIFY_IS_TRUE(messageReceived.wait(static_cast<DWORD>(PlayWait.count())), L"a message arrives from the device");

    Log::Comment(String().Format(L"First message 0x%08X", firstWord.load()));

    MidiBluetoothDeviceInformation after{ nullptr };

    auto const counted = WaitForDevice(deviceId, CountersWait,
        [&before](MidiBluetoothDeviceInformation const& candidate)
        {
            return candidate != nullptr &&
                candidate.MessagesReceived() > before.MessagesReceived() &&
                candidate.PacketsReceived() > before.PacketsReceived();
        },
        after);

    LogDevice(L"After playing", after);

    VERIFY_IS_TRUE(counted, L"and the transport counted it");
}

void MidiBluetoothHardwareTests::TestAbsentDeviceDoesNotHoldUpAnother()
{
    SKIP_IF_NO_BLUETOOTH_TRANSPORT();
    SKIP_IF_RADIO_CANNOT_CONNECT();
    REQUIRE_DEVICE_ID(deviceId, PresentDeviceIdParameter);
    REQUIRE_DEVICE_ID(absentDeviceId, AbsentDeviceIdParameter);

    VERIFY_IS_TRUE(deviceId != absentDeviceId, L"the two parameters name different devices");

    auto const absent = MidiBluetoothTransportManager::GetDevice(absentDeviceId);

    LogDevice(L"Absent device", absent);

    if (absent == nullptr || absent.IsPresent())
    {
        Log::Result(TestResults::Blocked, String().Format(
            L"/p:%s must name a paired device which is switched off. Windows lists a paired device even when it cannot hear it.",
            AbsentDeviceIdParameter));
        return;
    }

    RememberStateOf(deviceId);
    RememberStateOf(absentDeviceId);

    // Neither is wanted to begin with, so the absent device's attempt starts at once rather than
    // after a retry gap
    StartDisconnected(absentDeviceId);
    StartDisconnected(deviceId);

    MidiBluetoothDeviceInformation present{ nullptr };

    VERIFY_IS_TRUE(WaitForDevice(deviceId, DiscoveryWait, IsListedAndPresent, present), L"the device is switched on and advertising");

    // Asked for first, so an attempt at the absent device is already running when the other
    // request arrives
    auto const absentRequested = std::chrono::steady_clock::now();
    auto const absentResponse = MidiBluetoothTransportManager::ConnectDeviceAsync(MidiBluetoothDeviceConnectConfig(absentDeviceId)).get();

    VERIFY_IS_TRUE(absentResponse != nullptr);
    LogConnectResponse(absentResponse);
    VERIFY_IS_TRUE(absentResponse.Success(), L"the absent device is asked for");

    std::this_thread::sleep_for(AttemptStartDelay);

    auto const requested = std::chrono::steady_clock::now();
    auto const response = MidiBluetoothTransportManager::ConnectDeviceAsync(MidiBluetoothDeviceConnectConfig(deviceId)).get();

    VERIFY_IS_TRUE(response != nullptr);
    LogConnectResponse(response);
    VERIFY_IS_TRUE(response.Success(), L"and then the device which is switched on");

    std::optional<long long> absentAttemptEnded{};
    std::optional<long long> connectedAfter{};

    while (!connectedAfter.has_value() && ElapsedMilliseconds(requested) < ConnectWait.count())
    {
        if (!absentAttemptEnded.has_value())
        {
            auto const absentNow = MidiBluetoothTransportManager::GetDevice(absentDeviceId);

            if (absentNow != nullptr && absentNow.ConnectionState() != MidiBluetoothConnectionState::Connecting)
            {
                absentAttemptEnded = ElapsedMilliseconds(absentRequested);
            }
        }

        if (IsConnectedWithEndpoint(MidiBluetoothTransportManager::GetDevice(deviceId)))
        {
            connectedAfter = ElapsedMilliseconds(requested);
            break;
        }

        std::this_thread::sleep_for(PollInterval);
    }

    LogDevice(L"Absent device afterwards", MidiBluetoothTransportManager::GetDevice(absentDeviceId));
    LogDevice(L"Device afterwards", MidiBluetoothTransportManager::GetDevice(deviceId));

    if (absentAttemptEnded.has_value())
    {
        Log::Comment(String().Format(L"The attempt at the absent device ended within %lld ms", *absentAttemptEnded));
    }
    else
    {
        Log::Comment(L"The attempt at the absent device had not ended when the other device connected");
    }

    VERIFY_IS_TRUE(connectedAfter.has_value(), L"the device connects while an absent device is being tried");

    Log::Comment(String().Format(L"Connected %lld ms after it was asked for", *connectedAfter));

    // At worst it waits out the attempt already running, then makes its own
    VERIFY_IS_TRUE(*connectedAfter <= 2 * ConnectAttemptLimit.count(), L"no later than one attempt at the absent device plus its own");
}
