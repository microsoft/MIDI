// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "BleMidiValidationTests.h"

#include <set>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace json = ::winrt::Windows::Data::Json;

namespace
{
    constexpr wchar_t TestKey[] = L"value";

    // Builds { "value": <literal> } so a wrong type can be handed to the accessors the same way
    // a hand-edited configuration file would.
    json::JsonObject ObjectWithValue(_In_ std::wstring const& jsonLiteral)
    {
        json::JsonObject result{ nullptr };

        VERIFY_IS_TRUE(json::JsonObject::TryParse(
            winrt::hstring{ L"{\"value\":" + jsonLiteral + L"}" }, result));

        return result;
    }

    // every json type except the one under test
    std::vector<std::wstring> WrongTypesExcept(_In_ std::wstring const& correct)
    {
        std::vector<std::wstring> all{ L"\"text\"", L"42", L"true", L"[1,2]", L"{\"a\":1}" };

        std::vector<std::wstring> result;

        for (auto const& entry : all)
        {
            if (entry != correct)
            {
                result.push_back(entry);
            }
        }

        return result;
    }
}


bool BleMidiValidationTests::ClassSetup()
{
    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    return true;
}

bool BleMidiValidationTests::ClassCleanup()
{
    return true;
}


void BleMidiValidationTests::TestSafeGetObjectRejectsEveryWrongType()
{
    for (auto const& literal : WrongTypesExcept(L"{\"a\":1}"))
    {
        auto const parent = ObjectWithValue(literal);

        json::JsonObject value{ nullptr };

        VERIFY_IS_FALSE(MidiBleProtocol::SafeJson::TryGetObject(parent, TestKey, value));
        VERIFY_IS_TRUE(value == nullptr);
    }
}

void BleMidiValidationTests::TestSafeGetObjectAcceptsAnObject()
{
    auto const parent = ObjectWithValue(L"{\"a\":1}");

    json::JsonObject value{ nullptr };

    VERIFY_IS_TRUE(MidiBleProtocol::SafeJson::TryGetObject(parent, TestKey, value));
    VERIFY_IS_TRUE(value != nullptr);
}

void BleMidiValidationTests::TestSafeGetArrayRejectsEveryWrongType()
{
    for (auto const& literal : WrongTypesExcept(L"[1,2]"))
    {
        auto const parent = ObjectWithValue(literal);

        json::JsonArray value{ nullptr };

        VERIFY_IS_FALSE(MidiBleProtocol::SafeJson::TryGetArray(parent, TestKey, value));
        VERIFY_IS_TRUE(value == nullptr);
    }
}

void BleMidiValidationTests::TestSafeGetArrayAcceptsAnArray()
{
    auto const parent = ObjectWithValue(L"[1,2]");

    json::JsonArray value{ nullptr };

    VERIFY_IS_TRUE(MidiBleProtocol::SafeJson::TryGetArray(parent, TestKey, value));
    VERIFY_IS_TRUE(value != nullptr);
    VERIFY_ARE_EQUAL(2u, value.Size());
}

void BleMidiValidationTests::TestSafeGetStringRejectsEveryWrongType()
{
    for (auto const& literal : WrongTypesExcept(L"\"text\""))
    {
        auto const parent = ObjectWithValue(literal);

        VERIFY_ARE_EQUAL(
            winrt::hstring{ L"fallback" },
            MidiBleProtocol::SafeJson::GetString(parent, TestKey, winrt::hstring{ L"fallback" }));
    }
}

void BleMidiValidationTests::TestSafeGetStringAcceptsAString()
{
    auto const parent = ObjectWithValue(L"\"text\"");

    VERIFY_ARE_EQUAL(
        winrt::hstring{ L"text" },
        MidiBleProtocol::SafeJson::GetString(parent, TestKey, winrt::hstring{ L"fallback" }));
}

void BleMidiValidationTests::TestSafeGetBooleanRejectsEveryWrongType()
{
    for (auto const& literal : WrongTypesExcept(L"true"))
    {
        auto const parent = ObjectWithValue(literal);

        // asserted both ways round so a hard-coded return could not pass
        VERIFY_IS_TRUE(MidiBleProtocol::SafeJson::GetBoolean(parent, TestKey, true));
        VERIFY_IS_FALSE(MidiBleProtocol::SafeJson::GetBoolean(parent, TestKey, false));
    }
}

