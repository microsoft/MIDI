// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The dialogs for the (N)RPN filter, the (N)RPN transform and the gate.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "DialogParts.h"
#include "StringResources.h"

using namespace winrt::Microsoft::UI::Xaml;

namespace patchbay = ::midipatchbay;
namespace resources = ::midipatchbay::resources;

namespace winrt::midipatchbay::implementation
{
    namespace
    {
        using patchbay::parts::Card;
        using patchbay::parts::Heading;
        using patchbay::parts::Hint;

        controls::StackPanel Row()
        {
            controls::StackPanel row{};

            row.Orientation(controls::Orientation::Horizontal);
            row.Spacing(8);

            return row;
        }

        controls::ComboBox Choices(
            _In_ winrt::hstring const& header,
            _In_ std::initializer_list<wchar_t const*> keys,
            _In_ int32_t selected)
        {
            controls::ComboBox box{};

            box.Header(winrt::box_value(header));

            for (auto const key : keys)
            {
                box.Items().Append(winrt::box_value(resources::GetString(key)));
            }

            box.SelectedIndex(selected);

            return box;
        }

        // Empty means any, which the file keeps as -1.
        controls::NumberBox OptionalNumber(
            _In_ winrt::hstring const& header,
            _In_ int32_t maximum,
            _In_ int32_t value)
        {
            controls::NumberBox box{};

            box.Header(winrt::box_value(header));
            box.Minimum(0);
            box.Maximum(maximum);
            box.SmallChange(1);
            box.Width(110);
            box.PlaceholderText(resources::GetString(L"ParameterAny"));
            box.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Inline);
            box.ValidationMode(controls::NumberBoxValidationMode::InvalidInputOverwritten);
            box.Value(value < 0 ? std::numeric_limits<double>::quiet_NaN() : value);

            return box;
        }

        int16_t OptionalValue(_In_ double value, _In_ int32_t maximum) noexcept
        {
            return std::isnan(value)
                ? int16_t{ -1 }
                : static_cast<int16_t>(std::clamp(std::lround(value), 0L, static_cast<long>(maximum)));
        }

        controls::NumberBox PercentBox(_In_ winrt::hstring const& header, _In_ int32_t hundredths)
        {
            controls::NumberBox box{};

            box.Header(winrt::box_value(header));
            box.Minimum(0);
            box.Maximum(100);
            box.SmallChange(1);
            box.Width(110);
            box.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Inline);
            box.ValidationMode(controls::NumberBoxValidationMode::InvalidInputOverwritten);
            box.Value(hundredths / 100.0);

