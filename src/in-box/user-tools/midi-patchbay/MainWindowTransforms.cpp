// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "BackgroundWork.h"
#include "GeneralMidi.h"
#include "StringResources.h"
#include "ThemeBrushes.h"

using namespace winrt::Microsoft::UI::Xaml;

namespace patchbay = ::midipatchbay;
namespace resources = ::midipatchbay::resources;
namespace numbers = ::winrt::Windows::Globalization::NumberFormatting;

namespace winrt::midipatchbay::implementation
{
    namespace
    {
        constexpr double CurvePreviewSize = 132.0;
        constexpr int32_t CurveSampleCount = 33;

        // Everything one of the mapping tables needs to draw itself. The stored value is always
        // the number on the wire; DisplayOffset is only what the customer sees, so channels can
        // be counted from one the way musicians count them.
        struct MapSectionInfo
        {
            wchar_t const* Heading;
            wchar_t const* Hint;
            wchar_t const* AddButton;
            wchar_t const* EmptyText;
            wchar_t const* FromHeader;
            wchar_t const* ToHeader;
            int32_t Maximum;
            int32_t DefaultValue;
            int32_t DisplayOffset;
            bool HasPlayButton;
            bool HasNames;
        };

        MapSectionInfo const& SectionInfo(_In_ MainWindow::TransformMap which) noexcept
        {
            static constexpr MapSectionInfo sections[]
            {
                // Note
                { L"TransformSectionNoteMap", L"TransformSectionNoteMapHint", L"TransformAddNoteMapping",
                  L"TransformNoNoteMappings", L"TransformNoteFrom", L"TransformNoteTo", 127, 60, 0, true, true },

                // Control
                { L"TransformSectionControlMap", L"TransformSectionControlMapHint", L"TransformAddControlMapping",
                  L"TransformNoControlMappings", L"TransformControlFrom", L"TransformControlTo", 127, 1, 0, false, false },

                // Channel
                { L"TransformSectionChannelMap", L"TransformSectionChannelMapHint", L"TransformAddChannelMapping",
                  L"TransformNoChannelMappings", L"TransformChannelFrom", L"TransformChannelTo", 15, 0, 1, false, false },

                // Program
                { L"TransformSectionProgramMap", L"TransformSectionProgramMapHint", L"TransformAddProgramMapping",
                  L"TransformNoProgramMappings", L"TransformProgramFrom", L"TransformProgramTo", 127, 0, 0, false, true },

                // BankMsb
                { L"TransformSectionBankMsbMap", L"TransformSectionBankMsbMapHint", L"TransformAddBankMsbMapping",
                  L"TransformNoBankMsbMappings", L"TransformBankFrom", L"TransformBankTo", 127, 0, 0, false, false },

                // BankLsb
                { L"TransformSectionBankLsbMap", L"TransformSectionBankLsbMapHint", L"TransformAddBankLsbMapping",
                  L"TransformNoBankLsbMappings", L"TransformBankFrom", L"TransformBankTo", 127, 0, 0, false, false },
            };

            static_assert(std::size(sections) == MainWindow::TransformMapCount);

            auto const index = static_cast<size_t>(which);

            return sections[index < std::size(sections) ? index : 0];
        }

        controls::TextBlock TransformHeading(_In_ winrt::hstring const& text) noexcept
        {
            controls::TextBlock block{};

            block.Text(text);
            block.FontSize(13);
            block.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());

            return block;
        }

        controls::TextBlock TransformHint(_In_ winrt::hstring const& text) noexcept
        {
            controls::TextBlock block{};

            block.Text(text);
            block.FontSize(11);
            block.TextWrapping(xaml::TextWrapping::Wrap);
            block.Margin(xaml::ThicknessHelper::FromLengths(0, -4, 0, 4));
            block.Foreground(patchbay::ThemeBrushes::Current().Get(L"TextFillColorTertiaryBrush"));

            return block;
        }

        controls::Border TransformCard(_In_ xaml::UIElement const& content) noexcept
        {
            controls::Border card{};

            card.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(6));
            card.BorderThickness(xaml::ThicknessHelper::FromUniformLength(1));
            card.Padding(xaml::ThicknessHelper::FromLengths(12, 10, 12, 12));
            card.BorderBrush(patchbay::ThemeBrushes::Current().Get(L"CardStrokeColorDefaultBrush"));
            card.Background(patchbay::ThemeBrushes::Current().Get(L"CardBackgroundFillColorSecondaryBrush"));
            card.Child(content);

