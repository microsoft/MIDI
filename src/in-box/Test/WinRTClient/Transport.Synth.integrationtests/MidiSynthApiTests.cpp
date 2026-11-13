// Copyright (c) Microsoft Corporation. All rights reserved.

#include "stdafx.h"

#include "MidiSynthApiTests.h"

using namespace WEX::Common;
using namespace WEX::Logging;

namespace
{
    // Every test needs the transport present, and a machine without it should produce skips
    // rather than a wall of red.
    bool SynthAvailable()
    {
        return MidiSynthManager::IsTransportAvailable() && MidiSynthManager::GetStatus() != nullptr;
    }


    // Restores whatever the synthesizer held before a test changed it, so a failure partway
    // through cannot leave the machine's synthesizer reconfigured.
    struct SynthStateRestorer
    {
        MidiSynthStatus Original{ nullptr };

        SynthStateRestorer() : Original(MidiSynthManager::GetStatus()) {}

        ~SynthStateRestorer()
        {
            if (Original == nullptr)
            {
                return;
            }

            MidiSynthConfig config{ Original };

            MidiServiceTransportPluginConfigManager::SendUpdate(config);
            MidiServiceTransportPluginConfigManager::SaveUpdate(config);
        }
    };


    bool SendAndVerify(MidiSynthConfig const& config)
    {
        auto const response = MidiServiceTransportPluginConfigManager::SendUpdate(config);

        return response != nullptr &&
            response.Status() == MidiServiceConfigResponseStatus::Success;
    }


    // The API has no EndpointDeviceId of its own, so the supported way to find the synthesizer is
    // to match enumeration against the transport id the manager reports.
    winrt::hstring FindSynthEndpointDeviceIdByEnumeration()
    {
        auto const transportId = MidiSynthManager::TransportId();

        for (auto const& endpoint : MidiEndpointDeviceInformation::FindAll())
        {
            if (endpoint.GetTransportSuppliedInfo().TransportId() == transportId)
            {
                return endpoint.EndpointDeviceId();
            }
        }

        return {};
    }
}


void MidiSynthApiTests::TestTransportAvailabilityAndId()
{
    if (!SynthAvailable())
    {
        Log::Result(TestResults::Skipped, L"The General MIDI synthesizer transport is not available.");
        return;
    }

    VERIFY_IS_TRUE(MidiSynthManager::IsTransportAvailable());

    // The id the manager reports has to be the one the transport is registered under, or a caller
    // reading endpoint properties would never match the synthesizer's own endpoint.
    winrt::guid const expected{ 0x7605713E, 0xFEA9, 0x409D, { 0xA9, 0x0F, 0xA8, 0x12, 0x33, 0x20, 0x0D, 0x0A } };

    VERIFY_ARE_EQUAL(expected, MidiSynthManager::TransportId());
}


void MidiSynthApiTests::TestStatusIsReadable()
{
    if (!SynthAvailable())
    {
        Log::Result(TestResults::Skipped, L"The General MIDI synthesizer transport is not available.");
        return;
    }

    auto const status = MidiSynthManager::GetStatus();

    VERIFY_IS_NOT_NULL(status);

    // Volume has a defined range, so a garbage read shows up here rather than as silence later.
    VERIFY_IS_GREATER_THAN_OR_EQUAL(status.VolumeDecibels(), -60.0);
    VERIFY_IS_LESS_THAN_OR_EQUAL(status.VolumeDecibels(), 12.0);

    Log::Comment(String().Format(L"enabled=%d volume=%.1f",
        status.IsEnabled() ? 1 : 0, status.VolumeDecibels()));
}


void MidiSynthApiTests::TestSoundSetIsReadable()
{
    if (!SynthAvailable())
    {
        Log::Result(TestResults::Skipped, L"The General MIDI synthesizer transport is not available.");
        return;
    }

    auto const soundSet = MidiSynthManager::GetSoundSetInfo();

    VERIFY_IS_NOT_NULL(soundSet);
    VERIFY_IS_FALSE(soundSet.Name().empty());
    VERIFY_IS_GREATER_THAN(soundSet.MelodicInstrumentCount(), 0u);
    VERIFY_IS_GREATER_THAN(soundSet.WaveCount(), 0u);

    auto const kits = soundSet.DrumKits();

    VERIFY_IS_NOT_NULL(kits);
    VERIFY_IS_GREATER_THAN(kits.Size(), 0u);

    // A kit is addressed by program change, so the program has to be a valid one.
    for (auto const& kit : kits)
    {
        VERIFY_IS_LESS_THAN_OR_EQUAL(static_cast<uint32_t>(kit.Program()), 127u);
        VERIFY_IS_FALSE(kit.Name().empty());
    }
}


