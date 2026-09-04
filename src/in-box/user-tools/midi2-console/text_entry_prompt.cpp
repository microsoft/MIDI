// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "console_output.h"
#include "ftxui_support.h"
#include "text_entry_prompt.h"

using namespace ftxui;

namespace midi2console
{
    std::optional<std::string> PromptForText(
        _In_ std::string_view title,
        _In_ std::string_view hint,
        _In_ std::string_view placeholder)
    {
        std::string value;
        bool accepted{ false };

        InputOption inputOption = InputOption::Default();
        inputOption.multiline = false;
        inputOption.placeholder = std::string{ placeholder };

        auto input = Input(&value, inputOption);

        auto screen = ScreenInteractive::TerminalOutput();

        auto renderer = Renderer(input, [&]
        {
            return vbox({
                text(std::string{ title }) | bold | color(Color::LightYellow3),
                separator(),
                hbox({
                    text("> ") | color(Color::GrayDark),
                    input->Render() | flex
                    }),
                separator(),
                text(std::string{ hint }) | color(Color::GrayDark)
                }) | border | size(WIDTH, LESS_THAN, 80);
        });

        auto withEvents = CatchEvent(renderer, [&](Event event)
        {
            if (event == Event::Escape)
            {
                accepted = false;
                screen.Exit();
                return true;
            }

            if (event == Event::Return)
            {
                accepted = true;
                screen.Exit();
                return true;
            }

            return false;
        });

        {
            RunFtxuiLoop(screen, withEvents);
        }

        ErasePopupFrame(screen.dimy());

        if (!accepted)
        {
            return std::nullopt;
        }

        return value;
    }
}