void BleMidiValidationTests::TestSafeGetBooleanAcceptsABoolean()
{
    auto const parent = ObjectWithValue(L"true");

    VERIFY_IS_TRUE(MidiBleProtocol::SafeJson::GetBoolean(parent, TestKey, false));
}

void BleMidiValidationTests::TestSafeAccessorsToleratePresentButNullValues()
{
    auto const parent = ObjectWithValue(L"null");

    json::JsonObject object{ nullptr };
    json::JsonArray array{ nullptr };

    VERIFY_IS_FALSE(MidiBleProtocol::SafeJson::TryGetObject(parent, TestKey, object));
    VERIFY_IS_FALSE(MidiBleProtocol::SafeJson::TryGetArray(parent, TestKey, array));
    VERIFY_ARE_EQUAL(winrt::hstring{ L"d" }, MidiBleProtocol::SafeJson::GetString(parent, TestKey, winrt::hstring{ L"d" }));
    VERIFY_IS_TRUE(MidiBleProtocol::SafeJson::GetBoolean(parent, TestKey, true));
}

void BleMidiValidationTests::TestSafeAccessorsTolerateAMissingKey()
{
    json::JsonObject parent;

    json::JsonObject object{ nullptr };
    json::JsonArray array{ nullptr };

    VERIFY_IS_FALSE(MidiBleProtocol::SafeJson::TryGetObject(parent, TestKey, object));
    VERIFY_IS_FALSE(MidiBleProtocol::SafeJson::TryGetArray(parent, TestKey, array));
    VERIFY_ARE_EQUAL(winrt::hstring{ L"d" }, MidiBleProtocol::SafeJson::GetString(parent, TestKey, winrt::hstring{ L"d" }));
    VERIFY_IS_TRUE(MidiBleProtocol::SafeJson::GetBoolean(parent, TestKey, true));
}


void BleMidiValidationTests::TestWellFormedDeviceIdAcceptsATwelveDigitAddress()
{
    VERIFY_IS_TRUE(MidiBleUtilities::IsWellFormedBluetoothDeviceId(L"48B6201A719D"));
    VERIFY_IS_TRUE(MidiBleUtilities::IsWellFormedBluetoothDeviceId(L"48b6201a719d"));
    VERIFY_IS_TRUE(MidiBleUtilities::IsWellFormedBluetoothDeviceId(L"000000000000"));
    VERIFY_IS_TRUE(MidiBleUtilities::IsWellFormedBluetoothDeviceId(L"FFFFFFFFFFFF"));
}

void BleMidiValidationTests::TestWellFormedDeviceIdAcceptsSeparatedForms()
{
    VERIFY_IS_TRUE(MidiBleUtilities::IsWellFormedBluetoothDeviceId(L"48:B6:20:1A:71:9D"));
    VERIFY_IS_TRUE(MidiBleUtilities::IsWellFormedBluetoothDeviceId(L"48-B6-20-1A-71-9D"));
    VERIFY_IS_TRUE(MidiBleUtilities::IsWellFormedBluetoothDeviceId(L"48 B6 20 1A 71 9D"));
}

void BleMidiValidationTests::TestWellFormedDeviceIdRejectsWrongLengths()
{
    VERIFY_IS_FALSE(MidiBleUtilities::IsWellFormedBluetoothDeviceId(L"48B6201A719"));
    VERIFY_IS_FALSE(MidiBleUtilities::IsWellFormedBluetoothDeviceId(L"48B6201A719DD"));
    VERIFY_IS_FALSE(MidiBleUtilities::IsWellFormedBluetoothDeviceId(L"AB"));
}

void BleMidiValidationTests::TestWellFormedDeviceIdRejectsNonHex()
{
    VERIFY_IS_FALSE(MidiBleUtilities::IsWellFormedBluetoothDeviceId(L"ZZZZZZZZZZZZ"));
    VERIFY_IS_FALSE(MidiBleUtilities::IsWellFormedBluetoothDeviceId(L"48B6201A719G"));
    VERIFY_IS_FALSE(MidiBleUtilities::IsWellFormedBluetoothDeviceId(L"48B6201A719\u00d7"));
}

