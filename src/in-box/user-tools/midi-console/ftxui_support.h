// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <utility>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>

namespace midi2console
{
    // Escape is the prefix of every escape sequence, so FTXUI holds it back until its 50 ms
    // parser timeout decides no sequence followed. That timeout only runs on a read that finds
    // the console input queue empty, and on Windows the queue is never empty for long: FTXUI
    // asks the terminal for the cursor position when it redraws, the reply is itself an input
    // event that triggers another redraw, and the loop sustains itself at roughly 18 ms. A
    // pending Escape is therefore glued to the leading byte of the next reply and the parser
    // emits Special("\x1b\x1b") instead of Event::Escape.
    //
    // Both forms mean the user pressed Escape once. Matching on the event rather than on timing
    // keeps this independent of thread scheduling and timer resolution.
    inline bool IsCancelEvent(_In_ ftxui::Event const& event)
    {
        return event == ftxui::Event::Escape || event == ftxui::Event::Special("\x1B\x1B");
    }

    inline void RunFtxuiLoop(_In_ ftxui::ScreenInteractive& screen, _In_ ftxui::Component component)
    {
        screen.Loop(std::move(component));
    }
}
