// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace NetworkMidiTest;

namespace json = winrt::Windows::Data::Json;

namespace
{
    // Bounds and defaults, kept here rather than included so that a change to the transport's
    // own header has to be made deliberately in both places.
    constexpr uint32_t FecDefault{ 2 };
    constexpr uint32_t FecUpper{ 10 };
    constexpr uint32_t RetransmitDefault{ 50 };
    constexpr uint32_t RetransmitUpper{ 1000 };
    constexpr uint32_t PingDefault{ 2000 };
    constexpr uint32_t PingLower{ 250 };
    constexpr uint32_t PingUpper{ 120000 };
    constexpr uint32_t InvitationDefault{ 120000 };
    constexpr uint32_t InvitationLower{ 1000 };
    constexpr uint32_t InvitationUpper{ 600000 };
    constexpr uint32_t MaxConnectionsDefault{ 64 };
    constexpr uint32_t MaxConnectionsLower{ 1 };
    constexpr uint32_t MaxConnectionsUpper{ 512 };
    constexpr uint32_t ScanDefault{ 20000 };
    constexpr uint32_t ScanLower{ 250 };
    constexpr uint32_t ScanUpper{ 300000 };

    // What the service was running with before this class touched anything. These settings are
    // machine-wide and survive until the service restarts, so leaving a test value behind would
    // change the behavior of everything else on the machine.
    std::wstring g_originalSettingsJson{ };
    bool g_haveOriginalSettings{ false };

    std::optional<json::JsonObject> ReadSettings()
    {
        auto const result = GetTransportSettings();

        if (!result.IsSuccess())
        {
            return std::nullopt;
        }

        json::JsonObject parsed{ nullptr };

        if (!json::JsonObject::TryParse(winrt::hstring{ result.ResponseJson }, parsed))
        {
            return std::nullopt;
        }

        if (!parsed.HasKey(L"transportSettings"))
        {
            return std::nullopt;
        }

        return parsed.GetNamedObject(L"transportSettings");
    }

    // Sends the body and returns what the transport ended up with, so a test reads one value
    // rather than repeating the send-then-read pair.
    std::optional<json::JsonObject> ApplyAndRead(_In_ std::wstring const& settingsBodyJson)
    {
        auto const applied = SetRawTransportSettings(settingsBodyJson);

        if (!applied.IsSuccess())
        {
            Log::Error(String().Format(L"Transport settings were refused: %s", applied.ResponseJson.c_str()));
            return std::nullopt;
        }

        return ReadSettings();
    }

    uint32_t ValueOf(_In_ json::JsonObject const& settings, _In_ std::wstring const& name)
    {
        return static_cast<uint32_t>(settings.GetNamedNumber(winrt::hstring{ name }, -1.0));
    }

    // Every setting at its default, as a transportSettings body.
    std::wstring DefaultsBody()
    {
        return
            L"\"maxForwardErrorCorrectionCommandPackets\":" + std::to_wstring(FecDefault) + L"," +
            L"\"maxRetransmitBufferCommandPackets\":" + std::to_wstring(RetransmitDefault) + L"," +
            L"\"outboundPingInterval\":" + std::to_wstring(PingDefault) + L"," +
            L"\"invitationPendingTimeout\":" + std::to_wstring(InvitationDefault) + L"," +
            L"\"maxHostConnections\":" + std::to_wstring(MaxConnectionsDefault) + L"," +
            L"\"directConnectionScanInterval\":" + std::to_wstring(ScanDefault);
    }
}


bool NetworkMidiTransportSettingsTests::ClassSetup()
{
    if (!IsServiceAvailable())
    {
        Log::Result(TestResults::Skipped, L"The MIDI service is not available.");
        return false;
    }

    if (auto const settings = ReadSettings())
    {
        g_originalSettingsJson = settings->Stringify();
        g_haveOriginalSettings = true;
    }
    else
    {
        Log::Comment(L"The transport did not report its settings. It is probably an older build.");
    }

    return true;
}


