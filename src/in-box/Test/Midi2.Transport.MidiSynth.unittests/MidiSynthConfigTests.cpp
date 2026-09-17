// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include "MidiSynthConfigTests.h"

#include <functional>
#include <random>

#include "midi_synth_json_defs.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace TransportConfigTest;

namespace
{
    // {7605713E-FEA9-409D-A90F-A81233200D0A}
    constexpr GUID MidiSynthTransportId
    {
        0x7605713E, 0xFEA9, 0x409D, { 0xA9, 0x0F, 0xA8, 0x12, 0x33, 0x20, 0x0D, 0x0A }
    };

    constexpr wchar_t MidiSynthTransportIdString[]{ L"{7605713E-FEA9-409D-A90F-A81233200D0A}" };

    constexpr wchar_t StatusCommandJson[]
    {
        L"{\"transportCommand\":{\"commandName\":\"" MIDI_SYNTH_COMMAND_STATUS L"\"}}"
    };


    ServiceConfigResult SendSynthConfig(std::wstring const& json)
    {
        return SendTransportConfig(MidiSynthTransportId, MidiSynthTransportIdString, json);
    }


    ServiceConfigResult SendStatus()
    {
        return SendSynthConfig(StatusCommandJson);
    }


    bool SynthAvailable()
    {
        return SendStatus().CallSucceeded;
    }


    // The status reply is the only way to see what actually took effect, so several tests read it
    // back rather than trusting the success flag on the write.
    std::optional<winrt::Windows::Data::Json::JsonObject> ReadStatusObject()
    {
        auto const result = SendStatus();

        if (!result.IsSuccess())
        {
            return std::nullopt;
        }

        winrt::Windows::Data::Json::JsonObject parsed{ nullptr };

        if (!winrt::Windows::Data::Json::JsonObject::TryParse(
            winrt::hstring{ result.ResponseJson }, parsed))
        {
            return std::nullopt;
        }

        return parsed;
    }


    // Digs the transport's own response object out of whichever envelope the service wrapped it in,
    // so the tests do not have to care about the wrapping.
    std::optional<double> ReadStatusNumber(std::wstring const& key)
    {
        auto const status = ReadStatusObject();

        if (!status.has_value())
        {
            return std::nullopt;
        }

        std::optional<double> found{ std::nullopt };

        std::function<void(winrt::Windows::Data::Json::JsonObject const&)> search =
            [&](winrt::Windows::Data::Json::JsonObject const& object)
            {
                if (found.has_value())
                {
                    return;
                }

                for (auto const& pair : object)
                {
                    auto const value = pair.Value();

                    if (value == nullptr)
                    {
                        continue;
                    }

                    if (pair.Key() == winrt::hstring{ key } &&
                        value.ValueType() == winrt::Windows::Data::Json::JsonValueType::Number)
                    {
                        found = value.GetNumber();
                        return;
                    }

                    if (value.ValueType() == winrt::Windows::Data::Json::JsonValueType::Object)
                    {
                        search(value.GetObject());
                    }
                }
            };

        search(status.value());

        return found;
    }


    std::optional<bool> ReadStatusBoolean(std::wstring const& key)
    {
        auto const status = ReadStatusObject();

        if (!status.has_value())
        {
            return std::nullopt;
        }

        std::optional<bool> found{ std::nullopt };

        std::function<void(winrt::Windows::Data::Json::JsonObject const&)> search =
            [&](winrt::Windows::Data::Json::JsonObject const& object)
            {
                if (found.has_value())
                {
                    return;
                }

                for (auto const& pair : object)
                {
                    auto const value = pair.Value();

                    if (value == nullptr)
                    {
                        continue;
                    }

                    if (pair.Key() == winrt::hstring{ key } &&
                        value.ValueType() == winrt::Windows::Data::Json::JsonValueType::Boolean)
                    {
                        found = value.GetBoolean();
                        return;
                    }

                    if (value.ValueType() == winrt::Windows::Data::Json::JsonValueType::Object)
                    {
                        search(value.GetObject());
                    }
                }
            };

        search(status.value());

        return found;
    }


    std::wstring BuildSettingsJson(std::wstring const& body)
    {
        return L"{" + body + L"}";
    }
}


