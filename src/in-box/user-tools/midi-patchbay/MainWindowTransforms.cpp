// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "BackgroundWork.h"
#include "DialogParts.h"
#include "RoundedShape.h"
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

        using patchbay::parts::Card;
        using patchbay::parts::CurveFrame;
        using patchbay::parts::DrawCurve;
        using patchbay::parts::Heading;
        using patchbay::parts::HeadingHint;

        controls::NumberBox SmallNumberBox(_In_ double minimum, _In_ double maximum, _In_ double value)
        {
            auto box = patchbay::parts::NumberBox({}, minimum, maximum, value, 12);

            // Wide enough that the value is still readable once the clear button appears next to
            // the two inline spin buttons.
            box.Width(148);

            return box;
        }

        controls::FontIcon MapRowArrow()
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
    }

    // The controller value rules are edited as a row list, filled here from the array the step
    // keeps, and collected back into it by CommitTransformMaps.
    void MainWindow::PrepareTransformRows() noexcept
    {
        try
        {
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
            // A repeated controller wins on its last row, the same as the mapping lists.
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
            case patchbay::BlockKind::Velocity:
                BuildValueScaleSection();
                BuildVelocitySection();
                break;

            case patchbay::BlockKind::Aftertouch:
                BuildValueScaleSection();
                content.Children().Append(Card(BuildAftertouchSection()));
                break;

            case patchbay::BlockKind::ControlChangeValue:
                BuildValueScaleSection();
                content.Children().Append(Card(BuildControlValueSection()));
                break;

            default:
                break;
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
            body.Children().Append(HeadingHint(resources::GetString(L"TransformValueScaleHint")));

            BlockDialogContent().Children().Append(Card(body));
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the value scale section.")
    }

    void MainWindow::BuildVelocitySection() noexcept
    {
        try
        {
            auto weak = get_weak();

            controls::StackPanel body{};
            body.Spacing(4);

            body.Children().Append(Heading(resources::GetString(L"TransformSectionVelocity")));
            body.Children().Append(HeadingHint(resources::GetString(L"TransformSectionVelocityHint")));

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

            BlockDialogContent().Children().Append(Card(body));
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the velocity section.")
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

            body.Children().Append(Heading(resources::GetString(L"TransformSectionAftertouch")));
            body.Children().Append(HeadingHint(resources::GetString(L"TransformSectionAftertouchHint")));

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

            body.Children().Append(Heading(resources::GetString(L"TransformSectionControlValues")));
            body.Children().Append(HeadingHint(resources::GetString(L"TransformSectionControlValuesHint")));

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
                auto empty = HeadingHint(resources::GetString(L"TransformNoControlValues"));
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

            auto const& parameterRows = m_editingSettings.ParameterTransform.Rows;
            auto const parameterCount = std::min(m_parameterCurveCanvases.size(), parameterRows.size());

            for (size_t i = 0; i < parameterCount; i++)
            {
                auto const shape = parameterRows[i].Shape;

                DrawCurve(m_parameterCurveCanvases[i], ShapeRowPreviewSize,
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

    _Use_decl_annotations_
    void MainWindow::FindTestDestination(std::wstring const& blockId) noexcept
    {
        m_testEndpointDeviceId.clear();
        m_testGroupIndex = patchbay::AllGroups;

        try
        {
            auto const* patch = CurrentPatch();

            if (patch == nullptr)
            {
                return;
            }

            std::vector<std::wstring> pending{ blockId };
            std::unordered_set<std::wstring> seen{};

            while (!pending.empty() && m_testEndpointDeviceId.empty())
            {
                auto const current = pending.front();
                pending.erase(pending.begin());

                if (!seen.insert(current).second)
                {
                    continue;
                }

                for (auto const& link : patch->Connections)
                {
                    if (link.SourceId != current)
                    {
                        continue;
                    }

                    if (auto const* next = patch->FindBlock(link.DestinationId))
                    {
                        // What goes into an LFO doesn't come out of it.
                        if (!patchbay::IsGenerator(next->Kind))
                        {
                            pending.push_back(link.DestinationId);
                        }

                        continue;
                    }

                    if (auto const* destination = patch->FindEndpoint(link.DestinationId))
                    {
                        if (auto const live = patchbay::ResolveEndpoint(*destination))
                        {
                            m_testEndpointDeviceId = live->EndpointDeviceId;
                            m_testGroupIndex = link.DestinationGroupIndex;
                            break;
                        }
                    }
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to find where a test note plays.")
    }
}