bool NetworkMidiTransportSettingsTests::ClassCleanup()
{
    // Safety net. A test which fails an assertion unwinds through its own scope_exit, but one
    // which is skipped or blocked does not run at all, and a leaked host keeps advertising.
    for (auto const& entry : {
        L"{6f9619ff-8b86-d011-b42d-00cf4fc964ff}",
        L"{6f9619ff-8b86-d011-b42d-00cf4fc964fe}",
        L"{6f9619ff-8b86-d011-b42d-00cf4fc964fd}",
        L"{6f9619ff-8b86-d011-b42d-00cf4fc964fc}",
        L"{6f9619ff-8b86-d011-b42d-00cf4fc964fb}" })
    {
        StopHost(entry);
        RemoveHost(entry);
        DisconnectClient(entry);
    }

    if (g_haveOriginalSettings)
    {
        // Stringify wraps the body in braces, which SetRawTransportSettings adds itself
        auto body = g_originalSettingsJson;

        if (body.length() >= 2 && body.front() == L'{' && body.back() == L'}')
        {
            body = body.substr(1, body.length() - 2);
        }

        SetRawTransportSettings(body);
    }

    return true;
}


// A malformed host entry which still carries a usable name is accepted and started, so these
// tests do create endpoints, and the service deactivates rather than deletes one. These two
// hooks remove exactly the nodes this test method caused to be created.
bool NetworkMidiTransportSettingsTests::TestSetup()
{
    m_deviceNodeTracker.Start();

    return true;
}

bool NetworkMidiTransportSettingsTests::TestCleanup()
{
    m_deviceNodeTracker.RemoveDeviceNodesCreatedSinceStart();

    return true;
}


void NetworkMidiTransportSettingsTests::SettingsAreReportedBack()
{
    auto const settings = ApplyAndRead(DefaultsBody());

    VERIFY_IS_TRUE(settings.has_value(), L"The transport reports its settings");

    VERIFY_ARE_EQUAL(FecDefault, ValueOf(*settings, L"maxForwardErrorCorrectionCommandPackets"));
    VERIFY_ARE_EQUAL(RetransmitDefault, ValueOf(*settings, L"maxRetransmitBufferCommandPackets"));
    VERIFY_ARE_EQUAL(PingDefault, ValueOf(*settings, L"outboundPingInterval"));
    VERIFY_ARE_EQUAL(InvitationDefault, ValueOf(*settings, L"invitationPendingTimeout"));
    VERIFY_ARE_EQUAL(MaxConnectionsDefault, ValueOf(*settings, L"maxHostConnections"));
    VERIFY_ARE_EQUAL(ScanDefault, ValueOf(*settings, L"directConnectionScanInterval"));
}


void NetworkMidiTransportSettingsTests::ValuesAboveTheRangeAreClampedToTheMaximum()
{
    auto const settings = ApplyAndRead(
        L"\"maxForwardErrorCorrectionCommandPackets\":9999,"
        L"\"maxRetransmitBufferCommandPackets\":999999,"
        L"\"outboundPingInterval\":99999999,"
        L"\"invitationPendingTimeout\":99999999,"
        L"\"maxHostConnections\":99999,"
        L"\"directConnectionScanInterval\":99999999");

    VERIFY_IS_TRUE(settings.has_value(), L"Out of range settings are accepted rather than refused");

    VERIFY_ARE_EQUAL(FecUpper, ValueOf(*settings, L"maxForwardErrorCorrectionCommandPackets"));
    VERIFY_ARE_EQUAL(RetransmitUpper, ValueOf(*settings, L"maxRetransmitBufferCommandPackets"));
    VERIFY_ARE_EQUAL(PingUpper, ValueOf(*settings, L"outboundPingInterval"));
    VERIFY_ARE_EQUAL(InvitationUpper, ValueOf(*settings, L"invitationPendingTimeout"));
    VERIFY_ARE_EQUAL(MaxConnectionsUpper, ValueOf(*settings, L"maxHostConnections"));
    VERIFY_ARE_EQUAL(ScanUpper, ValueOf(*settings, L"directConnectionScanInterval"));
}