void MidiSynthConfigTests::TestStatusCommandAnswers()
{
    if (!SynthAvailable())
    {
        Log::Result(TestResults::Skipped, L"The General MIDI synthesizer transport is not available.");
        return;
    }

    auto const result = SendStatus();

    VERIFY_IS_TRUE(result.IsSuccess());
}


void MidiSynthConfigTests::TestMalformedJsonIsRejected()
{
    if (!SynthAvailable())
    {
        Log::Result(TestResults::Skipped, L"The General MIDI synthesizer transport is not available.");
        return;
    }

    // not valid json at all
    auto const truncated = SendRawServiceConfig(
        MidiSynthTransportId, L"{\"endpointTransportPluginSettings\":");
    VERIFY_IS_FALSE(truncated.IsSuccess());

    auto const garbage = SendRawServiceConfig(MidiSynthTransportId, L"}{not json at all");
    VERIFY_IS_FALSE(garbage.IsSuccess());

    auto const empty = SendRawServiceConfig(MidiSynthTransportId, L"");
    VERIFY_IS_FALSE(empty.IsSuccess());

    VERIFY_IS_TRUE(SynthAvailable(), L"The transport still answers after malformed payloads.");
}


// The regression this suite exists for. GetNamedString(key, default) only covers a MISSING key:
// it throws when the key is present holding another type, and a throw escaping a transport takes
// midisrv down for the whole machine, not just this transport.
void MidiSynthConfigTests::TestWrongTypesDoNotCrashTheTransport()
{
    if (!SynthAvailable())
    {
        Log::Result(TestResults::Skipped, L"The General MIDI synthesizer transport is not available.");
        return;
    }

    std::vector<std::wstring> const payloads
    {
        // string keys carrying every other json type
        L"\"" MIDI_SYNTH_JSON_SYNTH_MODE_PROPERTY_KEY L"\":123",
        L"\"" MIDI_SYNTH_JSON_SYNTH_MODE_PROPERTY_KEY L"\":true",
        L"\"" MIDI_SYNTH_JSON_SYNTH_MODE_PROPERTY_KEY L"\":null",
        L"\"" MIDI_SYNTH_JSON_SYNTH_MODE_PROPERTY_KEY L"\":{}",
        L"\"" MIDI_SYNTH_JSON_SYNTH_MODE_PROPERTY_KEY L"\":[]",
        L"\"" MIDI_SYNTH_JSON_AUDIO_MODE_PROPERTY_KEY L"\":123",
        L"\"" MIDI_SYNTH_JSON_AUDIO_MODE_PROPERTY_KEY L"\":[1,2,3]",
        L"\"" MIDI_SYNTH_JSON_BANK_SELECT_PROPERTY_KEY L"\":false",
        L"\"" MIDI_SYNTH_JSON_BANK_SELECT_PROPERTY_KEY L"\":{\"a\":1}",

        // boolean keys carrying every other json type
        L"\"" MIDI_SYNTH_JSON_ENABLED_PROPERTY_KEY L"\":\"yes\"",
        L"\"" MIDI_SYNTH_JSON_ENABLED_PROPERTY_KEY L"\":1",
        L"\"" MIDI_SYNTH_JSON_ENABLED_PROPERTY_KEY L"\":null",
        L"\"" MIDI_SYNTH_JSON_EFFECTS_PROPERTY_KEY L"\":\"on\"",
        L"\"" MIDI_SYNTH_JSON_EFFECTS_PROPERTY_KEY L"\":[]",

        // number key carrying every other json type
        L"\"" MIDI_SYNTH_JSON_VOLUME_PROPERTY_KEY L"\":\"-6\"",
        L"\"" MIDI_SYNTH_JSON_VOLUME_PROPERTY_KEY L"\":true",
        L"\"" MIDI_SYNTH_JSON_VOLUME_PROPERTY_KEY L"\":{}",
        L"\"" MIDI_SYNTH_JSON_VOLUME_PROPERTY_KEY L"\":null",
    };

    for (auto const& body : payloads)
    {
        // Logged before the send, so if the service dies the last line names the payload.
        Log::Comment(String().Format(L"sending: %s", body.c_str()));

        auto const result = SendSynthConfig(BuildSettingsJson(body));

        VERIFY_IS_TRUE(result.CallSucceeded, L"The call returned rather than killing the service.");

        // This is the assertion that discriminates. With the throwing accessors the wrong type
        // raised an exception, the catch failed the whole section, and this came back false. A
        // wrong-typed value has to be treated as absent instead, so the section still succeeds
        // and the keys around it still apply.
        VERIFY_IS_TRUE(result.IsSuccess(), L"A wrong-typed value is ignored, not fatal to the section.");

        VERIFY_IS_TRUE(SynthAvailable(), L"The transport still answers after the wrong-typed value.");
    }
}


