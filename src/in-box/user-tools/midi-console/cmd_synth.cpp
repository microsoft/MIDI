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

namespace midi2console
{
    namespace
    {
        template <typename TEnum>
        struct EnumToken
        {
            char const* Token;
            TEnum Value;
        };

        // One table drives both parsing and display, so a value printed by "midi synth status" can
        // be pasted straight back into "midi synth configure".
        constexpr EnumToken<midi2synth::MidiSynthRenderMode> RenderModeTokens[]
        {
            { "compatible", midi2synth::MidiSynthRenderMode::Compatible },
            { "modern",     midi2synth::MidiSynthRenderMode::Modern },
        };

        constexpr EnumToken<midi2synth::MidiSynthAudioOutputMode> AudioOutputModeTokens[]
        {
            { "shared",           midi2synth::MidiSynthAudioOutputMode::WasapiShared },
            { "sharedLowLatency", midi2synth::MidiSynthAudioOutputMode::WasapiSharedLowLatency },
            { "exclusive",        midi2synth::MidiSynthAudioOutputMode::WasapiExclusive },
            { "asio",             midi2synth::MidiSynthAudioOutputMode::Asio },
        };

        constexpr EnumToken<midi2synth::MidiSynthBankSelectMode> BankSelectModeTokens[]
        {
            { "gs",        midi2synth::MidiSynthBankSelectMode::RolandGS },
            { "xg",        midi2synth::MidiSynthBankSelectMode::YamahaXG },
            { "gm2",       midi2synth::MidiSynthBankSelectMode::GeneralMidi2 },
            { "automatic", midi2synth::MidiSynthBankSelectMode::Automatic },
        };

        template <typename TEnum, size_t TCount>
        std::optional<TEnum> ParseEnum(
            _In_ EnumToken<TEnum> const (&tokens)[TCount],
            _In_ std::string const& text)
        {
            for (auto const& entry : tokens)
            {
                if (EqualsIgnoreCase(text, entry.Token))
                {
                    return entry.Value;
                }
            }

            return std::nullopt;
        }

        template <typename TEnum, size_t TCount>
        std::string TokenForEnum(_In_ EnumToken<TEnum> const (&tokens)[TCount], _In_ TEnum value)
        {
            for (auto const& entry : tokens)
            {
                if (entry.Value == value)
                {
                    return entry.Token;
                }
            }

            return {};
        }

        // Null when the synthesizer is not installed or the service did not answer. The two are
        // reported differently because they call for different things from the customer.
        midi2synth::MidiSynthStatus GetStatusOrReportFailure()
        {
            if (!midi2synth::MidiSynthManager::IsTransportAvailable())
            {
                WriteErrorLine(ResourceString(IDS_SYNTH_NOT_AVAILABLE));
                return nullptr;
            }

            auto const status = midi2synth::MidiSynthManager::GetStatus();

            if (status == nullptr)
            {
                WriteErrorLine(FormatResourceString(IDS_SYNTH_COMMAND_FAILED, std::string{}));
            }

            return status;
        }

        void WriteState(_In_ midi2synth::MidiSynthStatus const& status)
        {
            if (status == nullptr)
            {
                return;
            }

            ConsoleTable table{ ResourceString(IDS_SYNTH_TABLE_TITLE) };

            table.AddColumn(ResourceString(IDS_SYNTH_LABEL_SETTING), ColumnAlignment::Left);
            table.AddColumn(ResourceString(IDS_SYNTH_LABEL_VALUE), ColumnAlignment::Left);
            table.SetLastColumnShrinkable();

            auto addRow = [&table](UINT labelId, std::string const& value)
            {
                table.BeginRow();
                table.AddCell(ResourceString(labelId));
                table.AddCell(value);
            };

            auto const enabled = status.IsEnabled();

            addRow(IDS_SYNTH_LABEL_ENABLED,
                ResourceString(enabled ? IDS_SYNTH_STATE_ON : IDS_SYNTH_STATE_OFF));
            addRow(IDS_SYNTH_LABEL_SYNTH_MODE, TokenForEnum(RenderModeTokens, status.RenderMode()));
            addRow(IDS_SYNTH_LABEL_AUDIO_MODE, TokenForEnum(AudioOutputModeTokens, status.AudioOutputMode()));
            addRow(IDS_SYNTH_LABEL_BANK_SELECT, TokenForEnum(BankSelectModeTokens, status.BankSelectMode()));
            addRow(IDS_SYNTH_LABEL_VOLUME, std::format("{:.1f} dB", status.VolumeDecibels()));
            addRow(IDS_SYNTH_LABEL_EFFECTS,
                ResourceString(status.AreEffectsEnabled() ? IDS_SYNTH_STATE_ON : IDS_SYNTH_STATE_OFF));

            table.Render();

            // The off state is worth explaining, because it does more than mute: this is what
            // frees the audio device for an exclusive mode or ASIO application.
            WriteInfoLine(ResourceString(enabled ? IDS_SYNTH_EXPLAIN_ON : IDS_SYNTH_EXPLAIN_OFF));
        }

        // Applies the configuration, then optionally persists it. Sending and saving are separate
        // because a temporary change is one the customer can undo by restarting the service.
        bool ApplyConfig(_In_ midi2synth::MidiSynthConfig const& config, _In_ bool temporary)
        {
            auto const response =
                midi2config::MidiServiceTransportPluginConfigManager::SendUpdate(config);

            if (response == nullptr ||
                response.Status() != midi2config::MidiServiceConfigResponseStatus::Success)
            {
                auto const message = response == nullptr
                    ? std::string{}
                    : ToUtf8(response.ServiceErrorMessage());

                WriteErrorLine(FormatResourceString(IDS_SYNTH_COMMAND_FAILED, message));
                return false;
            }

            if (temporary)
            {
                return true;
            }

            auto const saveResponse =
                midi2config::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

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

            return true;
        }
    }

