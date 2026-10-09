// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The mapping steps, edited right in the inspector. Channels and groups are a table of sixteen,
// both laid out the same way. Notes, programs, banks and controllers are lists of rows.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "DialogParts.h"
#include "GeneralMidi.h"
#include "StringResources.h"
#include "ThemeBrushes.h"

#include <span>

using namespace winrt::Microsoft::UI::Xaml;

namespace patchbay = ::midipatchbay;
namespace resources = ::midipatchbay::resources;

namespace winrt::midipatchbay::implementation
{
    namespace
    {
        using patchbay::parts::Check;
        using patchbay::parts::Hint;

        // Everything one list needs to draw itself. Values are the numbers on the wire.
        struct ListInfo
        {
            wchar_t const* Heading;
            wchar_t const* Hint;
            wchar_t const* AddButton;
            wchar_t const* EmptyText;
            wchar_t const* FromHeader;
            wchar_t const* ToHeader;
            int32_t Maximum;
            int32_t DefaultValue;
            bool HasNames;
            bool HasPlayButton;
        };

        ListInfo const& InfoFor(_In_ MainWindow::TransformMap which) noexcept
        {
            static constexpr ListInfo lists[]
            {
                // Note
                { L"TransformSectionNoteMap", L"TransformSectionNoteMapHint", L"TransformAddNoteMapping",
                  L"TransformNoNoteMappings", L"TransformNoteFrom", L"TransformNoteTo", 127, 60, true, true },

                // Control
                { L"TransformSectionControlMap", L"TransformSectionControlMapHint", L"TransformAddControlMapping",
                  L"TransformNoControlMappings", L"TransformControlFrom", L"TransformControlTo", 127, 1, false, false },

                // Channel, which is a table and never a list
                { L"TransformSectionChannelMap", L"TransformSectionChannelMapHint", L"", L"", L"", L"", 15, 0, false, false },

                // Program
                { L"TransformSectionProgramMap", L"TransformSectionProgramMapHint", L"TransformAddProgramMapping",
                  L"TransformNoProgramMappings", L"TransformProgramFrom", L"TransformProgramTo", 127, 0, true, false },

                // BankMsb
                { L"TransformSectionBankMsbMap", L"TransformSectionBankMsbMapHint", L"TransformAddBankMsbMapping",
                  L"TransformNoBankMsbMappings", L"TransformBankFrom", L"TransformBankTo", 127, 0, false, false },

                // BankLsb
                { L"TransformSectionBankLsbMap", L"TransformSectionBankLsbMapHint", L"TransformAddBankLsbMapping",
                  L"TransformNoBankLsbMappings", L"TransformBankFrom", L"TransformBankTo", 127, 0, false, false },
            };

            static_assert(std::size(lists) == MainWindow::TransformMapCount);

            auto const index = static_cast<size_t>(which);

            return lists[index < std::size(lists) ? index : 0];
        }

        // -1 is a value left as it is.
        std::span<int16_t> MapOf(_Inout_ patchbay::MessageTransform& transform, _In_ MainWindow::TransformMap which) noexcept
        {
            switch (which)
            {
            case MainWindow::TransformMap::Note:
                return transform.NoteMap;

            case MainWindow::TransformMap::Control:
                return transform.ControlMap;

            case MainWindow::TransformMap::Channel:
                return transform.ChannelMap;

            case MainWindow::TransformMap::Program:
                return transform.ProgramMap;

            case MainWindow::TransformMap::BankMsb:
                return transform.BankMsbMap;

            default:
                return transform.BankLsbMap;
            }
        }

        controls::TextBlock Label(_In_ winrt::hstring const& text)
        {
            controls::TextBlock block{};

            block.Text(text);
            block.FontSize(12);
            block.VerticalAlignment(xaml::VerticalAlignment::Center);
            block.Foreground(patchbay::ThemeBrushes::Current().Get(L"TextFillColorSecondaryBrush"));

            return block;
        }

        controls::FontIcon Arrow()
        {
            controls::FontIcon arrow{};

            arrow.Glyph(L"\uE72A");
            arrow.FontSize(12);
            arrow.VerticalAlignment(xaml::VerticalAlignment::Center);
            arrow.Foreground(patchbay::ThemeBrushes::Current().Get(L"TextFillColorTertiaryBrush"));

            return arrow;
        }

