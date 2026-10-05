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
        constexpr double ShapeRowPreviewSize = 64.0;
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

        int32_t& FieldOf(_Inout_ patchbay::ValueShape& shape, _In_ MainWindow::ShapeField field) noexcept
        {
            switch (field)
            {
            case MainWindow::ShapeField::InputMaximum:
                return shape.InputMaximumHundredths;

            case MainWindow::ShapeField::OutputMinimum:
                return shape.OutputMinimumHundredths;

            case MainWindow::ShapeField::OutputMaximum:
                return shape.OutputMaximumHundredths;

            default:
                return shape.InputMinimumHundredths;
            }
        }

        wchar_t const* ShapeFieldHeader(_In_ MainWindow::ShapeField field) noexcept
        {
            switch (field)
            {
            case MainWindow::ShapeField::InputMaximum:
                return L"TransformShapeInputHigh";

            case MainWindow::ShapeField::OutputMinimum:
                return L"TransformShapeOutputLow";

            case MainWindow::ShapeField::OutputMaximum:
                return L"TransformShapeOutputHigh";

            default:
                return L"TransformShapeInputLow";
            }
        }

        controls::Border CurveFrame(_In_ controls::Canvas const& canvas, _In_ double size) noexcept
        {
            controls::Border frame{};

            frame.Width(size);
            frame.Height(size);
            frame.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(4));
            frame.BorderThickness(xaml::ThicknessHelper::FromUniformLength(1));
            frame.VerticalAlignment(xaml::VerticalAlignment::Top);
            frame.BorderBrush(patchbay::ThemeBrushes::Current().Get(L"CardStrokeColorDefaultBrush"));
            frame.Background(patchbay::ThemeBrushes::Current().Get(L"SolidBackgroundFillColorTertiaryBrush"));

            canvas.Width(size);
            canvas.Height(size);

            frame.Child(canvas);

            return frame;
        }

        // A dashed straight line for reference and the shaped line over it, both running 0 to 1
        // across and up.
        void DrawCurve(
            _In_ controls::Canvas const& canvas,
            _In_ double size,
            _In_ std::function<double(double)> const& shape) noexcept
        {
            try
            {
                if (canvas == nullptr)
                {
                    return;
                }

                canvas.Children().Clear();

                shapes::Polyline reference{};
                shapes::Polyline shaped{};

                media::PointCollection referencePoints{};
                media::PointCollection shapedPoints{};

                for (int32_t i = 0; i < CurveSampleCount; i++)
                {
                    auto const unit = static_cast<double>(i) / (CurveSampleCount - 1);
                    auto const x = static_cast<float>(unit * size);

                    referencePoints.Append(foundation::Point{
                        x, static_cast<float>(size - unit * size) });

                    shapedPoints.Append(foundation::Point{
                        x, static_cast<float>(size - shape(unit) * size) });
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

                canvas.Children().Append(reference);
                canvas.Children().Append(shaped);
            }
            MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to draw a curve preview.")
        }
    }

    // The mapping tables are edited as row lists, filled here from the sparse arrays the step
    // keeps, and collected back into them by CommitTransformMaps.
    void MainWindow::PrepareTransformRows() noexcept
    {
        try
        {
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

            m_controlValueRows.clear();

            for (size_t i = 0; i < m_editingTransform.ControlValueShapes.size(); i++)
            {
                if (!m_editingTransform.ControlValueShapes[i].ChangesNothing())
                {
                    m_controlValueRows.push_back(ControlValueRow{
                        static_cast<int32_t>(i), m_editingTransform.ControlValueShapes[i] });
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to list the mappings.")
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

            // A repeated controller wins on its last row, the same as the mapping tables.
            m_editingTransform.ControlValueShapes.fill(patchbay::ValueShape{});

            for (auto const& row : m_controlValueRows)
            {
                if (row.Controller >= 0 && row.Controller < static_cast<int32_t>(patchbay::ControlMapSize))
                {
                    m_editingTransform.ControlValueShapes[static_cast<size_t>(row.Controller)] = row.Shape;
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to collect the mapping tables.")
    }

    void MainWindow::BuildTransformSections() noexcept
    {
        try
        {
            auto const content = BlockDialogContent();

            switch (m_editingKind)
            {
            case patchbay::BlockKind::ChannelMap:
                content.Children().Append(TransformCard(BuildMapSection(TransformMap::Channel)));
                break;

            case patchbay::BlockKind::GroupMap:
                BuildGroupMapSection();
                break;

            case patchbay::BlockKind::NoteMap:
                content.Children().Append(TransformCard(BuildMapSection(TransformMap::Note)));
                BuildTransposeSection();
                break;

            case patchbay::BlockKind::Velocity:
                BuildValueScaleSection();
                BuildVelocitySection();
                break;

            case patchbay::BlockKind::Aftertouch:
                BuildValueScaleSection();
                content.Children().Append(TransformCard(BuildAftertouchSection()));
                break;

            case patchbay::BlockKind::ControlChangeMap:
                content.Children().Append(TransformCard(BuildMapSection(TransformMap::Control)));
                break;

            case patchbay::BlockKind::ControlChangeValue:
                BuildValueScaleSection();
                content.Children().Append(TransformCard(BuildControlValueSection()));
                break;

            case patchbay::BlockKind::ProgramMap:
            {
                content.Children().Append(TransformCard(BuildMapSection(TransformMap::Program)));

                controls::StackPanel body{};
                body.Spacing(4);

                body.Children().Append(BuildMapSection(TransformMap::BankMsb));

                auto lsb = BuildMapSection(TransformMap::BankLsb);
                lsb.Margin(xaml::ThicknessHelper::FromLengths(0, 10, 0, 0));

                body.Children().Append(lsb);

                content.Children().Append(TransformCard(body));
                break;
            }

            default:
                break;
            }

            // Each does nothing for a table or a list this kind doesn't show.
            for (int32_t i = 0; i < static_cast<int32_t>(TransformMapCount); i++)
            {
                RebuildMapRows(static_cast<TransformMap>(i));
            }

            RebuildControlValueRows();
            ApplyValueScale();
            RefreshVelocityEnabledState();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the step's sections.")
    }

    // First, for the kinds that show values as numbers, because it decides how every value below
    // it is read and typed.
    void MainWindow::BuildValueScaleSection() noexcept
    {
        try
        {
            auto weak = get_weak();

            controls::StackPanel body{};
            body.Spacing(4);

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
                    s->UpdateBlockSummary();
                });

            body.Children().Append(scale);
            body.Children().Append(TransformHint(resources::GetString(L"TransformValueScaleHint")));

            BlockDialogContent().Children().Append(TransformCard(body));
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the value scale section.")
    }

    void MainWindow::BuildTransposeSection() noexcept
    {
        try
        {
            auto weak = get_weak();

            controls::StackPanel body{};
            body.Spacing(4);

            body.Children().Append(TransformHeading(resources::GetString(L"TransformSectionExactPitch")));

            // The note map moves the note number the same way a transpose does, so both need to
            // know what to do with a MIDI 2.0 note that carries its own pitch.
            controls::CheckBox exactPitch{};

            exactPitch.Content(winrt::box_value(resources::GetString(L"TransformIgnoreExactPitch")));
            exactPitch.IsChecked(m_editingTransform.IgnoreExactPitchNotes);

            auto const setIgnoreExactPitch = [weak](bool value)
                {
                    if (auto s = weak.get())
                    {
                        s->m_editingTransform.IgnoreExactPitchNotes = value;
                        s->UpdateBlockSummary();
                    }
                };

            exactPitch.Checked([setIgnoreExactPitch](auto&&, auto&&) { setIgnoreExactPitch(true); });
            exactPitch.Unchecked([setIgnoreExactPitch](auto&&, auto&&) { setIgnoreExactPitch(false); });

            body.Children().Append(exactPitch);
            body.Children().Append(TransformHint(resources::GetString(L"TransformIgnoreExactPitchHint")));

            BlockDialogContent().Children().Append(TransformCard(body));
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the transpose section.")
    }

    void MainWindow::BuildVelocitySection() noexcept
    {
        try
        {
            auto weak = get_weak();

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
                            s->UpdateBlockSummary();
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

                    s->UpdateBlockSummary();
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
                    s->UpdateBlockSummary();
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

                            s->UpdateBlockSummary();
                        });

                    range.Children().Append(box);

                    return box;
                };

            m_minimumVelocityBox = addRangeBox(resources::GetString(L"TransformQuietest"), true);
            m_maximumVelocityBox = addRangeBox(resources::GetString(L"TransformLoudest"), false);

            options.Children().Append(range);

            controls::Grid::SetColumn(options, 0);
            layout.Children().Append(options);

            m_velocityCurveCanvas = controls::Canvas{};

            auto frame = CurveFrame(m_velocityCurveCanvas, CurvePreviewSize);
            controls::Grid::SetColumn(frame, 1);
            layout.Children().Append(frame);

            body.Children().Append(layout);

            BlockDialogContent().Children().Append(TransformCard(body));
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the velocity section.")
    }

    void MainWindow::BuildGroupMapSection() noexcept
    {
        try
        {
            auto weak = get_weak();

            controls::StackPanel body{};
            body.Spacing(4);

            body.Children().Append(TransformHeading(resources::GetString(L"TransformSectionGroupMap")));
            body.Children().Append(TransformHint(resources::GetString(L"TransformSectionGroupMapHint")));

            controls::VariableSizedWrapGrid grid{};
            grid.Orientation(controls::Orientation::Horizontal);
            grid.MaximumRowsOrColumns(2);
            grid.ItemWidth(350);
            grid.ItemHeight(44);

            for (size_t group = 0; group < m_editingSettings.GroupMap.size(); group++)
            {
                controls::StackPanel row{};
                row.Orientation(controls::Orientation::Horizontal);
                row.Spacing(10);

                // Groups are counted from one on screen and from zero in the file.
                controls::TextBlock label{};
                label.Text(resources::FormatString(L"FilterGroupFormat", static_cast<int>(group) + 1));
                label.Width(80);
                label.VerticalAlignment(xaml::VerticalAlignment::Center);
                row.Children().Append(label);

                auto arrow = MapRowArrow();
                arrow.VerticalAlignment(xaml::VerticalAlignment::Center);
                arrow.Margin(xaml::ThicknessHelper::FromUniformLength(0));
                row.Children().Append(arrow);

                controls::ComboBox target{};
                target.Width(170);
                target.Items().Append(winrt::box_value(resources::GetString(L"TransformGroupUnchanged")));

                for (int32_t to = 0; to < 16; to++)
                {
                    target.Items().Append(winrt::box_value(resources::FormatString(L"FilterGroupFormat", to + 1)));
                }

                auto const mapped = m_editingSettings.GroupMap[group];
                target.SelectedIndex(mapped < 0 || mapped > 15 ? 0 : mapped + 1);

                xaml::Automation::AutomationProperties::SetName(target,
                    resources::FormatString(L"TransformGroupMapAccessibleFormat", static_cast<int>(group) + 1));

                target.SelectionChanged([weak, group](foundation::IInspectable const& sender, auto&&)
                    {
                        auto s = weak.get();
                        auto const combo = sender.try_as<controls::ComboBox>();

                        if (s == nullptr || combo == nullptr || combo.SelectedIndex() < 0)
                        {
                            return;
                        }

                        s->m_editingSettings.GroupMap[group] = static_cast<int8_t>(combo.SelectedIndex() - 1);
                        s->UpdateBlockSummary();
                    });

                row.Children().Append(target);
                grid.Children().Append(row);
            }

            body.Children().Append(grid);

            BlockDialogContent().Children().Append(TransformCard(body));
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the group map section.")
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
                    s->UpdateBlockSummary();
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
                                s->UpdateBlockSummary();
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
                        s->UpdateBlockSummary();
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

            for (auto const& entry : m_shapeRangeBoxes)
            {
                if (auto* shape = EditingShape(entry.Which))
                {
                    applyBox(entry.Box, FieldOf(*shape, entry.Field));
                }
            }
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

            auto preview = m_editingTransform;
            preview.IsActive = true;

            DrawCurve(m_velocityCurveCanvas, CurvePreviewSize,
                [&preview](double unit) { return preview.ShapeUnit(unit); });
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to draw the velocity curve.")
    }

    xaml::UIElement MainWindow::BuildAftertouchSection() noexcept
    {
        controls::StackPanel body{};

        try
        {
            body.Spacing(4);

            body.Children().Append(TransformHeading(resources::GetString(L"TransformSectionAftertouch")));
            body.Children().Append(TransformHint(resources::GetString(L"TransformSectionAftertouchHint")));

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

            options.Children().Append(ShapeCurveBox(AftertouchShapeIndex));

            for (auto const& [low, high] : { std::pair{ ShapeField::InputMinimum, ShapeField::InputMaximum },
                                             std::pair{ ShapeField::OutputMinimum, ShapeField::OutputMaximum } })
            {
                controls::StackPanel range{};
                range.Orientation(controls::Orientation::Horizontal);
                range.Spacing(12);

                range.Children().Append(ShapeRangeBox(AftertouchShapeIndex, low));
                range.Children().Append(ShapeRangeBox(AftertouchShapeIndex, high));

                options.Children().Append(range);
            }

            controls::Grid::SetColumn(options, 0);
            layout.Children().Append(options);

            m_aftertouchPreview = controls::Canvas{};

            auto frame = CurveFrame(m_aftertouchPreview, CurvePreviewSize);
            controls::Grid::SetColumn(frame, 1);
            layout.Children().Append(frame);

            body.Children().Append(layout);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the aftertouch section.")

        return body;
    }

    controls::StackPanel MainWindow::BuildControlValueSection() noexcept
    {
        controls::StackPanel body{};

        try
        {
            body.Spacing(4);

            body.Children().Append(TransformHeading(resources::GetString(L"TransformSectionControlValues")));
            body.Children().Append(TransformHint(resources::GetString(L"TransformSectionControlValuesHint")));

            m_controlValuePanel = controls::StackPanel{};
            m_controlValuePanel.Spacing(12);
            m_controlValuePanel.Margin(xaml::ThicknessHelper::FromLengths(0, 4, 0, 6));

            body.Children().Append(m_controlValuePanel);

            controls::Button add{};
            add.Content(winrt::box_value(resources::GetString(L"TransformAddControlValue")));

            auto weak = get_weak();

            add.Click([weak](auto&&, auto&&)
                {
                    auto s = weak.get();

                    if (s == nullptr)
                    {
                        return;
                    }

                    auto& rows = s->m_controlValueRows;

                    if (rows.size() >= patchbay::ControlMapSize)
                    {
                        return;
                    }

                    // The first free controller counting up from 1, so a new row does not start
                    // out overriding one that is already there.
                    int32_t controller{ 1 };

                    for (size_t step = 0; step < patchbay::ControlMapSize; step++)
                    {
                        auto const candidate = static_cast<int32_t>((step + 1) % patchbay::ControlMapSize);

                        if (std::none_of(rows.begin(), rows.end(),
                            [candidate](ControlValueRow const& row) { return row.Controller == candidate; }))
                        {
                            controller = candidate;
                            break;
                        }
                    }

                    rows.push_back(ControlValueRow{ controller, patchbay::ValueShape{} });

                    s->RebuildControlValueRows();
                    s->UpdateBlockSummary();
                });

            body.Children().Append(add);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the controller value section.")

        return body;
    }

    void MainWindow::RebuildControlValueRows() noexcept
    {
        try
        {
            if (m_controlValuePanel == nullptr)
            {
                return;
            }

            m_controlValuePanel.Children().Clear();
            m_controlValuePreviews.clear();

            // The old rows' boxes go with them. The aftertouch boxes stay.
            m_shapeRangeBoxes.erase(std::remove_if(m_shapeRangeBoxes.begin(), m_shapeRangeBoxes.end(),
                [](ShapeRangeBoxEntry const& entry) { return entry.Which != AftertouchShapeIndex; }),
                m_shapeRangeBoxes.end());

            if (m_controlValueRows.empty())
            {
                auto empty = TransformHint(resources::GetString(L"TransformNoControlValues"));
                empty.Margin(xaml::ThicknessHelper::FromUniformLength(0));
                m_controlValuePanel.Children().Append(empty);

                return;
            }

            auto weak = get_weak();

            for (size_t position = 0; position < m_controlValueRows.size(); position++)
            {
                auto const which = static_cast<int32_t>(position);

                if (position > 0)
                {
                    shapes::Rectangle divider{};
                    divider.Height(1);
                    divider.Fill(patchbay::ThemeBrushes::Current().Get(L"DividerStrokeColorDefaultBrush"));

                    m_controlValuePanel.Children().Append(divider);
                }

                controls::Grid row{};
                row.ColumnSpacing(12);

                for (int32_t i = 0; i < 2; i++)
                {
                    controls::ColumnDefinition column{};
                    column.Width(xaml::GridLength{ 0, xaml::GridUnitType::Auto });
                    row.ColumnDefinitions().Append(column);
                }

                controls::StackPanel lines{};
                lines.Spacing(6);

                controls::StackPanel first{};
                first.Orientation(controls::Orientation::Horizontal);
                first.Spacing(8);

                auto controller = SmallNumberBox(0, 127, m_controlValueRows[position].Controller);
                controller.Header(winrt::box_value(resources::GetString(L"TransformShapeController")));

                controller.ValueChanged([weak, position](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                    {
                        auto s = weak.get();

                        if (s == nullptr || std::isnan(args.NewValue()) ||
                            position >= s->m_controlValueRows.size())
                        {
                            return;
                        }

                        s->m_controlValueRows[position].Controller =
                            static_cast<int32_t>(std::clamp(args.NewValue(), 0.0, 127.0));

                        s->UpdateBlockSummary();
                    });

                first.Children().Append(controller);
                first.Children().Append(ShapeCurveBox(which));
                first.Children().Append(ShapeInvertBox(which));

                controls::Button remove{};
                remove.Content(winrt::box_value(resources::GetString(L"TransformRemove")));
                remove.VerticalAlignment(xaml::VerticalAlignment::Bottom);

                remove.Click([weak, position](auto&&, auto&&)
                    {
                        auto s = weak.get();

                        if (s == nullptr || position >= s->m_controlValueRows.size())
                        {
                            return;
                        }

                        s->m_controlValueRows.erase(s->m_controlValueRows.begin() + static_cast<ptrdiff_t>(position));

                        s->RebuildControlValueRows();
                        s->UpdateBlockSummary();
                    });

                first.Children().Append(remove);
                lines.Children().Append(first);

                controls::StackPanel second{};
                second.Orientation(controls::Orientation::Horizontal);
                second.Spacing(8);

                second.Children().Append(ShapeRangeBox(which, ShapeField::InputMinimum));
                second.Children().Append(ShapeRangeBox(which, ShapeField::InputMaximum));
                second.Children().Append(MapRowArrow());
                second.Children().Append(ShapeRangeBox(which, ShapeField::OutputMinimum));
                second.Children().Append(ShapeRangeBox(which, ShapeField::OutputMaximum));

                lines.Children().Append(second);

                controls::Grid::SetColumn(lines, 0);
                row.Children().Append(lines);

                controls::Canvas preview{};

                auto frame = CurveFrame(preview, ShapeRowPreviewSize);
                frame.VerticalAlignment(xaml::VerticalAlignment::Center);

                controls::Grid::SetColumn(frame, 1);
                row.Children().Append(frame);

                m_controlValuePreviews.push_back(preview);
                m_controlValuePanel.Children().Append(row);
            }

            // New boxes need the number format and the top of the range for the current scale.
            ApplyValueScale();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to list the controller values.")
    }

    void MainWindow::DrawShapePreviews() noexcept
    {
        try
        {
            if (m_aftertouchPreview != nullptr)
            {
                auto const shape = m_editingTransform.AftertouchShape;

                DrawCurve(m_aftertouchPreview, CurvePreviewSize,
                    [&shape](double unit) { return shape.ShapeUnit(unit); });
            }

            auto const count = std::min(m_controlValuePreviews.size(), m_controlValueRows.size());

            for (size_t i = 0; i < count; i++)
            {
                auto const shape = m_controlValueRows[i].Shape;

                DrawCurve(m_controlValuePreviews[i], ShapeRowPreviewSize,
                    [&shape](double unit) { return shape.ShapeUnit(unit); });
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to draw the value previews.")
    }

    _Use_decl_annotations_
    ::midipatchbay::ValueShape* MainWindow::EditingShape(int32_t which) noexcept
    {
        if (which == AftertouchShapeIndex)
        {
            return &m_editingTransform.AftertouchShape;
        }

        if (which >= 0 && static_cast<size_t>(which) < m_controlValueRows.size())
        {
            return &m_controlValueRows[static_cast<size_t>(which)].Shape;
        }

        return nullptr;
    }

    _Use_decl_annotations_
    controls::ComboBox MainWindow::ShapeCurveBox(int32_t which) noexcept
    {
        controls::ComboBox box{};

        try
        {
            box.Header(winrt::box_value(resources::GetString(L"TransformShapeCurve")));
            box.MinWidth(160);

            box.Items().Append(winrt::box_value(resources::GetString(L"TransformShapeCurveLinear")));
            box.Items().Append(winrt::box_value(resources::GetString(L"TransformShapeCurveSlowRise")));
            box.Items().Append(winrt::box_value(resources::GetString(L"TransformShapeCurveFastRise")));

            auto const* shape = EditingShape(which);
            box.SelectedIndex(shape != nullptr ? static_cast<int32_t>(shape->Curve) : 0);

            auto weak = get_weak();

            box.SelectionChanged([weak, which](foundation::IInspectable const& sender, auto&&)
                {
                    auto s = weak.get();

                    if (s == nullptr)
                    {
                        return;
                    }

                    auto const combo = sender.try_as<controls::ComboBox>();

                    if (combo == nullptr || combo.SelectedIndex() < 0)
                    {
                        return;
                    }

                    auto* target = s->EditingShape(which);

                    if (target == nullptr)
                    {
                        return;
                    }

                    target->Curve = static_cast<patchbay::ValueCurve>(std::clamp(combo.SelectedIndex(), 0, 2));
                    s->UpdateBlockSummary();
                });
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build a curve picker.")

        return box;
    }

    _Use_decl_annotations_
    controls::CheckBox MainWindow::ShapeInvertBox(int32_t which) noexcept
    {
        controls::CheckBox check{};

        try
        {
            check.Content(winrt::box_value(resources::GetString(L"TransformShapeInvert")));
            check.VerticalAlignment(xaml::VerticalAlignment::Bottom);

            auto const* shape = EditingShape(which);
            check.IsChecked(shape != nullptr && shape->Invert);

            auto weak = get_weak();

            auto const setInvert = [weak, which](bool value)
                {
                    auto s = weak.get();

                    if (s == nullptr)
                    {
                        return;
                    }

                    if (auto* target = s->EditingShape(which))
                    {
                        target->Invert = value;
                        s->UpdateBlockSummary();
                    }
                };

            check.Checked([setInvert](auto&&, auto&&) { setInvert(true); });
            check.Unchecked([setInvert](auto&&, auto&&) { setInvert(false); });
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build an invert box.")

        return check;
    }

    _Use_decl_annotations_
    controls::NumberBox MainWindow::ShapeRangeBox(int32_t which, ShapeField field) noexcept
    {
        controls::NumberBox box{ nullptr };

        try
        {
            auto* shape = EditingShape(which);

            box = SmallNumberBox(0, 127, shape != nullptr
                ? patchbay::DisplayFromHundredths(FieldOf(*shape, field), m_editingTransform.Scale)
                : 0.0);

            box.Header(winrt::box_value(resources::GetString(ShapeFieldHeader(field))));

            auto weak = get_weak();

            box.ValueChanged([weak, which, field](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                {
                    auto s = weak.get();

                    if (s == nullptr || s->m_applyingValueScale || std::isnan(args.NewValue()))
                    {
                        return;
                    }

                    if (auto* target = s->EditingShape(which))
                    {
                        FieldOf(*target, field) =
                            patchbay::HundredthsFromDisplay(args.NewValue(), s->m_editingTransform.Scale);

                        s->UpdateBlockSummary();
                    }
                });

            m_shapeRangeBoxes.push_back(ShapeRangeBoxEntry{ box, which, field });
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build a range box.")

        return box;
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
