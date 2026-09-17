// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "cmd_synth.h"
#include "console_output.h"
#include "console_table.h"
#include "midi_formatting.h"
#include "strings.h"

// windows.h defines GetObject as a macro, which renames the JSON accessor at the call site.
// try_as<JsonObject> is not a substitute: it returns null for an element of a JsonArray.
#pragma push_macro("GetObject")
#undef GetObject

namespace midi2console
{
    namespace
    {
        // Must match the CLSID the synthesizer transport is registered under.
        constexpr winrt::guid SynthTransportId
        {
            0x7605713e, 0xfea9, 0x409d, { 0xa9, 0x0f, 0xa8, 0x12, 0x33, 0x20, 0x0d, 0x0a }
        };

        constexpr wchar_t VerbStatus[]{ L"status" };
        constexpr wchar_t VerbEnable[]{ L"enable" };
        constexpr wchar_t VerbDisable[]{ L"disable" };
        constexpr wchar_t VerbSoundSet[]{ L"soundset" };

        // Sends a verb and hands back whatever the transport reported, so the caller can print the
        // resulting state rather than assuming the change took.
        midi2config::MidiServiceConfigResponse SendVerb(_In_ std::wstring_view verb)
        {
            midi2config::MidiServiceTransportCommand command(SynthTransportId, winrt::hstring{ verb });

            return midi2config::MidiServiceTransportPluginConfigManager::SendCommand(command);
        }

        bool Succeeded(_In_ midi2config::MidiServiceConfigResponse const& response)
        {
            return response != nullptr &&
                response.Status() == midi2config::MidiServiceConfigResponseStatus::Success;
        }

        void ReportFailure(_In_ midi2config::MidiServiceConfigResponse const& response)
        {
            auto const message = response == nullptr
                ? std::string{}
                : ToUtf8(response.ServiceErrorMessage());

            WriteErrorLine(FormatResourceString(IDS_SYNTH_COMMAND_FAILED, message));
        }

        void WriteState(_In_ midi2config::MidiServiceConfigResponse const& response)
        {
            auto const json = response.ResponseJson();

            if (json == nullptr)
            {
                return;
            }

            auto const enabled = json.GetNamedBoolean(L"enabled", false);

            ConsoleTable table{ ResourceString(IDS_SYNTH_TABLE_TITLE) };

            table.AddColumn(ResourceString(IDS_SYNTH_LABEL_SETTING), ColumnAlignment::Left);
            table.AddColumn(ResourceString(IDS_SYNTH_LABEL_VALUE), ColumnAlignment::Left);
            table.SetLastColumnShrinkable();

            table.BeginRow();
            table.AddCell(ResourceString(IDS_SYNTH_LABEL_ENABLED));
            table.AddCell(ResourceString(enabled ? IDS_SYNTH_STATE_ON : IDS_SYNTH_STATE_OFF));

            table.BeginRow();
            table.AddCell(ResourceString(IDS_SYNTH_LABEL_SYNTH_MODE));
            table.AddCell(ToUtf8(json.GetNamedString(L"synthMode", L"")));

            table.BeginRow();
            table.AddCell(ResourceString(IDS_SYNTH_LABEL_AUDIO_MODE));
            table.AddCell(ToUtf8(json.GetNamedString(L"audioMode", L"")));

            table.BeginRow();
            table.AddCell(ResourceString(IDS_SYNTH_LABEL_BANK_SELECT));
            table.AddCell(ToUtf8(json.GetNamedString(L"bankSelectMode", L"")));

            table.BeginRow();
            table.AddCell(ResourceString(IDS_SYNTH_LABEL_VOLUME));
            table.AddCell(std::format("{:.1f} dB", json.GetNamedNumber(L"volumeDecibels", 0.0)));

            table.BeginRow();
            table.AddCell(ResourceString(IDS_SYNTH_LABEL_EFFECTS));
            table.AddCell(ResourceString(
                json.GetNamedBoolean(L"effectsEnabled", true) ? IDS_SYNTH_STATE_ON : IDS_SYNTH_STATE_OFF));

            table.Render();

            // The off state is worth explaining, because it does more than mute: this is what
            // frees the audio device for an exclusive mode or ASIO application.
            WriteInfoLine(ResourceString(enabled ? IDS_SYNTH_EXPLAIN_ON : IDS_SYNTH_EXPLAIN_OFF));
        }