        // Value, arrow, value, then whatever comes after: the same columns on every row, so the
        // boxes line up under their titles.
        controls::Grid RowGrid(_In_ bool withTrailingColumn)
        {
            controls::Grid grid{};
            grid.ColumnSpacing(6);

            auto const add = [&grid](xaml::GridLength const& width)
                {
                    controls::ColumnDefinition column{};
                    column.Width(width);
                    grid.ColumnDefinitions().Append(column);
                };

            add(xaml::GridLength{ 1, xaml::GridUnitType::Star });
            add(xaml::GridLengthHelper::Auto());
            add(xaml::GridLength{ 1, xaml::GridUnitType::Star });

            if (withTrailingColumn)
            {
                add(xaml::GridLengthHelper::Auto());
            }

            return grid;
        }

        winrt::hstring RowNames(_In_ MainWindow::TransformMap which, _In_ std::pair<int32_t, int32_t> const& row)
        {
            auto const from = static_cast<uint8_t>(std::clamp(row.first, 0, 127));
            auto const to = static_cast<uint8_t>(std::clamp(row.second, 0, 127));

            // The General MIDI names are what an instrument without a program list would play.
            return which == MainWindow::TransformMap::Program
                ? resources::FormatString(L"TransformMapLabelFormat",
                    midiapp::GeneralMidiProgramName(from), midiapp::GeneralMidiProgramName(to))
                : resources::FormatString(L"TransformMapLabelFormat",
                    patchbay::DescribeNote(from), patchbay::DescribeNote(to));
        }
    }

    _Use_decl_annotations_
    void MainWindow::BuildSixteenMapSettings(patchbay::PatchBlock const& block, controls::StackPanel const& body) noexcept
    {
        try
        {
            auto const blockId = block.Id;
            auto const isChannels = block.Kind == patchbay::BlockKind::ChannelMap;
            auto weak = get_weak();

            // What each of the sixteen goes to: 0 is unchanged, and 1 to 16 count from one.
            auto const targetOf = [&block, isChannels](size_t i) -> int32_t
                {
                    auto const mapped = isChannels ? block.Settings.Transform.ChannelMap[i] : block.Settings.GroupMap[i];

                    return mapped < 0 || mapped > 15 ? 0 : mapped + 1;
                };

            auto const store = [isChannels](patchbay::BlockSettings& s, size_t i, int32_t target)
                {
                    if (isChannels)
                    {
                        s.Transform.ChannelMap[i] = static_cast<int16_t>(target - 1);
                    }
                    else
                    {
                        s.GroupMap[i] = static_cast<int8_t>(target - 1);
                    }
                };

            controls::Grid header{};
            header.ColumnDefinitions().Append(controls::ColumnDefinition{});

            controls::ColumnDefinition resetColumn{};
            resetColumn.Width(xaml::GridLengthHelper::Auto());
            header.ColumnDefinitions().Append(resetColumn);

            header.Children().Append(Label(resources::GetString(isChannels
                ? L"TransformSectionChannelMap"
                : L"TransformSectionGroupMap")));

            controls::HyperlinkButton reset{};
            reset.Content(winrt::box_value(resources::GetString(L"MapReset")));
            reset.Padding(xaml::ThicknessHelper::FromLengths(6, 2, 6, 2));
            controls::Grid::SetColumn(reset, 1);

            xaml::Automation::AutomationProperties::SetName(reset,
                resources::GetString(isChannels ? L"MapResetChannelsAccessible" : L"MapResetGroupsAccessible"));

            header.Children().Append(reset);
            body.Children().Append(header);

            controls::Grid table{};
            table.ColumnSpacing(10);
            table.RowSpacing(4);

            for (auto const width : { xaml::GridLengthHelper::Auto(),
                                      xaml::GridLengthHelper::Auto(),
                                      xaml::GridLength{ 1, xaml::GridUnitType::Star } })
            {
                controls::ColumnDefinition column{};
                column.Width(width);
                table.ColumnDefinitions().Append(column);
            }

            auto const itemFormat = isChannels ? L"FilterChannelFormat" : L"FilterGroupFormat";
            auto combos = std::make_shared<std::vector<controls::ComboBox>>();

            for (size_t i = 0; i < 16; i++)
            {
                table.RowDefinitions().Append(controls::RowDefinition{});

                auto label = Label(resources::FormatString(itemFormat, static_cast<int>(i) + 1));
                label.MinWidth(72);
                controls::Grid::SetRow(label, static_cast<int32_t>(i));
                table.Children().Append(label);

                auto arrow = Arrow();
                controls::Grid::SetRow(arrow, static_cast<int32_t>(i));
                controls::Grid::SetColumn(arrow, 1);
                table.Children().Append(arrow);

                controls::ComboBox target{};
                target.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                target.Items().Append(winrt::box_value(resources::GetString(L"TransformGroupUnchanged")));

                for (int32_t to = 0; to < 16; to++)
                {
                    target.Items().Append(winrt::box_value(resources::FormatString(itemFormat, to + 1)));
                }

                target.SelectedIndex(targetOf(i));

                xaml::Automation::AutomationProperties::SetName(target, resources::FormatString(
                    isChannels ? L"TransformChannelMapAccessibleFormat" : L"TransformGroupMapAccessibleFormat",
                    static_cast<int>(i) + 1));

                target.SelectionChanged([weak, blockId, store, i](foundation::IInspectable const& sender, auto&&)
                    {
                        auto strong = weak.get();
                        auto const combo = sender.try_as<controls::ComboBox>();

                        if (strong == nullptr || combo == nullptr || combo.SelectedIndex() < 0 || strong->m_updatingStepSettings)
                        {
                            return;
                        }

                        auto const selected = combo.SelectedIndex();

                        strong->ChangeStepSettings(blockId, [store, i, selected](patchbay::BlockSettings& s) { store(s, i, selected); });
                    });

                controls::Grid::SetRow(target, static_cast<int32_t>(i));
                controls::Grid::SetColumn(target, 2);
                table.Children().Append(target);

                combos->push_back(target);

                if (i == 0)
                {
                    m_stepSettingsFocus = target;
                }
            }

            // One change, so one undo puts all sixteen back.
            reset.Click([weak, blockId, store, combos](auto&&, auto&&)
                {
                    auto strong = weak.get();

                    if (strong == nullptr)
                    {
                        return;
                    }

                    strong->m_updatingStepSettings = true;

                    for (auto const& combo : *combos)
                    {
                        combo.SelectedIndex(0);
                    }

                    strong->m_updatingStepSettings = false;

                    strong->ChangeStepSettings(blockId, [store](patchbay::BlockSettings& s)
                        {
                            for (size_t i = 0; i < 16; i++)
                            {
                                store(s, i, 0);
                            }
                        });
                });

            body.Children().Append(table);
            body.Children().Append(Hint(resources::GetString(isChannels
                ? L"TransformSectionChannelMapHint"
                : L"TransformSectionGroupMapHint")));
        }
        catch (...)
        {
            m_updatingStepSettings = false;
            MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(L"Unable to build the mapping table.");
        }
    }

