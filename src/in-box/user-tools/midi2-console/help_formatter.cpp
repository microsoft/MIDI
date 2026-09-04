// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include <algorithm>
#include <sstream>

#include "console_output.h"
#include "console_table.h"
#include "help_formatter.h"
#include "strings.h"

namespace midi2console
{
    namespace
    {
        constexpr size_t NameColumnWidth = 32;
        constexpr size_t Indent = 2;

        // Styling has to be applied AFTER measuring, or the escape sequences are counted as width.
        std::string StyledNameColumn(_In_ std::string_view text, _In_ fmt::text_style const& nameStyle)
        {
            std::string result = fmt::format("{}", Styled(text, nameStyle));

            // A name wider than the column would otherwise run straight into the description.
            if (DisplayWidth(text) + 1 > NameColumnWidth)
            {
                result += "\n" + std::string(Indent + NameColumnWidth, ' ');
            }
            else
            {
                result += std::string(NameColumnWidth - DisplayWidth(text), ' ');
            }

            return result;
        }

        // Wraps at the description column so long text does not run off the right edge.
        std::string WrapDescription(_In_ std::string_view description, _In_ size_t leftColumn)
        {
            auto const available = ConsoleWidth() > leftColumn + 20
                ? ConsoleWidth() - leftColumn - 1
                : 60;

            std::string result;
            std::string line;

            std::istringstream stream{ std::string{ description } };
            std::string word;

            while (stream >> word)
            {
                if (!line.empty() && line.size() + 1 + word.size() > available)
                {
                    if (!result.empty())
                    {
                        result += "\n" + std::string(leftColumn, ' ');
                    }

                    result += line;
                    line.clear();
                }

                if (!line.empty())
                {
                    line += " ";
                }

                line += word;
            }

            if (!line.empty())
            {
                if (!result.empty())
                {
                    result += "\n" + std::string(leftColumn, ' ');
                }

                result += line;
            }

            return result;
        }

        std::string JoinAliases(_In_ std::vector<std::string> const& aliases)
        {
            std::string result;

            for (auto const& alias : aliases)
            {
                if (!result.empty())
                {
                    result += ", ";
                }

                result += alias;
            }

            return result;
        }
    }

    MidiHelpFormatter::MidiHelpFormatter()
    {
        column_width(NameColumnWidth);
        label("OPTIONS", "OPTIONS");
        label("SUBCOMMANDS", "COMMANDS");
        label("POSITIONALS", "ARGUMENTS");
        label("REQUIRED", "required");
    }

    // CLI11 2.6 re-flows make_* output as paragraphs, which discards the indentation and the
    // wrapping done here. Assembling the help directly keeps the layout intact.
    std::string MidiHelpFormatter::make_help(const CLI::App* app, std::string name, CLI::AppFormatMode mode) const
    {
        if (mode == CLI::AppFormatMode::Sub)
        {
            return make_expanded(app, mode);
        }

        std::stringstream out;

        out << make_description(app);
        out << make_usage(app, name);
        out << make_positionals(app);
        out << make_groups(app, mode);
        out << make_subcommands(app, mode);
        out << make_footer(app);

        return out.str();
    }

    std::string MidiHelpFormatter::make_description(const CLI::App* app) const
    {
        auto const description = app->get_description();

        if (description.empty())
        {
            return {};
        }

        return fmt::format("{}\n\n", Styled(WrapDescription(description, 0), normalTextStyle));
    }

    std::string MidiHelpFormatter::make_usage(const CLI::App* app, std::string name) const
    {
        std::string usage = name.empty() ? app->get_display_name(true) : name;

        if (!app->get_options().empty())
        {
            usage += " [OPTIONS]";
        }

        for (auto const* positional : app->get_options([](CLI::Option const* opt) { return opt->get_positional(); }))
        {
            usage += " " + positional->get_name(true, false);
        }

        if (!app->get_subcommands({}).empty())
        {
            usage += app->get_require_subcommand_min() == 0 ? " [COMMAND]" : " <COMMAND>";
        }

        return fmt::format("{}\n{}{}\n\n",
            Styled("USAGE:", tableTitleTextStyle),
            std::string(Indent, ' '),
            Styled(usage, endpointNameTextStyle));
    }