void BleMidiValidationTests::TestWellFormedDeviceIdRejectsEmptyAndSeparatorsOnly()
{
    VERIFY_IS_FALSE(MidiBleUtilities::IsWellFormedBluetoothDeviceId(L""));
    VERIFY_IS_FALSE(MidiBleUtilities::IsWellFormedBluetoothDeviceId(L"::::::"));
    VERIFY_IS_FALSE(MidiBleUtilities::IsWellFormedBluetoothDeviceId(L"            "));
}

void BleMidiValidationTests::TestWellFormedDeviceIdRejectsAVeryLongValue()
{
    // a configuration file is a file, so there is no length ceiling on what can appear in it
    VERIFY_IS_FALSE(MidiBleUtilities::IsWellFormedBluetoothDeviceId(std::wstring(5000, L'A')));
}


void BleMidiValidationTests::TestParseAddressAcceptsAFullAddress()
{
    uint64_t address{ 0 };

    VERIFY_IS_TRUE(MidiBleUtilities::TryParseBluetoothAddress(L"48B6201A719D", address));
    VERIFY_ARE_EQUAL(static_cast<uint64_t>(0x48B6201A719Dull), address);

    VERIFY_IS_TRUE(MidiBleUtilities::TryParseBluetoothAddress(L"48:b6:20:1a:71:9d", address));
    VERIFY_ARE_EQUAL(static_cast<uint64_t>(0x48B6201A719Dull), address);
}

void BleMidiValidationTests::TestParseAddressIsDeliberatelyLenientAboutLength()
{
    // This is why it is not a validator: a person can type a partial address and get a value
    // back. IsWellFormedBluetoothDeviceId is what guards anything which came out of a file.
    uint64_t address{ 0 };

    VERIFY_IS_TRUE(MidiBleUtilities::TryParseBluetoothAddress(L"AB", address));
    VERIFY_ARE_EQUAL(static_cast<uint64_t>(0xABull), address);

    VERIFY_IS_FALSE(MidiBleUtilities::IsWellFormedBluetoothDeviceId(L"AB"));
}

void BleMidiValidationTests::TestParseAddressRejectsNonHexAndOverlongValues()
{
    uint64_t address{ 0 };

    VERIFY_IS_FALSE(MidiBleUtilities::TryParseBluetoothAddress(L"ZZ", address));
    VERIFY_IS_FALSE(MidiBleUtilities::TryParseBluetoothAddress(L"48B6201A719DD", address));
    VERIFY_IS_FALSE(MidiBleUtilities::TryParseBluetoothAddress(L"", address));
}


void BleMidiValidationTests::TestFormatAddressAlwaysProducesTwelveUpperCaseDigits()
{
    VERIFY_ARE_EQUAL(winrt::hstring{ L"48B6201A719D" }, MidiBleUtilities::FormatBluetoothAddress(0x48B6201A719Dull));
    VERIFY_ARE_EQUAL(winrt::hstring{ L"000000000000" }, MidiBleUtilities::FormatBluetoothAddress(0));
    VERIFY_ARE_EQUAL(winrt::hstring{ L"0000000000AB" }, MidiBleUtilities::FormatBluetoothAddress(0xABull));
}

void BleMidiValidationTests::TestFormatAddressDiscardsBitsAboveFortyEight()
{
    // a Bluetooth address is 48 bits, so anything above that is not part of it
    VERIFY_ARE_EQUAL(
        winrt::hstring{ L"48B6201A719D" },
        MidiBleUtilities::FormatBluetoothAddress(0xFFFF48B6201A719Dull));
}

void BleMidiValidationTests::TestAddressRoundTripsThroughFormatAndParse()
{
    uint64_t const originals[] = { 0ull, 1ull, 0xABull, 0x48B6201A719Dull, 0xFFFFFFFFFFFFull };

    for (auto const original : originals)
    {
        auto const formatted = MidiBleUtilities::FormatBluetoothAddress(original);

        VERIFY_IS_TRUE(MidiBleUtilities::IsWellFormedBluetoothDeviceId(std::wstring{ formatted }));

        uint64_t parsed{ 0 };

        VERIFY_IS_TRUE(MidiBleUtilities::TryParseBluetoothAddress(std::wstring{ formatted }, parsed));
        VERIFY_ARE_EQUAL(original, parsed);
    }
}