            return box;
        }

        controls::Button RemoveButton()
        {
            controls::Button button{};

            button.Content(winrt::box_value(resources::GetString(L"TransformRemove")));
            button.VerticalAlignment(xaml::VerticalAlignment::Bottom);

            return button;
        }

        // Words as eight hex digits each, the way the monitor shows them.
        std::wstring WordsText(_In_ patchbay::GateTrigger const& trigger)
        {
            std::wstring text{};

            for (uint8_t i = 0; i < trigger.WordCount && i < patchbay::MaximumUmpWords; i++)
            {
                wchar_t buffer[12]{};
                swprintf_s(buffer, L"%08X", trigger.Words[i]);

                if (!text.empty())
                {
                    text += L' ';
                }

                text += buffer;
            }

            return text;
        }

        // One to four words of hex, with or without 0x. Nothing else is accepted.
        bool TryReadWords(
            _In_ std::wstring_view text,
            _Out_ std::array<uint32_t, patchbay::MaximumUmpWords>& words,
            _Out_ uint8_t& count) noexcept
        {
            words = {};
            count = 0;

            size_t position{ 0 };

            while (position < text.size())
            {
                while (position < text.size() && (text[position] == L' ' || text[position] == L','))
                {
                    position++;
                }

                if (position >= text.size())
                {
                    break;
                }

                if (count >= patchbay::MaximumUmpWords)
                {
                    return false;
                }

                if (text.substr(position, 2) == L"0x" || text.substr(position, 2) == L"0X")
                {
                    position += 2;
                }

                uint32_t value{ 0 };
                size_t digits{ 0 };

                while (position < text.size() && std::iswxdigit(text[position]))
                {
                    if (digits >= 8)
                    {
                        return false;
                    }

                    auto const ch = text[position];
                    auto const digit = ch <= L'9' ? ch - L'0' : (ch | 0x20) - L'a' + 10;

                    value = (value << 4) | static_cast<uint32_t>(digit);
                    digits++;
                    position++;
                }

                if (digits == 0 || (position < text.size() && text[position] != L' ' && text[position] != L','))
                {
                    return false;
                }

                words[count++] = value;
            }

            return count > 0;
        }

        // Any, then 1 to 16 on screen, stored from 0.
        controls::ComboBox SixteenOrAny(_In_ wchar_t const* headerKey, _In_ wchar_t const* itemFormat, _In_ int32_t value)
        {
            controls::ComboBox box{};

            box.Header(winrt::box_value(resources::GetString(headerKey)));
            box.Items().Append(winrt::box_value(resources::GetString(L"ParameterAny")));

            for (int32_t i = 0; i < 16; i++)
            {
                box.Items().Append(winrt::box_value(resources::FormatString(itemFormat, i + 1)));
            }

            box.SelectedIndex(value < 0 ? 0 : std::clamp(value, 0, 15) + 1);

            return box;
        }
    }

    // ------------------------------------------------------------------- (N)RPN filter

    void MainWindow::BuildParameterFilterSection() noexcept
    {
        try
        {
            auto weak = get_weak();

            controls::StackPanel body{};
            body.Spacing(8);

            body.Children().Append(Heading(resources::GetString(L"ParameterFilterSection")));
            body.Children().Append(Hint(resources::GetString(L"ParameterFilterHint")));

            controls::RadioButtons action{};

            action.Header(winrt::box_value(resources::GetString(L"FilterActionHeader")));
            action.Items().Append(winrt::box_value(resources::GetString(L"FilterActionLetThrough")));
            action.Items().Append(winrt::box_value(resources::GetString(L"FilterActionKeepOut")));
            action.SelectedIndex(m_editingSettings.ParameterFilter.Action == patchbay::FilterAction::KeepOut ? 1 : 0);

            action.SelectionChanged([weak](foundation::IInspectable const& sender, auto&&)
                {
                    auto s = weak.get();
                    auto const list = sender.try_as<controls::RadioButtons>();

                    if (s != nullptr && list != nullptr && list.SelectedIndex() >= 0)
                    {
                        s->m_editingSettings.ParameterFilter.Action = list.SelectedIndex() == 1
                            ? patchbay::FilterAction::KeepOut
                            : patchbay::FilterAction::LetThrough;
                        s->UpdateBlockSummary();
                    }
                });

            body.Children().Append(action);

            m_parameterRowsPanel = controls::StackPanel{};
            m_parameterRowsPanel.Spacing(8);
            body.Children().Append(m_parameterRowsPanel);

            controls::Button add{};
            add.Content(winrt::box_value(resources::GetString(L"ParameterAdd")));

            add.Click([weak](auto&&, auto&&)
                {
                    auto s = weak.get();

                    if (s == nullptr || s->m_editingSettings.ParameterFilter.Parameters.size() >= patchbay::MaximumParameterRows)
                    {
                        return;
                    }

                    // Pitch bend range, the parameter people reach for first.
                    s->m_editingSettings.ParameterFilter.Parameters.push_back(
                        patchbay::ParameterMatch{ patchbay::ParameterKind::Registered, 0, 0 });

                    s->RebuildParameterRows();
                    s->UpdateBlockSummary();
                });

            body.Children().Append(add);
            body.Children().Append(Hint(resources::GetString(L"ParameterMidi1Hint")));

            BlockDialogContent().Children().Append(Card(body));

            RebuildParameterRows();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the (N)RPN filter section.")
    }

    // ---------------------------------------------------------------- (N)RPN transform

    void MainWindow::BuildParameterTransformSection() noexcept
    {
        try
        {
            auto weak = get_weak();

            controls::StackPanel body{};
            body.Spacing(8);

            body.Children().Append(Heading(resources::GetString(L"ParameterTransformSection")));
            body.Children().Append(Hint(resources::GetString(L"ParameterTransformHint")));

            m_parameterRowsPanel = controls::StackPanel{};
            m_parameterRowsPanel.Spacing(8);
            body.Children().Append(m_parameterRowsPanel);

            controls::Button add{};
            add.Content(winrt::box_value(resources::GetString(L"ParameterAddRow")));

            add.Click([weak](auto&&, auto&&)
                {
                    auto s = weak.get();

                    if (s == nullptr || s->m_editingSettings.ParameterTransform.Rows.size() >= patchbay::MaximumParameterRows)
                    {
                        return;
                    }

                    patchbay::ParameterMapRow row{};
                    row.From = patchbay::ParameterMatch{ patchbay::ParameterKind::Registered, 0, 0 };

                    s->m_editingSettings.ParameterTransform.Rows.push_back(row);

                    s->RebuildParameterRows();
                    s->UpdateBlockSummary();
                });

            body.Children().Append(add);
            body.Children().Append(Hint(resources::GetString(L"ParameterTransformMidi1Hint")));

            BlockDialogContent().Children().Append(Card(body));

            RebuildParameterRows();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the (N)RPN transform section.")
    }

    void MainWindow::RebuildParameterRows() noexcept
    {
        try
        {
            if (m_parameterRowsPanel == nullptr)
            {
                return;
            }

            m_parameterRowsPanel.Children().Clear();

            auto weak = get_weak();
            auto const isFilter = m_editingKind == patchbay::BlockKind::ParameterFilter;

            // Every handler finds its row again by number, after checking it is still there.
            auto const changeMatch = [weak, isFilter](size_t i, std::function<void(patchbay::ParameterMatch&)> const& apply)
                {
                    auto s = weak.get();

                    if (s == nullptr)
                    {
                        return;
                    }

                    if (isFilter)
                    {
                        auto& list = s->m_editingSettings.ParameterFilter.Parameters;

                        if (i < list.size())
                        {
                            apply(list[i]);
                        }
                    }
                    else
                    {
                        auto& rows = s->m_editingSettings.ParameterTransform.Rows;

                        if (i < rows.size())
                        {
                            apply(rows[i].From);
                        }
                    }

                    s->UpdateBlockSummary();
                };

            auto const changeRow = [weak](size_t i, std::function<void(patchbay::ParameterMapRow&)> const& apply)
                {
                    auto s = weak.get();

                    if (s == nullptr)
                    {
                        return;
                    }

                    auto& rows = s->m_editingSettings.ParameterTransform.Rows;

                    if (i < rows.size())
                    {
                        apply(rows[i]);
                        s->UpdateBlockSummary();
                    }
                };

            auto const addMatchControls = [&changeMatch](controls::StackPanel const& line, size_t i, patchbay::ParameterMatch const& match)
                {
                    auto kind = Choices(resources::GetString(L"ParameterType"),
                        { L"ParameterKindRegistered", L"ParameterKindAssignable", L"ParameterKindEither" },
                        static_cast<int32_t>(match.Kind));

                    kind.SelectionChanged([changeMatch, i](foundation::IInspectable const& sender, auto&&)
                        {
                            auto const combo = sender.try_as<controls::ComboBox>();

                            if (combo != nullptr && combo.SelectedIndex() >= 0)
                            {
                                auto const value = static_cast<patchbay::ParameterKind>(combo.SelectedIndex());
                                changeMatch(i, [value](patchbay::ParameterMatch& m) { m.Kind = value; });
                            }
                        });

                    line.Children().Append(kind);

                    for (auto const isBank : { true, false })
                    {
                        auto box = OptionalNumber(resources::GetString(isBank ? L"ParameterBank" : L"ParameterIndex"), 127,
                            isBank ? match.Bank : match.Index);

                        box.ValueChanged([changeMatch, i, isBank](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                            {
                                auto const value = OptionalValue(args.NewValue(), 127);
                                changeMatch(i, [value, isBank](patchbay::ParameterMatch& m) { (isBank ? m.Bank : m.Index) = value; });
                            });

                        line.Children().Append(box);
                    }
                };

            auto const addRemove = [weak, isFilter](controls::StackPanel const& line, size_t i)
                {
                    auto remove = RemoveButton();

                    remove.Click([weak, isFilter, i](auto&&, auto&&)
                        {
                            auto s = weak.get();

                            if (s == nullptr)
                            {
                                return;
                            }

                            if (isFilter)
                            {
                                auto& list = s->m_editingSettings.ParameterFilter.Parameters;

                                if (i < list.size())
                                {
                                    list.erase(list.begin() + static_cast<ptrdiff_t>(i));
                                }
                            }
                            else
                            {
                                auto& rows = s->m_editingSettings.ParameterTransform.Rows;

                                if (i < rows.size())
                                {
                                    rows.erase(rows.begin() + static_cast<ptrdiff_t>(i));
                                }
                            }

                            // Rebuilt after this handler returns, so the button isn't removed under it.
                            s->DispatcherQueue().TryEnqueue([weak]()
                                {
                                    if (auto t = weak.get())
                                    {
                                        t->RebuildParameterRows();
                                        t->UpdateBlockSummary();
                                    }
                                });
                        });

                    line.Children().Append(remove);
                };

            if (isFilter)
            {
                auto const& list = m_editingSettings.ParameterFilter.Parameters;

                if (list.empty())
                {
                    m_parameterRowsPanel.Children().Append(Hint(resources::GetString(L"ParameterNone")));
                }

                for (size_t i = 0; i < list.size(); i++)
                {
                    auto line = Row();

                    addMatchControls(line, i, list[i]);
                    addRemove(line, i);

                    m_parameterRowsPanel.Children().Append(line);
                }

                return;
            }

            auto const& rows = m_editingSettings.ParameterTransform.Rows;

            if (rows.empty())
            {
                m_parameterRowsPanel.Children().Append(Hint(resources::GetString(L"ParameterNoRows")));
            }

            for (size_t i = 0; i < rows.size(); i++)
            {
                auto const& row = rows[i];

                controls::StackPanel rowBody{};
                rowBody.Spacing(6);

                {
                    auto line = Row();

                    auto label = Heading(resources::GetString(L"ParameterFrom"));
                    label.VerticalAlignment(xaml::VerticalAlignment::Bottom);
                    label.Margin(xaml::ThicknessHelper::FromLengths(0, 0, 0, 6));
                    label.Width(40);
                    line.Children().Append(label);

                    addMatchControls(line, i, row.From);
                    addRemove(line, i);

                    rowBody.Children().Append(line);
                }

                {
                    auto line = Row();

                    auto label = Heading(resources::GetString(L"ParameterTo"));
                    label.VerticalAlignment(xaml::VerticalAlignment::Bottom);
                    label.Margin(xaml::ThicknessHelper::FromLengths(0, 0, 0, 6));
                    label.Width(40);
                    line.Children().Append(label);

                    // Same, RPN, NRPN in the list; Either, Registered, Assignable in the file.
                    auto const toIndex = row.ToKind == patchbay::ParameterKind::Either ? 0
                        : row.ToKind == patchbay::ParameterKind::Registered ? 1 : 2;

                    auto kind = Choices(resources::GetString(L"ParameterType"),
                        { L"ParameterKindSame", L"ParameterKindRegistered", L"ParameterKindAssignable" }, toIndex);

                    kind.SelectionChanged([changeRow, i](foundation::IInspectable const& sender, auto&&)
                        {
                            auto const combo = sender.try_as<controls::ComboBox>();

                            if (combo == nullptr || combo.SelectedIndex() < 0)
                            {
                                return;
                            }

                            auto const value = combo.SelectedIndex() == 0 ? patchbay::ParameterKind::Either
                                : combo.SelectedIndex() == 1 ? patchbay::ParameterKind::Registered
                                : patchbay::ParameterKind::Assignable;

                            changeRow(i, [value](patchbay::ParameterMapRow& r) { r.ToKind = value; });
                        });

                    line.Children().Append(kind);

                    for (auto const isBank : { true, false })
                    {
                        auto box = OptionalNumber(resources::GetString(isBank ? L"ParameterBank" : L"ParameterIndex"), 127,
                            isBank ? row.ToBank : row.ToIndex);

                        box.PlaceholderText(resources::GetString(L"ParameterSame"));

                        box.ValueChanged([changeRow, i, isBank](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                            {
                                auto const value = OptionalValue(args.NewValue(), 127);
                                changeRow(i, [value, isBank](patchbay::ParameterMapRow& r) { (isBank ? r.ToBank : r.ToIndex) = value; });
                            });

                        line.Children().Append(box);
                    }

                    rowBody.Children().Append(line);
                }

                {
                    auto line = Row();

                    auto curve = Choices(resources::GetString(L"TransformShapeCurve"),
                        { L"TransformShapeCurveLinear", L"TransformShapeCurveSlowRise", L"TransformShapeCurveFastRise" },
                        static_cast<int32_t>(row.Shape.Curve));

                    curve.SelectionChanged([changeRow, i](foundation::IInspectable const& sender, auto&&)
                        {
                            auto const combo = sender.try_as<controls::ComboBox>();

                            if (combo != nullptr && combo.SelectedIndex() >= 0)
                            {
                                auto const value = static_cast<patchbay::ValueCurve>(combo.SelectedIndex());
                                changeRow(i, [value](patchbay::ParameterMapRow& r) { r.Shape.Curve = value; });
                            }
                        });

                    line.Children().Append(curve);

                    for (auto const low : { true, false })
                    {
                        auto box = PercentBox(resources::GetString(low ? L"ParameterOutputLowest" : L"ParameterOutputHighest"),
                            low ? row.Shape.OutputMinimumHundredths : row.Shape.OutputMaximumHundredths);

                        box.ValueChanged([changeRow, i, low](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                            {
                                if (std::isnan(args.NewValue()))
                                {
                                    return;
                                }

                                auto const value = static_cast<int32_t>(std::lround(std::clamp(args.NewValue(), 0.0, 100.0) * 100.0));

                                changeRow(i, [value, low](patchbay::ParameterMapRow& r)
                                    {
                                        (low ? r.Shape.OutputMinimumHundredths : r.Shape.OutputMaximumHundredths) = value;
                                    });
                            });

                        line.Children().Append(box);
                    }

                    controls::CheckBox invert{};
                    invert.Content(winrt::box_value(resources::GetString(L"TransformShapeInvert")));
                    invert.IsChecked(row.Shape.Invert);
                    invert.VerticalAlignment(xaml::VerticalAlignment::Bottom);

                    auto const setInvert = [changeRow, i](bool on)
                        {
                            changeRow(i, [on](patchbay::ParameterMapRow& r) { r.Shape.Invert = on; });
                        };

                    invert.Checked([setInvert](auto&&, auto&&) { setInvert(true); });
                    invert.Unchecked([setInvert](auto&&, auto&&) { setInvert(false); });

                    line.Children().Append(invert);
                    rowBody.Children().Append(line);
                }

                m_parameterRowsPanel.Children().Append(Card(rowBody));
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the parameters.")
    }

    // --------------------------------------------------------------------------- gate

    void MainWindow::BuildGateSections() noexcept
    {
        try
        {
            auto weak = get_weak();
            auto const content = BlockDialogContent();

            for (auto const open : { true, false })
            {
                controls::StackPanel body{};
                body.Spacing(8);

                body.Children().Append(Heading(resources::GetString(open ? L"GateOpensOn" : L"GateClosesOn")));

                auto& panel = m_gateTriggerPanels[open ? 0 : 1];
                panel = controls::StackPanel{};
                panel.Spacing(8);

                body.Children().Append(panel);
                content.Children().Append(Card(body));

                RebuildGateTrigger(open);
            }

            controls::StackPanel options{};
            options.Spacing(4);

            for (auto const startsOpen : { true, false })
            {
                controls::CheckBox check{};

                check.Content(winrt::box_value(resources::GetString(startsOpen ? L"GateStartsOpen" : L"GatePassTriggers")));
                check.IsChecked(startsOpen ? m_editingSettings.Gate.StartsOpen : m_editingSettings.Gate.PassesTriggers);

                auto const set = [weak, startsOpen](bool on)
                    {
                        if (auto s = weak.get())
                        {
                            (startsOpen ? s->m_editingSettings.Gate.StartsOpen : s->m_editingSettings.Gate.PassesTriggers) = on;
                            s->UpdateBlockSummary();
                        }
                    };

                check.Checked([set](auto&&, auto&&) { set(true); });
                check.Unchecked([set](auto&&, auto&&) { set(false); });

                options.Children().Append(check);
                options.Children().Append(Hint(resources::GetString(startsOpen ? L"GateStartsOpenHint" : L"GatePassTriggersHint")));
            }

            options.Children().Append(Hint(resources::GetString(L"GateNoteOffHint")));

            content.Children().Append(Card(options));
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the gate sections.")
    }

    _Use_decl_annotations_
    void MainWindow::RebuildGateTrigger(bool open) noexcept
    {
        try
        {
            auto const panel = m_gateTriggerPanels[open ? 0 : 1];

            if (panel == nullptr)
            {
                return;
            }

            panel.Children().Clear();

            auto weak = get_weak();
            auto const& trigger = open ? m_editingSettings.Gate.Open : m_editingSettings.Gate.Close;

            auto const change = [weak, open](std::function<void(patchbay::GateTrigger&)> const& apply)
                {
                    if (auto s = weak.get())
                    {
                        apply(open ? s->m_editingSettings.Gate.Open : s->m_editingSettings.Gate.Close);
                        s->UpdateBlockSummary();
                    }
                };

            auto kind = Choices(resources::GetString(L"GateMessage"),
                { L"GateKindNoteOn", L"GateKindNoteOff", L"GateKindControlChange", L"GateKindProgramChange",
                  L"GateKindStart", L"GateKindContinue", L"GateKindStop", L"GateKindWords" },
                static_cast<int32_t>(trigger.Kind));

            kind.SelectionChanged([weak, open, change](foundation::IInspectable const& sender, auto&&)
                {
                    auto const combo = sender.try_as<controls::ComboBox>();

                    if (combo == nullptr || combo.SelectedIndex() < 0)
                    {
                        return;
                    }

                    auto const value = static_cast<patchbay::GateTriggerKind>(combo.SelectedIndex());

                    change([value](patchbay::GateTrigger& t) { t.Kind = value; });

                    // The fields below depend on the kind, so they are shown again once this is done.
                    if (auto s = weak.get())
                    {
                        s->DispatcherQueue().TryEnqueue([weak, open]()
                            {
                                if (auto t = weak.get())
                                {
                                    t->RebuildGateTrigger(open);
                                }
                            });
                    }
                });

            panel.Children().Append(kind);

            auto line = Row();

            auto group = SixteenOrAny(L"GeneratorGroup", L"FilterGroupFormat", trigger.Group);

            group.SelectionChanged([change](foundation::IInspectable const& sender, auto&&)
                {
                    auto const combo = sender.try_as<controls::ComboBox>();

                    if (combo != nullptr && combo.SelectedIndex() >= 0)
                    {
                        auto const value = static_cast<int8_t>(combo.SelectedIndex() - 1);
                        change([value](patchbay::GateTrigger& t) { t.Group = value; });
                    }
                });

            line.Children().Append(group);

            auto const voice = trigger.Kind == patchbay::GateTriggerKind::NoteOn ||
                trigger.Kind == patchbay::GateTriggerKind::NoteOff ||
                trigger.Kind == patchbay::GateTriggerKind::ControlChange ||
                trigger.Kind == patchbay::GateTriggerKind::ProgramChange;

            if (voice)
            {
                auto channel = SixteenOrAny(L"GateChannel", L"FilterChannelFormat", trigger.Channel);

                channel.SelectionChanged([change](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const combo = sender.try_as<controls::ComboBox>();

                        if (combo != nullptr && combo.SelectedIndex() >= 0)
                        {
                            auto const value = static_cast<int8_t>(combo.SelectedIndex() - 1);
                            change([value](patchbay::GateTrigger& t) { t.Channel = value; });
                        }
                    });

                line.Children().Append(channel);

                auto const numberKey = trigger.Kind == patchbay::GateTriggerKind::ControlChange ? L"GateController"
                    : trigger.Kind == patchbay::GateTriggerKind::ProgramChange ? L"GateProgram" : L"GateNote";

                auto number = OptionalNumber(resources::GetString(numberKey), 127, trigger.Number);

                number.ValueChanged([change](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                    {
                        auto const value = OptionalValue(args.NewValue(), 127);
                        change([value](patchbay::GateTrigger& t) { t.Number = value; });
                    });

                line.Children().Append(number);
            }

            panel.Children().Append(line);

            if (trigger.Kind == patchbay::GateTriggerKind::ControlChange)
            {
                auto valueLine = Row();

                auto test = Choices(resources::GetString(L"GateValueTest"),
                    { L"GateTestAny", L"GateTestAtLeast", L"GateTestBelow" }, static_cast<int32_t>(trigger.Test));

                test.SelectionChanged([change](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const combo = sender.try_as<controls::ComboBox>();

                        if (combo != nullptr && combo.SelectedIndex() >= 0)
                        {
                            auto const value = static_cast<patchbay::GateValueTest>(combo.SelectedIndex());
                            change([value](patchbay::GateTrigger& t) { t.Test = value; });
                        }
                    });

                valueLine.Children().Append(test);

                auto value = OptionalNumber(resources::GetString(L"GateValue"), 127, trigger.Value);
                value.PlaceholderText(L"");

                value.ValueChanged([change](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                    {
                        if (!std::isnan(args.NewValue()))
                        {
                            auto const number = static_cast<uint8_t>(std::clamp(std::lround(args.NewValue()), 0L, 127L));
                            change([number](patchbay::GateTrigger& t) { t.Value = number; });
                        }
                    });

                valueLine.Children().Append(value);
                panel.Children().Append(valueLine);
                panel.Children().Append(Hint(resources::GetString(L"GateValueHint")));
            }

            if (trigger.Kind == patchbay::GateTriggerKind::Words)
            {
                controls::TextBox words{};

                words.Header(winrt::box_value(resources::GetString(L"GateWords")));
                words.PlaceholderText(L"20B04000");
                words.FontFamily(media::FontFamily{ L"Consolas" });
                words.Text(winrt::hstring{ WordsText(trigger) });

                auto caption = Hint(resources::GetString(L"GateWordsHint"));

                words.TextChanged([change, caption](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const box = sender.try_as<controls::TextBox>();

                        if (box == nullptr)
                        {
                            return;
                        }

                        std::array<uint32_t, patchbay::MaximumUmpWords> read{};
                        uint8_t count{ 0 };

                        if (!TryReadWords(std::wstring_view{ box.Text() }, read, count))
                        {
                            caption.Text(resources::GetString(L"GateWordsNotUnderstood"));
                            return;
                        }

                        caption.Text(resources::GetString(L"GateWordsHint"));
                        change([read, count](patchbay::GateTrigger& t) { t.Words = read; t.WordCount = count; });
                    });

                panel.Children().Append(words);
                panel.Children().Append(caption);
                panel.Children().Append(midiapp::MakeUmpPrimerLink(resources::GetString(L"UmpPrimerLink")));
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the gate trigger.")
    }
}
