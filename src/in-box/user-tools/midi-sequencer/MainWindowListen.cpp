// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Listening to another clip or a track (design section 29): a list of the sequence's tracks and
// clips to learn from, Learn to learn again, and Keep listening, which learns again each pass from
// what the source played during the pass before. Picking a source learns from it straight away,
// so there's always something to start from.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "GeneratorKinds.h"
#include "GeneratorListen.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        controls::TextBlock Text(winrt::hstring const& text, media::Brush const& brush, double size = 12)
        {
            controls::TextBlock block{};
            block.Text(text);
            block.FontSize(size);
            block.TextWrapping(xaml::TextWrapping::Wrap);
            block.Foreground(brush);
            block.VerticalAlignment(xaml::VerticalAlignment::Center);
            return block;
        }

        struct SourceChoice
        {
            seq::ListenKind Kind{ seq::ListenKind::None };
            std::wstring Id{};
        };

        controls::ScrollViewer FindScrollViewer(xaml::DependencyObject const& root)
        {
            if (root == nullptr)
            {
                return nullptr;
            }

            if (auto const viewer = root.try_as<controls::ScrollViewer>())
            {
                return viewer;
            }

            auto const count = media::VisualTreeHelper::GetChildrenCount(root);

            for (int32_t i = 0; i < count; ++i)
            {
                if (auto const found = FindScrollViewer(media::VisualTreeHelper::GetChild(root, i)))
                {
                    return found;
                }
            }

            return nullptr;
        }
    }

    _Use_decl_annotations_
    void MainWindow::AddListenSource(controls::StackPanel const& panel, seq::Clip const& clip, GeneratorEdit const& edit)
    {
        auto const& generator = *clip.Generator;
        auto const weak = get_weak();
        auto const listening = generator.ListenTo != seq::ListenKind::None && !generator.ListenId.empty();

        // The tracks, then the clips, under a heading each. Any clip but this one, and any track,
        // including the one this clip plays on: a loop is part of the fun.
        auto choices = std::make_shared<std::vector<SourceChoice>>();

        controls::ComboBox sources{};
        sources.Style(RootGrid().Resources().Lookup(winrt::box_value(L"SmallComboStyle")).as<xaml::Style>());
        sources.MinWidth(220);
        sources.MaxWidth(260);
        automation::AutomationProperties::SetName(sources, res::GetString(L"ListenPick"));
        sources.PlaceholderText(res::GetString(L"ListenPick"));

        // A heading can't be picked, and reads as one: bold, with room above the second.
        auto const heading = [&](wchar_t const* key)
        {
            controls::TextBlock title{};
            title.Text(res::GetString(key));
            title.FontSize(12);
            title.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());

            controls::ComboBoxItem item{};
            item.Content(title);
            item.IsEnabled(false);
            automation::AutomationProperties::SetName(item, res::GetString(key));

            if (!choices->empty())
            {
                item.Margin(xaml::Thickness{ 0, 6, 0, 0 });
            }

            sources.Items().Append(item);
            choices->push_back(SourceChoice{});
        };

        auto const choice = [&](std::wstring const& name, seq::ListenKind kind, std::wstring const& id)
        {
            controls::ComboBoxItem item{};
            item.Content(winrt::box_value(winrt::hstring{ name }));
            automation::AutomationProperties::SetName(item, winrt::hstring{ name });
            sources.Items().Append(item);
            choices->push_back(SourceChoice{ kind, id });
        };

        heading(L"ListenTracks");

        seq::ForEachTrack(m_doc, [&](seq::Track const& track, size_t)
        {
            if (!track.IsFolder)
            {
                choice(track.Name, seq::ListenKind::Track, track.Id);
            }

            return true;
        });

        heading(L"ListenClips");

        for (auto const& candidate : m_doc.Clips)
        {
            if (candidate.Id != clip.Id)
            {
                choice(candidate.Name.empty() ? std::wstring{ res::GetString(L"UntitledClip") } : candidate.Name, seq::ListenKind::Clip, candidate.Id);
            }
        }

        // The one it learns from stays picked, so Learn can learn it again after it changes. A
        // clip learned from before there was listening is found by its name.
        int32_t picked{ -1 };

        for (size_t i = 0; i < choices->size() && picked < 0; ++i)
        {
            auto const& candidate = (*choices)[i];

            if (listening ? (candidate.Kind == generator.ListenTo && candidate.Id == generator.ListenId)
                : (generator.LearnFrom == seq::LearnSource::Clip && candidate.Kind == seq::ListenKind::Clip && !generator.LearnedFrom.empty() &&
                    seq::FindClip(m_doc, candidate.Id) != nullptr && seq::FindClip(m_doc, candidate.Id)->Name == generator.LearnedFrom))
            {
                picked = static_cast<int32_t>(i);
            }
        }

        // Shown as picked only while it learns from it, rather than from a tune or words.
        if (generator.Type != seq::GeneratorType::Learn || generator.LearnFrom == seq::LearnSource::Clip)
        {
            sources.SelectedIndex(picked);
        }

        // With nothing picked, the list opens at its top, with the tracks, rather than its middle.
        sources.DropDownOpened([](foundation::IInspectable const& sender, auto&&)
        {
            auto const combo = sender.as<controls::ComboBox>();

            if (combo.SelectedIndex() >= 0)
            {
                return;
            }

            combo.DispatcherQueue().TryEnqueue([combo]()
            {
                try
                {
                    for (auto const& popup : media::VisualTreeHelper::GetOpenPopupsForXamlRoot(combo.XamlRoot()))
                    {
                        if (auto const viewer = FindScrollViewer(popup.Child()))
                        {
                            viewer.ChangeView(nullptr, foundation::IReference<double>{ 0.0 }, nullptr, true);
                        }
                    }
                }
                catch (...)
                {
                }
            });
        });

        // Picking builds the panel again, so this list is the one that gets the focus back.
        m_listenSources = sources;

        // Set after the list shows what's picked, so only a pick made here learns.
        sources.SelectionChanged([weak, choices](foundation::IInspectable const& sender, auto&&)
        {
            auto const index = sender.as<controls::ComboBox>().SelectedIndex();

            if (index < 0 || static_cast<size_t>(index) >= choices->size() || (*choices)[static_cast<size_t>(index)].Kind == seq::ListenKind::None)
            {
                return;
            }

            // Learning builds the panel again, so it waits for the list's own event to finish.
            auto const chosen = (*choices)[static_cast<size_t>(index)];

            if (auto s = weak.get())
            {
                s->DispatcherQueue().TryEnqueue([weak, chosen]()
                {
                    if (auto window = weak.get())
                    {
                        window->LearnFromSource(chosen.Kind, chosen.Id);
                    }
                });
            }
        });

        // Learn again, from the source picked.
        controls::Button learn{};
        learn.Style(RootGrid().Resources().Lookup(winrt::box_value(L"SmallButtonStyle")).as<xaml::Style>());
        learn.Content(winrt::box_value(res::GetString(L"LearnButton")));
        learn.IsEnabled(picked >= 0);
        learn.VerticalAlignment(xaml::VerticalAlignment::Center);
        learn.Click([weak, choices, sources](auto&&, auto&&)
        {
            auto const index = sources.SelectedIndex();

            if (index < 0 || static_cast<size_t>(index) >= choices->size())
            {
                return;
            }

            auto const chosen = (*choices)[static_cast<size_t>(index)];

            if (auto s = weak.get(); s && chosen.Kind != seq::ListenKind::None)
            {
                s->DispatcherQueue().TryEnqueue([weak, chosen]()
                {
                    if (auto window = weak.get())
                    {
                        window->LearnFromSource(chosen.Kind, chosen.Id);
                    }
                });
            }
        });

        // Keep listening: on or off by itself, so it's set apart.
        auto keep = MakeChip(res::GetString(L"ListenKeep"), generator.KeepListening, [edit](bool on)
        {
            edit([on](seq::GeneratorSettings& edited)
            {
                edited.KeepListening = on;

                // Life and Evolve hear between passes, so they change each pass to hear anything.
                if (on && seq::CarriesHearing(edited.Type))
                {
                    edited.ChangesEachPass = true;
                }
            });
        });

        keep.IsEnabled(listening);
        keep.VerticalAlignment(xaml::VerticalAlignment::Center);
        controls::ToolTipService::SetToolTip(keep, winrt::box_value(res::GetString(L"ListenKeepTip")));
        automation::AutomationProperties::SetFullDescription(keep, res::GetString(L"ListenKeepTip"));

        controls::Border line{};
        line.Width(1);
        line.Height(14);
        line.Margin(xaml::Thickness{ 4, 0, 4, 0 });
        line.VerticalAlignment(xaml::VerticalAlignment::Center);
        line.Background(BrushFor(m_palette.StrokeStrong));

        controls::StackPanel row{};
        row.Orientation(controls::Orientation::Horizontal);
        row.Spacing(8);
        row.Children().Append(sources);
        row.Children().Append(learn);
        row.Children().Append(line);
        row.Children().Append(keep);
        panel.Children().Append(row);

        // What it learned from, or is listening to.
        auto const name = seq::ListenSourceName(m_doc, generator.ListenTo, generator.ListenId);

        if (seq::ListensEachPass(generator) && !name.empty())
        {
            panel.Children().Append(Text(res::FormatString(L"ListeningToFormat", name), BrushFor(m_palette.Text2)));
        }
        else if ((generator.Type != seq::GeneratorType::Learn || generator.LearnFrom == seq::LearnSource::Clip) && !generator.LearnedFrom.empty() &&
            (!generator.Learned.empty() || generator.Type == seq::GeneratorType::Life || generator.Type == seq::GeneratorType::LifeDrums))
        {
            panel.Children().Append(Text(res::FormatString(L"LearnedFromFormat", generator.LearnedFrom), BrushFor(m_palette.Text2)));
        }
    }

    _Use_decl_annotations_
    void MainWindow::LearnFromSource(seq::ListenKind kind, std::wstring const& id) noexcept
    {
        try
        {
            auto const self = seq::FindClip(m_doc, m_editorClipId);

            if (self == nullptr || !self->Generator.has_value() || kind == seq::ListenKind::None)
            {
                return;
            }

            // Learned once now: Life drops a pass of the source onto its start board as cells, and
            // the others learn its notes. A source that's quiet so far is still picked, so Keep
            // listening can hear it when it plays, and what was learned before stays.
            auto const& generator = *self->Generator;
            auto const life = generator.Type == seq::GeneratorType::Life || generator.Type == seq::GeneratorType::LifeDrums;
            auto const name = seq::ListenSourceName(m_doc, kind, id);

            std::vector<seq::LearnedNote> learned{};
            std::vector<seq::LifeCell> start{};
            auto heard = false;

            if (life)
            {
                auto const columns = static_cast<int32_t>(std::clamp<int64_t>(seq::GeneratorStepCount(*self), 1, seq::MaximumGeneratorSteps));
                auto const rows = static_cast<int32_t>(std::max<size_t>(1, seq::LifeRowNotes(generator).size()));
                auto const cells = seq::HeardCells(generator, seq::HearOnce(m_doc, *self, kind, id), columns);
                heard = cells.size() >= 2;

                // On the board as it starts now, scattered or drawn.
                if (heard)
                {
                    start = generator.Scatter ? seq::ScatteredCells(generator, self->Seed, columns, rows) : generator.LifeCells;
                    start.insert(start.end(), cells.begin(), cells.end());
                    std::sort(start.begin(), start.end());
                    start.erase(std::unique(start.begin(), start.end()), start.end());
                }
            }
            else
            {
                learned = seq::LearnOnce(m_doc, generator.StepTicks, kind, id);
                heard = learned.size() >= 2;
            }

            if (!heard)
            {
                ShowMessage(res::GetString(L"LearnNeedsNotes"));
            }

            EditGenerator(std::wstring{ res::GetString(L"UndoLearn") }, [kind, id, name, life, heard, learned = std::move(learned), start = std::move(start)](seq::GeneratorSettings& edited)
            {
                if (edited.Type == seq::GeneratorType::Learn)
                {
                    edited.LearnFrom = seq::LearnSource::Clip;
                }

                edited.ListenTo = kind;
                edited.ListenId = id;

                if (heard && life)
                {
                    edited.Scatter = false;
                    edited.LifeCells = start;
                    edited.LearnedFrom = name.substr(0, 128);
                }
                else if (heard)
                {
                    edited.Learned = learned;
                    edited.LearnedFrom = name.substr(0, 128);
                }
            });

            // The panel was built again, so the focus goes back to the new list.
            if (m_listenSources != nullptr)
            {
                m_listenSources.Focus(xaml::FocusState::Programmatic);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to learn from a clip or a track.")
    }

    _Use_decl_annotations_
    int64_t MainWindow::ListenPassStart(seq::Clip const& clip, uint64_t pass) const noexcept
    {
        // Where the clip's first placement puts the pass, for a source that's a track; a clip is
        // heard side by side, wherever it plays.
        int64_t start{ 0 };
        auto found = false;

        try
        {
            seq::ForEachTrack(m_doc, [&](seq::Track const& track, size_t)
            {
                for (auto const& placement : track.Timeline)
                {
                    if (placement.ClipId == clip.Id && (!found || placement.Tick < start))
                    {
                        start = placement.Tick;
                        found = true;
                    }
                }

                return true;
            });
        }
        catch (...)
        {
        }

        return start + static_cast<int64_t>(pass) * std::max<int64_t>(1, clip.Length);
    }

    _Use_decl_annotations_
    seq::GeneratorSettings MainWindow::ShownPassSettings(seq::Clip const& clip, uint64_t pass) const
    {
        auto settings = *clip.Generator;

        // A clip that learns again each pass shows what it plays after what it heard. Life and
        // Evolve carry what they heard from pass to pass instead: ShownPassHearing.
        if (seq::ListensEachPass(settings) && !seq::CarriesHearing(settings.Type))
        {
            seq::ListenContext const listen{ &m_doc, nullptr };

            if (auto heard = seq::HeardBefore(listen, clip, ListenPassStart(clip, pass), pass).Learned; heard.size() >= 2)
            {
                settings.Learned = std::move(heard);
            }
        }

        return settings;
    }

    _Use_decl_annotations_
    seq::PassHearing MainWindow::ShownPassHearing(seq::Clip const& clip, uint64_t pass) const
    {
        if (!clip.Generator.has_value() || !seq::CarriesHearing(clip.Generator->Type))
        {
            return {};
        }

        seq::ListenContext const listen{ &m_doc, nullptr };
        return seq::MakePassHearing(listen, clip, ListenPassStart(clip, pass), pass);
    }
}