void BleMidiValidationTests::TestProtocolStringsRoundTrip()
{
    VERIFY_ARE_EQUAL(
        winrt::hstring{ MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PROTOCOL_VALUE_MIDI1 },
        MidiBleUtilities::ProtocolToJsonString(MidiBleProtocol::Protocol::Midi1));

    VERIFY_ARE_EQUAL(
        winrt::hstring{ MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PROTOCOL_VALUE_MIDI2_UMP },
        MidiBleUtilities::ProtocolToJsonString(MidiBleProtocol::Protocol::Midi2Ump));

    VERIFY_ARE_EQUAL(
        winrt::hstring{ MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PROTOCOL_VALUE_UNKNOWN },
        MidiBleUtilities::ProtocolToJsonString(MidiBleProtocol::Protocol::Unknown));
}

void BleMidiValidationTests::TestNativeDataFormatStrings()
{
    VERIFY_ARE_EQUAL(
        winrt::hstring{ MIDI_CONFIG_JSON_BLUETOOTH_MIDI_NATIVE_DATA_FORMAT_VALUE_MIDI1 },
        MidiBleUtilities::NativeDataFormatToJsonString(MidiBleProtocol::NativeDataFormat::TimestampedMidi1ByteStream));

    VERIFY_ARE_EQUAL(
        winrt::hstring{ MIDI_CONFIG_JSON_BLUETOOTH_MIDI_NATIVE_DATA_FORMAT_VALUE_UMP },
        MidiBleUtilities::NativeDataFormatToJsonString(MidiBleProtocol::NativeDataFormat::UniversalMidiPacket));

    VERIFY_ARE_EQUAL(
        winrt::hstring{ MIDI_CONFIG_JSON_BLUETOOTH_MIDI_NATIVE_DATA_FORMAT_VALUE_UNKNOWN },
        MidiBleUtilities::NativeDataFormatToJsonString(MidiBleProtocol::NativeDataFormat::Unknown));
}

void BleMidiValidationTests::TestConnectionParameterPreferenceStringsRoundTrip()
{
    MidiBleProtocol::ConnectionParameterPreference const all[] =
    {
        MidiBleProtocol::ConnectionParameterPreference::SystemDefault,
        MidiBleProtocol::ConnectionParameterPreference::ThroughputOptimized,
        MidiBleProtocol::ConnectionParameterPreference::Balanced,
        MidiBleProtocol::ConnectionParameterPreference::PowerOptimized,
    };

    for (auto const preference : all)
    {
        auto const text = MidiBleUtilities::ConnectionParameterPreferenceToJsonString(preference);

        // the fallback is deliberately the wrong answer, so a round trip which silently fell
        // through to it would fail here
        auto const parsed = MidiBleUtilities::ConnectionParameterPreferenceFromJsonString(
            text,
            MidiBleProtocol::ConnectionParameterPreference::Balanced);

        VERIFY_ARE_EQUAL(static_cast<uint8_t>(preference), static_cast<uint8_t>(parsed));
    }
}

void BleMidiValidationTests::TestConnectionParameterPreferenceFallsBackOnUnknownValue()
{
    auto const parsed = MidiBleUtilities::ConnectionParameterPreferenceFromJsonString(
        winrt::hstring{ L"somethingElse" },
        MidiBleProtocol::ConnectionParameterPreference::PowerOptimized);

    VERIFY_ARE_EQUAL(
        static_cast<uint8_t>(MidiBleProtocol::ConnectionParameterPreference::PowerOptimized),
        static_cast<uint8_t>(parsed));
}