void NetworkMidiTransportSettingsTests::ValuesBelowTheRangeAreClampedToTheMinimum()
{
    // Negative rather than merely small, because a negative number reaching an unsigned field
    // without being caught first would come out enormous instead of clamped.
    auto const settings = ApplyAndRead(
        L"\"maxForwardErrorCorrectionCommandPackets\":-5,"
        L"\"maxRetransmitBufferCommandPackets\":-5,"
        L"\"outboundPingInterval\":-5,"
        L"\"invitationPendingTimeout\":0,"
        L"\"maxHostConnections\":0,"
        L"\"directConnectionScanInterval\":1");

    VERIFY_IS_TRUE(settings.has_value(), L"Below range settings are accepted rather than refused");

    // Zero is the bottom of the range for both of these, so a negative lands on zero
    VERIFY_ARE_EQUAL(0u, ValueOf(*settings, L"maxForwardErrorCorrectionCommandPackets"));
    VERIFY_ARE_EQUAL(0u, ValueOf(*settings, L"maxRetransmitBufferCommandPackets"));
    VERIFY_ARE_EQUAL(PingLower, ValueOf(*settings, L"outboundPingInterval"));
    VERIFY_ARE_EQUAL(InvitationLower, ValueOf(*settings, L"invitationPendingTimeout"));
    VERIFY_ARE_EQUAL(MaxConnectionsLower, ValueOf(*settings, L"maxHostConnections"));
    VERIFY_ARE_EQUAL(ScanLower, ValueOf(*settings, L"directConnectionScanInterval"));
}


void NetworkMidiTransportSettingsTests::WrongTypesFallBackToTheDefault()
{
    // A string, a bool, null, an object and an array. GetNamedNumber throws on every one of
    // these, so an unguarded read would take the whole configuration update down with it.
    auto const settings = ApplyAndRead(
        L"\"maxForwardErrorCorrectionCommandPackets\":\"three\","
        L"\"maxRetransmitBufferCommandPackets\":true,"
        L"\"outboundPingInterval\":null,"
        L"\"invitationPendingTimeout\":{\"nested\":1},"
        L"\"maxHostConnections\":[1,2,3],"
        L"\"directConnectionScanInterval\":\"20000\"");

    VERIFY_IS_TRUE(settings.has_value(), L"Wrongly typed settings do not fail the update");

    VERIFY_ARE_EQUAL(FecDefault, ValueOf(*settings, L"maxForwardErrorCorrectionCommandPackets"));
    VERIFY_ARE_EQUAL(RetransmitDefault, ValueOf(*settings, L"maxRetransmitBufferCommandPackets"));
    VERIFY_ARE_EQUAL(PingDefault, ValueOf(*settings, L"outboundPingInterval"));
    VERIFY_ARE_EQUAL(InvitationDefault, ValueOf(*settings, L"invitationPendingTimeout"));
    VERIFY_ARE_EQUAL(MaxConnectionsDefault, ValueOf(*settings, L"maxHostConnections"));

    // A quoted number is still a string, and is not quietly converted
    VERIFY_ARE_EQUAL(ScanDefault, ValueOf(*settings, L"directConnectionScanInterval"));
}


void NetworkMidiTransportSettingsTests::MalformedNumbersFallBackToTheDefault()
{
    // A fraction truncates rather than falling back, because it is still a usable number.
    // 1e18 is a valid JSON number but far beyond what the field holds, so it has to be clamped
    // on the way in rather than wrapped by the conversion.
    auto const settings = ApplyAndRead(
        L"\"maxHostConnections\":12.7,"
        L"\"outboundPingInterval\":1e18");

    VERIFY_IS_TRUE(settings.has_value(), L"Odd numbers do not fail the update");

    VERIFY_ARE_EQUAL(12u, ValueOf(*settings, L"maxHostConnections"));
    VERIFY_ARE_EQUAL(PingUpper, ValueOf(*settings, L"outboundPingInterval"));

    // 1e400 overflows a double, so the JSON parser rejects the document before any of this is
    // reached. Refusing it is correct: nothing can be salvaged from a payload which will not
    // parse, and the previous settings are left as they were.
    auto const unparseable = SetRawTransportSettings(L"\"outboundPingInterval\":1e400");

    VERIFY_IS_FALSE(unparseable.IsSuccess(), L"A number a double cannot hold is refused as malformed JSON");

    auto const unchanged = ReadSettings();

    VERIFY_IS_TRUE(unchanged.has_value(), L"Settings are still readable after a malformed payload");
    VERIFY_ARE_EQUAL(PingUpper, ValueOf(*unchanged, L"outboundPingInterval"));
}