        // The command verb changes the running service. Persisting is a separate step, and it
        // writes the settings object rather than the verb, because a command is an action and the
        // configuration file holds state.
        void SaveEnabledState(_In_ bool enabled)
        {
            json::JsonObject config;
            config.SetNamedValue(L"enabled", json::JsonValue::CreateBooleanValue(enabled));

            auto const response =
                midi2config::MidiServiceTransportPluginConfigManager::SaveUpdate(SynthTransportId, config);

            if (response != nullptr && response.Success())
            {
                WriteSuccessLine(ResourceString(IDS_SYNTH_SAVED_TO_CONFIG));
                return;
            }

            auto const message = response == nullptr ? std::string{} : ToUtf8(response.ErrorMessage());

            WriteWarningLine(FormatResourceString(IDS_SYNTH_SAVE_FAILED, message));
        }
    }

    int RunSynthStatusCommand()
    {
        auto const response = SendVerb(VerbStatus);

        if (!Succeeded(response))
        {
            ReportFailure(response);
            return 1;
        }

        WriteState(response);

        return 0;
    }

    int RunSynthEnableCommand(_In_ SynthEnableOptions const& options)
    {
        auto const response = SendVerb(options.Enabled ? VerbEnable : VerbDisable);

        if (!Succeeded(response))
        {
            ReportFailure(response);
            return 1;
        }

        WriteSuccessLine(ResourceString(options.Enabled ? IDS_SYNTH_ENABLED : IDS_SYNTH_DISABLED));

        if (!options.Temporary)
        {
            SaveEnabledState(options.Enabled);
        }

        WriteState(response);

        return 0;
    }

    int RunSynthSoundSetCommand()
    {
        auto const response = SendVerb(VerbSoundSet);

        if (!Succeeded(response) || response.ResponseJson() == nullptr)
        {
            ReportFailure(response);
            return 1;
        }

        auto const json = response.ResponseJson();

        ConsoleTable table{ ResourceString(IDS_SYNTH_SOUNDSET_TITLE) };

        table.AddColumn(ResourceString(IDS_SYNTH_LABEL_SETTING), ColumnAlignment::Left);
        table.AddColumn(ResourceString(IDS_SYNTH_LABEL_VALUE), ColumnAlignment::Left);
        table.SetLastColumnShrinkable();

        auto addRow = [&](UINT labelId, std::string const& value)
        {
            table.BeginRow();
            table.AddCell(ResourceString(labelId));
            table.AddCell(value);
        };

        addRow(IDS_SYNTH_LABEL_SOUNDSET_NAME, ToUtf8(json.GetNamedString(L"soundSetName", L"")));
        addRow(IDS_SYNTH_LABEL_SOUNDSET_VERSION, ToUtf8(json.GetNamedString(L"soundSetVersion", L"")));
        addRow(IDS_SYNTH_LABEL_SOUNDSET_PATH, ToUtf8(json.GetNamedString(L"soundSetPath", L"")));
        addRow(IDS_SYNTH_LABEL_SOUNDSET_MELODIC,
            std::format("{:.0f}", json.GetNamedNumber(L"melodicCount", 0.0)));
        addRow(IDS_SYNTH_LABEL_SOUNDSET_WAVES,
            std::format("{:.0f}", json.GetNamedNumber(L"waveCount", 0.0)));

        table.Render();

        auto const kits = json.GetNamedArray(L"drumKits", nullptr);

        if (kits == nullptr || kits.Size() == 0)
        {
            return 0;
        }

        ConsoleTable kitTable{ ResourceString(IDS_SYNTH_KITS_TITLE) };

        kitTable.AddColumn(ResourceString(IDS_SYNTH_LABEL_KIT_PROGRAM), ColumnAlignment::Right);
        kitTable.AddColumn(ResourceString(IDS_SYNTH_LABEL_KIT_NAME), ColumnAlignment::Left);
        kitTable.SetLastColumnShrinkable();

        for (auto const& entry : kits)
        {
            auto const kit = entry.GetObject();

            kitTable.BeginRow();
            kitTable.AddCell(std::format("{:.0f}", kit.GetNamedNumber(L"program", 0.0)));
            kitTable.AddCell(ToUtf8(kit.GetNamedString(L"name", L"")));
        }

        kitTable.Render();

        // Kits are addressed by program change on a drum channel, and until now only channel 10
        // could be one. Saying so is the difference between nine kits and one.
        WriteInfoLine(ResourceString(IDS_SYNTH_KITS_HELP));

        return 0;
    }

