// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "cmd_loopback.h"
#include "console_output.h"
#include "console_table.h"
#include "midi_formatting.h"
#include "strings.h"

namespace midi2console
{
    namespace
    {
        // The create commands accept either explicit A/B names or a root name that gets the
        // "(A)" and "(B)" suffixes, matching the shipping console.
        void ApplyLoopbackNames(
            _In_ LoopbackCreateOptions const& options,
            _Out_ std::string& nameA,
            _Out_ std::string& nameB)
        {
            if (!options.RootName.empty())
            {
                nameA = options.RootName + " (A)";
                nameB = options.RootName + " (B)";
                return;
            }

            nameA = options.NameA.empty() ? "MIDI Console Loopback (A)" : options.NameA;
            nameB = options.NameB.empty() ? "MIDI Console Loopback (B)" : options.NameB;
        }

        bool TryGetAssociationId(_In_ std::string const& text, _Out_ winrt::guid& associationId)
        {
            bool parsed{ false };

            associationId = ParseGuid(text, parsed);

            if (!parsed)
            {
                WriteErrorLine(ResourceString(IDS_ERROR_INVALID_ASSOCIATION_ID));
            }

            return parsed;
        }
    }

    int RunLoopbackListCommand()
    {
        if (!midi2loop::MidiLoopbackManager::IsTransportAvailable())
        {
            WriteErrorLine(ResourceString(IDS_LOOPBACK_NONE));
            return 1;
        }

        auto const entries = midi2loop::MidiLoopbackManager::GetActiveLoopbackEntries();

        if (entries == nullptr || entries.Size() == 0)
        {
            WriteWarningLine(ResourceString(IDS_LOOPBACK_NONE));
            return 0;
        }

        ConsoleTable table{ ResourceString(IDS_LOOPBACK_TABLE_TITLE) };

        table.AddColumn(ResourceString(IDS_LOOPBACK_LABEL_ASSOCIATION_ID), ColumnAlignment::Left, guidTextStyle);
        table.AddColumn(ResourceString(IDS_LOOPBACK_LABEL_ENDPOINT_A), ColumnAlignment::Left, endpointNameTextStyle);
        table.SetLastColumnShrinkable();
        table.AddColumn(ResourceString(IDS_LOOPBACK_LABEL_ENDPOINT_B), ColumnAlignment::Left, endpointNameTextStyle);
        table.SetLastColumnShrinkable();

        for (auto const& entry : entries)
        {
            table.BeginRow();
            table.AddCell(FormatGuid(entry.AssociationId()));
            table.AddCell(entry.EndpointA() == nullptr ? std::string{} : ToUtf8(entry.EndpointA().Name()));
            table.AddCell(entry.EndpointB() == nullptr ? std::string{} : ToUtf8(entry.EndpointB().Name()));
        }

        table.Render();

        return 0;
    }