    _Use_decl_annotations_
    void MainWindow::BuildListMapSettings(patchbay::PatchBlock const& block, controls::StackPanel const& body) noexcept
    {
        try
        {
            auto weak = get_weak();

            m_inlineMapBlockId = block.Id;

            std::vector<TransformMap> lists{};

            switch (block.Kind)
            {
            case patchbay::BlockKind::NoteMap:
                lists = { TransformMap::Note };
                break;

            case patchbay::BlockKind::ControlChangeMap:
                lists = { TransformMap::Control };
                break;

            default:
                lists = { TransformMap::Program, TransformMap::BankMsb, TransformMap::BankLsb };
                break;
            }

            auto transform = block.Settings.Transform;

            for (auto const which : lists)
            {
                auto const index = static_cast<size_t>(which);
                auto const& info = InfoFor(which);

                auto& rows = m_inlineMapRows[index];
                rows.clear();

                auto const map = MapOf(transform, which);

                for (size_t i = 0; i < map.size(); i++)
                {
                    if (map[i] >= 0)
                    {
                        rows.emplace_back(static_cast<int32_t>(i), map[i]);
                    }
                }

                auto heading = Label(resources::GetString(info.Heading));

                if (which != lists.front())
                {
                    heading.Margin(xaml::ThicknessHelper::FromLengths(0, 8, 0, 0));
                }

                body.Children().Append(heading);

                m_inlineMapHosts[index] = controls::StackPanel{};
                m_inlineMapHosts[index].Spacing(6);
                body.Children().Append(m_inlineMapHosts[index]);

                controls::Button add{};
                add.Content(winrt::box_value(resources::GetString(info.AddButton)));

                add.Click([weak, which](auto&&, auto&&)
                    {
                        auto strong = weak.get();

                        if (strong == nullptr)
                        {
                            return;
                        }

                        auto& target = strong->m_inlineMapRows[static_cast<size_t>(which)];
                        auto const& added = InfoFor(which);

                        if (target.size() >= std::min(patchbay::MaximumMapEntries, static_cast<size_t>(added.Maximum) + 1))
                        {
                            return;
                        }

                        // Maps nothing until one side is changed, so nothing is saved yet.
                        target.emplace_back(added.DefaultValue, added.DefaultValue);

                        strong->RebuildInlineMapRows(which);
                    });

                body.Children().Append(add);
                body.Children().Append(Hint(resources::GetString(info.Hint)));

                if (m_stepSettingsFocus == nullptr)
                {
                    m_stepSettingsFocus = add;
                }

                RebuildInlineMapRows(which);
            }

            // A note map moves the note number the way a transpose does, so it needs to know what to
            // do with a MIDI 2.0 note that carries its own pitch.
            if (block.Kind == patchbay::BlockKind::NoteMap)
            {
                auto const blockId = block.Id;
                auto exactPitch = Check(resources::GetString(L"TransformIgnoreExactPitch"), transform.IgnoreExactPitchNotes);

                auto const setExactPitch = [weak, blockId](bool on)
                    {
                        auto strong = weak.get();

                        if (strong != nullptr && !strong->m_updatingStepSettings)
                        {
                            strong->ChangeStepSettings(blockId, [on](patchbay::BlockSettings& s) { s.Transform.IgnoreExactPitchNotes = on; });
                        }
                    };

                exactPitch.Checked([setExactPitch](auto&&, auto&&) { setExactPitch(true); });
                exactPitch.Unchecked([setExactPitch](auto&&, auto&&) { setExactPitch(false); });

                body.Children().Append(exactPitch);
                body.Children().Append(Hint(resources::GetString(L"TransformIgnoreExactPitchHint")));
            }
        }
        catch (...)
        {
            m_updatingStepSettings = false;
            MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(L"Unable to build the mapping lists.");
        }
    }