// A wrong-typed key must not stop the keys after it from applying. The throwing accessors raised
// on the first bad value, so everything later in the section was silently lost; that is the
// difference this test can actually observe from outside.
void MidiSynthConfigTests::TestWrongTypeDoesNotDiscardLaterKeys()
{
    if (!SynthAvailable())
    {
        Log::Result(TestResults::Skipped, L"The General MIDI synthesizer transport is not available.");
        return;
    }

    auto const original = ReadStatusBoolean(MIDI_SYNTH_JSON_EFFECTS_PROPERTY_KEY);

    if (!original.has_value())
    {
        Log::Result(TestResults::Skipped, L"The status reply did not carry the effects state.");
        return;
    }

    auto restore = wil::scope_exit([&]()
        {
            SendSynthConfig(BuildSettingsJson(
                std::wstring{ L"\"" MIDI_SYNTH_JSON_EFFECTS_PROPERTY_KEY L"\":" } +
                (original.value() ? L"true" : L"false")));
        });

    auto const flipped = !original.value();

    // synthMode is read before effectsEnabled, so a throw on it would take the effects change
    // with it and the value below would come back unchanged.
    auto const result = SendSynthConfig(BuildSettingsJson(
        std::wstring{ L"\"" MIDI_SYNTH_JSON_SYNTH_MODE_PROPERTY_KEY L"\":123," } +
        L"\"" MIDI_SYNTH_JSON_EFFECTS_PROPERTY_KEY L"\":" + (flipped ? L"true" : L"false")));

    VERIFY_IS_TRUE(result.IsSuccess());

    auto const after = ReadStatusBoolean(MIDI_SYNTH_JSON_EFFECTS_PROPERTY_KEY);

    VERIFY_IS_TRUE(after.has_value());
    VERIFY_ARE_EQUAL(flipped, after.value(),
        L"The key after the wrong-typed one still applied.");
}


void MidiSynthConfigTests::TestUnknownModeStringsAreRejected()
{
    if (!SynthAvailable())
    {
        Log::Result(TestResults::Skipped, L"The General MIDI synthesizer transport is not available.");
        return;
    }

    auto const badSynthMode = SendSynthConfig(
        BuildSettingsJson(L"\"" MIDI_SYNTH_JSON_SYNTH_MODE_PROPERTY_KEY L"\":\"nonsense\""));
    VERIFY_IS_FALSE(badSynthMode.IsSuccess());

    auto const badAudioMode = SendSynthConfig(
        BuildSettingsJson(L"\"" MIDI_SYNTH_JSON_AUDIO_MODE_PROPERTY_KEY L"\":\"nonsense\""));
    VERIFY_IS_FALSE(badAudioMode.IsSuccess());

    auto const badBankSelect = SendSynthConfig(
        BuildSettingsJson(L"\"" MIDI_SYNTH_JSON_BANK_SELECT_PROPERTY_KEY L"\":\"nonsense\""));
    VERIFY_IS_FALSE(badBankSelect.IsSuccess());

    VERIFY_IS_TRUE(SynthAvailable());
}


