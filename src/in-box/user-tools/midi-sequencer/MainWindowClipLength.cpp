// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// A clip's own length, and whether it loops (#1326; Pete, November 1 2026, picked the loop
// switch that Ableton Live and Bitwig have).
//
// - A clip that loops repeats when the end of a placement is dragged past it, as before. One that
//   doesn't gets longer or shorter itself, the way Cubase and Studio One work. New notes clips on
//   the timeline don't loop; ones made in the launcher do.
// - The length chip in the clip editor opens the clip's length, in bars and beats, and Loop.
// - Dragging the end of a clip that doesn't loop, where the clip plays in other places too, asks
//   first: only this one, which becomes a separate copy, or everywhere. A copy takes "(copy)" on
//   its name, loses the link mark that shared clips show, and the status bar says it's separate.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "SequenceMerge.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    // ---------------------------------------------------------------- dragging a clip's end

    _Use_decl_annotations_
    bool MainWindow::FinishClipResize(std::vector<seq::Track> before) noexcept
    {
        try
        {
            auto const track = seq::FindTrack(m_doc, m_selectedTrackId);

            if (track == nullptr || m_selectedPlacement >= track->Timeline.size())
            {
                return false;
            }

            auto const clip = seq::FindClip(m_doc, track->Timeline[m_selectedPlacement].ClipId);

            if (clip == nullptr || clip->Loop)
            {
                return false;
            }

            auto const trackId = m_selectedTrackId;
            auto const placement = m_selectedPlacement;
            auto const length = track->Timeline[placement].Length;

            if (seq::CountClipUses(m_doc, clip->Id) > 1)
            {
                AskHowToResizeClip(std::move(before), trackId, placement, length);
            }
            else
            {
                ApplyClipResize(before, trackId, placement, length, false);
            }

            return true;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return false;
        }
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::AskHowToResizeClip(std::vector<seq::Track> before, std::wstring trackId, size_t placement, int64_t length)
    {
        auto strong = get_strong();

        try
        {
            auto const track = seq::FindTrack(m_doc, trackId);
            auto const clip = track != nullptr && placement < track->Timeline.size() ? seq::FindClip(m_doc, track->Timeline[placement].ClipId) : nullptr;

            if (clip == nullptr)
            {
                m_doc.Tracks = std::move(before);
                InvalidateArrange();
                co_return;
            }

            controls::ContentDialog dialog{};
            dialog.XamlRoot(Content().XamlRoot());
            dialog.Title(winrt::box_value(res::GetString(L"ClipResizeSharedTitle")));
            dialog.Content(winrt::box_value(res::FormatString(L"ClipResizeSharedBodyFormat", clip->Name, seq::CountClipUses(m_doc, clip->Id))));
            dialog.PrimaryButtonText(res::GetString(L"ClipResizeOnlyThis"));
            dialog.SecondaryButtonText(res::GetString(L"ClipResizeAll"));
            dialog.CloseButtonText(res::GetString(L"ClipResizeCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Primary);

            auto const result = co_await dialog.ShowAsync();

            if (result == controls::ContentDialogResult::Primary || result == controls::ContentDialogResult::Secondary)
            {
                ApplyClipResize(before, trackId, placement, length, result == controls::ContentDialogResult::Primary);
            }
            else
            {
                // Cancel: as it was before the drag.
                m_doc.Tracks = std::move(before);
                InvalidateArrange();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to ask how to change a clip's length.")
    }

    _Use_decl_annotations_
    void MainWindow::ApplyClipResize(std::vector<seq::Track> const& before, std::wstring const& trackId, size_t placement, int64_t length, bool onlyHere) noexcept
    {
        try
        {
            // From the tracks as they were before the drag, so the undo has one step for all of it.
            m_doc.Tracks = before;

            auto const track = seq::FindTrack(m_doc, trackId);

            if (track == nullptr || placement >= track->Timeline.size())
            {
                InvalidateArrange();
                return;
            }

            auto const original = seq::FindClip(m_doc, track->Timeline[placement].ClipId);

            if (original == nullptr)
            {
                InvalidateArrange();
                return;
            }

            auto const clipBefore = *original;
            auto const copyName = std::wstring{ res::FormatString(L"UniqueClipNameFormat", clipBefore.Name) };
            auto const changed = seq::ResizeClipThroughPlacement(m_doc, trackId, placement, length, onlyHere, seq::NewId(L"c"), copyName);

            if (!changed.has_value())
            {
                InvalidateArrange();
                return;
            }

            auto const separate = changed->Id != clipBefore.Id;

            seq::ChangeList changes{};

            if (separate)
            {
                changes.push_back(seq::MakeClipPresenceChange(*changed, true));
            }
            else
            {
                changes.push_back(seq::MakeClipSettingsChange(clipBefore, *changed));
            }

            changes.push_back(seq::MakeTracksChange(before, m_doc.Tracks));
            Commit(std::wstring{ res::GetString(L"UndoResizeClip") }, std::move(changes));

            if (separate)
            {
                // It's no longer linked: the editor shows the copy, and the status bar says so.
                OpenClipInEditor(changed->Id, trackId);
                ShowMessage(res::FormatString(L"ClipNowSeparateFormat", changed->Name));
            }

            RefreshEditor();
            InvalidateArrange();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change a clip's length.")
    }

    // ---------------------------------------------------------------- the length chip

    _Use_decl_annotations_
    void MainWindow::OnEditorLengthClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr || clip->Kind != seq::ClipKind::Notes)
            {
                return;
            }

            auto const& meter = m_doc.Meter.empty() ? seq::MeterChange{} : m_doc.Meter.front();
            auto const perBar = seq::TicksPerBar(meter);
            auto const perBeat = seq::TicksPerBeat(meter);
            auto const beatsPerBar = std::max<int64_t>(1, perBar / std::max<int64_t>(1, perBeat));

            controls::StackPanel panel{};
            panel.Spacing(12);
            panel.Width(260);

            controls::StackPanel lengthRow{};
            lengthRow.Orientation(controls::Orientation::Horizontal);
            lengthRow.Spacing(12);

            controls::NumberBox bars{};
            controls::NumberBox beats{};

            auto const weak = get_weak();

            // Both boxes make one length; a clip is never shorter than a beat.
            auto const apply = [weak, perBar, perBeat, bars, beats]()
            {
                if (auto strong = weak.get())
                {
                    auto const barCount = std::isfinite(bars.Value()) ? std::llround(bars.Value()) : 0;
                    auto const beatCount = std::isfinite(beats.Value()) ? std::llround(beats.Value()) : 0;
                    strong->SetEditorClipLength(std::max<int64_t>(perBeat, barCount * perBar + beatCount * perBeat));
                }
            };

            auto const setUp = [&](controls::NumberBox const& box, wchar_t const* headerKey, double maximum, double value)
            {
                box.Header(winrt::box_value(res::GetString(headerKey)));
                automation::AutomationProperties::SetName(box, res::GetString(headerKey));
                box.Minimum(0);
                box.Maximum(maximum);
                box.SmallChange(1);
                box.LargeChange(4);
                box.Value(value);
                box.Width(120);
                box.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);
                box.ValueChanged([apply](controls::NumberBox const&, controls::NumberBoxValueChangedEventArgs const& args)
                {
                    if (std::isfinite(args.NewValue()) && args.NewValue() != args.OldValue())
                    {
                        apply();
                    }
                });
                lengthRow.Children().Append(box);
            };

            setUp(bars, L"ClipLengthBars", 999, static_cast<double>(clip->Length / perBar));
            setUp(beats, L"ClipLengthBeats", static_cast<double>(beatsPerBar - 1), static_cast<double>((clip->Length % perBar) / perBeat));
            panel.Children().Append(lengthRow);

            controls::ToggleSwitch loop{};
            loop.Header(winrt::box_value(res::GetString(L"ClipLoopSwitch")));
            automation::AutomationProperties::SetName(loop, res::GetString(L"ClipLoopSwitch"));
            automation::AutomationProperties::SetHelpText(loop, res::GetString(L"ClipLoopHelp"));
            loop.IsOn(clip->Loop);
            loop.Toggled([weak](foundation::IInspectable const& toggled, auto&&)
            {
                if (auto strong = weak.get())
                {
                    auto const id = strong->m_editorClipId;
                    strong->SetClipLoop(id, toggled.as<controls::ToggleSwitch>().IsOn());
                }
            });
            panel.Children().Append(loop);

            controls::TextBlock help{};
            help.Text(res::GetString(L"ClipLoopHelp"));
            help.TextWrapping(xaml::TextWrapping::Wrap);
            help.FontSize(12);
            help.Foreground(BrushFor(m_palette.Text2));
            panel.Children().Append(help);

            controls::Flyout flyout{};
            flyout.Content(panel);
            flyout.Placement(controls::Primitives::FlyoutPlacementMode::BottomEdgeAlignedLeft);
            flyout.ShowAt(sender.as<xaml::FrameworkElement>());
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the clip's length.")
    }

    _Use_decl_annotations_
    void MainWindow::SetEditorClipLength(int64_t length) noexcept
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr || clip->Length == length)
            {
                return;
            }

            auto const clipBefore = *clip;
            auto const tracksBefore = m_doc.Tracks;

            if (!seq::ResizeClip(m_doc, m_editorClipId, length))
            {
                return;
            }

            seq::ChangeList changes{};
            changes.push_back(seq::MakeClipSettingsChange(clipBefore, *seq::FindClip(m_doc, m_editorClipId)));
            changes.push_back(seq::MakeTracksChange(tracksBefore, m_doc.Tracks));
            Commit(std::wstring{ res::GetString(L"UndoResizeClip") }, std::move(changes));

            RefreshEditor();
            InvalidateArrange();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change the clip's length.")
    }

    _Use_decl_annotations_
    void MainWindow::SetClipLoop(std::wstring const& clipId, bool loop) noexcept
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, clipId);

            if (clip == nullptr || clip->Loop == loop)
            {
                return;
            }

            auto changed = *clip;
            changed.Loop = loop;

            seq::ChangeList changes{};
            changes.push_back(seq::MakeClipSettingsChange(*clip, changed));
            m_undo.ApplyAndCommit(m_doc, std::wstring{ res::GetString(L"UndoClipLoop") }, std::move(changes));
            DocumentChanged();

            RefreshEditor();
            InvalidateArrange();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to turn the clip's looping on or off.")
    }
}