    _Use_decl_annotations_
    void MainWindow::RebuildInlineMapRows(TransformMap which) noexcept
    {
        try
        {
            auto const index = static_cast<size_t>(which);
            auto const host = m_inlineMapHosts[index];

            if (host == nullptr)
            {
                return;
            }

            host.Children().Clear();

            auto const& info = InfoFor(which);
            auto const& rows = m_inlineMapRows[index];
            auto weak = get_weak();

            if (rows.empty())
            {
                host.Children().Append(Hint(resources::GetString(info.EmptyText)));
                return;
            }

            // The titles once, over the columns, rather than on every box.
            auto titles = RowGrid(true);

            auto fromTitle = Label(resources::GetString(info.FromHeader));
            titles.Children().Append(fromTitle);

            auto toTitle = Label(resources::GetString(info.ToHeader));
            controls::Grid::SetColumn(toTitle, 2);
            titles.Children().Append(toTitle);

            host.Children().Append(titles);

            for (size_t position = 0; position < rows.size(); position++)
            {
                controls::StackPanel row{};
                row.Spacing(2);

                auto line = RowGrid(true);

                controls::TextBlock names{ nullptr };

                if (info.HasNames)
                {
                    names = controls::TextBlock{};
                    names.FontSize(12);
                    names.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
                    names.VerticalAlignment(xaml::VerticalAlignment::Center);
                    names.Foreground(patchbay::ThemeBrushes::Current().Get(L"TextFillColorSecondaryBrush"));
                    names.Text(RowNames(which, rows[position]));
                }

                for (auto const isSource : { true, false })
                {
                    auto box = patchbay::parts::NumberBox({}, 0, info.Maximum,
                        isSource ? rows[position].first : rows[position].second, 12);

                    // No spin buttons: two boxes and their arrow have to fit the narrowest panel.
                    box.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Hidden);
                    box.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                    box.MinWidth(0);

                    xaml::Automation::AutomationProperties::SetName(box, resources::FormatString(L"MapRowBoxAccessibleFormat",
                        resources::GetString(isSource ? info.FromHeader : info.ToHeader), static_cast<int>(position) + 1));

                    box.ValueChanged([weak, which, position, isSource, names](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                        {
                            auto strong = weak.get();

                            if (strong == nullptr || strong->m_updatingStepSettings || std::isnan(args.NewValue()))
                            {
                                return;
                            }

                            auto& target = strong->m_inlineMapRows[static_cast<size_t>(which)];

                            if (position >= target.size())
                            {
                                return;
                            }

                            auto const value = static_cast<int32_t>(std::clamp(args.NewValue(), 0.0,
                                static_cast<double>(InfoFor(which).Maximum)));

                            (isSource ? target[position].first : target[position].second) = value;

                            if (names != nullptr)
                            {
                                names.Text(RowNames(which, target[position]));
                            }

                            strong->SaveInlineMap(which);
                        });

                    controls::Grid::SetColumn(box, isSource ? 0 : 2);
                    line.Children().Append(box);
                }

                auto arrow = Arrow();
                controls::Grid::SetColumn(arrow, 1);
                line.Children().Append(arrow);

                controls::FontIcon removeGlyph{};
                removeGlyph.Glyph(L"\uE711");
                removeGlyph.FontSize(12);

                controls::Button remove{};
                remove.Content(removeGlyph);
                remove.Padding(xaml::ThicknessHelper::FromLengths(8, 6, 8, 6));
                remove.VerticalAlignment(xaml::VerticalAlignment::Stretch);

                auto const removeName = resources::FormatString(L"MapRemoveRowFormat", static_cast<int>(position) + 1);
                xaml::Automation::AutomationProperties::SetName(remove, removeName);
                controls::ToolTipService::SetToolTip(remove, winrt::box_value(removeName));

                remove.Click([weak, which, position](auto&&, auto&&)
                    {
                        auto strong = weak.get();

                        if (strong == nullptr)
                        {
                            return;
                        }

                        auto& target = strong->m_inlineMapRows[static_cast<size_t>(which)];

                        if (position >= target.size())
                        {
                            return;
                        }

                        target.erase(target.begin() + static_cast<ptrdiff_t>(position));

                        strong->SaveInlineMap(which);

                        // After this handler returns, so the button isn't removed under it.
                        strong->DispatcherQueue().TryEnqueue([weak, which]()
                            {
                                if (auto later = weak.get())
                                {
                                    later->RebuildInlineMapRows(which);
                                }
                            });
                    });

                controls::Grid::SetColumn(remove, 3);
                line.Children().Append(remove);

                row.Children().Append(line);

                if (names != nullptr)
                {
                    controls::Grid detail{};
                    detail.ColumnSpacing(6);
                    detail.ColumnDefinitions().Append(controls::ColumnDefinition{});

                    controls::ColumnDefinition playColumn{};
                    playColumn.Width(xaml::GridLengthHelper::Auto());
                    detail.ColumnDefinitions().Append(playColumn);

                    detail.Children().Append(names);

                    if (info.HasPlayButton)
                    {
                        controls::Button play{};
                        play.Content(winrt::box_value(resources::GetString(L"TransformPlay")));
                        play.Padding(xaml::ThicknessHelper::FromLengths(10, 2, 10, 3));

                        xaml::Automation::AutomationProperties::SetName(play, resources::GetString(L"TransformPlayAccessibleName"));

                        play.Click([weak, which, position](auto&&, auto&&)
                            {
                                auto strong = weak.get();

                                if (strong == nullptr)
                                {
                                    return;
                                }

                                auto const& target = strong->m_inlineMapRows[static_cast<size_t>(which)];

                                if (position >= target.size())
                                {
                                    return;
                                }

                                strong->FindTestDestination(strong->m_inlineMapBlockId);
                                strong->PlayTestNoteAsync(static_cast<uint8_t>(std::clamp(target[position].second, 0, 127)));
                            });

                        controls::Grid::SetColumn(play, 1);
                        detail.Children().Append(play);
                    }

                    row.Children().Append(detail);
                }

                host.Children().Append(row);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to list the mappings.")
    }

    _Use_decl_annotations_
    void MainWindow::SaveInlineMap(TransformMap which) noexcept
    {
        try
        {
            auto const rows = m_inlineMapRows[static_cast<size_t>(which)];

            ChangeStepSettings(m_inlineMapBlockId, [which, rows](patchbay::BlockSettings& s)
                {
                    auto map = MapOf(s.Transform, which);

                    std::fill(map.begin(), map.end(), static_cast<int16_t>(-1));

                    // A repeated source wins on its last row, which is the one the customer sees last.
                    for (auto const& [from, to] : rows)
                    {
                        if (from >= 0 && from < static_cast<int32_t>(map.size()) && to >= 0 && to < static_cast<int32_t>(map.size()))
                        {
                            map[static_cast<size_t>(from)] = static_cast<int16_t>(to);
                        }
                    }
                });
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to keep the mappings.")
    }
}