void MidiSynthConfigTests::TestVolumeIsClampedAndNonFiniteRejected()
{
    if (!SynthAvailable())
    {
        Log::Result(TestResults::Skipped, L"The General MIDI synthesizer transport is not available.");
        return;
    }

    auto const original = ReadStatusNumber(MIDI_SYNTH_JSON_VOLUME_PROPERTY_KEY);

    if (!original.has_value())
    {
        Log::Result(TestResults::Skipped, L"The status reply did not carry a volume to restore.");
        return;
    }

    auto restore = wil::scope_exit([&]()
        {
            SendSynthConfig(BuildSettingsJson(
                L"\"" MIDI_SYNTH_JSON_VOLUME_PROPERTY_KEY L"\":" + std::to_wstring(original.value())));
        });

    // Far outside the range the engine supports, in both directions.
    SendSynthConfig(BuildSettingsJson(L"\"" MIDI_SYNTH_JSON_VOLUME_PROPERTY_KEY L"\":100000"));

    auto const high = ReadStatusNumber(MIDI_SYNTH_JSON_VOLUME_PROPERTY_KEY);
    VERIFY_IS_TRUE(high.has_value());
    VERIFY_IS_LESS_THAN_OR_EQUAL(high.value(), 12.0, L"Volume is clamped to the engine maximum.");

    SendSynthConfig(BuildSettingsJson(L"\"" MIDI_SYNTH_JSON_VOLUME_PROPERTY_KEY L"\":-100000"));

    auto const low = ReadStatusNumber(MIDI_SYNTH_JSON_VOLUME_PROPERTY_KEY);
    VERIFY_IS_TRUE(low.has_value());
    VERIFY_IS_GREATER_THAN_OR_EQUAL(low.value(), -60.0, L"Volume is clamped to the engine minimum.");

    // A NaN would survive a min/max clamp and silence the synthesizer, so it has to be refused
    // before the clamp rather than after it. Json has no NaN literal, so this arrives as a string.
    SendSynthConfig(BuildSettingsJson(L"\"" MIDI_SYNTH_JSON_VOLUME_PROPERTY_KEY L"\":\"NaN\""));

    auto const afterNan = ReadStatusNumber(MIDI_SYNTH_JSON_VOLUME_PROPERTY_KEY);
    VERIFY_IS_TRUE(afterNan.has_value());
    VERIFY_IS_TRUE(afterNan.value() == afterNan.value(), L"Volume is still a number after a NaN attempt.");

    VERIFY_IS_TRUE(SynthAvailable());
}


void MidiSynthConfigTests::TestUnknownKeysAreIgnored()
{
    if (!SynthAvailable())
    {
        Log::Result(TestResults::Skipped, L"The General MIDI synthesizer transport is not available.");
        return;
    }

    // A newer build writing a key this one does not know must not make the section unreadable.
    auto const result = SendSynthConfig(BuildSettingsJson(
        L"\"somethingFromAFutureVersion\":{\"nested\":[1,2,3]},"
        L"\"" MIDI_SYNTH_JSON_ENABLED_PROPERTY_KEY L"\":true"));

    VERIFY_IS_TRUE(result.IsSuccess());
    VERIFY_IS_TRUE(SynthAvailable());
}


void MidiSynthConfigTests::TestServiceSurvivesConfigFuzzing()
{
    if (!SynthAvailable())
    {
        Log::Result(TestResults::Skipped, L"The General MIDI synthesizer transport is not available.");
        return;
    }

    // Deterministic seed so a failure can be reproduced exactly.
    constexpr unsigned int seed{ 0x474D5359 };
    std::mt19937 rng{ seed };

    Log::Comment(String().Format(L"Fuzzing with seed 0x%08X", seed));

    std::vector<std::wstring> const fragments
    {
        L"{", L"}", L"[", L"]", L"\"", L":", L",", L"null", L"true", L"1e400", L"-0",
        L"\\u0000", L"\\", L"0.0000000000000000001", L"99999999999999999999999999",
        MIDI_SYNTH_JSON_ENABLED_PROPERTY_KEY, MIDI_SYNTH_JSON_VOLUME_PROPERTY_KEY,
        MIDI_SYNTH_JSON_SYNTH_MODE_PROPERTY_KEY, MIDI_SYNTH_JSON_AUDIO_MODE_PROPERTY_KEY,
    };

    std::uniform_int_distribution<size_t> pick{ 0, fragments.size() - 1 };
    std::uniform_int_distribution<int> lengthDistribution{ 1, 40 };

    constexpr int iterations{ 200 };

    for (int i = 0; i < iterations; i++)
    {
        std::wstring payload;

        auto const pieces = lengthDistribution(rng);

        for (int p = 0; p < pieces; p++)
        {
            payload += fragments[pick(rng)];
        }

        // Half go in raw, half inside the settings envelope, so both the service's own parse and
        // the transport's parse see garbage.
        auto const result = (i % 2) == 0
            ? SendRawServiceConfig(MidiSynthTransportId, payload)
            : SendSynthConfig(payload);

        // Nothing is asserted about the answer: a refusal and an acceptance are both fine. What
        // matters is that there is an answer at all.
        (void)result;
    }

    VERIFY_IS_TRUE(SynthAvailable(), L"The transport still answers after fuzzed configuration.");
}