    int RunLoopbackCreateCommand(_In_ LoopbackCreateOptions const& options)
    {
        if (!midi2loop::MidiLoopbackManager::IsTransportAvailable())
        {
            WriteErrorLine(FormatResourceString(IDS_LOOPBACK_CREATE_FAILED, std::string{}));
            return 1;
        }

        std::string nameA;
        std::string nameB;

        ApplyLoopbackNames(options, nameA, nameB);

        midi2loop::MidiLoopbackCreationConfig config;

        config.EndpointDefinitionA().Name(winrt::hstring{ FromUtf8(nameA) });
        config.EndpointDefinitionB().Name(winrt::hstring{ FromUtf8(nameB) });

        if (!options.UniqueIdentifier.empty())
        {
            config.EndpointDefinitionA().UniqueId(winrt::hstring{ FromUtf8(options.UniqueIdentifier) });
            config.EndpointDefinitionB().UniqueId(winrt::hstring{ FromUtf8(options.UniqueIdentifier) });
        }

        auto const response = midi2loop::MidiLoopbackManager::CreateTransientLoopback(config);

        if (response == nullptr || !response.Success())
        {
            auto const message = response == nullptr ? std::string{} : ToUtf8(response.ErrorMessage());

            WriteErrorLine(FormatResourceString(IDS_LOOPBACK_CREATE_FAILED, message));
            return 1;
        }

        WriteSuccessLine(ResourceString(IDS_LOOPBACK_CREATED));

        auto const entry = response.CreatedLoopbackEntry();

        if (entry != nullptr)
        {
            WriteField(ResourceString(IDS_LOOPBACK_LABEL_ASSOCIATION_ID),
                FormatGuid(entry.AssociationId()), guidTextStyle);

            if (entry.EndpointA() != nullptr)
            {
                WriteField(ResourceString(IDS_LOOPBACK_LABEL_ENDPOINT_A),
                    ToUtf8(entry.EndpointA().EndpointDeviceId()), endpointIdTextStyle);
            }

            if (entry.EndpointB() != nullptr)
            {
                WriteField(ResourceString(IDS_LOOPBACK_LABEL_ENDPOINT_B),
                    ToUtf8(entry.EndpointB().EndpointDeviceId()), endpointIdTextStyle);
            }

            if (options.SaveToConfig)
            {
                auto const saved = midi2config::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

                WriteField(ResourceString(IDS_LOOPBACK_LABEL_SAVED_TO_CONFIG),
                    FormatBoolean(saved != nullptr && saved.Success()), successTextStyle);
            }
        }

        return 0;
    }

    int RunLoopbackRemoveCommand(_In_ LoopbackRemoveOptions const& options)
    {
        winrt::guid associationId{};

        if (!TryGetAssociationId(options.AssociationId, associationId))
        {
            return 1;
        }

        midi2loop::MidiLoopbackRemovalConfig const config{ associationId };

        auto const response = midi2loop::MidiLoopbackManager::RemoveTransientLoopback(config);

        if (response == nullptr || !response.Success())
        {
            auto const message = response == nullptr ? std::string{} : ToUtf8(response.ErrorMessage());

            WriteErrorLine(FormatResourceString(IDS_LOOPBACK_REMOVE_FAILED, message));
            return 1;
        }

        WriteSuccessLine(ResourceString(IDS_LOOPBACK_REMOVED));

        return 0;
    }

    int RunBasicLoopbackListCommand()
    {
        if (!midi2basicloop::MidiBasicLoopbackManager::IsTransportAvailable())
        {
            WriteErrorLine(ResourceString(IDS_LOOPBACK_NONE));
            return 1;
        }

        auto const entries = midi2basicloop::MidiBasicLoopbackManager::GetActiveLoopbackEntries();

        if (entries == nullptr || entries.Size() == 0)
        {
            WriteWarningLine(ResourceString(IDS_LOOPBACK_NONE));
            return 0;
        }

        ConsoleTable table{ ResourceString(IDS_LOOPBACK_BASIC_TABLE_TITLE) };

        table.AddColumn(ResourceString(IDS_LOOPBACK_LABEL_ASSOCIATION_ID), ColumnAlignment::Left, guidTextStyle);
        table.AddColumn(ResourceString(IDS_LABEL_NAME), ColumnAlignment::Left, endpointNameTextStyle);
        table.SetLastColumnShrinkable();

        for (auto const& entry : entries)
        {
            table.BeginRow();
            table.AddCell(FormatGuid(entry.AssociationId()));
            table.AddCell(ToUtf8(entry.Name()));
        }

        table.Render();

        return 0;
    }