            return card;
        }

        controls::NumberBox SmallNumberBox(_In_ double minimum, _In_ double maximum, _In_ double value) noexcept
        {
            controls::NumberBox box{};

            box.Minimum(minimum);
            box.Maximum(maximum);
            box.SmallChange(1);
            box.LargeChange(12);

            // Wide enough that the value is still readable once the clear button appears next to
            // the two inline spin buttons.
            box.Width(148);

            // Inline, not Compact: the compact spin buttons live in a popup that the dialog's
            // scroll viewer does not clip, so they hang over everything and never go away.
            box.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Inline);
            box.ValidationMode(controls::NumberBoxValidationMode::InvalidInputOverwritten);
            box.Value(value);

            return box;
        }

        controls::FontIcon MapRowArrow() noexcept
        {
            controls::FontIcon arrow{};

            arrow.Glyph(L"\uE72A");
            arrow.FontSize(12);
            arrow.VerticalAlignment(xaml::VerticalAlignment::Bottom);
            arrow.Margin(xaml::ThicknessHelper::FromLengths(0, 0, 0, 8));
            arrow.Foreground(patchbay::ThemeBrushes::Current().Get(L"TextFillColorTertiaryBrush"));

            return arrow;
        }
    }

    winrt::fire_and_forget MainWindow::ShowTransformDialogAsync(std::wstring connectionId)
    {
        auto strong = get_strong();

        try
        {
            auto* patch = CurrentPatch();

            if (patch == nullptr)
            {
                co_return;
            }

            auto const* connection = patch->FindConnection(connectionId);

            if (connection == nullptr)
            {
                co_return;
            }

            m_editingTransform = connection->Transform;
            m_editingTransformConnectionId = connectionId;

            // Captured now so the audition button knows where to play, even if the selection
            // changes behind the dialog.
            m_testEndpointDeviceId.clear();
            m_testGroupIndex = connection->DestinationGroupIndex;

            if (auto const* destination = patch->FindEndpoint(connection->DestinationEndpointId))
            {
                if (auto const live = patchbay::EndpointCatalog::Current().Resolve(*destination))
                {
                    m_testEndpointDeviceId = live->EndpointDeviceId;
                }
            }

            auto const fillRows = [this](TransformMap which, int16_t const* map, size_t count)
                {
                    auto& rows = m_mapRows[static_cast<size_t>(which)];

                    rows.clear();

                    for (size_t i = 0; i < count; i++)
                    {
                        if (map[i] >= 0)
                        {
                            rows.emplace_back(static_cast<int32_t>(i), map[i]);
                        }
                    }
                };

            fillRows(TransformMap::Note, m_editingTransform.NoteMap.data(), m_editingTransform.NoteMap.size());
            fillRows(TransformMap::Control, m_editingTransform.ControlMap.data(), m_editingTransform.ControlMap.size());
            fillRows(TransformMap::Channel, m_editingTransform.ChannelMap.data(), m_editingTransform.ChannelMap.size());
            fillRows(TransformMap::Program, m_editingTransform.ProgramMap.data(), m_editingTransform.ProgramMap.size());
            fillRows(TransformMap::BankMsb, m_editingTransform.BankMsbMap.data(), m_editingTransform.BankMsbMap.size());
            fillRows(TransformMap::BankLsb, m_editingTransform.BankLsbMap.data(), m_editingTransform.BankLsbMap.size());

            BuildTransformDialog();

            TransformDialog().XamlRoot(Content().XamlRoot());

            auto const result = co_await TransformDialog().ShowAsync();

            if (result == controls::ContentDialogResult::None)
            {
                co_return;
            }

            if (result == controls::ContentDialogResult::Secondary)
            {
                m_editingTransform.Reset();
            }
            else
            {
                CommitTransformMaps();
            }

            m_editingTransform.IsActive = true;
            m_editingTransform.IsActive = !m_editingTransform.ChangesNothing();

            patch = CurrentPatch();

            if (patch == nullptr)
            {
                co_return;
            }

            auto* target = patch->FindConnection(connectionId);

            if (target == nullptr)
            {
                co_return;
            }

            target->Transform = m_editingTransform;

            MarkDirty();
            RefreshInspector();
            ApplyRouting();
            UpdateMessages();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to edit the transforms.")
    }

    void MainWindow::CommitTransformMaps() noexcept
    {
        try
        {
            auto const collect = [this](TransformMap which, int16_t* map, size_t count)
                {
                    for (size_t i = 0; i < count; i++)
                    {
                        map[i] = -1;
                    }

                    // A repeated source wins on its last row, which is what the customer sees last.
                    for (auto const& [from, to] : m_mapRows[static_cast<size_t>(which)])
                    {
                        if (from >= 0 && from < static_cast<int32_t>(count) &&
                            to >= 0 && to < static_cast<int32_t>(count))
                        {
                            map[static_cast<size_t>(from)] = static_cast<int16_t>(to);
                        }
                    }
                };

            collect(TransformMap::Note, m_editingTransform.NoteMap.data(), m_editingTransform.NoteMap.size());
            collect(TransformMap::Control, m_editingTransform.ControlMap.data(), m_editingTransform.ControlMap.size());
            collect(TransformMap::Channel, m_editingTransform.ChannelMap.data(), m_editingTransform.ChannelMap.size());
            collect(TransformMap::Program, m_editingTransform.ProgramMap.data(), m_editingTransform.ProgramMap.size());
            collect(TransformMap::BankMsb, m_editingTransform.BankMsbMap.data(), m_editingTransform.BankMsbMap.size());
            collect(TransformMap::BankLsb, m_editingTransform.BankLsbMap.data(), m_editingTransform.BankLsbMap.size());
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to collect the mapping tables.")
    }

    void MainWindow::UpdateTransformSummary() noexcept
    {
        try
        {
            CommitTransformMaps();

            auto preview = m_editingTransform;
            preview.IsActive = true;

            TransformSummaryText().Text(resources::FormatString(L"TransformDialogSummaryFormat",
                patchbay::SummarizeTransform(preview)));

            if (m_transposeExampleText != nullptr)
            {
                // Middle C, so the example means something to a musician.
                constexpr uint8_t exampleNote = 60;

                m_transposeExampleText.Text(resources::FormatString(L"TransformTransposeExampleFormat",
                    patchbay::DescribeNote(exampleNote),
                    patchbay::DescribeNote(preview.ResultingNote(exampleNote))));
            }

            DrawVelocityCurve();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to summarize the transforms.")
    }

    void MainWindow::BuildTransformDialog() noexcept
    {
        try
        {
            TransformContent().Children().Clear();

            m_transposeExampleText = nullptr;
            m_velocityCurveCanvas = nullptr;
            m_fixedVelocityBox = nullptr;
            m_minimumVelocityBox = nullptr;
            m_maximumVelocityBox = nullptr;
            m_velocityRescaleCheck = nullptr;

            for (auto& panel : m_mapPanels)
            {
                panel = nullptr;
            }

            for (auto& labels : m_mapLabels)
            {
                labels.clear();
            }

            auto weak = get_weak();

            // ------------------------------------------------------------ channels
            TransformContent().Children().Append(TransformCard(BuildMapSection(TransformMap::Channel)));

            // ------------------------------------------------- transpose and notes
            {
                controls::StackPanel body{};
                body.Spacing(4);

                body.Children().Append(TransformHeading(resources::GetString(L"TransformSectionTranspose")));
                body.Children().Append(TransformHint(resources::GetString(L"TransformSectionTransposeHint")));

                controls::StackPanel row{};
                row.Orientation(controls::Orientation::Horizontal);
                row.Spacing(12);

                auto box = SmallNumberBox(patchbay::MinimumTranspose, patchbay::MaximumTranspose,
                    m_editingTransform.TransposeSemitones);

                box.Header(winrt::box_value(resources::GetString(L"TransformSemitones")));

                box.ValueChanged([weak](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                    {
                        auto s = weak.get();

                        if (s == nullptr || std::isnan(args.NewValue()))
                        {
                            return;
                        }

                        s->m_editingTransform.TransposeSemitones = static_cast<int32_t>(std::clamp(
                            args.NewValue(),
                            static_cast<double>(patchbay::MinimumTranspose),
                            static_cast<double>(patchbay::MaximumTranspose)));

                        s->UpdateTransformSummary();
                    });

                row.Children().Append(box);

                m_transposeExampleText = controls::TextBlock{};
                m_transposeExampleText.FontSize(13);
                m_transposeExampleText.VerticalAlignment(xaml::VerticalAlignment::Bottom);
                m_transposeExampleText.Margin(xaml::ThicknessHelper::FromLengths(0, 0, 0, 6));

                row.Children().Append(m_transposeExampleText);

                body.Children().Append(row);

                controls::CheckBox exactPitch{};

                exactPitch.Content(winrt::box_value(resources::GetString(L"TransformIgnoreExactPitch")));
                exactPitch.IsChecked(m_editingTransform.IgnoreExactPitchNotes);
                exactPitch.Margin(xaml::ThicknessHelper::FromLengths(0, 8, 0, 0));

                auto const setIgnoreExactPitch = [weak](bool value)
                    {
                        if (auto s = weak.get())
                        {
                            s->m_editingTransform.IgnoreExactPitchNotes = value;
                            s->UpdateTransformSummary();
                        }
                    };

                exactPitch.Checked([setIgnoreExactPitch](auto&&, auto&&) { setIgnoreExactPitch(true); });
                exactPitch.Unchecked([setIgnoreExactPitch](auto&&, auto&&) { setIgnoreExactPitch(false); });

                body.Children().Append(exactPitch);
                body.Children().Append(TransformHint(resources::GetString(L"TransformIgnoreExactPitchHint")));

                auto noteMap = BuildMapSection(TransformMap::Note);
                noteMap.Margin(xaml::ThicknessHelper::FromLengths(0, 10, 0, 0));

                body.Children().Append(noteMap);

                TransformContent().Children().Append(TransformCard(body));
            }

            // ------------------------------------------------------------ velocity
            {
                controls::StackPanel body{};
                body.Spacing(4);

                body.Children().Append(TransformHeading(resources::GetString(L"TransformSectionVelocity")));
                body.Children().Append(TransformHint(resources::GetString(L"TransformSectionVelocityHint")));

                controls::Grid layout{};
                layout.ColumnSpacing(20);

                for (auto const width : { xaml::GridLength{ 1, xaml::GridUnitType::Star },
                                          xaml::GridLength{ 0, xaml::GridUnitType::Auto } })
                {
                    controls::ColumnDefinition column{};
                    column.Width(width);
                    layout.ColumnDefinitions().Append(column);
                }

                controls::StackPanel options{};
                options.Spacing(6);

                // First, because it decides how every number below it is read and typed.
                {
                    controls::RadioButtons scale{};

                    scale.Header(winrt::box_value(resources::GetString(L"TransformValueScale")));
                    scale.MaxColumns(2);

                    scale.Items().Append(winrt::box_value(resources::GetString(L"TransformValueScaleSevenBit")));
                    scale.Items().Append(winrt::box_value(resources::GetString(L"TransformValueScalePercent")));

                    scale.SelectedIndex(m_editingTransform.Scale == patchbay::ValueScale::Percent ? 1 : 0);

                    scale.SelectionChanged([weak](foundation::IInspectable const& sender, auto&&)
                        {
                            auto s = weak.get();

                            if (s == nullptr || s->m_applyingValueScale)
                            {
                                return;
                            }

                            auto const list = sender.try_as<controls::RadioButtons>();

                            if (list == nullptr || list.SelectedIndex() < 0)
                            {
                                return;
                            }

                            s->m_editingTransform.Scale = list.SelectedIndex() == 1
                                ? patchbay::ValueScale::Percent
                                : patchbay::ValueScale::SevenBit;

                            s->ApplyValueScale();
                            s->UpdateTransformSummary();
                        });

                    options.Children().Append(scale);
                    options.Children().Append(TransformHint(resources::GetString(L"TransformValueScaleHint")));
                }

                auto const addCurveOption = [this, weak, &options](winrt::hstring const& text, patchbay::VelocityCurve curve)
                    {
                        controls::RadioButton radio{};

                        radio.Content(winrt::box_value(text));
                        radio.GroupName(L"VelocityCurve");
                        radio.IsChecked(m_editingTransform.Curve == curve);

                        radio.Checked([weak, curve](auto&&, auto&&)
                            {
                                auto s = weak.get();

                                if (s == nullptr)
                                {
                                    return;
                                }

                                s->m_editingTransform.Curve = curve;
                                s->RefreshVelocityEnabledState();
                                s->UpdateTransformSummary();
                            });

                        options.Children().Append(radio);
                    };

                addCurveOption(resources::GetString(L"TransformCurveUnchanged"), patchbay::VelocityCurve::Unchanged);
                addCurveOption(resources::GetString(L"TransformCurveLinearToCurved"), patchbay::VelocityCurve::LinearToCurved);
                addCurveOption(resources::GetString(L"TransformCurveCurvedToLinear"), patchbay::VelocityCurve::CurvedToLinear);
                addCurveOption(resources::GetString(L"TransformCurveFixed"), patchbay::VelocityCurve::Fixed);

                m_fixedVelocityBox = SmallNumberBox(0, 127, patchbay::DisplayFromHundredths(
                    m_editingTransform.FixedVelocityHundredths, m_editingTransform.Scale));

                m_fixedVelocityBox.Header(winrt::box_value(resources::GetString(L"TransformFixedVelocity")));
                m_fixedVelocityBox.Margin(xaml::ThicknessHelper::FromLengths(28, 0, 0, 0));
                m_fixedVelocityBox.HorizontalAlignment(xaml::HorizontalAlignment::Left);

                m_fixedVelocityBox.ValueChanged([weak](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                    {
                        auto s = weak.get();

                        if (s == nullptr || s->m_applyingValueScale || std::isnan(args.NewValue()))
                        {
                            return;
                        }

                        s->m_editingTransform.FixedVelocityHundredths =
                            patchbay::HundredthsFromDisplay(args.NewValue(), s->m_editingTransform.Scale);

                        s->UpdateTransformSummary();
                    });

                options.Children().Append(m_fixedVelocityBox);

                m_velocityRescaleCheck = controls::CheckBox{};

                m_velocityRescaleCheck.Content(winrt::box_value(resources::GetString(L"TransformRescaleVelocity")));
                m_velocityRescaleCheck.IsChecked(m_editingTransform.RescaleVelocity);
                m_velocityRescaleCheck.Margin(xaml::ThicknessHelper::FromLengths(0, 8, 0, 0));

                auto const setRescale = [weak](bool value)
                    {
                        auto s = weak.get();

                        if (s == nullptr)
                        {
                            return;
                        }

                        s->m_editingTransform.RescaleVelocity = value;
                        s->RefreshVelocityEnabledState();
                        s->UpdateTransformSummary();
                    };

                m_velocityRescaleCheck.Checked([setRescale](auto&&, auto&&) { setRescale(true); });
                m_velocityRescaleCheck.Unchecked([setRescale](auto&&, auto&&) { setRescale(false); });

                options.Children().Append(m_velocityRescaleCheck);

                controls::StackPanel range{};
                range.Orientation(controls::Orientation::Horizontal);
                range.Spacing(12);
                range.Margin(xaml::ThicknessHelper::FromLengths(0, 4, 0, 0));

                auto const addRangeBox = [this, weak, &range](winrt::hstring const& header, bool isLow)
                    {
                        auto box = SmallNumberBox(0, 127, patchbay::DisplayFromHundredths(
                            isLow ? m_editingTransform.MinimumVelocityHundredths
                                  : m_editingTransform.MaximumVelocityHundredths,
                            m_editingTransform.Scale));

                        box.Header(winrt::box_value(header));

                        box.ValueChanged([weak, isLow](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                            {
                                auto s = weak.get();

                                if (s == nullptr || s->m_applyingValueScale || std::isnan(args.NewValue()))
                                {
                                    return;
                                }

                                auto const value = patchbay::HundredthsFromDisplay(
                                    args.NewValue(), s->m_editingTransform.Scale);

                                if (isLow)
                                {
                                    s->m_editingTransform.MinimumVelocityHundredths = value;
                                }
                                else
                                {
                                    s->m_editingTransform.MaximumVelocityHundredths = value;
                                }

                                s->UpdateTransformSummary();
                            });

                        range.Children().Append(box);

                        return box;
                    };

                m_minimumVelocityBox = addRangeBox(resources::GetString(L"TransformQuietest"), true);
                m_maximumVelocityBox = addRangeBox(resources::GetString(L"TransformLoudest"), false);

                options.Children().Append(range);

                controls::Grid::SetColumn(options, 0);
                layout.Children().Append(options);

                controls::Border preview{};
                preview.Width(CurvePreviewSize);
                preview.Height(CurvePreviewSize);
                preview.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(4));
                preview.BorderThickness(xaml::ThicknessHelper::FromUniformLength(1));
                preview.VerticalAlignment(xaml::VerticalAlignment::Top);
                preview.BorderBrush(patchbay::ThemeBrushes::Current().Get(L"CardStrokeColorDefaultBrush"));
                preview.Background(patchbay::ThemeBrushes::Current().Get(L"SolidBackgroundFillColorTertiaryBrush"));

                m_velocityCurveCanvas = controls::Canvas{};
                m_velocityCurveCanvas.Width(CurvePreviewSize);
                m_velocityCurveCanvas.Height(CurvePreviewSize);

                preview.Child(m_velocityCurveCanvas);

                controls::Grid::SetColumn(preview, 1);
                layout.Children().Append(preview);

                body.Children().Append(layout);

                TransformContent().Children().Append(TransformCard(body));
            }

            // ------------------------------------------------------ control change
            TransformContent().Children().Append(TransformCard(BuildMapSection(TransformMap::Control)));

            // ------------------------------------------------------------ programs
            TransformContent().Children().Append(TransformCard(BuildMapSection(TransformMap::Program)));

            // --------------------------------------------------------- bank select
            {
                controls::StackPanel body{};
                body.Spacing(4);

                body.Children().Append(BuildMapSection(TransformMap::BankMsb));

                auto lsb = BuildMapSection(TransformMap::BankLsb);
                lsb.Margin(xaml::ThicknessHelper::FromLengths(0, 10, 0, 0));

                body.Children().Append(lsb);

                TransformContent().Children().Append(TransformCard(body));
            }

            for (int32_t i = 0; i < static_cast<int32_t>(TransformMapCount); i++)
            {
                RebuildMapRows(static_cast<TransformMap>(i));
            }

            ApplyValueScale();
            RefreshVelocityEnabledState();
            UpdateTransformSummary();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the transform dialog.")
    }

    _Use_decl_annotations_
    controls::StackPanel MainWindow::BuildMapSection(TransformMap which) noexcept
    {
        controls::StackPanel body{};

        try
        {
            auto const& info = SectionInfo(which);
            auto const index = static_cast<size_t>(which);

            body.Spacing(4);

            body.Children().Append(TransformHeading(resources::GetString(info.Heading)));
            body.Children().Append(TransformHint(resources::GetString(info.Hint)));

            m_mapPanels[index] = controls::StackPanel{};
            m_mapPanels[index].Spacing(6);
            m_mapPanels[index].Margin(xaml::ThicknessHelper::FromLengths(0, 4, 0, 6));

            body.Children().Append(m_mapPanels[index]);

            controls::Button add{};
            add.Content(winrt::box_value(resources::GetString(info.AddButton)));

            auto weak = get_weak();

            add.Click([weak, which](auto&&, auto&&)
                {
                    auto s = weak.get();

                    if (s == nullptr)
                    {
                        return;
                    }

                    auto& rows = s->m_mapRows[static_cast<size_t>(which)];
                    auto const& added = SectionInfo(which);

                    if (rows.size() >= std::min(patchbay::MaximumMapEntries,
                        static_cast<size_t>(added.Maximum) + 1))
                    {
                        return;
                    }

                    rows.emplace_back(added.DefaultValue, added.DefaultValue);

                    s->RebuildMapRows(which);
                    s->UpdateTransformSummary();
                });

            body.Children().Append(add);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build a mapping section.")

        return body;
    }

    _Use_decl_annotations_
    void MainWindow::RebuildMapRows(TransformMap which) noexcept
    {
        try
        {
            auto const index = static_cast<size_t>(which);

            if (m_mapPanels[index] == nullptr)
            {
                return;
            }

            auto const& info = SectionInfo(which);
            auto& rows = m_mapRows[index];

            m_mapPanels[index].Children().Clear();
            m_mapLabels[index].clear();

            auto weak = get_weak();

            if (rows.empty())
            {
                auto empty = TransformHint(resources::GetString(info.EmptyText));
                empty.Margin(xaml::ThicknessHelper::FromUniformLength(0));
                m_mapPanels[index].Children().Append(empty);

                return;
            }

            for (size_t position = 0; position < rows.size(); position++)
            {
                controls::StackPanel row{};
                row.Orientation(controls::Orientation::Horizontal);
                row.Spacing(8);

                auto const addBox = [weak, which, &info, &rows, position, &row](bool isSource)
                    {
                        auto box = SmallNumberBox(info.DisplayOffset, info.Maximum + info.DisplayOffset,
                            (isSource ? rows[position].first : rows[position].second) + info.DisplayOffset);

                        box.Header(winrt::box_value(resources::GetString(
                            isSource ? info.FromHeader : info.ToHeader)));

                        box.ValueChanged([weak, which, position, isSource](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                            {
                                auto s = weak.get();

                                if (s == nullptr || std::isnan(args.NewValue()))
                                {
                                    return;
                                }

                                auto& target = s->m_mapRows[static_cast<size_t>(which)];

                                if (position >= target.size())
                                {
                                    return;
                                }

                                auto const& bounds = SectionInfo(which);

                                auto const value = static_cast<int32_t>(std::clamp(args.NewValue(),
                                    static_cast<double>(bounds.DisplayOffset),
                                    static_cast<double>(bounds.Maximum + bounds.DisplayOffset))) - bounds.DisplayOffset;

                                if (isSource)
                                {
                                    target[position].first = value;
                                }
                                else
                                {
                                    target[position].second = value;
                                }

                                s->RefreshMapRowLabels(which);
                                s->UpdateTransformSummary();
                            });

                        row.Children().Append(box);
                    };

                addBox(true);
                row.Children().Append(MapRowArrow());
                addBox(false);

                if (info.HasNames)
                {
                    controls::TextBlock names{};
                    names.FontSize(12);
                    names.VerticalAlignment(xaml::VerticalAlignment::Bottom);
                    names.Margin(xaml::ThicknessHelper::FromLengths(0, 0, 0, 8));
                    names.MinWidth(120);
                    names.MaxWidth(240);
                    names.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
                    names.Foreground(patchbay::ThemeBrushes::Current().Get(L"TextFillColorSecondaryBrush"));
                    row.Children().Append(names);

                    m_mapLabels[index].push_back(names);
                }

                if (info.HasPlayButton)
                {
                    controls::Button play{};
                    play.Content(winrt::box_value(resources::GetString(L"TransformPlay")));
                    play.VerticalAlignment(xaml::VerticalAlignment::Bottom);

                    xaml::Automation::AutomationProperties::SetName(play,
                        resources::GetString(L"TransformPlayAccessibleName"));

                    play.Click([weak, which, position](auto&&, auto&&)
                        {
                            auto s = weak.get();

                            if (s == nullptr)
                            {
                                return;
                            }

                            auto const& target = s->m_mapRows[static_cast<size_t>(which)];

                            if (position >= target.size())
                            {
                                return;
                            }

                            s->PlayTestNoteAsync(static_cast<uint8_t>(
                                std::clamp(target[position].second, 0, 127)));
                        });

                    row.Children().Append(play);
                }

                controls::Button remove{};
                remove.Content(winrt::box_value(resources::GetString(L"TransformRemove")));
                remove.VerticalAlignment(xaml::VerticalAlignment::Bottom);

                remove.Click([weak, which, position](auto&&, auto&&)
                    {
                        auto s = weak.get();

                        if (s == nullptr)
                        {
                            return;
                        }

                        auto& target = s->m_mapRows[static_cast<size_t>(which)];

                        if (position >= target.size())
                        {
                            return;
                        }

                        target.erase(target.begin() + static_cast<ptrdiff_t>(position));

                        s->RebuildMapRows(which);
                        s->UpdateTransformSummary();
                    });

                row.Children().Append(remove);

                m_mapPanels[index].Children().Append(row);
            }

            RefreshMapRowLabels(which);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to list the mappings.")
    }

    _Use_decl_annotations_
    void MainWindow::RefreshMapRowLabels(TransformMap which) noexcept
    {
        try
        {
            auto const index = static_cast<size_t>(which);

            if (!SectionInfo(which).HasNames)
            {
                return;
            }

            auto const count = std::min(m_mapLabels[index].size(), m_mapRows[index].size());

            for (size_t i = 0; i < count; i++)
            {
                if (m_mapLabels[index][i] == nullptr)
                {
                    continue;
                }

                auto const from = static_cast<uint8_t>(std::clamp(m_mapRows[index][i].first, 0, 127));
                auto const to = static_cast<uint8_t>(std::clamp(m_mapRows[index][i].second, 0, 127));

                // The General MIDI names are what an instrument without a program list would
                // play. A device with its own names is not consulted here.
                m_mapLabels[index][i].Text(which == TransformMap::Program
                    ? resources::FormatString(L"TransformMapLabelFormat",
                        midiapp::GeneralMidiProgramName(from), midiapp::GeneralMidiProgramName(to))
                    : resources::FormatString(L"TransformMapLabelFormat",
                        patchbay::DescribeNote(from), patchbay::DescribeNote(to)));
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to label the mappings.")
    }

    void MainWindow::ApplyValueScale() noexcept
    {
        try
        {
            m_applyingValueScale = true;
            auto const done = wil::scope_exit([this]() { m_applyingValueScale = false; });

            auto const scale = m_editingTransform.Scale;
            auto const isPercent = scale == patchbay::ValueScale::Percent;

            numbers::DecimalFormatter formatter{};

            formatter.IntegerDigits(1);
            formatter.FractionDigits(isPercent ? 2 : 0);
            formatter.IsGrouped(false);

            // The maximum has to move before the value does. A 127 sitting in a box that is about
            // to top out at 100 would otherwise be clamped on the way past.
            auto const applyBox = [&formatter, isPercent, scale](controls::NumberBox const& box, int32_t hundredths)
                {
                    if (box == nullptr)
                    {
                        return;
                    }

                    box.NumberFormatter(formatter);
                    box.Maximum(isPercent ? 100.0 : 127.0);
                    box.Value(patchbay::DisplayFromHundredths(hundredths, scale));
                };

            applyBox(m_fixedVelocityBox, m_editingTransform.FixedVelocityHundredths);
            applyBox(m_minimumVelocityBox, m_editingTransform.MinimumVelocityHundredths);
            applyBox(m_maximumVelocityBox, m_editingTransform.MaximumVelocityHundredths);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to change the value scale.")
    }

    void MainWindow::RefreshVelocityEnabledState() noexcept
    {
        try
        {
            auto const isFixed = m_editingTransform.Curve == patchbay::VelocityCurve::Fixed;

            if (m_fixedVelocityBox != nullptr)
            {
                m_fixedVelocityBox.IsEnabled(isFixed);
            }

            // A fixed velocity ignores what was played, so a range on top of it would mean nothing.
            if (m_velocityRescaleCheck != nullptr)
            {
                m_velocityRescaleCheck.IsEnabled(!isFixed);
            }

            auto const rangeEnabled = !isFixed && m_editingTransform.RescaleVelocity;

            if (m_minimumVelocityBox != nullptr)
            {
                m_minimumVelocityBox.IsEnabled(rangeEnabled);
            }

            if (m_maximumVelocityBox != nullptr)
            {
                m_maximumVelocityBox.IsEnabled(rangeEnabled);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to update the velocity controls.")
    }

    void MainWindow::DrawVelocityCurve() noexcept
    {
        try
        {
            if (m_velocityCurveCanvas == nullptr)
            {
                return;
            }

            m_velocityCurveCanvas.Children().Clear();

            auto preview = m_editingTransform;
            preview.IsActive = true;

            shapes::Polyline reference{};
            shapes::Polyline shaped{};

            media::PointCollection referencePoints{};
            media::PointCollection shapedPoints{};

            for (int32_t i = 0; i < CurveSampleCount; i++)
            {
                auto const unit = static_cast<double>(i) / (CurveSampleCount - 1);
                auto const x = static_cast<float>(unit * CurvePreviewSize);

                referencePoints.Append(foundation::Point{
                    x, static_cast<float>(CurvePreviewSize - unit * CurvePreviewSize) });

                shapedPoints.Append(foundation::Point{
                    x, static_cast<float>(CurvePreviewSize - preview.ShapeUnit(unit) * CurvePreviewSize) });
            }

            reference.Points(referencePoints);
            reference.StrokeThickness(1.0);
            reference.Stroke(patchbay::ThemeBrushes::Current().Get(L"TextFillColorTertiaryBrush"));
            reference.StrokeDashArray([]()
                {
                    media::DoubleCollection dashes{};
                    dashes.Append(3.0);
                    dashes.Append(3.0);
                    return dashes;
                }());

            shaped.Points(shapedPoints);
            shaped.StrokeThickness(2.0);
            shaped.Stroke(patchbay::ThemeBrushes::Current().Get(L"AccentFillColorDefaultBrush"));

            m_velocityCurveCanvas.Children().Append(reference);
            m_velocityCurveCanvas.Children().Append(shaped);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to draw the velocity curve.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::PlayTestNoteAsync(uint8_t note)
    {
        auto strong = get_strong();

        try
        {
            auto const endpointDeviceId = m_testEndpointDeviceId;
            auto const group = m_testGroupIndex;

            if (endpointDeviceId.empty())
            {
                ShowStatus(resources::GetString(L"TransformTestNoDestination"),
                    controls::InfoBarSeverity::Warning);
                co_return;
            }

            bool played{ false };

            // The SDK calls block on the service, and the note is held for a moment, so none of
            // this can happen on the UI thread.
            co_await patchbay::RunOnBackgroundAsync([&played, endpointDeviceId, group, note]()
                {
                    played = patchbay::RouteEngine::Current().SendTestNote(endpointDeviceId, group, note);
                });

            if (!played)
            {
                ShowStatus(resources::GetString(L"TransformTestFailed"), controls::InfoBarSeverity::Warning);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to play the test note.")
    }
}