    int RunSynthConfigureCommand(_In_ SynthConfigureOptions const& options)
    {
        if (options.SynthMode.empty() && options.AudioMode.empty() &&
            options.BankSelectMode.empty() && options.Effects.empty() && options.Volume.empty())
        {
            WriteErrorLine(ResourceString(IDS_SYNTH_NOTHING_TO_CHANGE));
            return 1;
        }

        json::JsonObject config;

        auto setString = [&config](std::wstring_view key, std::string const& value)
        {
            if (!value.empty())
            {
                config.SetNamedValue(winrt::hstring{ key },
                    json::JsonValue::CreateStringValue(winrt::hstring{ FromUtf8(value) }));
            }
        };

        setString(L"synthMode", options.SynthMode);
        setString(L"audioMode", options.AudioMode);
        setString(L"bankSelectMode", options.BankSelectMode);

        if (!options.Effects.empty())
        {
            if (!EqualsIgnoreCase(options.Effects, "on") && !EqualsIgnoreCase(options.Effects, "off"))
            {
                WriteErrorLine(FormatResourceString(IDS_ERROR_INVALID_ENUM_VALUE,
                    options.Effects, std::string{ "--effects" }));
                return 1;
            }

            config.SetNamedValue(L"effectsEnabled",
                json::JsonValue::CreateBooleanValue(EqualsIgnoreCase(options.Effects, "on")));
        }

        if (!options.Volume.empty())
        {
            try
            {
                config.SetNamedValue(L"volumeDecibels",
                    json::JsonValue::CreateNumberValue(std::stod(options.Volume)));
            }
            catch (...)
            {
                WriteErrorLine(FormatResourceString(IDS_ERROR_INVALID_ENUM_VALUE,
                    options.Volume, std::string{ "--volume" }));
                return 1;
            }
        }

        auto const response =
            midi2config::MidiServiceTransportPluginConfigManager::SendUpdate(SynthTransportId, config);

        if (!Succeeded(response))
        {
            ReportFailure(response);
            return 1;
        }

        if (!options.Temporary)
        {
            auto const saveResponse =
                midi2config::MidiServiceTransportPluginConfigManager::SaveUpdate(SynthTransportId, config);

            if (saveResponse != nullptr && saveResponse.Success())
            {
                WriteSuccessLine(ResourceString(IDS_SYNTH_SAVED_TO_CONFIG));
            }
            else
            {
                auto const message =
                    saveResponse == nullptr ? std::string{} : ToUtf8(saveResponse.ErrorMessage());

                WriteWarningLine(FormatResourceString(IDS_SYNTH_SAVE_FAILED, message));
            }
        }

        WriteState(response);

        return 0;
    }
}

#pragma pop_macro("GetObject")
