// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include <algorithm>
#include <thread>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "console_output.h"
#include "ftxui_support.h"
#include "pickers.h"
#include "strings.h"

using namespace ftxui;

namespace midi2console
{
    namespace
    {
        Element RenderEntry(_In_ PickerEntry const& entry, _In_ bool focused)
        {
            Elements parts;

            if (!entry.Icon.empty())
            {
                parts.push_back(text(entry.Icon + " "));
            }

            if (entry.IsCancelEntry)
            {
                parts.push_back(text(entry.PrimaryText) | color(Color::GrayLight));
            }
            else
            {
                parts.push_back(text(entry.PrimaryText) | color(Color::LightSkyBlue1));

                if (!entry.SecondaryText.empty())
                {
                    parts.push_back(text(entry.SecondaryText) | color(Color::SteelBlue));
                }

                if (!entry.TertiaryText.empty())
                {
                    parts.push_back(text(entry.TertiaryText) | color(Color::GrayDark));
                }

                if (!entry.QuaternaryText.empty())
                {
                    parts.push_back(text(entry.QuaternaryText) | color(Color::SlateBlue1));
                }
            }

            // filler() makes the row claim the full width so the selection reads as one
            // continuous bar rather than a highlight that stops at the end of the text.
            parts.push_back(filler());

            auto row = hbox(std::move(parts));

            if (focused)
            {
                row = row | color(Color::White) | bgcolor(Color::RGB(0x00, 0x3E, 0x5F)) | focus;
            }

            return row;
        }
    }

    bool CanShowInteractiveUI()
    {
        DWORD mode{ 0 };

        return GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode) != 0 &&
               GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode) != 0;
    }

    void FinalizePickerEntries(_Inout_ std::vector<PickerEntry>& entries)
    {
        size_t maxPrimary{ 0 };
        size_t maxSecondary{ 0 };
        size_t maxTertiary{ 0 };

        for (auto const& entry : entries)
        {
            maxPrimary = std::max(maxPrimary, DisplayWidth(entry.PrimaryText));
            maxSecondary = std::max(maxSecondary, DisplayWidth(entry.SecondaryText));
            maxTertiary = std::max(maxTertiary, DisplayWidth(entry.TertiaryText));
        }

        for (auto& entry : entries)
        {
            entry.PrimaryText = PadRightToWidth(entry.PrimaryText, maxPrimary + 2);

            if (maxSecondary > 0)
            {
                entry.SecondaryText = PadRightToWidth(entry.SecondaryText, maxSecondary + 2);
            }

            if (maxTertiary > 0)
            {
                entry.TertiaryText = PadRightToWidth(entry.TertiaryText, maxTertiary + 2);
            }
        }

        PickerEntry cancelEntry;
        cancelEntry.Icon = "\U0001F519";        // back arrow, two cells like the other icons
        cancelEntry.PrimaryText = ResourceString(IDS_PROMPT_CANCEL_ENTRY);
        cancelEntry.IsCancelEntry = true;

        entries.push_back(std::move(cancelEntry));
    }

    PickerResult ShowPicker(_In_ std::string_view prompt, _In_ std::vector<PickerEntry> entries)
    {
        PickerResult result;

        if (entries.empty())
        {
            return result;
        }

        int selectedIndex{ 0 };
        bool accepted{ false };

        auto container = Container::Vertical({}, &selectedIndex);

        for (size_t i = 0; i < entries.size(); i++)
        {
            MenuEntryOption option;

            option.transform = [&entries, i](EntryState const& state)
            {
                return RenderEntry(entries[i], state.focused);
            };

            container->Add(MenuEntry(entries[i].PrimaryText, option));
        }

        auto screen = ScreenInteractive::TerminalOutput();

        auto const visibleRows = static_cast<int>(std::min<size_t>(entries.size(), 15));

        auto renderer = Renderer(container, [&]
        {
            return vbox({
                text(std::string{ prompt }) | color(Color::SteelBlue1),
                separator(),
                container->Render() | vscroll_indicator | yframe | size(HEIGHT, LESS_THAN, visibleRows),
                separator(),
                text(ResourceString(IDS_PROMPT_PICKER_KEYS)) | color(Color::GrayDark)
                }) | border;
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
        if (!accepted || selectedIndex < 0 || static_cast<size_t>(selectedIndex) >= entries.size())
        {
            return result;
        }

        auto const& selected = entries[static_cast<size_t>(selectedIndex)];

        if (selected.IsCancelEntry)
        {
            return result;
        }

        result.Canceled = false;
        result.Value = selected.Value;
        result.SelectedIndex = selectedIndex;

        result.DisplayText = selected.PrimaryText;

        while (!result.DisplayText.empty() && result.DisplayText.back() == ' ')
        {
            result.DisplayText.pop_back();
        }

        return result;
    }
}