void BleMidiValidationTests::TestGenericDeviceNameIsMatchedWholeAndCaseInsensitively()
{
    VERIFY_IS_TRUE(MidiBleUtilities::IsGenericDeviceName(winrt::hstring{ L"iPhone" }));
    VERIFY_IS_TRUE(MidiBleUtilities::IsGenericDeviceName(winrt::hstring{ L"IPHONE" }));
    VERIFY_IS_TRUE(MidiBleUtilities::IsGenericDeviceName(winrt::hstring{ L"iPad" }));

    // matched whole, so a real name which merely contains one of these is left alone
    VERIFY_IS_FALSE(MidiBleUtilities::IsGenericDeviceName(winrt::hstring{ L"Pete's iPhone" }));
    VERIFY_IS_FALSE(MidiBleUtilities::IsGenericDeviceName(winrt::hstring{ L"LUMI Keys Block" }));
    VERIFY_IS_FALSE(MidiBleUtilities::IsGenericDeviceName(winrt::hstring{ L"" }));
}


void BleMidiValidationTests::TestApprovalScopeStringsRoundTrip()
{
    MidiBleProtocol::ApprovalScope const scopes[] =
    {
        MidiBleProtocol::ApprovalScope::Once,
        MidiBleProtocol::ApprovalScope::UntilRestart,
        MidiBleProtocol::ApprovalScope::Always,
    };

    for (auto const& scope : scopes)
    {
        MidiBleProtocol::ApprovalScope parsed{};

        VERIFY_IS_TRUE(MidiBleUtilities::TryApprovalScopeFromJsonString(
            MidiBleUtilities::ApprovalScopeToJsonString(scope), parsed));

        VERIFY_ARE_EQUAL(static_cast<uint32_t>(scope), static_cast<uint32_t>(parsed));
    }
}

void BleMidiValidationTests::TestApprovalScopeRejectsAnUnrecognizedValue()
{
    MidiBleProtocol::ApprovalScope parsed{ MidiBleProtocol::ApprovalScope::Always };

    // Guessing here would either write down a permission nobody asked for or silently drop one
    // they did, so an unknown scope is refused rather than defaulted.
    VERIFY_IS_FALSE(MidiBleUtilities::TryApprovalScopeFromJsonString(winrt::hstring{ L"forever" }, parsed));
    VERIFY_IS_FALSE(MidiBleUtilities::TryApprovalScopeFromJsonString(winrt::hstring{ L"ALWAYS" }, parsed));
    VERIFY_IS_FALSE(MidiBleUtilities::TryApprovalScopeFromJsonString(winrt::hstring{ L"never" }, parsed));
}

void BleMidiValidationTests::TestApprovalScopeTreatsEmptyAsOnce()
{
    MidiBleProtocol::ApprovalScope parsed{ MidiBleProtocol::ApprovalScope::Always };

    // The narrowest scope is the safe reading of a command which did not say.
    VERIFY_IS_TRUE(MidiBleUtilities::TryApprovalScopeFromJsonString(winrt::hstring{ L"" }, parsed));
    VERIFY_ARE_EQUAL(
        static_cast<uint32_t>(MidiBleProtocol::ApprovalScope::Once),
        static_cast<uint32_t>(parsed));
}

void BleMidiValidationTests::TestClientPolicyDefaultsToRequiringApproval()
{
    // A typo in the configuration file must not silently open the peripheral to any Central.
    VERIFY_ARE_EQUAL(
        static_cast<uint32_t>(MidiBleProtocol::PeripheralClientPolicy::RequireApproval),
        static_cast<uint32_t>(MidiBleUtilities::PeripheralClientPolicyFromJsonString(winrt::hstring{ L"allowAnyone" })));

    VERIFY_ARE_EQUAL(
        static_cast<uint32_t>(MidiBleProtocol::PeripheralClientPolicy::RequireApproval),
        static_cast<uint32_t>(MidiBleUtilities::PeripheralClientPolicyFromJsonString(winrt::hstring{ L"" })));
}

void BleMidiValidationTests::TestClientPolicyStringsRoundTrip()
{
    MidiBleProtocol::PeripheralClientPolicy const policies[] =
    {
        MidiBleProtocol::PeripheralClientPolicy::RequireApproval,
        MidiBleProtocol::PeripheralClientPolicy::AllowAny,
    };

    for (auto const& policy : policies)
    {
        VERIFY_ARE_EQUAL(
            static_cast<uint32_t>(policy),
            static_cast<uint32_t>(MidiBleUtilities::PeripheralClientPolicyFromJsonString(
                MidiBleUtilities::PeripheralClientPolicyToJsonString(policy))));
    }
}

