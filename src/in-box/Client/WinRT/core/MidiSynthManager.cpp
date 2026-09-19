// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiSynthManager.h"
#include "Transports.Synth.MidiSynthManager.g.cpp"

#include "MidiSynthStatus.h"
#include "MidiSynthSoundSetInfo.h"
#include "MidiSynthInstrumentInfo.h"

#include "MidiReporting.h"
#include "MidiServiceConfigResponse.h"
#include "MidiServiceTransportCommand.h"
#include "MidiServiceTransportPluginConfigManager.h"

#include "..\..\..\Transport\MidiSynthTransport\midi_synth_json_defs.h"

namespace
{
    // Returns a null object when the transport is not installed or refused the command, so every
    // caller has one thing to check rather than a status code and a payload.
    json::JsonObject SendSynthCommand(
        _In_ winrt::hstring const& verb,
        _In_ std::map<std::wstring, std::wstring> const& arguments) noexcept
    {
        try
        {
            svc::MidiServiceTransportCommand command{
                winrt::Windows::Devices::Midi2::Transports::Synth::implementation::MidiSynthManager::TransportId(),
                verb };

            for (auto const& argument : arguments)
            {
                command.Arguments().Insert(argument.first, argument.second);
            }

            auto const response = svc::MidiServiceTransportPluginConfigManager::SendCommand(command);

            if (response == nullptr ||
                response.Status() != svc::MidiServiceConfigResponseStatus::Success)
            {
                return nullptr;
            }

            return response.ResponseJson();
        }
        catch (...)
        {
            return nullptr;
        }
    }
}

namespace winrt::Windows::Devices::Midi2::Transports::Synth::implementation
{
    bool MidiSynthManager::IsTransportAvailable() noexcept
    {
        try
        {
            for (auto const& transport : rpt::MidiReporting::GetInstalledTransportPlugins())
            {
                if (transport.TransportId() == TransportId())
                {
                    return true;
                }
            }

            return false;
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error checking synthesizer transport availability.");
            return false;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception checking synthesizer transport availability.");
            return false;
        }
    }

    synth::MidiSynthStatus MidiSynthManager::GetStatus() noexcept
    {
        try
        {
            auto const responseJson = SendSynthCommand(MIDI_SYNTH_COMMAND_STATUS, {});

            if (responseJson == nullptr)
            {
                return nullptr;
            }

            auto status = winrt::make_self<implementation::MidiSynthStatus>();
            status->InternalInitializeFromJson(responseJson);

            return *status;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception reading synthesizer status.");
            return nullptr;
        }
    }

    synth::MidiSynthSoundSetInfo MidiSynthManager::GetSoundSetInfo() noexcept
    {
        try
        {
            auto const responseJson = SendSynthCommand(MIDI_SYNTH_COMMAND_SOUND_SET, {});

            if (responseJson == nullptr)
            {
                return nullptr;
            }

            auto info = winrt::make_self<implementation::MidiSynthSoundSetInfo>();
            info->InternalInitialize(responseJson);

            return *info;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception reading the synthesizer sound set.");
            return nullptr;
        }
    }

    foundation::Collections::IVectorView<synth::MidiSynthInstrumentInfo>
        MidiSynthManager::GetMelodicInstruments() noexcept
    {
        auto instruments = winrt::single_threaded_vector<synth::MidiSynthInstrumentInfo>();

        try
        {
            auto const responseJson = SendSynthCommand(MIDI_SYNTH_COMMAND_INSTRUMENT_LIST, {});

            if (responseJson == nullptr)
            {
                return instruments.GetView();
            }

            auto const entries = responseJson.GetNamedArray(MIDI_SYNTH_JSON_INSTRUMENTS_KEY, nullptr);

            if (entries == nullptr)
            {
                return instruments.GetView();
            }

            for (auto const& element : entries)
            {
                // Iterating a JsonArray yields IJsonValue, which does not QI to JsonObject. A
                // try_as here would silently skip every entry.
                if (element == nullptr || element.ValueType() != json::JsonValueType::Object)
                {
                    continue;
                }

                auto const entry = element.GetObject();

                auto info = winrt::make_self<implementation::MidiSynthInstrumentInfo>();

                info->InternalSet(
                    entry.GetNamedString(MIDI_SYNTH_JSON_INSTRUMENT_NAME_KEY, L""),
                    static_cast<uint8_t>(entry.GetNamedNumber(MIDI_SYNTH_JSON_INSTRUMENT_BANK_MSB_KEY, 0.0)),
                    static_cast<uint8_t>(entry.GetNamedNumber(MIDI_SYNTH_JSON_INSTRUMENT_BANK_LSB_KEY, 0.0)),
                    static_cast<uint8_t>(entry.GetNamedNumber(MIDI_SYNTH_JSON_INSTRUMENT_PROGRAM_KEY, 0.0)));

                instruments.Append(*info);
            }
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception reading the synthesizer instrument list.");
        }

        return instruments.GetView();
    }

    winrt::hstring MidiSynthManager::EndpointDeviceId() noexcept
    {
        try
        {
            auto const status = GetStatus();

            return status == nullptr ? winrt::hstring{} : status.EndpointDeviceId();
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception reading the synthesizer endpoint device id.");
            return {};
        }
    }

    _Use_decl_annotations_
    bool MidiSynthManager::SetDrumChannel(uint8_t const channelIndex, bool const isDrumChannel) noexcept
    {
        try
        {
            // One group of sixteen channels, which is what General MIDI is defined over.
            if (channelIndex >= 16)
            {
                return false;
            }

            std::map<std::wstring, std::wstring> arguments
            {
                { MIDI_SYNTH_COMMAND_ARG_CHANNEL, std::to_wstring(channelIndex) },
                { MIDI_SYNTH_COMMAND_ARG_IS_DRUM_CHANNEL, isDrumChannel ? L"true" : L"false" },
            };

            return SendSynthCommand(MIDI_SYNTH_COMMAND_SET_DRUM_CHANNEL, arguments) != nullptr;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception setting a synthesizer drum channel.");
            return false;
        }
    }
}
