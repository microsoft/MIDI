// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "json_transport_command_helper.h"

namespace
{
    // The configuration file is writable by a standard user, so every value here is untrusted.
    // The two-argument GetNamedXxx accessors only cover a MISSING key: they throw when the key is
    // present and holds another type, and an exception escaping a transport takes down midisrv.
    // A wrong-typed value is treated as absent so one bad key cannot discard the whole section.
    json::JsonValue SafeLookup(
        _In_ json::JsonObject const& parent,
        _In_ std::wstring_view key,
        _In_ json::JsonValueType expectedType) noexcept
    {
        try
        {
            winrt::hstring const name{ key };

            if (!parent.HasKey(name))
            {
                return nullptr;
            }

            auto const found = parent.Lookup(name);

            if (found == nullptr || found.ValueType() != expectedType)
            {
                return nullptr;
            }

            return found.try_as<json::JsonValue>();
        }
        catch (...)
        {
            return nullptr;
        }
    }

    winrt::hstring SafeGetNamedString(
        _In_ json::JsonObject const& parent,
        _In_ std::wstring_view key) noexcept
    {
        auto const value = SafeLookup(parent, key, json::JsonValueType::String);

        return value == nullptr ? winrt::hstring{} : value.GetString();
    }

    bool SafeGetNamedBoolean(
        _In_ json::JsonObject const& parent,
        _In_ std::wstring_view key,
        _In_ bool defaultValue) noexcept
    {
        auto const value = SafeLookup(parent, key, json::JsonValueType::Boolean);

        return value == nullptr ? defaultValue : value.GetBoolean();
    }

    // JSON permits values which are not finite once parsed. A NaN volume would survive a clamp and
    // silence the synthesizer, so it is rejected here rather than in the engine.
    bool TryGetNamedFiniteNumber(
        _In_ json::JsonObject const& parent,
        _In_ std::wstring_view key,
        _Out_ double& value) noexcept
    {
        value = 0.0;

        auto const found = SafeLookup(parent, key, json::JsonValueType::Number);

        if (found == nullptr)
        {
            return false;
        }

        auto const number = found.GetNumber();

        if (!std::isfinite(number))
        {
            return false;
        }

        value = number;

        return true;
    }
}