void BleMidiValidationTests::TestClientMatchKeyIgnoresSeparatorsAndCase()
{
    // A remembered decision typed by hand has to match what the radio reports.
    auto const expected = std::wstring{ L"48B6201A719D" };

    VERIFY_ARE_EQUAL(expected, MidiBleUtilities::NormalizeClientMatchKey(L"48B6201A719D"));
    VERIFY_ARE_EQUAL(expected, MidiBleUtilities::NormalizeClientMatchKey(L"48b6201a719d"));
    VERIFY_ARE_EQUAL(expected, MidiBleUtilities::NormalizeClientMatchKey(L"48:B6:20:1A:71:9D"));
    VERIFY_ARE_EQUAL(expected, MidiBleUtilities::NormalizeClientMatchKey(L"48-b6-20-1a-71-9d"));
    VERIFY_ARE_EQUAL(expected, MidiBleUtilities::NormalizeClientMatchKey(L" 48 B6 20 1A 71 9D "));
}

void BleMidiValidationTests::TestClientMatchKeyOfAnUnusableAddressIsEmpty()
{
    // An empty key is what makes the caller fall through to asking a person, so it must not be
    // producible from something which carries no address at all.
    VERIFY_IS_TRUE(MidiBleUtilities::NormalizeClientMatchKey(L"").empty());
    VERIFY_IS_TRUE(MidiBleUtilities::NormalizeClientMatchKey(L"::::").empty());
    VERIFY_IS_TRUE(MidiBleUtilities::NormalizeClientMatchKey(L"  -- ").empty());
}

void BleMidiValidationTests::TestClientDecisionStrings()
{
    VERIFY_ARE_EQUAL(
        winrt::hstring{ MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CLIENT_DECISION_VALUE_PENDING },
        MidiBleUtilities::PeripheralClientDecisionToJsonString(MidiBleProtocol::PeripheralClientDecision::Pending));

    VERIFY_ARE_EQUAL(
        winrt::hstring{ MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CLIENT_DECISION_VALUE_ALLOWED },
        MidiBleUtilities::PeripheralClientDecisionToJsonString(MidiBleProtocol::PeripheralClientDecision::Allowed));

    VERIFY_ARE_EQUAL(
        winrt::hstring{ MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CLIENT_DECISION_VALUE_DENIED },
        MidiBleUtilities::PeripheralClientDecisionToJsonString(MidiBleProtocol::PeripheralClientDecision::Denied));
}


void BleMidiValidationTests::TestRandomAddressKindComesFromTheTopTwoBits()
{
    // Core Specification Vol 6 Part B 1.3.2. Only the low 48 bits are the address, so the kind
    // lives in bits 47:46.
    VERIFY_ARE_EQUAL(
        static_cast<uint32_t>(MidiBleUtilities::RandomAddressKind::NonResolvablePrivate),
        static_cast<uint32_t>(MidiBleUtilities::ClassifyRandomAddress(0x00B6201A719Dull)));

    VERIFY_ARE_EQUAL(
        static_cast<uint32_t>(MidiBleUtilities::RandomAddressKind::ResolvablePrivate),
        static_cast<uint32_t>(MidiBleUtilities::ClassifyRandomAddress(0x48B6201A719Dull)));

    VERIFY_ARE_EQUAL(
        static_cast<uint32_t>(MidiBleUtilities::RandomAddressKind::Reserved),
        static_cast<uint32_t>(MidiBleUtilities::ClassifyRandomAddress(0x88B6201A719Dull)));

    VERIFY_ARE_EQUAL(
        static_cast<uint32_t>(MidiBleUtilities::RandomAddressKind::Static),
        static_cast<uint32_t>(MidiBleUtilities::ClassifyRandomAddress(0xC8B6201A719Dull)));
}

void BleMidiValidationTests::TestPublicAddressIsAlwaysRememberable()
{
    // An IEEE assigned address never changes, bonded or not.
    VERIFY_IS_TRUE(MidiBleUtilities::IsRememberableAddress(winrt::hstring{ L"public" }, 0x48B6201A719Dull, false));
    VERIFY_IS_TRUE(MidiBleUtilities::IsRememberableAddress(winrt::hstring{ L"public" }, 0x000000000001ull, false));
}

