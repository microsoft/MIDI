// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <CLI/CLI.hpp>

namespace midi2console
{
    // CLI11's stock formatter jams every alias into the name column, which runs into the
    // description, and it has no color. This one puts aliases on their own line and styles the
    // output the way the rest of the tool is styled.
    class MidiHelpFormatter : public CLI::Formatter
    {
    public:
        MidiHelpFormatter();

        std::string make_help(const CLI::App* app, std::string name, CLI::AppFormatMode mode) const override;
        std::string make_subcommand(const CLI::App* sub) const override;
        std::string make_option(const CLI::Option* opt, bool is_positional) const override;
        std::string make_group(std::string group, bool is_positional, std::vector<const CLI::Option*> opts) const override;
        std::string make_description(const CLI::App* app) const override;
        std::string make_usage(const CLI::App* app, std::string name) const override;
        std::string make_footer(const CLI::App* app) const override;
        std::string make_subcommands(const CLI::App* app, CLI::AppFormatMode mode) const override;
        std::string make_expanded(const CLI::App* sub, CLI::AppFormatMode mode) const override;
    };

    // Examples are shown under the command's help. CLI11 has no first-class concept for them,
    // so they are rendered into the footer.
    void SetCommandExamples(_In_ CLI::App* command, _In_ std::vector<std::string> const& examples);
}