// Fixed for a given sound set: bank select mode changes how an incoming bank select is read, not
// what the sound set contains. So this is safe to cache, and safe to read before connecting.
void MidiSynthApiTests::TestMelodicInstrumentsAreReadable()
{
    if (!SynthAvailable())
    {
        Log::Result(TestResults::Skipped, L"The General MIDI synthesizer transport is not available.");
        return;
    }

    auto const instruments = MidiSynthManager::GetMelodicInstruments();

    VERIFY_IS_NOT_NULL(instruments);
    VERIFY_IS_GREATER_THAN(instruments.Size(), 0u);

    auto const soundSet = MidiSynthManager::GetSoundSetInfo();
    VERIFY_IS_NOT_NULL(soundSet);

    // The count the sound set reports and the list it hands out have to agree, or one of them is
    // filtering drum kits differently.
    VERIFY_ARE_EQUAL(soundSet.MelodicInstrumentCount(), instruments.Size());

    for (auto const& instrument : instruments)
    {
        VERIFY_IS_FALSE(instrument.Name().empty());

        // Every address is a 7-bit value on the wire.
        VERIFY_IS_LESS_THAN_OR_EQUAL(static_cast<uint32_t>(instrument.BankMsb()), 127u);
        VERIFY_IS_LESS_THAN_OR_EQUAL(static_cast<uint32_t>(instrument.BankLsb()), 127u);
        VERIFY_IS_LESS_THAN_OR_EQUAL(static_cast<uint32_t>(instrument.Program()), 127u);
    }

    // Reading it twice has to give the same thing, since nothing about it depends on state.
    auto const again = MidiSynthManager::GetMelodicInstruments();

    VERIFY_ARE_EQUAL(instruments.Size(), again.Size());
}


void MidiSynthApiTests::TestDefaultConfigHasSaneValues()
{
    // Constructing a config touches nothing, so this runs without the transport.
    MidiSynthConfig config;

    VERIFY_IS_GREATER_THAN_OR_EQUAL(config.VolumeDecibels(), -60.0);
    VERIFY_IS_LESS_THAN_OR_EQUAL(config.VolumeDecibels(), 12.0);

    // A default-constructed config is sent as a complete set, so it must not be able to turn the
    // synthesizer off by accident.
    VERIFY_IS_TRUE(config.IsEnabled());
}


void MidiSynthApiTests::TestConfigFromStatusCopiesEveryProperty()
{
    if (!SynthAvailable())
    {
        Log::Result(TestResults::Skipped, L"The General MIDI synthesizer transport is not available.");
        return;
    }

    auto const status = MidiSynthManager::GetStatus();

    MidiSynthConfig config{ status };

    // Every property the status carries has to survive the copy, because the config is written in
    // full: anything missed here would be silently reset the next time a caller changed one value.
    VERIFY_ARE_EQUAL(status.IsEnabled(), config.IsEnabled());
    VERIFY_IS_TRUE(status.RenderMode() == config.RenderMode());
    VERIFY_IS_TRUE(status.AudioOutputMode() == config.AudioOutputMode());
    VERIFY_IS_TRUE(status.BankSelectMode() == config.BankSelectMode());
    VERIFY_ARE_EQUAL(status.VolumeDecibels(), config.VolumeDecibels());
    VERIFY_ARE_EQUAL(status.AreEffectsEnabled(), config.AreEffectsEnabled());
}


// The reason MidiSynthConfig(status) exists. Changing one property must not disturb the others,
// even though the whole set is written every time.
void MidiSynthApiTests::TestSinglePropertyChangeLeavesTheRestAlone()
{
    if (!SynthAvailable())
    {
        Log::Result(TestResults::Skipped, L"The General MIDI synthesizer transport is not available.");
        return;
    }

    SynthStateRestorer restorer;

    auto const before = MidiSynthManager::GetStatus();
    VERIFY_IS_NOT_NULL(before);

    auto const flippedEffects = !before.AreEffectsEnabled();

    MidiSynthConfig config{ before };
    config.AreEffectsEnabled(flippedEffects);

    VERIFY_IS_TRUE(SendAndVerify(config));

    auto const after = MidiSynthManager::GetStatus();
    VERIFY_IS_NOT_NULL(after);

    VERIFY_ARE_EQUAL(flippedEffects, after.AreEffectsEnabled(), L"The one property changed.");

    VERIFY_ARE_EQUAL(before.IsEnabled(), after.IsEnabled());
    VERIFY_IS_TRUE(before.RenderMode() == after.RenderMode());
    VERIFY_IS_TRUE(before.AudioOutputMode() == after.AudioOutputMode());
    VERIFY_IS_TRUE(before.BankSelectMode() == after.BankSelectMode());
    VERIFY_ARE_EQUAL(before.VolumeDecibels(), after.VolumeDecibels());
}


void MidiSynthApiTests::TestVolumeIsClampedByTheService()
{
    if (!SynthAvailable())
    {
        Log::Result(TestResults::Skipped, L"The General MIDI synthesizer transport is not available.");
        return;
    }

    SynthStateRestorer restorer;

    // The API does not clamp on the client side, deliberately: the engine owns the limits, so the
    // service is the thing that has to hold the line.
    MidiSynthConfig high{ MidiSynthManager::GetStatus() };
    high.VolumeDecibels(100000.0);

    VERIFY_IS_TRUE(SendAndVerify(high));

    auto const afterHigh = MidiSynthManager::GetStatus();
    VERIFY_IS_NOT_NULL(afterHigh);
    VERIFY_IS_LESS_THAN_OR_EQUAL(afterHigh.VolumeDecibels(), 12.0);

    MidiSynthConfig low{ MidiSynthManager::GetStatus() };
    low.VolumeDecibels(-100000.0);

    VERIFY_IS_TRUE(SendAndVerify(low));

    auto const afterLow = MidiSynthManager::GetStatus();
    VERIFY_IS_NOT_NULL(afterLow);
    VERIFY_IS_GREATER_THAN_OR_EQUAL(afterLow.VolumeDecibels(), -60.0);
}