void BleMidiValidationTests::TestStaticRandomAddressIsRememberable()
{
    VERIFY_IS_TRUE(MidiBleUtilities::IsRememberableAddress(winrt::hstring{ L"random" }, 0xC8B6201A719Dull, false));
    VERIFY_IS_TRUE(MidiBleUtilities::IsRememberableAddress(winrt::hstring{ L"random" }, 0xFFFFFFFFFFFFull, false));
}

void BleMidiValidationTests::TestPrivateRandomAddressIsNotRememberableUnlessPaired()
{
    // These rotate, so a permanent decision about one would quietly stop matching.
    VERIFY_IS_FALSE(MidiBleUtilities::IsRememberableAddress(winrt::hstring{ L"random" }, 0x48B6201A719Dull, false));
    VERIFY_IS_FALSE(MidiBleUtilities::IsRememberableAddress(winrt::hstring{ L"random" }, 0x00B6201A719Dull, false));

    // Bonding gives Windows the key to resolve the rotating address back to a stable one.
    VERIFY_IS_TRUE(MidiBleUtilities::IsRememberableAddress(winrt::hstring{ L"random" }, 0x48B6201A719Dull, true));
    VERIFY_IS_TRUE(MidiBleUtilities::IsRememberableAddress(winrt::hstring{ L"random" }, 0x00B6201A719Dull, true));
}

void BleMidiValidationTests::TestUnspecifiedAddressTypeIsNotRememberableUnlessPaired()
{
    // The radio did not say, and guessing wrong offers a permission which does not work.
    VERIFY_IS_FALSE(MidiBleUtilities::IsRememberableAddress(winrt::hstring{ L"unspecified" }, 0xC8B6201A719Dull, false));
    VERIFY_IS_FALSE(MidiBleUtilities::IsRememberableAddress(winrt::hstring{ L"" }, 0xC8B6201A719Dull, false));

    VERIFY_IS_TRUE(MidiBleUtilities::IsRememberableAddress(winrt::hstring{ L"unspecified" }, 0xC8B6201A719Dull, true));
}


