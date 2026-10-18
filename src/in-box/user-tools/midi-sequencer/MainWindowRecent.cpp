// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The sequences opened or saved on this PC, and the ones pinned so they never age out. The list
// lives in the PC's settings, not in the sequence files (design section 26).

#include "pch.h"
#include "MainWindow.xaml.h"

#include <winrt/Windows.Globalization.DateTimeFormatting.h>

#include "BackgroundWork.h"
#include "DialogParts.h"
#include "RecentSequences.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    using namespace dialogparts;

    namespace
    {
        // Every sequencer window writes the list, so a change is made to what's there now, one
        // window at a time.
        constexpr wchar_t RecentSequencesMutexName[] = L"Local\\WindowsMidiSequencerRecentSequences";
        constexpr DWORD RecentSequencesWaitMilliseconds = 2000;

        constexpr double RecentDialogWidth = 600.0;

        int64_t Now() noexcept
        {
            return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }

        std::wstring Lowered(std::wstring text)
        {
            if (!text.empty())
            {
                ::CharLowerBuffW(text.data(), static_cast<DWORD>(text.size()));
            }

            return text;
        }

        std::wstring UsedAtText(int64_t const usedAt)
        {
            if (usedAt <= 0)
            {
                return {};
            }

            try
            {
                winrt::Windows::Globalization::DateTimeFormatting::DateTimeFormatter formatter{ L"shortdate shorttime" };
                return std::wstring{ formatter.Format(winrt::clock::from_time_t(static_cast<time_t>(usedAt))) };
            }
            catch (...)
            {
                return {};
            }
        }

        // What the dialog shows, shared by its controls' handlers.
        struct RecentDialogState
        {
            std::vector<::midisequencer::RecentSequence> List{};
            std::set<std::wstring> Missing{};
            std::wstring Filter{};
            std::wstring Chosen{};
        };
    }

    _Use_decl_annotations_
    std::vector<seq::RecentSequence> MainWindow::EditRecentSequences(std::function<void(std::vector<seq::RecentSequence>&)> const& edit) noexcept
    {
        try
        {
            wil::unique_handle mutex{ ::CreateMutexW(nullptr, FALSE, RecentSequencesMutexName) };
            auto owned{ false };

            if (mutex)
            {
                auto const wait = ::WaitForSingleObject(mutex.get(), RecentSequencesWaitMilliseconds);
                owned = wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED;
            }

            // A window that never lets go isn't allowed to stop this one; at worst one change is lost.
            auto const release = wil::scope_exit([&]()
            {
                if (owned)
                {
                    ::ReleaseMutex(mutex.get());
                }
            });

            auto& settings = seq::AppSettings::Current();
            auto list = seq::ReadRecentSequences(settings.RecentSequencesText());

            if (edit)
            {
                edit(list);
                settings.RecentSequencesText(seq::WriteRecentSequences(list));
            }

            return list;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return {};
        }
    }

    _Use_decl_annotations_
    void MainWindow::RememberRecentSequence(std::wstring const& path) noexcept
    {
        try
        {
            if (path.empty())
            {
                return;
            }

            EditRecentSequences([&path](std::vector<seq::RecentSequence>& list)
            {
                seq::RememberSequence(list, path, Now());
            });
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to remember a recent sequence.")
    }

    _Use_decl_annotations_
    void MainWindow::OpenRecentSequence(std::wstring const& path) noexcept
    {
        try
        {
            if (seq::SameSequencePath(path, m_path))
            {
                return;
            }

            if (CanReplaceDocument())
            {
                OpenFileAsync(path);
            }
            else
            {
                OpenInNewWindow(path);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to open a recent sequence.")
    }

    void MainWindow::ShowRecentSequences()
    {
        [](winrt::com_ptr<MainWindow> strong) -> winrt::fire_and_forget
        {
            try
            {
                auto const state = std::make_shared<RecentDialogState>();
                state->List = strong->EditRecentSequences(nullptr);

                // Which files are still there. A network drive can be slow to answer, so it's
                // asked off the UI thread.
                std::vector<std::wstring> paths{};

                for (auto const& entry : state->List)
                {
                    paths.push_back(entry.Path);
                }

                std::set<std::wstring> missing{};

                co_await ::midisequencer::RunOnBackgroundAsync([&paths, &missing]()
                {
                    for (auto const& path : paths)
                    {
                        std::error_code error{};

                        if (!std::filesystem::is_regular_file(std::filesystem::path{ path }, error))
                        {
                            missing.insert(path);
                        }
                    }
                });

                if (strong->m_closing)
                {
                    co_return;
                }

                state->Missing = std::move(missing);

                auto const secondary = strong->BrushFor(strong->m_palette.Text2);
                auto const tertiary = strong->BrushFor(strong->m_palette.Text3);
                auto const accent = strong->BrushFor(strong->m_palette.Accent);
                auto const warning = strong->ThemeBrush(L"SeqWarnTextBrush");
                auto const subtle = strong->RootGrid().Resources().Lookup(winrt::box_value(L"SubtleSmallButtonStyle")).as<xaml::Style>();

                controls::StackPanel panel{};
                panel.Spacing(8);
                panel.Width(RecentDialogWidth);

                controls::TextBox find{};
                find.PlaceholderText(res::GetString(L"RecentFindPlaceholder"));
                automation::AutomationProperties::SetName(find, res::GetString(L"RecentFindName"));
                panel.Children().Append(find);

                controls::StackPanel rows{};
                rows.Spacing(2);

                controls::ScrollViewer scroller{};
                scroller.Content(rows);
                scroller.MaxHeight(440);
                scroller.Padding(xaml::Thickness{ 0, 0, 14, 0 });
                scroller.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Auto);
                panel.Children().Append(scroller);

                auto const note = Note(L"", warning);
                note.Visibility(xaml::Visibility::Collapsed);
                panel.Children().Append(note);

                controls::ContentDialog dialog{};
                dialog.XamlRoot(strong->Content().XamlRoot());
                dialog.Title(winrt::box_value(res::GetString(L"RecentDialogTitle")));
                dialog.Content(panel);

                // A dialog stops at 548 wide unless told otherwise, which would cut off the dates and
                // the remove buttons.
                dialog.Resources().Insert(winrt::box_value(L"ContentDialogMaxWidth"), winrt::box_value(RecentDialogWidth + 80.0));
                dialog.CloseButtonText(res::GetString(L"DialogClose"));
                dialog.DefaultButton(controls::ContentDialogButton::None);
                dialog.RequestedTheme(strong->RootGrid().ActualTheme());

                auto const weak = strong->get_weak();
                auto const rebuild = std::make_shared<std::function<void()>>();

                // Handlers hold weak references: a control that held its own dialog would never be freed.
                *rebuild = [weak, weakState = std::weak_ptr<RecentDialogState>{ state }, weakRows = winrt::make_weak(rows),
                    weakNote = winrt::make_weak(note), weakDialog = winrt::make_weak(dialog), weakRebuild = std::weak_ptr<std::function<void()>>{ rebuild },
                    secondary, tertiary, accent, warning, subtle]()
                {
                    auto const window = weak.get();
                    auto const current = weakState.lock();
                    auto const host = weakRows.get();

                    if (!window || !current || !host)
                    {
                        return;
                    }

                    host.Children().Clear();

                    auto const filter = Lowered(current->Filter);
                    auto const shown = seq::RecentSequencesForDisplay(current->List);
                    auto anyPinned{ false };
                    auto anyRecent{ false };
                    size_t count{ 0 };

                    for (auto const& entry : shown)
                    {
                        std::filesystem::path const file{ entry.Path };
                        auto const name = file.stem().wstring();
                        auto const folder = file.parent_path().wstring();

                        if (!filter.empty() && Lowered(name).find(filter) == std::wstring::npos && Lowered(folder).find(filter) == std::wstring::npos)
                        {
                            continue;
                        }

                        // A heading before the first of each kind.
                        if (entry.Pinned && !anyPinned)
                        {
                            anyPinned = true;
                            host.Children().Append(Heading(res::GetString(L"RecentPinnedHeading")));
                        }
                        else if (!entry.Pinned && !anyRecent)
                        {
                            anyRecent = true;
                            host.Children().Append(Heading(res::GetString(L"RecentHeading")));
                        }

                        ++count;
                        auto const missingFile = current->Missing.contains(entry.Path);
                        auto const path = entry.Path;

                        controls::Grid row{};
                        row.ColumnSpacing(4);

                        for (auto const width : { xaml::GridLength{ 32, xaml::GridUnitType::Pixel }, xaml::GridLength{ 1, xaml::GridUnitType::Star },
                            xaml::GridLength{ 1, xaml::GridUnitType::Auto }, xaml::GridLength{ 32, xaml::GridUnitType::Pixel } })
                        {
                            controls::ColumnDefinition column{};
                            column.Width(width);
                            row.ColumnDefinitions().Append(column);
                        }

                        // ---- the pin ----

                        controls::Button pin{};
                        pin.Style(subtle);
                        pin.Width(32);
                        pin.Height(32);
                        pin.Padding(xaml::Thickness{ 0, 0, 0, 0 });
                        pin.VerticalAlignment(xaml::VerticalAlignment::Center);

                        controls::FontIcon pinGlyph{};
                        pinGlyph.Glyph(entry.Pinned ? L"\uE842" : L"\uE718");
                        pinGlyph.FontSize(14);

                        if (entry.Pinned)
                        {
                            pinGlyph.Foreground(accent);
                        }

                        pin.Content(pinGlyph);

                        auto const pinName = res::FormatString(entry.Pinned ? L"RecentUnpinFormat" : L"RecentPinFormat", name);
                        automation::AutomationProperties::SetName(pin, pinName);
                        controls::ToolTipService::SetToolTip(pin, winrt::box_value(pinName));

                        pin.Click([weak, weakState, weakNote, weakRebuild, path, pinned = entry.Pinned](auto&&, auto&&)
                        {
                            auto const window = weak.get();
                            auto const current = weakState.lock();
                            auto const again = weakRebuild.lock();

                            if (!window || !current || !again)
                            {
                                return;
                            }

                            auto changed{ false };

                            current->List = window->EditRecentSequences([&](std::vector<seq::RecentSequence>& list)
                            {
                                changed = seq::PinSequence(list, path, !pinned);
                            });

                            if (auto const text = weakNote.get())
                            {
                                text.Text(changed || pinned ? winrt::hstring{} : res::FormatString(L"RecentPinLimitFormat", seq::MaximumPinnedSequences));
                                text.Visibility(text.Text().empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
                            }

                            (*again)();
                        });

                        PlaceIn(row, pin, 0);

                        // ---- the sequence, which opens it ----

                        controls::Button open{};
                        open.Style(subtle);

                        // Two lines, the name and the folder: taller than the style's one-line height.
                        open.Height(std::numeric_limits<double>::quiet_NaN());
                        open.MinHeight(40);
                        open.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                        open.HorizontalContentAlignment(xaml::HorizontalAlignment::Left);
                        open.Padding(xaml::Thickness{ 8, 4, 8, 5 });

                        controls::StackPanel words{};

                        controls::TextBlock title{};
                        title.Text(winrt::hstring{ name });
                        title.FontSize(13);
                        title.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
                        title.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
                        words.Children().Append(title);

                        controls::TextBlock where{};
                        where.Text(missingFile ? res::FormatString(L"RecentMissingFormat", folder) : winrt::hstring{ folder });
                        where.FontSize(11);
                        where.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
                        where.Foreground(missingFile ? warning : tertiary);
                        words.Children().Append(where);

                        open.Content(words);
                        automation::AutomationProperties::SetName(open, res::FormatString(L"RecentOpenFormat", name));
                        controls::ToolTipService::SetToolTip(open, winrt::box_value(winrt::hstring{ path }));

                        open.Click([weakState, weakNote, weakDialog, path, missingFile, name](auto&&, auto&&)
                        {
                            auto const current = weakState.lock();

                            if (!current)
                            {
                                return;
                            }

                            // Opening a file that's gone would only say so; the list says it here.
                            if (missingFile)
                            {
                                if (auto const text = weakNote.get())
                                {
                                    text.Text(res::FormatString(L"RecentCantOpenFormat", name));
                                    text.Visibility(xaml::Visibility::Visible);
                                }

                                return;
                            }

                            current->Chosen = path;

                            if (auto const shownDialog = weakDialog.get())
                            {
                                shownDialog.Hide();
                            }
                        });

                        PlaceIn(row, open, 1);

                        // ---- when ----

                        controls::TextBlock when{};
                        when.Text(winrt::hstring{ UsedAtText(entry.UsedAt) });
                        when.FontSize(12);
                        when.Foreground(secondary);
                        when.VerticalAlignment(xaml::VerticalAlignment::Center);
                        when.Margin(xaml::Thickness{ 8, 0, 4, 0 });
                        PlaceIn(row, when, 2);

                        // ---- remove from the list ----

                        controls::Button remove{};
                        remove.Style(subtle);
                        remove.Width(32);
                        remove.Height(32);
                        remove.Padding(xaml::Thickness{ 0, 0, 0, 0 });
                        remove.VerticalAlignment(xaml::VerticalAlignment::Center);

                        controls::FontIcon removeGlyph{};
                        removeGlyph.Glyph(L"\uE711");
                        removeGlyph.FontSize(11);
                        remove.Content(removeGlyph);

                        auto const removeName = res::FormatString(L"RecentRemoveFormat", name);
                        automation::AutomationProperties::SetName(remove, removeName);
                        controls::ToolTipService::SetToolTip(remove, winrt::box_value(removeName));

                        remove.Click([weak, weakState, weakRebuild, path](auto&&, auto&&)
                        {
                            auto const window = weak.get();
                            auto const current = weakState.lock();
                            auto const again = weakRebuild.lock();

                            if (!window || !current || !again)
                            {
                                return;
                            }

                            current->List = window->EditRecentSequences([&path](std::vector<seq::RecentSequence>& list)
                            {
                                seq::ForgetSequence(list, path);
                            });

                            (*again)();
                        });

                        PlaceIn(row, remove, 3);
                        host.Children().Append(row);
                    }

                    if (count == 0)
                    {
                        host.Children().Append(Note(res::GetString(current->List.empty() ? L"RecentEmpty" : L"RecentNoneFound"), tertiary));
                    }
                };

                find.TextChanged([weakState = std::weak_ptr<RecentDialogState>{ state }, weakRebuild = std::weak_ptr<std::function<void()>>{ rebuild }](foundation::IInspectable const& sender, auto&&)
                {
                    auto const current = weakState.lock();
                    auto const again = weakRebuild.lock();
                    auto const box = sender.try_as<controls::TextBox>();

                    if (current && again && box)
                    {
                        current->Filter = std::wstring{ box.Text() };
                        (*again)();
                    }
                });

                (*rebuild)();

                co_await dialog.ShowAsync();

                if (!strong->m_closing && !state->Chosen.empty())
                {
                    strong->OpenRecentSequence(state->Chosen);
                }
            }
            MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the recent sequences.")
        }(get_strong());
    }

    _Use_decl_annotations_
    void MainWindow::OnOpenRecentClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        ShowRecentSequences();
    }
}