    int RunBasicLoopbackCreateCommand(_In_ BasicLoopbackCreateOptions const& options)
    {
        if (!midi2basicloop::MidiBasicLoopbackManager::IsTransportAvailable())
        {
            WriteErrorLine(FormatResourceString(IDS_LOOPBACK_CREATE_FAILED, std::string{}));
            return 1;
        }

        midi2basicloop::MidiBasicLoopbackCreationConfig config;

        auto const name = options.Name.empty() ? std::string{ "MIDI Console Basic Loopback" } : options.Name;

        config.EndpointDefinition().Name(winrt::hstring{ FromUtf8(name) });

        if (!options.UniqueIdentifier.empty())
        {
            config.EndpointDefinition().UniqueId(winrt::hstring{ FromUtf8(options.UniqueIdentifier) });
        }

        auto const response = midi2basicloop::MidiBasicLoopbackManager::CreateTransientLoopback(config);

        if (response == nullptr || !response.Success())
        {
            auto const message = response == nullptr ? std::string{} : ToUtf8(response.ErrorMessage());

            WriteErrorLine(FormatResourceString(IDS_LOOPBACK_CREATE_FAILED, message));
            return 1;
        }

        WriteSuccessLine(ResourceString(IDS_LOOPBACK_CREATED));

        auto const entry = response.CreatedLoopbackEntry();

        if (entry != nullptr)
        {
            WriteField(ResourceString(IDS_LOOPBACK_LABEL_ASSOCIATION_ID),
                FormatGuid(entry.AssociationId()), guidTextStyle);
            WriteField(ResourceString(IDS_LABEL_ID), ToUtf8(entry.EndpointDeviceId()), endpointIdTextStyle);

            if (options.SaveToConfig)
            {
                auto const saved = midi2config::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

                WriteField(ResourceString(IDS_LOOPBACK_LABEL_SAVED_TO_CONFIG),
                    FormatBoolean(saved != nullptr && saved.Success()), successTextStyle);
            }
        }

        return 0;
    }

    int RunBasicLoopbackRemoveCommand(_In_ LoopbackRemoveOptions const& options)
    {
        winrt::guid associationId{};

        if (!TryGetAssociationId(options.AssociationId, associationId))
        {
            return 1;
        }

        midi2basicloop::MidiBasicLoopbackRemovalConfig const config{ associationId };

        auto const response = midi2basicloop::MidiBasicLoopbackManager::RemoveTransientLoopback(config);

        if (response == nullptr || !response.Success())
        {
            auto const message = response == nullptr ? std::string{} : ToUtf8(response.ErrorMessage());

            WriteErrorLine(FormatResourceString(IDS_LOOPBACK_REMOVE_FAILED, message));
            return 1;
        }

        WriteSuccessLine(ResourceString(IDS_LOOPBACK_REMOVED));

        return 0;
    }

    int RunLoopbackMuteCommand(_In_ LoopbackMuteOptions const& options)
    {
        winrt::guid associationId{};

        if (!TryGetAssociationId(options.AssociationId, associationId))
        {
            return 1;
        }

        auto const response = options.Mute
            ? midi2loop::MidiLoopbackManager::MuteLoopback(associationId)
            : midi2loop::MidiLoopbackManager::UnmuteLoopback(associationId);

        if (response == nullptr || !response.Success())
        {
            auto const message = response == nullptr ? std::string{} : ToUtf8(response.ErrorMessage());

            WriteErrorLine(FormatResourceString(IDS_LOOPBACK_MUTE_FAILED, message));
            return 1;
        }

        WriteSuccessLine(ResourceString(options.Mute ? IDS_LOOPBACK_MUTED : IDS_LOOPBACK_UNMUTED));

        return 0;
    }

    int RunBasicLoopbackMuteCommand(_In_ LoopbackMuteOptions const& options)
    {
        winrt::guid associationId{};

        if (!TryGetAssociationId(options.AssociationId, associationId))
        {
            return 1;
        }

        auto const response = options.Mute
            ? midi2basicloop::MidiBasicLoopbackManager::MuteLoopback(associationId)
            : midi2basicloop::MidiBasicLoopbackManager::UnmuteLoopback(associationId);

        if (response == nullptr || !response.Success())
        {
            auto const message = response == nullptr ? std::string{} : ToUtf8(response.ErrorMessage());

            WriteErrorLine(FormatResourceString(IDS_LOOPBACK_MUTE_FAILED, message));
            return 1;
        }

        WriteSuccessLine(ResourceString(options.Mute ? IDS_LOOPBACK_MUTED : IDS_LOOPBACK_UNMUTED));

        return 0;
    }
}