_Use_decl_annotations_
HRESULT
CMidi2MidiSynthConfigurationManager::Initialize(
    GUID transportId,
    IMidiDeviceManager* midiDeviceManager,
    IMidiServiceConfigurationManager* midiServiceConfigurationManager
)
{
    UNREFERENCED_PARAMETER(midiServiceConfigurationManager);

    TraceLoggingWrite(
        MidiSynthTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, midiDeviceManager);
    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManager), (void**)&m_midiDeviceManager));

    m_transportId = transportId;

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2MidiSynthConfigurationManager::UpdateConfiguration(
    LPCWSTR configurationJsonSection,
    LPWSTR* response
)
{
    TraceLoggingWrite(
        MidiSynthTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(configurationJsonSection, "json")
    );

    // an empty section is normal: the synthesizer runs on its defaults until someone changes one
    if (configurationJsonSection == nullptr) return S_OK;

    auto responseObject = internal::BuildConfigurationResponseObject(false);

    try
    {
        json::JsonObject jsonObject{};

        if (!json::JsonObject::TryParse(winrt::to_hstring(configurationJsonSection), jsonObject))
        {
            TraceLoggingWrite(
                MidiSynthTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Failed to parse configuration JSON", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

            internal::SetConfigurationResponseObjectFail(
                responseObject, internal::ResourceGetWString(IDS_ERROR_PARSING_JSON));

            internal::JsonStringifyObjectToOutParam(responseObject, response);

            RETURN_IF_FAILED(E_INVALIDARG);
        }

        // A command takes precedence: when one is present nothing else in the payload is read.
        if (internal::MidiTransportCommandHelper::TransportObjectContainsCommand(jsonObject))
        {
            auto const hr = ProcessCommand(jsonObject, responseObject);

            internal::JsonStringifyObjectToOutParam(responseObject, response);

            return hr;
        }

        RETURN_IF_FAILED(ProcessSettings(jsonObject, responseObject));

        internal::JsonStringifyObjectToOutParam(responseObject, response);
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();

        internal::SetConfigurationResponseObjectFail(
            responseObject, internal::ResourceGetWString(IDS_ERROR_PARSING_JSON));

        internal::JsonStringifyObjectToOutParam(responseObject, response);

        RETURN_IF_FAILED(E_FAIL);
    }

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2MidiSynthConfigurationManager::ProcessCommand(
    json::JsonObject const& transportObject,
    json::JsonObject& responseObject)
{
    auto device = TransportState::Current().GetDevice();
    RETURN_HR_IF_NULL(E_UNEXPECTED, device);

    auto command = internal::MidiTransportCommandHelper::ParseCommand(transportObject);
    auto const verb = command.Command();

    // Reporting state is a read, so it does not go through the settings path at all. Without it a
    // caller would have to write something just to find out what is currently set.
    if (_wcsicmp(verb.c_str(), MIDI_SYNTH_COMMAND_STATUS) == 0)
    {
        AddCurrentSettingsToResponse(responseObject);
        internal::SetConfigurationResponseObjectSuccess(responseObject);

        return S_OK;
    }

    if (_wcsicmp(verb.c_str(), MIDI_SYNTH_COMMAND_SET_DRUM_CHANNEL) == 0)
    {
        auto const* const arguments = command.Arguments();
        RETURN_HR_IF_NULL(E_UNEXPECTED, arguments);

        auto const channelArgument = arguments->find(MIDI_SYNTH_COMMAND_ARG_CHANNEL);
        auto const drumArgument = arguments->find(MIDI_SYNTH_COMMAND_ARG_IS_DRUM_CHANNEL);

        if (channelArgument == arguments->end() || drumArgument == arguments->end())
        {
            internal::SetConfigurationResponseObjectFail(
                responseObject, internal::ResourceGetWString(IDS_ERROR_UNRECOGNIZED_COMMAND));

            RETURN_HR(E_INVALIDARG);
        }

        uint32_t channel{ MidiSynth::MidiChannelCount };

        // The argument comes from outside the service, so it is parsed rather than trusted.
        try
        {
            size_t consumed{ 0 };
            auto const parsed = std::stoul(channelArgument->second, &consumed);

            if (consumed == channelArgument->second.length() && parsed < MidiSynth::MidiChannelCount)
            {
                channel = static_cast<uint32_t>(parsed);
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        if (channel >= MidiSynth::MidiChannelCount)
        {
            internal::SetConfigurationResponseObjectFail(
                responseObject, internal::ResourceGetWString(IDS_ERROR_UNRECOGNIZED_COMMAND));

            RETURN_HR(E_INVALIDARG);
        }

        RETURN_IF_FAILED(device->SetDrumChannel(
            static_cast<uint8_t>(channel),
            _wcsicmp(drumArgument->second.c_str(), L"true") == 0));

        internal::SetConfigurationResponseObjectSuccess(responseObject);

        return S_OK;
    }

    if (_wcsicmp(verb.c_str(), MIDI_SYNTH_COMMAND_SOUND_SET) == 0)
    {
        RETURN_IF_FAILED(device->AddSoundSetInfoToResponse(responseObject));

        internal::SetConfigurationResponseObjectSuccess(responseObject);

        return S_OK;
    }

    if (_wcsicmp(verb.c_str(), MIDI_SYNTH_COMMAND_ENABLE) == 0 ||
        _wcsicmp(verb.c_str(), MIDI_SYNTH_COMMAND_DISABLE) == 0)
    {
        auto settings = device->Settings();
        settings.Enabled = (_wcsicmp(verb.c_str(), MIDI_SYNTH_COMMAND_ENABLE) == 0);

        RETURN_IF_FAILED(device->ApplySettings(settings));

        AddCurrentSettingsToResponse(responseObject);
        internal::SetConfigurationResponseObjectSuccess(responseObject);

        return S_OK;
    }

    TraceLoggingWrite(
        MidiSynthTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_WARNING,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Unrecognized command", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(verb.c_str(), "verb")
    );

    internal::SetConfigurationResponseObjectFail(
        responseObject, internal::ResourceGetWString(IDS_ERROR_UNRECOGNIZED_COMMAND));

    RETURN_HR(E_INVALIDARG);
}


_Use_decl_annotations_
HRESULT
CMidi2MidiSynthConfigurationManager::ProcessSettings(
    json::JsonObject const& jsonObject,
    json::JsonObject& responseObject)
{
    auto device = TransportState::Current().GetDevice();
    RETURN_HR_IF_NULL(E_UNEXPECTED, device);

    // Start from what is in effect, so a section which sets only one value leaves the rest alone.
    auto settings = device->Settings();

    auto const synthModeText = SafeGetNamedString(jsonObject, MIDI_SYNTH_JSON_SYNTH_MODE_PROPERTY_KEY);

    if (!synthModeText.empty())
    {
        if (!MidiSynthSettings::TryParseSynthMode(synthModeText.c_str(), settings.SynthMode))
        {
            internal::SetConfigurationResponseObjectFail(
                responseObject, internal::ResourceGetWString(IDS_ERROR_UNKNOWN_SYNTH_MODE));

            RETURN_HR(E_INVALIDARG);
        }
    }

    auto const audioModeText = SafeGetNamedString(jsonObject, MIDI_SYNTH_JSON_AUDIO_MODE_PROPERTY_KEY);

    if (!audioModeText.empty())
    {
        if (!MidiSynthSettings::TryParseAudioMode(audioModeText.c_str(), settings.AudioMode))
        {
            internal::SetConfigurationResponseObjectFail(
                responseObject, internal::ResourceGetWString(IDS_ERROR_UNKNOWN_AUDIO_MODE));

            RETURN_HR(E_INVALIDARG);
        }

        // Exclusive mode and ASIO are in the configuration schema because they are the modes this
        // will offer, but neither is built yet. Silently falling back to shared would be worse
        // than refusing: the caller would believe it had taken the device.
        if (!settings.AudioModeIsImplemented())
        {
            internal::SetConfigurationResponseObjectFail(
                responseObject, internal::ResourceGetWString(IDS_ERROR_AUDIO_MODE_NOT_AVAILABLE));

            RETURN_HR(E_NOTIMPL);
        }
    }

    settings.Enabled = SafeGetNamedBoolean(jsonObject, MIDI_SYNTH_JSON_ENABLED_PROPERTY_KEY, settings.Enabled);
    settings.EffectsEnabled = SafeGetNamedBoolean(jsonObject, MIDI_SYNTH_JSON_EFFECTS_PROPERTY_KEY, settings.EffectsEnabled);

    auto const bankSelectText = SafeGetNamedString(jsonObject, MIDI_SYNTH_JSON_BANK_SELECT_PROPERTY_KEY);

    if (!bankSelectText.empty())
    {
        if (!MidiSynthSettings::TryParseBankSelectMode(bankSelectText.c_str(), settings.BankSelect))
        {
            internal::SetConfigurationResponseObjectFail(
                responseObject, internal::ResourceGetWString(IDS_ERROR_UNKNOWN_BANK_SELECT_MODE));

            RETURN_HR(E_INVALIDARG);
        }
    }

    double requestedVolume{ 0.0 };

    if (TryGetNamedFiniteNumber(jsonObject, MIDI_SYNTH_JSON_VOLUME_PROPERTY_KEY, requestedVolume))
    {
        // Out of range is clamped rather than refused: a customer dragging a slider should not get
        // an error, and the engine owns the limits.
        settings.VolumeDecibels = (std::max)(MidiSynth::SynthEngine::MinimumUserVolumeDb,
            (std::min)(MidiSynth::SynthEngine::MaximumUserVolumeDb, requestedVolume));
    }

    RETURN_IF_FAILED(device->ApplySettings(settings));

    TraceLoggingWrite(
        MidiSynthTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Settings applied", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(MidiSynthSettings::SynthModeToString(settings.SynthMode), "synth mode"),
        TraceLoggingWideString(MidiSynthSettings::AudioModeToString(settings.AudioMode), "audio mode"),
        TraceLoggingBool(settings.Enabled, "enabled")
    );

    AddCurrentSettingsToResponse(responseObject);

    internal::SetConfigurationResponseObjectSuccess(responseObject);

    return S_OK;
}


_Use_decl_annotations_
void
CMidi2MidiSynthConfigurationManager::AddCurrentSettingsToResponse(json::JsonObject& responseObject)
{
    auto device = TransportState::Current().GetDevice();

    if (device == nullptr)
    {
        return;
    }

    auto const settings = device->Settings();

    responseObject.SetNamedValue(MIDI_SYNTH_JSON_SYNTH_MODE_PROPERTY_KEY,
        json::JsonValue::CreateStringValue(MidiSynthSettings::SynthModeToString(settings.SynthMode)));

    responseObject.SetNamedValue(MIDI_SYNTH_JSON_AUDIO_MODE_PROPERTY_KEY,
        json::JsonValue::CreateStringValue(MidiSynthSettings::AudioModeToString(settings.AudioMode)));

    responseObject.SetNamedValue(MIDI_SYNTH_JSON_ENABLED_PROPERTY_KEY,
        json::JsonValue::CreateBooleanValue(settings.Enabled));

    responseObject.SetNamedValue(MIDI_SYNTH_JSON_BANK_SELECT_PROPERTY_KEY,
        json::JsonValue::CreateStringValue(MidiSynthSettings::BankSelectModeToString(settings.BankSelect)));

    responseObject.SetNamedValue(MIDI_SYNTH_JSON_VOLUME_PROPERTY_KEY,
        json::JsonValue::CreateNumberValue(settings.VolumeDecibels));

    responseObject.SetNamedValue(MIDI_SYNTH_JSON_EFFECTS_PROPERTY_KEY,
        json::JsonValue::CreateBooleanValue(settings.EffectsEnabled));
}


HRESULT
CMidi2MidiSynthConfigurationManager::Shutdown()
{
    TraceLoggingWrite(
        MidiSynthTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    m_midiDeviceManager.reset();

    return S_OK;
}