void NetworkMidiTransportSettingsTests::AbsentSettingsAreLeftAlone()
{
    VERIFY_IS_TRUE(ApplyAndRead(DefaultsBody()).has_value(), L"Settings start from the defaults");

    auto const settings = ApplyAndRead(L"\"maxHostConnections\":99");

    VERIFY_IS_TRUE(settings.has_value(), L"A partial settings object is accepted");

    VERIFY_ARE_EQUAL(99u, ValueOf(*settings, L"maxHostConnections"));

    // Naming one setting must not reset the others to their defaults, which is what a caller
    // sending a single field from a settings page would otherwise do.
    VERIFY_ARE_EQUAL(FecDefault, ValueOf(*settings, L"maxForwardErrorCorrectionCommandPackets"));
    VERIFY_ARE_EQUAL(PingDefault, ValueOf(*settings, L"outboundPingInterval"));
    VERIFY_ARE_EQUAL(ScanDefault, ValueOf(*settings, L"directConnectionScanInterval"));
}


void NetworkMidiTransportSettingsTests::ProductInstanceIdIsNoLongerATransportSetting()
{
    // It identifies this PC to every remote which has already stored it, so it is derived from
    // the machine rather than configurable. Sending one is harmless and simply ignored.
    auto const settings = ApplyAndRead(L"\"productInstanceId\":\"should-be-ignored\"");

    VERIFY_IS_TRUE(settings.has_value(), L"An unrecognized setting does not fail the update");

    VERIFY_IS_FALSE(
        settings->HasKey(L"productInstanceId"),
        L"productInstanceId is not reported as a transport setting");
}


// The transport runs inside the service on a COM boundary. Every assertion below is really the
// same one: the call returns rather than throwing, and the service is still answering afterwards.
namespace
{
    void VerifyServiceStillResponds()
    {
        VERIFY_IS_TRUE(ReadSettings().has_value(), L"The service still answers after the malformed payload");
    }

    // A malformed entry which still carries a usable name is ACCEPTED, so these tests create
    // live hosts and clients in the running service. Left behind they keep advertising, hold
    // sockets, and are picked up by the background scanner, which is not this suite's business
    // and is not the next suite's problem either.
    void ForgetEntry(_In_ std::wstring const& entryIdentifier)
    {
        StopHost(entryIdentifier);
        RemoveHost(entryIdentifier);
        DisconnectClient(entryIdentifier);
    }

    // The scan interval governs how aggressively the background creator retries these entries.
    // A previous test in this class drives it to its floor, so anything creating an entry puts
    // it back first rather than inheriting a 250 ms scan.
    void RestoreDefaultScanInterval()
    {
        SetRawTransportSettings(L"\"directConnectionScanInterval\":" + std::to_wstring(ScanDefault));
    }
}


void NetworkMidiTransportSettingsTests::WrongTypesInAHostEntryDoNotThrow()
{
    RestoreDefaultScanInterval();

    std::wstring const entry{ L"{6f9619ff-8b86-d011-b42d-00cf4fc964ff}" };
    auto cleanup = wil::scope_exit([&] { ForgetEntry(entry); });

    // Every field the host parse reads, each holding a type it is not expecting. Before the
    // accessors were made type-safe, the first of these threw out of UpdateConfiguration.
    auto const result = SendRawCreateSection(
        L"\"hosts\":{\"" + entry + L"\":{"
        L"\"networkProtocol\":42,"
        L"\"enabled\":\"yes\","
        L"\"advertise\":\"no\","
        L"\"createMidi1Ports\":\"true\","
        L"\"name\":123,"
        L"\"productInstanceId\":false,"
        L"\"port\":9004,"
        L"\"allowPortFallback\":\"maybe\","
        L"\"authentication\":7,"
        L"\"remoteClientPolicy\":[\"requireApproval\"],"
        L"\"serviceInstanceName\":{\"nested\":1},"
        L"\"customEndpointName\":null"
        L"}}");

    VERIFY_IS_TRUE(result.CallSucceeded, L"The call returned rather than throwing");

    VerifyServiceStillResponds();
}