void MidiSynthApiTests::TestConfigFromNullStatusIsUsable()
{
    // A caller which did not check GetStatus for null must get defaults rather than a crash,
    // because the transport being absent is a normal state on a machine without it.
    MidiSynthStatus nullStatus{ nullptr };

    MidiSynthConfig config{ nullStatus };

    VERIFY_IS_GREATER_THAN_OR_EQUAL(config.VolumeDecibels(), -60.0);
    VERIFY_IS_LESS_THAN_OR_EQUAL(config.VolumeDecibels(), 12.0);

    // It still has to produce a payload, or the caller would send an empty section.
    VERIFY_IS_NOT_NULL(config.ConfigJson());
}


void MidiSynthApiTests::TestSetDrumChannelRejectsOutOfRange()
{
    if (!SynthAvailable())
    {
        Log::Result(TestResults::Skipped, L"The General MIDI synthesizer transport is not available.");
        return;
    }

    // There are sixteen channels, so anything past index 15 has to be refused rather than
    // indexing off the end of the channel array. This is checked in the projection, so it holds
    // whether or not the synthesizer is rendering.
    VERIFY_IS_FALSE(MidiSynthManager::SetDrumChannel(16, true));
    VERIFY_IS_FALSE(MidiSynthManager::SetDrumChannel(255, true));

    VERIFY_IS_TRUE(SynthAvailable(), L"The transport still answers after the rejected calls.");
}


// The id the manager hands out has to be the one enumeration reports for this transport, or an
// application would connect to something other than the synthesizer.
void MidiSynthApiTests::TestEndpointDeviceIdMatchesEnumeration()
{
    if (!SynthAvailable())
    {
        Log::Result(TestResults::Skipped, L"The General MIDI synthesizer transport is not available.");
        return;
    }

    auto const status = MidiSynthManager::GetStatus();
    VERIFY_IS_NOT_NULL(status);

    auto const fromManager = MidiSynthManager::EndpointDeviceId();

    VERIFY_ARE_EQUAL(status.EndpointDeviceId(), fromManager,
        L"The manager and the status agree.");

    if (!status.IsEnabled())
    {
        // Switched off means there is no endpoint, so an empty id is the correct answer.
        VERIFY_IS_TRUE(fromManager.empty(), L"A switched-off synthesizer names no endpoint.");
        return;
    }

    VERIFY_IS_FALSE(fromManager.empty());

    auto const fromEnumeration = FindSynthEndpointDeviceIdByEnumeration();

    VERIFY_IS_FALSE(fromEnumeration.empty(), L"Enumeration finds an endpoint for this transport.");

    // Endpoint ids are compared case-insensitively everywhere else in the API, so do the same here.
    VERIFY_ARE_EQUAL(0, _wcsicmp(fromManager.c_str(), fromEnumeration.c_str()),
        L"The manager names the same endpoint enumeration does.");
}


// The setting lives in the engine, and the engine only exists while something is connected, so an
// in-range call is refused until a connection is open. That precondition is easy to get wrong.
void MidiSynthApiTests::TestSetDrumChannelNeedsAnOpenConnection()
{
    if (!SynthAvailable())
    {
        Log::Result(TestResults::Skipped, L"The General MIDI synthesizer transport is not available.");
        return;
    }

    auto const status = MidiSynthManager::GetStatus();

    if (status == nullptr || !status.IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"The synthesizer is switched off, so it has no endpoint.");
        return;
    }

    auto const endpointId = MidiSynthManager::EndpointDeviceId();

    if (endpointId.empty())
    {
        Log::Result(TestResults::Skipped, L"The synthesizer reported no endpoint device id.");
        return;
    }

    auto session = MidiSession::Create(L"Synth API drum channel test");
    VERIFY_IS_NOT_NULL(session);

    auto connection = session.CreateEndpointConnection(endpointId);
    VERIFY_IS_NOT_NULL(connection);
    VERIFY_IS_TRUE(connection.Open());

    // Nothing is played here. Opening the connection is what starts the engine.
    VERIFY_IS_TRUE(MidiSynthManager::SetDrumChannel(9, true),
        L"Channel 10 can be set while a connection is open.");

    VERIFY_IS_TRUE(MidiSynthManager::SetDrumChannel(0, true),
        L"Any channel can be made a rhythm part.");

    VERIFY_IS_TRUE(MidiSynthManager::SetDrumChannel(0, false),
        L"And put back.");

    session.DisconnectEndpointConnection(connection.ConnectionId());
    session.Close();
}
