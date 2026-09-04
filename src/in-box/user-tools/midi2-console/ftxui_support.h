// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <atomic>
#include <chrono>
#include <thread>
#include <utility>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

namespace midi2console
{
    // Use instead of screen.Loop(). FTXUI can only tell a lone Escape from the start of an escape
    // sequence by waiting for its input parser to time out, and on Windows that wait does not
    // reliably happen, so the user has to press Escape twice. This watches the console input
    // queue directly and raises the event itself. Peeking does not consume the record, so FTXUI
    // still receives the key exactly as before.
    inline void RunFtxuiLoop(_In_ ftxui::ScreenInteractive& screen, _In_ ftxui::Component component)
    {
        std::atomic<bool> stop{ false };

        std::thread escapeWatcher([&screen, &stop]
        {
            auto const input = GetStdHandle(STD_INPUT_HANDLE);
            auto lastPost = std::chrono::steady_clock::now() - std::chrono::seconds(1);

            std::vector<INPUT_RECORD> records;

            while (!stop)
            {
                DWORD pending{ 0 };

                if (GetNumberOfConsoleInputEvents(input, &pending) && pending > 0)
                {
                    records.resize(pending);

                    DWORD peeked{ 0 };

                    if (PeekConsoleInputW(input, records.data(), pending, &peeked))
                    {
                        for (DWORD i = 0; i < peeked; i++)
                        {
                            auto const& record = records[i];

                            if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown)
                            {
                                continue;
                            }

                            auto const& key = record.Event.KeyEvent;

                            // Virtual terminal input mode leaves the virtual key code empty, so
                            // the character has to be checked as well.
                            if (key.wVirtualKeyCode != VK_ESCAPE && key.uChar.UnicodeChar != 0x1B)
                            {
                                continue;
                            }

                            auto const now = std::chrono::steady_clock::now();

                            // The record stays queued until FTXUI reads it, so throttle.
                            if (now - lastPost > std::chrono::milliseconds(250))
                            {
                                lastPost = now;
                                screen.PostEvent(ftxui::Event::Escape);
                            }

                            break;
                        }
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        });

        screen.Loop(std::move(component));

        stop = true;
        escapeWatcher.join();
    }
}