void NetworkMidiTransportSettingsTests::WrongTypesInAClientEntryDoNotThrow()
{
    RestoreDefaultScanInterval();

    std::wstring const entry{ L"{6f9619ff-8b86-d011-b42d-00cf4fc964fe}" };
    auto cleanup = wil::scope_exit([&] { ForgetEntry(entry); });

    auto const result = SendRawCreateSection(
        L"\"clients\":{\"" + entry + L"\":{"
        L"\"networkProtocol\":true,"
        L"\"enabled\":\"yes\","
        L"\"customEndpointName\":99,"
        L"\"match\":{"
        L"\"id\":12345,"
        L"\"directHostNameOrIP\":[\"127.0.0.1\"],"
        L"\"directPort\":{\"p\":1},"
        L"\"umpProductInstanceId\":false,"
        L"\"umpEndpointName\":null"
        L"}}}");

    VERIFY_IS_TRUE(result.CallSucceeded, L"The call returned rather than throwing");

    VerifyServiceStillResponds();
}


void NetworkMidiTransportSettingsTests::WrongTypesInAnAllowedClientsArrayDoNotThrow()
{
    RestoreDefaultScanInterval();

    // Both of these carry a usable name, so the host is accepted and started rather than
    // refused. They have to be removed again or they keep advertising.
    std::wstring const wrongTypeEntry{ L"{6f9619ff-8b86-d011-b42d-00cf4fc964fd}" };
    std::wstring const badElementsEntry{ L"{6f9619ff-8b86-d011-b42d-00cf4fc964fc}" };

    auto cleanup = wil::scope_exit([&]
        {
            ForgetEntry(wrongTypeEntry);
            ForgetEntry(badElementsEntry);
        });

    // The array itself is the wrong type in one case, and holds non-object elements in the
    // other. Both used to reach GetNamedArray / GetObjectAt unguarded.
    auto const wrongArrayType = SendRawCreateSection(
        L"\"hosts\":{\"" + wrongTypeEntry + L"\":{"
        L"\"name\":\"WrongArrayType\",\"allowedClients\":\"not-an-array\"}}");

    VERIFY_IS_TRUE(wrongArrayType.CallSucceeded, L"A non-array allowedClients returns rather than throwing");

    auto const badElements = SendRawCreateSection(
        L"\"hosts\":{\"" + badElementsEntry + L"\":{"
        L"\"name\":\"BadElements\",\"allowedClients\":[1,\"two\",null,{\"umpEndpointName\":5}]}}");

    VERIFY_IS_TRUE(badElements.CallSucceeded, L"Non-object array elements are skipped rather than throwing");

    VerifyServiceStillResponds();
}


void NetworkMidiTransportSettingsTests::OverlongServiceInstanceNameIsRejected()
{
    RestoreDefaultScanInterval();

    std::wstring const entry{ L"{6f9619ff-8b86-d011-b42d-00cf4fc964fb}" };
    auto cleanup = wil::scope_exit([&] { ForgetEntry(entry); });

    // A DNS label is 63 bytes. The SDK truncates on the way in, but a hand-written
    // configuration file reaches the transport without passing through it.
    std::wstring const tooLong(120, L'a');

    auto const result = SendRawCreateSection(
        L"\"hosts\":{\"" + entry + L"\":{"
        L"\"name\":\"Overlong Label Test\","
        L"\"serviceInstanceName\":\"" + tooLong + L"\"}}");

    VERIFY_IS_TRUE(result.CallSucceeded, L"The call returned rather than throwing");

    VERIFY_IS_FALSE(result.ReportedSuccess, L"An over-long service instance name is refused");

    VerifyServiceStillResponds();
}