    int RunSynthStatusCommand()
    {
        auto const status = GetStatusOrReportFailure();

        if (status == nullptr)
        {
            return 1;
        }

        WriteState(status);

        return 0;
    }

    int RunSynthEnableCommand(_In_ SynthEnableOptions const& options)
    {
        auto const status = GetStatusOrReportFailure();

        if (status == nullptr)
        {
            return 1;
        }

        midi2synth::MidiSynthConfig config{ status };
        config.IsEnabled(options.Enabled);

        if (!ApplyConfig(config, options.Temporary))
        {
            return 1;
        }

        WriteSuccessLine(ResourceString(options.Enabled ? IDS_SYNTH_ENABLED : IDS_SYNTH_DISABLED));

        WriteState(midi2synth::MidiSynthManager::GetStatus());

        return 0;
    }

    int RunSynthSoundSetCommand()
    {
        if (!midi2synth::MidiSynthManager::IsTransportAvailable())
        {
            WriteErrorLine(ResourceString(IDS_SYNTH_NOT_AVAILABLE));
            return 1;
        }

        auto const soundSet = midi2synth::MidiSynthManager::GetSoundSetInfo();

        if (soundSet == nullptr)
        {
            WriteErrorLine(FormatResourceString(IDS_SYNTH_COMMAND_FAILED, std::string{}));
            return 1;
        }

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

        addRow(IDS_SYNTH_LABEL_SOUNDSET_NAME, ToUtf8(soundSet.Name()));
        addRow(IDS_SYNTH_LABEL_SOUNDSET_VERSION, ToUtf8(soundSet.Version()));
        addRow(IDS_SYNTH_LABEL_SOUNDSET_PATH, ToUtf8(soundSet.FilePath()));
        addRow(IDS_SYNTH_LABEL_SOUNDSET_MELODIC, std::format("{}", soundSet.MelodicInstrumentCount()));
        addRow(IDS_SYNTH_LABEL_SOUNDSET_WAVES, std::format("{}", soundSet.WaveCount()));

        table.Render();

        auto const kits = soundSet.DrumKits();

        if (kits == nullptr || kits.Size() == 0)
        {
            return 0;
        }

        ConsoleTable kitTable{ ResourceString(IDS_SYNTH_KITS_TITLE) };

        kitTable.AddColumn(ResourceString(IDS_SYNTH_LABEL_KIT_PROGRAM), ColumnAlignment::Right);
        kitTable.AddColumn(ResourceString(IDS_SYNTH_LABEL_KIT_NAME), ColumnAlignment::Left);
        kitTable.SetLastColumnShrinkable();

        for (auto const& kit : kits)
        {
            kitTable.BeginRow();
            kitTable.AddCell(std::format("{}", static_cast<uint32_t>(kit.Program())));
            kitTable.AddCell(ToUtf8(kit.Name()));
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

        auto const status = GetStatusOrReportFailure();

        if (status == nullptr)
        {
            return 1;
        }

        // Starting from the current status is what makes changing one setting leave the rest alone.
        midi2synth::MidiSynthConfig config{ status };

        if (!options.SynthMode.empty())
        {
            auto const parsed = ParseEnum(RenderModeTokens, options.SynthMode);

            if (!parsed)
            {
                WriteErrorLine(FormatResourceString(IDS_ERROR_INVALID_ENUM_VALUE,
                    options.SynthMode, std::string{ "--synth-mode" }));
                return 1;
            }

            config.RenderMode(parsed.value());
        }

        if (!options.AudioMode.empty())
        {
            auto const parsed = ParseEnum(AudioOutputModeTokens, options.AudioMode);

            if (!parsed)
            {
                WriteErrorLine(FormatResourceString(IDS_ERROR_INVALID_ENUM_VALUE,
                    options.AudioMode, std::string{ "--audio-mode" }));
                return 1;
            }

            config.AudioOutputMode(parsed.value());
        }

        if (!options.BankSelectMode.empty())
        {
            auto const parsed = ParseEnum(BankSelectModeTokens, options.BankSelectMode);

            if (!parsed)
            {
                WriteErrorLine(FormatResourceString(IDS_ERROR_INVALID_ENUM_VALUE,
                    options.BankSelectMode, std::string{ "--bank-select-mode" }));
                return 1;
            }

            config.BankSelectMode(parsed.value());
        }

        if (!options.Effects.empty())
        {
            if (!EqualsIgnoreCase(options.Effects, "on") && !EqualsIgnoreCase(options.Effects, "off"))
            {
                WriteErrorLine(FormatResourceString(IDS_ERROR_INVALID_ENUM_VALUE,
                    options.Effects, std::string{ "--effects" }));
                return 1;
            }

            config.AreEffectsEnabled(EqualsIgnoreCase(options.Effects, "on"));
        }

        if (!options.Volume.empty())
        {
            try
            {
                config.VolumeDecibels(std::stod(options.Volume));
            }
            catch (...)
            {
                WriteErrorLine(FormatResourceString(IDS_ERROR_INVALID_ENUM_VALUE,
                    options.Volume, std::string{ "--volume" }));
                return 1;
            }
        }

        if (!ApplyConfig(config, options.Temporary))
        {
            return 1;
        }

        WriteState(midi2synth::MidiSynthManager::GetStatus());

        return 0;
    }
}
