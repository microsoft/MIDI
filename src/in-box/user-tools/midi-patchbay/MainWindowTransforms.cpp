// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "BackgroundWork.h"
#include "StringResources.h"
#include "ThemeBrushes.h"

using namespace winrt::Microsoft::UI::Xaml;

namespace patchbay = ::midipatchbay;
namespace resources = ::midipatchbay::resources;

namespace winrt::midipatchbay::implementation
{
    namespace
    {
        constexpr double CurvePreviewSize = 132.0;
        constexpr int32_t CurveSampleCount = 33;

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
            box.Width(112);
            box.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);
            box.ValidationMode(controls::NumberBoxValidationMode::InvalidInputOverwritten);
            box.Value(value);

            return box;
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

            m_noteMapRows.clear();
            m_controlMapRows.clear();

            for (size_t i = 0; i < patchbay::NoteMapSize; i++)
            {
                if (m_editingTransform.NoteMap[i] >= 0)
                {
                    m_noteMapRows.emplace_back(static_cast<int32_t>(i), m_editingTransform.NoteMap[i]);
                }
            }

            for (size_t i = 0; i < patchbay::ControlMapSize; i++)
            {
                if (m_editingTransform.ControlMap[i] >= 0)
                {
                    m_controlMapRows.emplace_back(static_cast<int32_t>(i), m_editingTransform.ControlMap[i]);
                }
            }

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
            m_editingTransform.NoteMap.fill(-1);
            m_editingTransform.ControlMap.fill(-1);

            // A repeated source wins on its last row, which is what the customer sees last.
            for (auto const& [from, to] : m_noteMapRows)
            {
                if (from >= 0 && from < static_cast<int32_t>(patchbay::NoteMapSize) && to >= 0 && to <= 127)
                {
                    m_editingTransform.NoteMap[static_cast<size_t>(from)] = static_cast<int16_t>(to);
                }
            }

            for (auto const& [from, to] : m_controlMapRows)
            {
                if (from >= 0 && from < static_cast<int32_t>(patchbay::ControlMapSize) && to >= 0 && to <= 127)
                {
                    m_editingTransform.ControlMap[static_cast<size_t>(from)] = static_cast<int16_t>(to);
                }
            }
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
            m_noteMapPanel = nullptr;
            m_controlMapPanel = nullptr;
            m_velocityRangeBoxes.clear();

            auto weak = get_weak();

            // ------------------------------------------------------------ transpose
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