void BleMidiValidationTests::TestEveryTransportErrorCodeIsDistinct()
{
    // These are a contract with the SDK, which casts the service's number straight into its own
    // enum. Two codes sharing a value would silently report the wrong cause.
    std::pair<wchar_t const*, uint32_t> const codes[] =
    {
        { L"UNRECOGNIZED_COMMAND",          BLUETOOTH_MIDI_ERROR_CODE_UNRECOGNIZED_COMMAND },
        { L"INVALID_JSON",                  BLUETOOTH_MIDI_ERROR_CODE_INVALID_JSON },
        { L"MISSING_DEVICE_ID",             BLUETOOTH_MIDI_ERROR_CODE_MISSING_DEVICE_ID },
        { L"INVALID_DEVICE_ID",             BLUETOOTH_MIDI_ERROR_CODE_INVALID_DEVICE_ID },
        { L"TRANSPORT_NOT_AVAILABLE",       BLUETOOTH_MIDI_ERROR_CODE_TRANSPORT_NOT_AVAILABLE },
        { L"DEVICE_NOT_DISCOVERED",         BLUETOOTH_MIDI_ERROR_CODE_DEVICE_NOT_DISCOVERED },
        { L"DEVICE_NOT_AVAILABLE",          BLUETOOTH_MIDI_ERROR_CODE_DEVICE_NOT_AVAILABLE },
        { L"MIDI_SERVICE_NOT_FOUND",        BLUETOOTH_MIDI_ERROR_CODE_MIDI_SERVICE_NOT_FOUND },
        { L"MIDI_CHARACTERISTIC_NOT_FOUND", BLUETOOTH_MIDI_ERROR_CODE_MIDI_CHARACTERISTIC_NOT_FOUND },
        { L"DEVICE_UNREACHABLE",            BLUETOOTH_MIDI_ERROR_CODE_DEVICE_UNREACHABLE },
        { L"GATT_ACCESS_DENIED",            BLUETOOTH_MIDI_ERROR_CODE_GATT_ACCESS_DENIED },
        { L"GATT_PROTOCOL_ERROR",           BLUETOOTH_MIDI_ERROR_CODE_GATT_PROTOCOL_ERROR },
        { L"DEVICE_IN_USE",                 BLUETOOTH_MIDI_ERROR_CODE_DEVICE_IN_USE },
        { L"ALREADY_CONNECTED",             BLUETOOTH_MIDI_ERROR_CODE_ALREADY_CONNECTED },
        { L"SESSION_CREATION_FAILED",       BLUETOOTH_MIDI_ERROR_CODE_SESSION_CREATION_FAILED },
        { L"OPERATION_ABORTED",             BLUETOOTH_MIDI_ERROR_CODE_OPERATION_ABORTED },
        { L"NOTIFY_FAILED",                 BLUETOOTH_MIDI_ERROR_CODE_NOTIFY_FAILED },
        { L"ENDPOINT_CREATION_FAILED",      BLUETOOTH_MIDI_ERROR_CODE_ENDPOINT_CREATION_FAILED },
        { L"NOT_CONNECTED",                 BLUETOOTH_MIDI_ERROR_CODE_NOT_CONNECTED },
        { L"PERIPHERAL_ALREADY_RUNNING",    BLUETOOTH_MIDI_ERROR_CODE_PERIPHERAL_ALREADY_RUNNING },
        { L"PERIPHERAL_NOT_RUNNING",        BLUETOOTH_MIDI_ERROR_CODE_PERIPHERAL_NOT_RUNNING },
        { L"PERIPHERAL_ROLE_NOT_AVAILABLE", BLUETOOTH_MIDI_ERROR_CODE_PERIPHERAL_ROLE_NOT_AVAILABLE },
        { L"PERIPHERAL_NO_CLIENT",          BLUETOOTH_MIDI_ERROR_CODE_PERIPHERAL_NO_CLIENT },
        { L"PERIPHERAL_INVALID_PROTOCOL",   BLUETOOTH_MIDI_ERROR_CODE_PERIPHERAL_INVALID_PROTOCOL },
        { L"PERIPHERAL_ADVERTISING_FAILED", BLUETOOTH_MIDI_ERROR_CODE_PERIPHERAL_ADVERTISING_FAILED },
        { L"CLIENT_NOT_PENDING",           BLUETOOTH_MIDI_ERROR_CODE_CLIENT_NOT_PENDING },
        { L"CLIENT_IDENTITY_MISMATCH",     BLUETOOTH_MIDI_ERROR_CODE_CLIENT_IDENTITY_MISMATCH },
        { L"INVALID_APPROVAL_SCOPE",       BLUETOOTH_MIDI_ERROR_CODE_INVALID_APPROVAL_SCOPE },
        { L"MISSING_CLIENT_ADDRESS",       BLUETOOTH_MIDI_ERROR_CODE_MISSING_CLIENT_ADDRESS },
        { L"CLIENT_NOT_REMEMBERED",        BLUETOOTH_MIDI_ERROR_CODE_CLIENT_NOT_REMEMBERED },
        { L"ADDRESS_NOT_REMEMBERABLE",     BLUETOOTH_MIDI_ERROR_CODE_ADDRESS_NOT_REMEMBERABLE },
        { L"RADIO_NOT_AVAILABLE",           BLUETOOTH_MIDI_ERROR_CODE_RADIO_NOT_AVAILABLE },
        { L"RADIO_NOT_SUPPORTED",           BLUETOOTH_MIDI_ERROR_CODE_RADIO_NOT_SUPPORTED },
        { L"PROTOCOL_NOT_SUPPORTED",        BLUETOOTH_MIDI_ERROR_CODE_PROTOCOL_NOT_SUPPORTED },
    };

    std::set<uint32_t> seen;

    for (auto const& entry : codes)
    {
        // zero means "no information", so no named cause may use it
        VERIFY_ARE_NOT_EQUAL(0u, entry.second,
            String().Format(L"%s is non-zero", entry.first));

        VERIFY_IS_TRUE(seen.insert(entry.second).second,
            String().Format(L"%s has a value no other code uses (0x%08X)", entry.first, entry.second));
    }

    VERIFY_ARE_EQUAL(ARRAYSIZE(codes), seen.size());
}
