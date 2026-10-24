// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// A fretboard's section of the inspector: the tuning, the list of strings with their notes and
// the buttons that reorder them, the frets that show, how it plays and what it sends, and the
// button that adds a tuning picker and a row of fretboard buttons beside it.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "StringResources.h"
#include "EditorNoteBoxes.h"
#include "Fretboard.h"
#include "PadGrid.h"

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        void Show(_In_ xaml::UIElement const& element, _In_ bool visible)
        {
            element.Visibility(visible ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
        }

        // The orders the combo boxes list their choices in, and what each index means.
        constexpr glass::StrumSide StrumSideOrder[]{ glass::StrumSide::Left, glass::StrumSide::Right, glass::StrumSide::None };
        constexpr wchar_t const* StrumSideKeys[]{ L"FretStrumLeft", L"FretStrumRight", L"FretStrumNone" };

        constexpr glass::FretPlayMode PlayModeOrder[]{ glass::FretPlayMode::Tap, glass::FretPlayMode::Strum };
        constexpr wchar_t const* PlayModeKeys[]{ L"FretPlayTap", L"FretPlayStrum" };

        constexpr glass::FretSustain SustainOrder[]{ glass::FretSustain::WhileHeld, glass::FretSustain::UntilReplayed };
        constexpr wchar_t const* SustainKeys[]{ L"FretSustainWhileHeld", L"FretSustainUntilReplayed" };

        constexpr glass::OutOfKeyFrets OutOfKeyOrder[]{ glass::OutOfKeyFrets::Play, glass::OutOfKeyFrets::Dim, glass::OutOfKeyFrets::Mute };
        constexpr wchar_t const* OutOfKeyKeys[]{ L"FretOutOfKeyPlay", L"FretOutOfKeyDim", L"FretOutOfKeyMute" };

        constexpr glass::FretSlide SlideOrder[]{ glass::FretSlide::Stepped, glass::FretSlide::Smooth, glass::FretSlide::Off };
        constexpr wchar_t const* SlideKeys[]{ L"FretSlideStepped", L"FretSlideSmooth", L"FretSlideOff" };

        constexpr glass::FretOutput OutputOrder[]{ glass::FretOutput::PerNoteBend, glass::FretOutput::ChannelPerString, glass::FretOutput::Mpe };
        constexpr wchar_t const* OutputKeys[]{ L"FretOutputPerNoteBend", L"FretOutputChannelPerString", L"FretOutputMpe" };

        static_assert(std::size(StrumSideOrder) == std::size(StrumSideKeys));
        static_assert(std::size(PlayModeOrder) == std::size(PlayModeKeys));
        static_assert(std::size(SustainOrder) == std::size(SustainKeys));
        static_assert(std::size(OutOfKeyOrder) == std::size(OutOfKeyKeys));
        static_assert(std::size(SlideOrder) == std::size(SlideKeys));
        static_assert(std::size(OutputOrder) == std::size(OutputKeys));

        template <typename TEnum, size_t N>
        int32_t IndexIn(_In_ TEnum const (&order)[N], _In_ TEnum value) noexcept
        {
            for (size_t index = 0; index < N; ++index)
            {
                if (order[index] == value)
                {
                    return static_cast<int32_t>(index);
                }
            }

            return -1;
        }

        template <typename TEnum, size_t N>
        void ReadChoice(_In_ controls::ComboBox const& combo, _In_ TEnum const (&order)[N], _Inout_ TEnum& target)
        {
            auto const index = combo.SelectedIndex();

            if (index >= 0 && index < static_cast<int32_t>(N))
            {
                target = order[index];
            }
        }

        void Fill(_In_ controls::ComboBox const& combo, _In_ std::initializer_list<wchar_t const*> keys)
        {
            for (auto const* const key : keys)
            {
                combo.Items().Append(box_value(resources::GetString(key)));
            }
        }

        template <size_t N>
        void Fill(_In_ controls::ComboBox const& combo, _In_ wchar_t const* const (&keys)[N])
        {
            for (auto const* const key : keys)
            {
                combo.Items().Append(box_value(resources::GetString(key)));
            }
        }

        // The channel a note row of this fretboard sends on, counted from 0: its first.
        int32_t NoteRowChannel(_In_ glass::Control const& control) noexcept
        {
            for (auto const& message : control.Messages)
            {
                if (message.Kind == glass::MessageKind::Note)
                {
                    return std::clamp<int32_t>(message.ChannelIndex, 0, 15);
                }
            }

            return 0;
        }

        // A string added to the bottom of the list sits a fourth under the one above it, the
        // way most fretted instruments are tuned.
        constexpr int32_t AddedStringInterval = 5;
    }

    void EditorWindow::BuildFretboardChoices()
    {
        for (auto tuning = 0; tuning <= static_cast<int32_t>(glass::FretTuning::Custom); ++tuning)
        {
            FretTuningCombo().Items().Append(box_value(
                resources::GetString(glass::TuningNameResourceKey(static_cast<glass::FretTuning>(tuning)))));
        }

        Fill(FretStrumSideCombo(), StrumSideKeys);
        Fill(FretPlayModeCombo(), PlayModeKeys);
        Fill(FretSustainCombo(), SustainKeys);
        Fill(FretOutOfKeyCombo(), OutOfKeyKeys);
        Fill(FretSlideCombo(), SlideKeys);
        Fill(FretOutputCombo(), OutputKeys);
        Fill(FretKeyCombo(), { L"FretDotsNone" });

        for (int32_t root = 0; root < 12; ++root)
        {
            FretKeyCombo().Items().Append(box_value(winrt::hstring{ glass::RootPickerName(root) }));
        }

        for (auto const scale : glass::PadScaleOrder)
        {
            FretScaleCombo().Items().Append(box_value(resources::GetString(glass::ScaleNameResourceKey(scale))));
        }
    }

    // Called with the inspector already marked as filling itself in, so nothing set here records
    // an edit.
    _Use_decl_annotations_
    void EditorWindow::RefreshFretboardPanel(glass::Control const& control)
    {
        auto const spec = glass::NormalizedFretboard(control.Fretboard);
        auto const keyed = glass::HasFretKey(spec);

        FretTuningCombo().SelectedIndex(static_cast<int32_t>(spec.Tuning));

        // ---- the strings, one row each ----

        auto rows = FretStringRows().Children();
        rows.Clear();

        auto const count = spec.Strings.size();

        for (size_t index = 0; index < count; ++index)
        {
            auto const& string = spec.Strings[index];
            auto const number = std::to_wstring(index + 1);

            controls::Grid row{};
            row.ColumnSpacing(4);

            for (auto const width : { 64.0, -1.0, 0.0, 0.0, 0.0 })
            {
                controls::ColumnDefinition column{};

                column.Width(width < 0.0
                    ? xaml::GridLengthHelper::FromValueAndType(1.0, xaml::GridUnitType::Star)
                    : (width == 0.0 ? xaml::GridLengthHelper::Auto() : xaml::GridLengthHelper::FromPixels(width)));

                row.ColumnDefinitions().Append(column);
            }

            controls::TextBlock name{};
            name.Text(resources::FormatString(L"FretStringFormat", number));
            name.VerticalAlignment(xaml::VerticalAlignment::Center);
            name.FontSize(12);

            controls::NumberBox note{};
            note.Minimum(0);
            note.Maximum(127);
            note.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);
            UseNoteNames(note, true);
            note.Value(string.Note);
            xaml::Automation::AutomationProperties::SetName(note, resources::FormatString(L"FretStringNoteNameFormat", number));
            controls::Grid::SetColumn(note, 1);

            auto const iconButton = [&](wchar_t const* glyph, wchar_t const* nameKey, int32_t column, bool enabled)
                {
                    controls::Button button{};
                    controls::FontIcon icon{};

                    icon.Glyph(glyph);
                    icon.FontSize(12);

                    button.Content(icon);
                    button.IsEnabled(enabled);
                    button.VerticalAlignment(xaml::VerticalAlignment::Stretch);

                    auto const label = resources::GetString(nameKey);

                    xaml::Automation::AutomationProperties::SetName(button, label);
                    controls::ToolTipService::SetToolTip(button, box_value(label));
                    controls::Grid::SetColumn(button, column);

                    return button;
                };

            auto up = iconButton(L"\uE70E", L"FretMoveUpName", 2, index > 0);
            auto down = iconButton(L"\uE70D", L"FretMoveDownName", 3, index + 1 < count);
            auto remove = iconButton(L"\uE711", L"FretRemoveStringName", 4, count > static_cast<size_t>(glass::MinimumFretStrings));

            row.Children().Append(name);
            row.Children().Append(note);
            row.Children().Append(up);
            row.Children().Append(down);
            row.Children().Append(remove);

            rows.Append(row);

            // A short string, like a banjo's fifth, says where it starts.
            if (string.StartFret > 0)
            {
                controls::TextBlock shortCaption{};

                shortCaption.Text(resources::FormatString(L"FretShortStringFormat", std::to_wstring(string.StartFret)));
                shortCaption.FontSize(11);
                shortCaption.Margin(xaml::ThicknessHelper::FromLengths(68, 0, 0, 2));
                shortCaption.Foreground(xaml::Application::Current().Resources()
                    .Lookup(box_value(L"TextFillColorTertiaryBrush")).as<media::Brush>());

                rows.Append(shortCaption);
            }

            // The handlers go on after the box is filled in, so filling it in edits nothing.
            note.ValueChanged([weak = get_weak(), index](controls::NumberBox const& box, controls::NumberBoxValueChangedEventArgs const&)
                {
                    auto strong = weak.get();

                    if (strong == nullptr || strong->m_updatingInspector || std::isnan(box.Value()))
                    {
                        return;
                    }

                    auto const value = static_cast<int32_t>(std::lround(box.Value()));

                    strong->EditFretboard([index, value](glass::FretboardSpec& fretboard)
                        {
                            if (index < fretboard.Strings.size())
                            {
                                fretboard.Strings[index].Note = value;
                            }
                        });
                });

            auto const swapWith = [weak = get_weak(), index](size_t other)
                {
                    return [weak, index, other](foundation::IInspectable const&, xaml::RoutedEventArgs const&)
                        {
                            if (auto strong = weak.get())
                            {
                                strong->EditFretboard([index, other](glass::FretboardSpec& fretboard)
                                    {
                                        if (index < fretboard.Strings.size() && other < fretboard.Strings.size())
                                        {
                                            std::swap(fretboard.Strings[index], fretboard.Strings[other]);
                                        }
                                    });
                            }
                        };
                };

            if (index > 0)
            {
                up.Click(swapWith(index - 1));
            }

            if (index + 1 < count)
            {
                down.Click(swapWith(index + 1));
            }

            remove.Click([weak = get_weak(), index](foundation::IInspectable const&, xaml::RoutedEventArgs const&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->EditFretboard([index](glass::FretboardSpec& fretboard)
                            {
                                if (index < fretboard.Strings.size() && fretboard.Strings.size() > static_cast<size_t>(glass::MinimumFretStrings))
                                {
                                    fretboard.Strings.erase(fretboard.Strings.begin() + static_cast<ptrdiff_t>(index));
                                }
                            });
                    }
                });
        }

        FretAddStringButton().IsEnabled(count < static_cast<size_t>(glass::MaximumFretStrings));

        // ---- the neck ----

        FretCountBox().Value(spec.Frets);
        FretCapoBox().Value(spec.Capo);
        FretCapoBox().Maximum(spec.Frets - 1);
        FretFirstBox().Value(spec.FirstFret);
        FretFirstBox().Maximum(spec.Frets - 1);
        FretLastBox().Value(spec.LastFret);
        FretLastBox().Maximum(spec.Frets);

        FretStrumSideCombo().SelectedIndex(IndexIn(StrumSideOrder, spec.Strum));
        FretPlayModeCombo().SelectedIndex(IndexIn(PlayModeOrder, spec.Play));
        FretSustainCombo().SelectedIndex(IndexIn(SustainOrder, spec.Sustain));

        auto const strumWithoutSide = spec.Play == glass::FretPlayMode::Strum && spec.Strum == glass::StrumSide::None;

        FretPlayModeCaption().Text(resources::GetString(
            strumWithoutSide ? L"FretPlayStrumNoSideCaption"
            : spec.Play == glass::FretPlayMode::Strum ? L"FretPlayStrumCaption"
            : L"FretPlayTapCaption"));

        // ---- the key the dots show ----

        FretKeyCombo().SelectedIndex(keyed ? spec.KeyRoot + 1 : 0);
        FretScaleCombo().SelectedIndex(IndexIn(glass::PadScaleOrder, spec.Scale));
        FretScaleCombo().IsEnabled(keyed);
        FretOutOfKeyCombo().SelectedIndex(IndexIn(OutOfKeyOrder, spec.OutOfKey));
        FretOutOfKeyCombo().IsEnabled(keyed);

        // ---- expression ----

        FretBendBox().Value(spec.BendRangeSemitones);
        FretSlideCombo().SelectedIndex(IndexIn(SlideOrder, spec.Slide));
        FretHammerOnsCheck().IsChecked(spec.HammerOns);
        FretOutputCombo().SelectedIndex(IndexIn(OutputOrder, spec.Output));

        // Which channels the strings land on, worked out from the note row the way the player
        // does, so the caption can never name channels the strings don't use.
        auto const base = NoteRowChannel(control);
        auto const strings = static_cast<int32_t>(count);
        std::wstring caption{};

        switch (spec.Output)
        {
        case glass::FretOutput::ChannelPerString:
            caption = resources::FormatString(L"FretOutputChannelCaptionFormat",
                std::to_wstring(base + 1), std::to_wstring(std::min(base + strings, 16)));

            if (base + strings > 16)
            {
                caption += L" " + std::wstring{ resources::GetString(L"FretChannelsWrapCaption") };
            }
            break;

        case glass::FretOutput::Mpe:
            caption = resources::FormatString(L"FretOutputMpeCaptionFormat",
                std::to_wstring(base + 1), std::to_wstring(std::min(base + 2, 16)), std::to_wstring(std::min(base + 1 + strings, 16)));

            if (base + 1 + strings > 16)
            {
                caption += L" " + std::wstring{ resources::GetString(L"FretChannelsWrapCaption") };
            }
            break;

        default:
            caption = resources::GetString(L"FretOutputPerNoteCaption");
            break;
        }

        FretOutputCaption().Text(winrt::hstring{ caption });
    }

    _Use_decl_annotations_
    void EditorWindow::EditFretboard(std::function<void(glass::FretboardSpec&)> const& change)
    {
        auto const* const control = SingleSelectedControl();

        if (control == nullptr || !glass::IsFretboard(control->Kind))
        {
            return;
        }

        try
        {
            auto const id = control->Id;
            auto spec = glass::NormalizedFretboard(control->Fretboard);

            change(spec);

            // Any change to the strings themselves makes the name follow them: a preset when they
            // match one again, and Custom when they don't.
            spec.Tuning = glass::MatchingTuning(spec.Strings);

            ApplyControlEdit(id, [&](std::wstring const& controlId)
                { return m_editor.SetControlFretboard(controlId, spec); });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change this fretboard.")
    }

    void EditorWindow::ApplyFretboardEdit()
    {
        if (m_updatingInspector)
        {
            return;
        }

        EditFretboard([this](glass::FretboardSpec& spec)
            {
                // A number box someone has cleared reads as not a number, and leaves its value alone.
                auto const whole = [](controls::NumberBox const& box, int32_t& target)
                    {
                        if (!std::isnan(box.Value()))
                        {
                            target = static_cast<int32_t>(std::lround(box.Value()));
                        }
                    };

                auto const firstBefore = spec.FirstFret;
                auto const lastBefore = spec.LastFret;

                whole(FretCountBox(), spec.Frets);
                whole(FretCapoBox(), spec.Capo);
                whole(FretFirstBox(), spec.FirstFret);
                whole(FretLastBox(), spec.LastFret);
                whole(FretBendBox(), spec.BendRangeSemitones);

                // Moving From past To pushes To along with it, rather than refusing the edit.
                if (spec.FirstFret != firstBefore && spec.LastFret == lastBefore && spec.FirstFret >= spec.LastFret)
                {
                    spec.LastFret = std::min(spec.FirstFret + (lastBefore - std::max(firstBefore, 1)) + 1, spec.Frets);
                }

                ReadChoice(FretStrumSideCombo(), StrumSideOrder, spec.Strum);
                ReadChoice(FretPlayModeCombo(), PlayModeOrder, spec.Play);
                ReadChoice(FretSustainCombo(), SustainOrder, spec.Sustain);
                ReadChoice(FretOutOfKeyCombo(), OutOfKeyOrder, spec.OutOfKey);
                ReadChoice(FretSlideCombo(), SlideOrder, spec.Slide);
                ReadChoice(FretOutputCombo(), OutputOrder, spec.Output);

                auto const keyIndex = FretKeyCombo().SelectedIndex();

                if (keyIndex >= 0)
                {
                    spec.KeyRoot = keyIndex == 0 ? glass::NoKey : keyIndex - 1;
                }

                auto const scaleIndex = FretScaleCombo().SelectedIndex();

                if (scaleIndex >= 0 && scaleIndex < static_cast<int32_t>(std::size(glass::PadScaleOrder)))
                {
                    spec.Scale = glass::PadScaleOrder[scaleIndex];
                }

                spec.HammerOns = FretHammerOnsCheck().IsChecked() != nullptr && FretHammerOnsCheck().IsChecked().Value();
            });
    }

    _Use_decl_annotations_
    void EditorWindow::OnFretTuningChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        auto const index = FretTuningCombo().SelectedIndex();

        // Custom is what editing a string makes the tuning. Picking it changes nothing.
        if (index < 0 || index >= glass::FretTuningPresetCount)
        {
            return;
        }

        EditFretboard([index](glass::FretboardSpec& spec)
            {
                spec.Strings = glass::TuningStrings(static_cast<glass::FretTuning>(index));
            });
    }

    _Use_decl_annotations_
    void EditorWindow::OnFretboardChoiceChanged(foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyFretboardEdit();
    }

    _Use_decl_annotations_
    void EditorWindow::OnFretboardNumberChanged(controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyFretboardEdit();
    }

    _Use_decl_annotations_
    void EditorWindow::OnFretboardCheckChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyFretboardEdit();
    }

    _Use_decl_annotations_
    void EditorWindow::OnFretAddStringClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        EditFretboard([](glass::FretboardSpec& spec)
            {
                if (spec.Strings.size() >= static_cast<size_t>(glass::MaximumFretStrings))
                {
                    return;
                }

                glass::FretString added{};

                added.Note = spec.Strings.empty() ? 40 : std::max(spec.Strings.back().Note - AddedStringInterval, 0);

                spec.Strings.push_back(added);
            });
    }

    _Use_decl_annotations_
    void EditorWindow::OnAddFretboardButtonsClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        auto const* const control = SingleSelectedControl();

        if (control == nullptr || !glass::IsFretboard(control->Kind))
        {
            return;
        }

        try
        {
            // Copied, because adding to the page can move the fretboard in memory.
            auto const fretboardId = control->Id;

            glass::EditorController::FretboardButtonLabels labels{};

            labels.Tuning = resources::GetString(L"SwitchPicksTuning");
            labels.CapoDown = resources::GetString(L"PadFunctionCapoDown");
            labels.Capo = resources::GetString(L"ReadoutSourceCapo");
            labels.CapoUp = resources::GetString(L"PadFunctionCapoUp");
            labels.DownTheNeck = resources::GetString(L"PadFunctionDownTheNeck");
            labels.UpTheNeck = resources::GetString(L"PadFunctionUpTheNeck");
            labels.Mute = resources::GetString(L"PadFunctionMuteStrings");

            if (m_editor.AddFretboardButtons(fretboardId, labels).empty())
            {
                return;
            }

            RebuildSurface();
            RebuildOutline();
            RefreshInspector();
            UpdateStatusBar();
            MarkChanged();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to add the fretboard buttons.")
    }
}