    std::string MidiHelpFormatter::make_group(
        std::string group,
        bool is_positional,
        std::vector<const CLI::Option*> opts) const
    {
        if (opts.empty())
        {
            return {};
        }

        // CLI11 passes the option group name, which is "Options" or "Positionals" unless the
        // command declared a custom group.
        std::string heading = group;

        if (heading.empty() || heading == "Options" || heading == "Positionals")
        {
            heading = is_positional ? get_label("POSITIONALS") : get_label("OPTIONS");
        }

        std::string result = fmt::format("{}\n", Styled(heading + ":", tableTitleTextStyle));

        for (auto const* opt : opts)
        {
            result += make_option(opt, is_positional);
        }

        return result + "\n";
    }

    std::string MidiHelpFormatter::make_option(const CLI::Option* opt, bool is_positional) const
    {
        auto const name = is_positional
            ? opt->get_name(true, false)
            : opt->get_name(false, true);

        std::string result = std::string(Indent, ' ');

        result += StyledNameColumn(name, warningTextStyle);

        auto description = opt->get_description();

        // CLI11 appends "[Required]" and default values to the description text.
        result += fmt::format("{}\n",
            Styled(WrapDescription(description, Indent + NameColumnWidth), normalTextStyle));

        return result;
    }

    std::string MidiHelpFormatter::make_subcommand(const CLI::App* sub) const
    {
        std::string result = std::string(Indent, ' ');

        result += StyledNameColumn(sub->get_name(), successTextStyle);

        result += fmt::format("{}\n",
            Styled(WrapDescription(sub->get_description(), Indent + NameColumnWidth), normalTextStyle));

        auto const aliases = sub->get_aliases();

        if (!aliases.empty())
        {
            result += std::string(Indent * 2, ' ');
            result += fmt::format("{} {}\n",
                Styled("aliases:", separatorTextStyle),
                Styled(JoinAliases(aliases), infoTextStyle));
        }

        return result;
    }

    std::string MidiHelpFormatter::make_subcommands(const CLI::App* app, CLI::AppFormatMode mode) const
    {
        auto const subcommands = app->get_subcommands({});

        if (subcommands.empty())
        {
            return {};
        }

        if (mode == CLI::AppFormatMode::All)
        {
            std::string expanded;

            for (auto const* sub : subcommands)
            {
                expanded += make_expanded(sub, CLI::AppFormatMode::All);
            }

            return expanded;
        }

        std::string result = fmt::format("{}\n", Styled(get_label("SUBCOMMANDS") + ":", tableTitleTextStyle));

        for (auto const* sub : subcommands)
        {
            result += make_subcommand(sub);
        }

        return result + "\n";
    }

    // Used by --help-all for each nested command.
    std::string MidiHelpFormatter::make_expanded(const CLI::App* sub, CLI::AppFormatMode mode) const
    {
        std::string result = fmt::format("{}", Styled(sub->get_display_name(), successTextStyle));

        auto const aliases = sub->get_aliases();

        if (!aliases.empty())
        {
            result += fmt::format(" {}", Styled(JoinAliases(aliases), infoTextStyle));
        }

        result += "\n";

        if (!sub->get_description().empty())
        {
            result += std::string(Indent, ' ');
            result += fmt::format("{}\n",
                Styled(WrapDescription(sub->get_description(), Indent), normalTextStyle));
        }

        result += "\n";
        result += make_positionals(sub);
        result += make_groups(sub, mode);
        result += make_subcommands(sub, mode);

        return result;
    }

    std::string MidiHelpFormatter::make_footer(const CLI::App* app) const
    {
        auto const footer = app->get_footer();

        if (footer.empty())
        {
            return {};
        }

        return fmt::format("{}\n", Styled(footer, separatorTextStyle));
    }

    void SetCommandExamples(_In_ CLI::App* command, _In_ std::vector<std::string> const& examples)
    {
        if (examples.empty())
        {
            return;
        }

        // Built once at setup time, so the escape sequences are baked into the footer string.
        std::string footer = fmt::format("{}\n", Styled(ResourceString(IDS_HELP_EXAMPLES_HEADING), tableTitleTextStyle));

        for (auto const& example : examples)
        {
            footer += fmt::format("{}{}\n", std::string(Indent, ' '), Styled(example, endpointNameTextStyle));
        }

        command->footer(footer);
    }
}