                TransformContent().Children().Append(TransformCard(body));
            }

            // ------------------------------------------------------------ note map
            {
                controls::StackPanel body{};
                body.Spacing(4);

                body.Children().Append(TransformHeading(resources::GetString(L"TransformSectionNoteMap")));
                body.Children().Append(TransformHint(resources::GetString(L"TransformSectionNoteMapHint")));

                m_noteMapPanel = controls::StackPanel{};
                m_noteMapPanel.Spacing(6);
                m_noteMapPanel.Margin(xaml::ThicknessHelper::FromLengths(0, 4, 0, 6));

                body.Children().Append(m_noteMapPanel);

                controls::Button add{};
                add.Content(winrt::box_value(resources::GetString(L"TransformAddNoteMapping")));

                add.Click([weak](auto&&, auto&&)
                    {
                        auto s = weak.get();

                        if (s == nullptr || s->m_noteMapRows.size() >= patchbay::MaximumMapEntries)
                        {
                            return;
                        }

                        s->m_noteMapRows.emplace_back(60, 60);
                        s->RebuildNoteMapRows();
                        s->UpdateTransformSummary();
                    });

                body.Children().Append(add);

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

                auto const addCurveOption = [this, weak, &options](winrt::hstring const& text, patchbay::VelocityCurve curve)
                    {
                        controls::RadioButton radio{};

                        radio.Content(winrt::box_value(text));
                        radio.GroupName(L"VelocityCurve");
                        radio.IsChecked(m_editingTransform.Curve == curve);

                        radio.Checked([weak, curve](auto&&, auto&&)
                            {
                                if (auto s = weak.get())
                                {
                                    s->m_editingTransform.Curve = curve;
                                    s->UpdateTransformSummary();
                                }
                            });

                        options.Children().Append(radio);
                    };

                addCurveOption(resources::GetString(L"TransformCurveUnchanged"), patchbay::VelocityCurve::Unchanged);
                addCurveOption(resources::GetString(L"TransformCurveLinearToCurved"), patchbay::VelocityCurve::LinearToCurved);
                addCurveOption(resources::GetString(L"TransformCurveCurvedToLinear"), patchbay::VelocityCurve::CurvedToLinear);

                controls::CheckBox rescale{};

                rescale.Content(winrt::box_value(resources::GetString(L"TransformRescaleVelocity")));
                rescale.IsChecked(m_editingTransform.RescaleVelocity);
                rescale.Margin(xaml::ThicknessHelper::FromLengths(0, 8, 0, 0));

                auto const setRescale = [weak](bool value)
                    {
                        auto s = weak.get();

                        if (s == nullptr)
                        {
                            return;
                        }

                        s->m_editingTransform.RescaleVelocity = value;

                        for (auto const& box : s->m_velocityRangeBoxes)
                        {
                            box.IsEnabled(value);
                        }

                        s->UpdateTransformSummary();
                    };

                rescale.Checked([setRescale](auto&&, auto&&) { setRescale(true); });
                rescale.Unchecked([setRescale](auto&&, auto&&) { setRescale(false); });

                options.Children().Append(rescale);

                controls::StackPanel range{};
                range.Orientation(controls::Orientation::Horizontal);
                range.Spacing(12);
                range.Margin(xaml::ThicknessHelper::FromLengths(0, 4, 0, 0));

                auto const addRangeBox = [this, weak, &range](winrt::hstring const& header, bool isLow)
                    {
                        auto box = SmallNumberBox(1, 127, isLow
                            ? m_editingTransform.MinimumVelocity : m_editingTransform.MaximumVelocity);

                        box.Header(winrt::box_value(header));
                        box.IsEnabled(m_editingTransform.RescaleVelocity);

                        box.ValueChanged([weak, isLow](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                            {
                                auto s = weak.get();

                                if (s == nullptr || std::isnan(args.NewValue()))
                                {
                                    return;
                                }

                                auto const value = static_cast<uint8_t>(std::clamp(args.NewValue(), 1.0, 127.0));

                                if (isLow)
                                {
                                    s->m_editingTransform.MinimumVelocity = value;
                                }
                                else
                                {
                                    s->m_editingTransform.MaximumVelocity = value;
                                }

                                s->UpdateTransformSummary();
                            });

                        m_velocityRangeBoxes.push_back(box);
                        range.Children().Append(box);
                    };

                addRangeBox(resources::GetString(L"TransformQuietest"), true);
                addRangeBox(resources::GetString(L"TransformLoudest"), false);

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

            // ------------------------------------------------------- control change map
            {
                controls::StackPanel body{};
                body.Spacing(4);

                body.Children().Append(TransformHeading(resources::GetString(L"TransformSectionControlMap")));
                body.Children().Append(TransformHint(resources::GetString(L"TransformSectionControlMapHint")));

                m_controlMapPanel = controls::StackPanel{};
                m_controlMapPanel.Spacing(6);
                m_controlMapPanel.Margin(xaml::ThicknessHelper::FromLengths(0, 4, 0, 6));

                body.Children().Append(m_controlMapPanel);

                controls::Button add{};
                add.Content(winrt::box_value(resources::GetString(L"TransformAddControlMapping")));

                add.Click([weak](auto&&, auto&&)
                    {
                        auto s = weak.get();

                        if (s == nullptr || s->m_controlMapRows.size() >= patchbay::MaximumMapEntries)
                        {
                            return;
                        }

                        s->m_controlMapRows.emplace_back(1, 1);
                        s->RebuildControlMapRows();
                        s->UpdateTransformSummary();
                    });

                body.Children().Append(add);

                TransformContent().Children().Append(TransformCard(body));
            }

            RebuildNoteMapRows();
            RebuildControlMapRows();
            UpdateTransformSummary();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the transform dialog.")
    }

    void MainWindow::RebuildNoteMapRows() noexcept
    {
        try
        {
            if (m_noteMapPanel == nullptr)
            {
                return;
            }

            m_noteMapPanel.Children().Clear();
            m_noteMapLabels.clear();

            auto weak = get_weak();

            if (m_noteMapRows.empty())
            {
                auto empty = TransformHint(resources::GetString(L"TransformNoNoteMappings"));
                empty.Margin(xaml::ThicknessHelper::FromUniformLength(0));
                m_noteMapPanel.Children().Append(empty);

                return;
            }

            for (size_t index = 0; index < m_noteMapRows.size(); index++)
            {
                controls::StackPanel row{};
                row.Orientation(controls::Orientation::Horizontal);
                row.Spacing(8);

                auto const addBox = [this, weak, index, &row](bool isSource)
                    {
                        auto box = SmallNumberBox(0, 127, isSource
                            ? m_noteMapRows[index].first : m_noteMapRows[index].second);

                        box.Width(96);
                        box.Header(winrt::box_value(resources::GetString(
                            isSource ? L"TransformNoteFrom" : L"TransformNoteTo")));

                        box.ValueChanged([weak, index, isSource](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                            {
                                auto s = weak.get();

                                if (s == nullptr || index >= s->m_noteMapRows.size() || std::isnan(args.NewValue()))
                                {
                                    return;
                                }

                                auto const value = static_cast<int32_t>(std::clamp(args.NewValue(), 0.0, 127.0));

                                if (isSource)
                                {
                                    s->m_noteMapRows[index].first = value;
                                }
                                else
                                {
                                    s->m_noteMapRows[index].second = value;
                                }

                                s->RefreshNoteMapRowLabels();
                                s->UpdateTransformSummary();
                            });

                        row.Children().Append(box);

                        return box;
                    };

                addBox(true);

                controls::FontIcon arrow{};
                arrow.Glyph(L"\uE72A");
                arrow.FontSize(12);
                arrow.VerticalAlignment(xaml::VerticalAlignment::Bottom);
                arrow.Margin(xaml::ThicknessHelper::FromLengths(0, 0, 0, 8));
                arrow.Foreground(patchbay::ThemeBrushes::Current().Get(L"TextFillColorTertiaryBrush"));
                row.Children().Append(arrow);

                addBox(false);

                controls::TextBlock names{};
                names.FontSize(12);
                names.VerticalAlignment(xaml::VerticalAlignment::Bottom);
                names.Margin(xaml::ThicknessHelper::FromLengths(0, 0, 0, 8));
                names.MinWidth(120);
                names.Foreground(patchbay::ThemeBrushes::Current().Get(L"TextFillColorSecondaryBrush"));
                row.Children().Append(names);

                m_noteMapLabels.push_back(names);

                controls::Button play{};
                play.Content(winrt::box_value(resources::GetString(L"TransformPlay")));
                play.VerticalAlignment(xaml::VerticalAlignment::Bottom);

                xaml::Automation::AutomationProperties::SetName(play,
                    resources::GetString(L"TransformPlayAccessibleName"));

                play.Click([weak, index](auto&&, auto&&)
                    {
                        auto s = weak.get();

                        if (s == nullptr || index >= s->m_noteMapRows.size())
                        {
                            return;
                        }

                        s->PlayTestNoteAsync(static_cast<uint8_t>(
                            std::clamp(s->m_noteMapRows[index].second, 0, 127)));
                    });

                row.Children().Append(play);

                controls::Button remove{};
                remove.Content(winrt::box_value(resources::GetString(L"TransformRemove")));
                remove.VerticalAlignment(xaml::VerticalAlignment::Bottom);

                remove.Click([weak, index](auto&&, auto&&)
                    {
                        auto s = weak.get();

                        if (s == nullptr || index >= s->m_noteMapRows.size())
                        {
                            return;
                        }

                        s->m_noteMapRows.erase(s->m_noteMapRows.begin() + static_cast<ptrdiff_t>(index));
                        s->RebuildNoteMapRows();
                        s->UpdateTransformSummary();
                    });

                row.Children().Append(remove);

                m_noteMapPanel.Children().Append(row);
            }

            RefreshNoteMapRowLabels();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to list the note mappings.")
    }

    void MainWindow::RefreshNoteMapRowLabels() noexcept
    {
        try
        {
            auto const count = std::min(m_noteMapLabels.size(), m_noteMapRows.size());

            for (size_t i = 0; i < count; i++)
            {
                if (m_noteMapLabels[i] == nullptr)
                {
                    continue;
                }

                m_noteMapLabels[i].Text(resources::FormatString(L"TransformNoteMapLabelFormat",
                    patchbay::DescribeNote(static_cast<uint8_t>(std::clamp(m_noteMapRows[i].first, 0, 127))),
                    patchbay::DescribeNote(static_cast<uint8_t>(std::clamp(m_noteMapRows[i].second, 0, 127)))));
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to label the note mappings.")
    }

    void MainWindow::RebuildControlMapRows() noexcept
    {
        try
        {
            if (m_controlMapPanel == nullptr)
            {
                return;
            }

            m_controlMapPanel.Children().Clear();

            auto weak = get_weak();

            if (m_controlMapRows.empty())
            {
                auto empty = TransformHint(resources::GetString(L"TransformNoControlMappings"));
                empty.Margin(xaml::ThicknessHelper::FromUniformLength(0));
                m_controlMapPanel.Children().Append(empty);

                return;
            }

            for (size_t index = 0; index < m_controlMapRows.size(); index++)
            {
                controls::StackPanel row{};
                row.Orientation(controls::Orientation::Horizontal);
                row.Spacing(8);

                auto const addBox = [this, weak, index, &row](bool isSource)
                    {
                        auto box = SmallNumberBox(0, 127, isSource
                            ? m_controlMapRows[index].first : m_controlMapRows[index].second);

                        box.Width(96);
                        box.Header(winrt::box_value(resources::GetString(
                            isSource ? L"TransformControlFrom" : L"TransformControlTo")));

                        box.ValueChanged([weak, index, isSource](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                            {
                                auto s = weak.get();

                                if (s == nullptr || index >= s->m_controlMapRows.size() || std::isnan(args.NewValue()))
                                {
                                    return;
                                }

                                auto const value = static_cast<int32_t>(std::clamp(args.NewValue(), 0.0, 127.0));

                                if (isSource)
                                {
                                    s->m_controlMapRows[index].first = value;
                                }
                                else
                                {
                                    s->m_controlMapRows[index].second = value;
                                }

                                s->UpdateTransformSummary();
                            });

                        row.Children().Append(box);
                    };

                addBox(true);

                controls::FontIcon arrow{};
                arrow.Glyph(L"\uE72A");
                arrow.FontSize(12);
                arrow.VerticalAlignment(xaml::VerticalAlignment::Bottom);
                arrow.Margin(xaml::ThicknessHelper::FromLengths(0, 0, 0, 8));
                arrow.Foreground(patchbay::ThemeBrushes::Current().Get(L"TextFillColorTertiaryBrush"));
                row.Children().Append(arrow);

                addBox(false);

                controls::Button remove{};
                remove.Content(winrt::box_value(resources::GetString(L"TransformRemove")));
                remove.VerticalAlignment(xaml::VerticalAlignment::Bottom);

                remove.Click([weak, index](auto&&, auto&&)
                    {
                        auto s = weak.get();

                        if (s == nullptr || index >= s->m_controlMapRows.size())
                        {
                            return;
                        }

                        s->m_controlMapRows.erase(s->m_controlMapRows.begin() + static_cast<ptrdiff_t>(index));
                        s->RebuildControlMapRows();
                        s->UpdateTransformSummary();
                    });

                row.Children().Append(remove);

                m_controlMapPanel.Children().Append(row);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to list the control change mappings.")
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
