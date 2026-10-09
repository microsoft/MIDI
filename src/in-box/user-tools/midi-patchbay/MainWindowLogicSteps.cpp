// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The settings of the logic steps: Branch, Switch, Set tag, Set memory and Put value. They are
// edited right in the inspector. What a field shows depends on the fields above it, so a change
// to one of those shows the settings again and puts the focus back where it was.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "DialogParts.h"
#include "LogicSteps.h"
#include "MessageText.h"
#include "RouteEngine.h"
#include "StringResources.h"
#include "TextMatch.h"
#include "ThemeBrushes.h"

#include <cwchar>
#include <cwctype>

using namespace winrt::Microsoft::UI::Xaml;

namespace patchbay = ::midipatchbay;
namespace resources = ::midipatchbay::resources;

namespace winrt::midipatchbay::implementation
{
    namespace
    {
        using patchbay::parts::Check;
        using patchbay::parts::Hint;

        using Change = std::function<std::optional<patchbay::BlockSettings>(std::function<void(patchbay::BlockSettings&)> const&)>;

        // Shows the settings again, then puts the focus on the control with this tag.
        using Refresh = std::function<void(std::wstring const&)>;

        // The part of the settings a group of controls edits.
        using SourceOf = std::function<patchbay::LogicSource&(patchbay::BlockSettings&)>;
        using PlaceOf = std::function<patchbay::PartPlace&(patchbay::BlockSettings&)>;
        using ConditionOf = std::function<patchbay::LogicCondition&(patchbay::BlockSettings&)>;
        using UnitOf = std::function<patchbay::LogicUnit&(patchbay::BlockSettings&)>;
        using ScaleOf = std::function<patchbay::ValueScale&(patchbay::BlockSettings&)>;

        controls::ComboBox Choices(
            _In_ winrt::hstring const& header,
            _In_ std::vector<winrt::hstring> const& items,
            _In_ int32_t selected,
            _In_ std::wstring const& tag)
        {
            controls::ComboBox box{};

            box.Header(winrt::box_value(header));
            box.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

            for (auto const& item : items)
            {
                box.Items().Append(winrt::box_value(item));
            }

            box.SelectedIndex(selected);

            if (!tag.empty())
            {
                box.Tag(winrt::box_value(winrt::hstring{ tag }));
            }

            return box;
        }

        int32_t SelectedIndexOf(_In_ foundation::IInspectable const& sender)
        {
            auto const box = sender.try_as<controls::ComboBox>();

            return box == nullptr ? -1 : box.SelectedIndex();
        }

        // Two controls side by side, half the width each.
        controls::Grid Pair(_In_ xaml::FrameworkElement const& left, _In_ xaml::FrameworkElement const& right)
        {
            controls::Grid grid{};

            grid.ColumnSpacing(8);
            grid.ColumnDefinitions().Append(controls::ColumnDefinition{});
            grid.ColumnDefinitions().Append(controls::ColumnDefinition{});

            controls::Grid::SetColumn(right, 1);

            grid.Children().Append(left);
            grid.Children().Append(right);

            return grid;
        }

        // A control in the settings by its tag, so the focus can go back to it once they are shown
        // again.
        controls::Control FindTagged(_In_ xaml::UIElement const& root, _In_ std::wstring_view tag)
        {
            if (root == nullptr || tag.empty())
            {
                return nullptr;
            }

            if (auto const element = root.try_as<xaml::FrameworkElement>())
            {
                if (auto const value = element.Tag(); value != nullptr &&
                    std::wstring_view{ winrt::unbox_value_or<winrt::hstring>(value, winrt::hstring{}) } == tag)
                {
                    return element.try_as<controls::Control>();
                }
            }

            if (auto const panel = root.try_as<controls::Panel>())
            {
                for (auto const& child : panel.Children())
                {
                    if (auto found = FindTagged(child, tag))
                    {
                        return found;
                    }
                }
            }

            return nullptr;
        }

        // ------------------------------------------------------------- numbers in a unit

        // On screen, channels and groups count from 1 and a value is in the step's format.
        double ShownMinimum(_In_ patchbay::LogicUnit unit) noexcept
        {
            return unit == patchbay::LogicUnit::Channel || unit == patchbay::LogicUnit::Group ? 1.0 : 0.0;
        }

        double ShownMaximum(_In_ patchbay::LogicUnit unit, _In_ patchbay::ValueScale scale) noexcept
        {
            switch (unit)
            {
            case patchbay::LogicUnit::Channel:
            case patchbay::LogicUnit::Group:
                return 16.0;

            case patchbay::LogicUnit::Note:
                return 127.0;

            case patchbay::LogicUnit::Value:
                return scale == patchbay::ValueScale::Percent ? 100.0 : 127.0;

            default:
                return 4294967295.0;
            }
        }

        double Shown(_In_ uint32_t number, _In_ patchbay::LogicUnit unit, _In_ patchbay::ValueScale scale) noexcept
        {
            switch (unit)
            {
            case patchbay::LogicUnit::Channel:
            case patchbay::LogicUnit::Group:
                return static_cast<double>(number) + 1.0;

            case patchbay::LogicUnit::Value:
                return patchbay::DisplayFromHundredths(
                    static_cast<int32_t>((std::min)(number, static_cast<uint32_t>(patchbay::FullScaleHundredths))), scale);

            default:
                return static_cast<double>(number);
            }
        }

        uint32_t Stored(_In_ double shown, _In_ patchbay::LogicUnit unit, _In_ patchbay::ValueScale scale) noexcept
        {
            auto const whole = std::llround(shown);

            switch (unit)
            {
            case patchbay::LogicUnit::Channel:
            case patchbay::LogicUnit::Group:
                return static_cast<uint32_t>(std::clamp(whole - 1, 0LL, 15LL));

            case patchbay::LogicUnit::Note:
                return static_cast<uint32_t>(std::clamp(whole, 0LL, 127LL));

            case patchbay::LogicUnit::Value:
                return static_cast<uint32_t>(std::clamp(patchbay::HundredthsFromDisplay(shown, scale), 0, patchbay::FullScaleHundredths));

            default:
                return static_cast<uint32_t>(std::clamp(whole, 0LL, 4294967295LL));
            }
        }

        // As typed: no thousands separators, so a list of them can be read back.
        std::wstring ShownText(_In_ uint32_t number, _In_ patchbay::LogicUnit unit, _In_ patchbay::ValueScale scale)
        {
            wchar_t buffer[32]{};

            if (unit == patchbay::LogicUnit::Value && scale == patchbay::ValueScale::Percent)
            {
                swprintf_s(buffer, L"%.2f", Shown(number, unit, scale));

                std::wstring text{ buffer };

                while (!text.empty() && text.back() == L'0')
                {
                    text.pop_back();
                }

                if (!text.empty() && text.back() == L'.')
                {
                    text.pop_back();
                }

                return text;
            }

            swprintf_s(buffer, L"%.0f", Shown(number, unit, scale));

            return buffer;
        }

        controls::NumberBox UnitBox(
            _In_ winrt::hstring const& header,
            _In_ uint32_t number,
            _In_ patchbay::LogicUnit unit,
            _In_ patchbay::ValueScale scale,
            _In_ std::wstring const& tag)
        {
            auto const small = unit == patchbay::LogicUnit::Channel || unit == patchbay::LogicUnit::Group;
            auto box = patchbay::parts::NumberBox(header, ShownMinimum(unit), ShownMaximum(unit, scale), Shown(number, unit, scale), small ? 4 : 10);

            box.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            box.Tag(winrt::box_value(winrt::hstring{ tag }));

            winrt::Windows::Globalization::NumberFormatting::DecimalFormatter formatter{};

            formatter.IntegerDigits(1);
            formatter.FractionDigits(unit == patchbay::LogicUnit::Value && scale == patchbay::ValueScale::Percent ? 2 : 0);
            formatter.IsGrouped(false);

            box.NumberFormatter(formatter);
            box.Value(Shown(number, unit, scale));

            // A note number with its name, so nobody has to work out which C it is.
            if (unit == patchbay::LogicUnit::Note)
            {
                box.Description(winrt::box_value(patchbay::DescribeNote(static_cast<uint8_t>((std::min)(number, 127u)))));
            }

            return box;
        }

        // Calls apply with the number as the step keeps it, whenever the box changes.
        void OnUnitBoxChanged(
            _In_ controls::NumberBox const& box,
            _In_ patchbay::LogicUnit unit,
            _In_ patchbay::ValueScale scale,
            _In_ std::function<void(uint32_t)> const& apply)
        {
            box.ValueChanged([unit, scale, apply](controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args)
                {
                    if (std::isnan(args.NewValue()))
                    {
                        return;
                    }

                    auto const stored = Stored(args.NewValue(), unit, scale);

                    if (unit == patchbay::LogicUnit::Note)
                    {
                        sender.Description(winrt::box_value(patchbay::DescribeNote(static_cast<uint8_t>(stored))));
                    }

                    apply(stored);
                });
        }

        // Numbers as typed, between commas or spaces. Nothing when one of them isn't a number in
        // the unit's range.
        std::optional<std::vector<uint32_t>> ReadValueList(
            _In_ std::wstring_view text,
            _In_ patchbay::LogicUnit unit,
            _In_ patchbay::ValueScale scale)
        {
            std::vector<uint32_t> values{};
            std::wstring token{};

            auto const flush = [&]() -> bool
                {
                    if (token.empty())
                    {
                        return true;
                    }

                    wchar_t* end{ nullptr };
                    auto const number = std::wcstod(token.c_str(), &end);

                    if (end == token.c_str() || *end != L'\0' || !std::isfinite(number) ||
                        number < ShownMinimum(unit) || number > ShownMaximum(unit, scale))
                    {
                        return false;
                    }

                    auto const stored = Stored(number, unit, scale);

                    if (values.size() < patchbay::MaximumLogicListValues &&
                        std::find(values.begin(), values.end(), stored) == values.end())
                    {
                        values.push_back(stored);
                    }

                    token.clear();
                    return true;
                };

            for (auto const c : text)
            {
                if (c == L',' || c == L';' || std::iswspace(c))
                {
                    if (!flush())
                    {
                        return std::nullopt;
                    }
                }
                else
                {
                    token += c;
                }
            }

            if (!flush())
            {
                return std::nullopt;
            }

            return values;
        }

        std::wstring ValueListText(
            _In_ std::vector<uint32_t> const& values,
            _In_ patchbay::LogicUnit unit,
            _In_ patchbay::ValueScale scale)
        {
            std::wstring text{};

            for (auto const value : values)
            {
                if (!text.empty())
                {
                    text += L", ";
                }

                text += ShownText(value, unit, scale);
            }

            return text;
        }

        // ------------------------------------------------------------- building blocks

        // Name a tag or a memory. The names already in the patch are offered as it is typed, so
        // one spelling is used everywhere.
        controls::AutoSuggestBox NameBox(
            _In_ winrt::hstring const& header,
            _In_ std::wstring const& name,
            _In_ std::vector<std::wstring> const& names,
            _In_ std::wstring const& tag,
            _In_ std::function<void(std::wstring const&)> const& commit)
        {
            controls::AutoSuggestBox box{};

            box.Header(winrt::box_value(header));
            box.PlaceholderText(resources::GetString(L"LogicNamePlaceholder"));
            box.Text(winrt::hstring{ name });
            box.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            box.Tag(winrt::box_value(winrt::hstring{ tag }));

            auto const known = std::make_shared<std::vector<std::wstring>>(names);

            auto const suggest = [known](controls::AutoSuggestBox const& target)
                {
                    auto items = winrt::single_threaded_vector<foundation::IInspectable>();
                    auto const typed = std::wstring{ target.Text() };

                    for (auto const& entry : *known)
                    {
                        if (typed.empty() || patchbay::ContainsText(entry, typed))
                        {
                            items.Append(winrt::box_value(winrt::hstring{ entry }));
                        }
                    }

                    target.ItemsSource(items);
                };

            box.TextChanged([suggest](controls::AutoSuggestBox const& sender, controls::AutoSuggestBoxTextChangedEventArgs const& args)
                {
                    if (args.Reason() == controls::AutoSuggestionBoxTextChangeReason::UserInput)
                    {
                        suggest(sender);
                    }
                });

            // An empty box shows every name in the patch, which is the list most people want.
            box.GotFocus([suggest, known](foundation::IInspectable const& sender, auto&&)
                {
                    auto const target = sender.try_as<controls::AutoSuggestBox>();

                    if (target != nullptr && target.Text().empty() && !known->empty())
                    {
                        suggest(target);
                        target.IsSuggestionListOpen(true);
                    }
                });

            box.SuggestionChosen([](controls::AutoSuggestBox const& sender, controls::AutoSuggestBoxSuggestionChosenEventArgs const& args)
                {
                    sender.Text(winrt::unbox_value_or<winrt::hstring>(args.SelectedItem(), sender.Text()));
                });

            box.QuerySubmitted([commit](controls::AutoSuggestBox const& sender, controls::AutoSuggestBoxQuerySubmittedEventArgs const& args)
                {
                    auto const chosen = args.ChosenSuggestion();

                    commit(std::wstring{ chosen != nullptr
                        ? winrt::unbox_value_or<winrt::hstring>(chosen, sender.Text())
                        : args.QueryText() });
                });

            box.LostFocus([commit](foundation::IInspectable const& sender, auto&&)
                {
                    if (auto const target = sender.try_as<controls::AutoSuggestBox>())
                    {
                        commit(std::wstring{ target.Text() });
                    }
                });

            return box;
        }

        void AddScaleChoice(
            _In_ controls::StackPanel const& host,
            _In_ patchbay::ValueScale scale,
            _In_ ScaleOf const& pick,
            _In_ Change const& change,
            _In_ Refresh const& refresh,
            _In_ std::wstring const& id)
        {
            auto const tag = id + L"Scale";

            auto box = Choices(resources::GetString(L"TransformValueScale"),
                { resources::GetString(L"TransformValueScaleSevenBit"), resources::GetString(L"TransformValueScalePercent") },
                scale == patchbay::ValueScale::Percent ? 1 : 0, tag);

            box.SelectionChanged([pick, change, refresh, tag](foundation::IInspectable const& sender, auto&&)
                {
                    auto const index = SelectedIndexOf(sender);

                    if (index < 0)
                    {
                        return;
                    }

                    auto const value = index == 1 ? patchbay::ValueScale::Percent : patchbay::ValueScale::SevenBit;

                    if (change([&](patchbay::BlockSettings& s) { pick(s) = value; }))
                    {
                        refresh(tag);
                    }
                });

            host.Children().Append(box);
        }

        // What a tag or a memory holds, which decides how its numbers are shown and compared.
        void AddUnitChoice(
            _In_ controls::StackPanel const& host,
            _In_ winrt::hstring const& header,
            _In_ patchbay::LogicUnit unit,
            _In_ bool allowsValue,
            _In_ UnitOf const& pick,
            _In_ Change const& change,
            _In_ Refresh const& refresh,
            _In_ std::wstring const& id)
        {
            std::vector<patchbay::LogicUnit> units{
                patchbay::LogicUnit::Number, patchbay::LogicUnit::Channel, patchbay::LogicUnit::Group, patchbay::LogicUnit::Note };

            if (allowsValue)
            {
                units.push_back(patchbay::LogicUnit::Value);
            }

            std::vector<winrt::hstring> names{};
            int32_t selected{ -1 };

            for (size_t i = 0; i < units.size(); i++)
            {
                names.push_back(patchbay::DescribeLogicUnit(units[i]));

                if (units[i] == unit)
                {
                    selected = static_cast<int32_t>(i);
                }
            }

            auto const tag = id + L"Unit";
            auto box = Choices(header, names, selected, tag);

            box.SelectionChanged([units, pick, change, refresh, tag](foundation::IInspectable const& sender, auto&&)
                {
                    auto const index = SelectedIndexOf(sender);

                    if (index < 0 || static_cast<size_t>(index) >= units.size())
                    {
                        return;
                    }

                    auto const value = units[static_cast<size_t>(index)];

                    if (change([&](patchbay::BlockSettings& s) { pick(s) = value; }))
                    {
                        refresh(tag);
                    }
                });

            host.Children().Append(box);
        }

        // A part of the message, and for Bits, which word and which bits.
        void AddPartControls(
            _In_ controls::StackPanel const& host,
            _In_ winrt::hstring const& header,
            _In_ patchbay::PartPlace const& place,
            _In_ PlaceOf const& pick,
            _In_ Change const& change,
            _In_ Refresh const& refresh,
            _In_ std::wstring const& id)
        {
            std::vector<winrt::hstring> names{};

            for (size_t i = 0; i < patchbay::MessagePartCount; i++)
            {
                names.push_back(patchbay::DescribeMessagePart(static_cast<patchbay::MessagePart>(i)));
            }

            auto const tag = id + L"Part";
            auto box = Choices(header, names, static_cast<int32_t>(place.Part), tag);

            box.SelectionChanged([pick, change, refresh, tag](foundation::IInspectable const& sender, auto&&)
                {
                    auto const index = SelectedIndexOf(sender);

                    if (index < 0 || static_cast<size_t>(index) >= patchbay::MessagePartCount)
                    {
                        return;
                    }

                    auto const part = static_cast<patchbay::MessagePart>(index);

                    if (change([&](patchbay::BlockSettings& s) { pick(s).Part = part; }))
                    {
                        refresh(tag);
                    }
                });

            host.Children().Append(box);

            if (place.Part != patchbay::MessagePart::Bits)
            {
                return;
            }

            controls::Grid grid{};
            grid.ColumnSpacing(8);

            enum class Field { Word, High, Low };

            struct BitsField
            {
                Field Which;
                wchar_t const* HeaderKey;
                double Minimum;
                double Maximum;
                double Value;
            };

            BitsField const fields[]{
                { Field::Word, L"LogicWordHeader", 1, static_cast<double>(patchbay::MaximumUmpWords), static_cast<double>(place.Word) + 1 },
                { Field::High, L"LogicHighBitHeader", 0, 31, static_cast<double>(place.HighBit) },
                { Field::Low, L"LogicLowBitHeader", 0, 31, static_cast<double>(place.LowBit) },
            };

            int32_t column{ 0 };

            for (auto const& field : fields)
            {
                grid.ColumnDefinitions().Append(controls::ColumnDefinition{});

                auto number = patchbay::parts::NumberBox(resources::GetString(field.HeaderKey), field.Minimum, field.Maximum, field.Value, 8);
                number.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

                // Three across leaves no room for the buttons. The arrow keys still step.
                number.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Hidden);

                auto const which = field.Which;

                number.ValueChanged([pick, change, refresh, which, id](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                    {
                        if (std::isnan(args.NewValue()))
                        {
                            return;
                        }

                        auto const value = static_cast<uint8_t>(std::clamp(std::lround(args.NewValue()), 0L, 31L));

                        auto const changed = change([&](patchbay::BlockSettings& s)
                            {
                                auto& target = pick(s);

                                switch (which)
                                {
                                case Field::Word: target.Word = static_cast<uint8_t>(value > 0 ? value - 1 : 0); break;
                                case Field::High: target.HighBit = value; break;
                                default:          target.LowBit = value; break;
                                }
                            });

                        // Bits typed the wrong way round are kept the right way round, so show them that way.
                        if (changed && which != Field::Word)
                        {
                            auto kept = *changed;

                            if ((which == Field::High && pick(kept).HighBit != value) ||
                                (which == Field::Low && pick(kept).LowBit != value))
                            {
                                refresh(id + L"Part");
                            }
                        }
                    });

                controls::Grid::SetColumn(number, column++);
                grid.Children().Append(number);
            }

            host.Children().Append(grid);
            host.Children().Append(Hint(resources::GetString(L"LogicBitsHint")));
        }

        wchar_t const* SourceKindKey(_In_ patchbay::LogicSourceKind kind) noexcept
        {
            switch (kind)
            {
            case patchbay::LogicSourceKind::Number: return L"LogicSourceNumber";
            case patchbay::LogicSourceKind::Part:   return L"LogicSourcePart";
            case patchbay::LogicSourceKind::Tag:    return L"LogicSourceTag";
            default:                                return L"LogicSourceMemory";
            }
        }

        // Where a value comes from: a number, a part of the message, a tag or a memory.
        void AddSourceControls(
            _In_ controls::StackPanel const& host,
            _In_ winrt::hstring const& header,
            _In_ patchbay::LogicSource const& source,
            _In_ bool allowsNumber,
            _In_ patchbay::ValueScale scale,
            _In_ std::vector<std::wstring> const& tags,
            _In_ std::vector<std::wstring> const& memories,
            _In_ SourceOf const& pick,
            _In_ Change const& change,
            _In_ Refresh const& refresh,
            _In_ std::wstring const& id)
        {
            std::vector<patchbay::LogicSourceKind> kinds{};

            if (allowsNumber)
            {
                kinds.push_back(patchbay::LogicSourceKind::Number);
            }

            kinds.push_back(patchbay::LogicSourceKind::Part);
            kinds.push_back(patchbay::LogicSourceKind::Tag);
            kinds.push_back(patchbay::LogicSourceKind::Memory);

            std::vector<winrt::hstring> names{};
            int32_t selected{ -1 };

            for (size_t i = 0; i < kinds.size(); i++)
            {
                names.push_back(resources::GetString(SourceKindKey(kinds[i])));

                if (kinds[i] == source.Kind)
                {
                    selected = static_cast<int32_t>(i);
                }
            }

            auto const kindTag = id + L"Kind";
            auto kindBox = Choices(header, names, selected, kindTag);

            kindBox.SelectionChanged([kinds, pick, change, refresh, kindTag](foundation::IInspectable const& sender, auto&&)
                {
                    auto const index = SelectedIndexOf(sender);

                    if (index < 0 || static_cast<size_t>(index) >= kinds.size())
                    {
                        return;
                    }

                    auto const kind = kinds[static_cast<size_t>(index)];

                    if (change([&](patchbay::BlockSettings& s) { pick(s).Kind = kind; }))
                    {
                        refresh(kindTag);
                    }
                });

            host.Children().Append(kindBox);

            switch (source.Kind)
            {
            case patchbay::LogicSourceKind::Number:
            {
                std::vector<winrt::hstring> units{};

                for (auto const unit : { patchbay::LogicUnit::Number, patchbay::LogicUnit::Channel, patchbay::LogicUnit::Group,
                                         patchbay::LogicUnit::Note, patchbay::LogicUnit::Value })
                {
                    units.push_back(patchbay::DescribeLogicUnit(unit));
                }

                auto const unitTag = id + L"Unit";
                auto unitBox = Choices(resources::GetString(L"LogicNumberKindHeader"), units, static_cast<int32_t>(source.Unit), unitTag);

                unitBox.SelectionChanged([pick, change, refresh, unitTag](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const index = SelectedIndexOf(sender);

                        if (index < 0 || index > static_cast<int32_t>(patchbay::LogicUnit::Value))
                        {
                            return;
                        }

                        auto const unit = static_cast<patchbay::LogicUnit>(index);

                        if (change([&](patchbay::BlockSettings& s) { pick(s).Unit = unit; }))
                        {
                            refresh(unitTag);
                        }
                    });

                auto numberBox = UnitBox(resources::GetString(L"LogicNumberHeader"), source.Number, source.Unit, scale, id + L"Number");

                OnUnitBoxChanged(numberBox, source.Unit, scale, [pick, change](uint32_t stored)
                    {
                        change([&](patchbay::BlockSettings& s) { pick(s).Number = stored; });
                    });

                host.Children().Append(Pair(unitBox, numberBox));
                break;
            }

            case patchbay::LogicSourceKind::Part:
                AddPartControls(host, resources::GetString(L"LogicPartHeader"), source.Place,
                    [pick](patchbay::BlockSettings& s) -> patchbay::PartPlace& { return pick(s).Place; },
                    change, refresh, id);
                break;

            case patchbay::LogicSourceKind::Tag:
            case patchbay::LogicSourceKind::Memory:
            {
                auto const isTag = source.Kind == patchbay::LogicSourceKind::Tag;

                host.Children().Append(NameBox(
                    resources::GetString(isTag ? L"LogicTagHeader" : L"LogicMemoryHeader"),
                    source.Name,
                    isTag ? tags : memories,
                    id + L"Name",
                    [pick, change](std::wstring const& text)
                    {
                        auto const name = patchbay::LogicNameFrom(text);

                        change([&](patchbay::BlockSettings& s) { pick(s).Name = name; });
                    }));
                break;
            }

            default:
                break;
            }
        }

        // A test and the numbers it compares with, in the step's unit.
        void AddConditionControls(
            _In_ controls::StackPanel const& host,
            _In_ winrt::hstring const& header,
            _In_ patchbay::LogicCondition const& condition,
            _In_ patchbay::LogicUnit unit,
            _In_ patchbay::ValueScale scale,
            _In_ bool allowsAnything,
            _In_ ConditionOf const& pick,
            _In_ Change const& change,
            _In_ Refresh const& refresh,
            _In_ std::wstring const& id)
        {
            std::vector<patchbay::LogicTest> tests{};

            if (allowsAnything)
            {
                tests.push_back(patchbay::LogicTest::Anything);
            }

            for (auto const test : { patchbay::LogicTest::Is, patchbay::LogicTest::IsNot, patchbay::LogicTest::AtLeast,
                                     patchbay::LogicTest::Below, patchbay::LogicTest::Between, patchbay::LogicTest::OneOf,
                                     patchbay::LogicTest::HasValue, patchbay::LogicTest::IsEmpty })
            {
                tests.push_back(test);
            }

            std::vector<winrt::hstring> names{};
            int32_t selected{ -1 };

            for (size_t i = 0; i < tests.size(); i++)
            {
                names.push_back(patchbay::DescribeLogicTest(tests[i]));

                if (tests[i] == condition.Test)
                {
                    selected = static_cast<int32_t>(i);
                }
            }

            auto const testTag = id + L"Test";
            auto testBox = Choices(header, names, selected, testTag);

            testBox.SelectionChanged([tests, pick, change, refresh, testTag](foundation::IInspectable const& sender, auto&&)
                {
                    auto const index = SelectedIndexOf(sender);

                    if (index < 0 || static_cast<size_t>(index) >= tests.size())
                    {
                        return;
                    }

                    auto const test = tests[static_cast<size_t>(index)];

                    if (change([&](patchbay::BlockSettings& s) { pick(s).Test = test; }))
                    {
                        refresh(testTag);
                    }
                });

            host.Children().Append(testBox);

            switch (condition.Test)
            {
            case patchbay::LogicTest::Is:
            case patchbay::LogicTest::IsNot:
            case patchbay::LogicTest::AtLeast:
            case patchbay::LogicTest::Below:
            {
                auto value = UnitBox(resources::GetString(L"LogicValueHeader"), condition.Value, unit, scale, id + L"Value");

                OnUnitBoxChanged(value, unit, scale, [pick, change](uint32_t stored)
                    {
                        change([&](patchbay::BlockSettings& s) { pick(s).Value = stored; });
                    });

                host.Children().Append(value);
                break;
            }

            case patchbay::LogicTest::Between:
            {
                auto lowest = UnitBox(resources::GetString(L"LogicFromHeader"), condition.Lowest, unit, scale, id + L"Lowest");
                auto highest = UnitBox(resources::GetString(L"LogicToHeader"), condition.Highest, unit, scale, id + L"Highest");

                for (auto const low : { true, false })
                {
                    OnUnitBoxChanged(low ? lowest : highest, unit, scale, [pick, change, refresh, low, id](uint32_t stored)
                        {
                            auto const changed = change([&](patchbay::BlockSettings& s)
                                {
                                    (low ? pick(s).Lowest : pick(s).Highest) = stored;
                                });

                            // The ends are kept in order, so show them in order.
                            if (changed)
                            {
                                auto kept = *changed;

                                if ((low ? pick(kept).Lowest : pick(kept).Highest) != stored)
                                {
                                    refresh(id + (low ? L"Lowest" : L"Highest"));
                                }
                            }
                        });
                }

                host.Children().Append(Pair(lowest, highest));
                break;
            }

            case patchbay::LogicTest::OneOf:
            {
                controls::TextBox values{};

                values.Header(winrt::box_value(resources::GetString(L"LogicValuesHeader")));
                values.PlaceholderText(resources::GetString(L"LogicValuesPlaceholder"));
                values.Text(winrt::hstring{ ValueListText(condition.Values, unit, scale) });
                values.Tag(winrt::box_value(winrt::hstring{ id + L"Values" }));

                auto caption = Hint(resources::GetString(L"LogicValuesHint"));

                auto const commit = [pick, change, unit, scale, caption](controls::TextBox const& box)
                    {
                        auto const read = ReadValueList(std::wstring_view{ box.Text() }, unit, scale);

                        if (!read.has_value())
                        {
                            caption.Text(resources::GetString(L"LogicValuesNotUnderstood"));
                            return;
                        }

                        caption.Text(resources::GetString(L"LogicValuesHint"));

                        auto const list = *read;

                        change([&](patchbay::BlockSettings& s) { pick(s).Values = list; });
                    };

                values.LostFocus([commit](foundation::IInspectable const& sender, auto&&)
                    {
                        if (auto const box = sender.try_as<controls::TextBox>())
                        {
                            commit(box);
                        }
                    });

                values.KeyDown([commit](foundation::IInspectable const& sender, input::KeyRoutedEventArgs const& args)
                    {
                        if (args.Key() != winrt::Windows::System::VirtualKey::Enter)
                        {
                            return;
                        }

                        args.Handled(true);

                        if (auto const box = sender.try_as<controls::TextBox>())
                        {
                            commit(box);
                        }
                    });

                host.Children().Append(values);
                host.Children().Append(caption);
                break;
            }

            default:
                break;
            }
        }

        // Where a message goes that has nothing to test, and where a bypassed step sends messages.
        void AddWayChoices(
            _In_ controls::StackPanel const& host,
            _In_ patchbay::BlockKind kind,
            _In_ patchbay::UnreadableWay unreadable,
            _In_ patchbay::BypassWay bypass,
            _In_ Change const& change)
        {
            auto const isBranch = kind == patchbay::BlockKind::Branch;

            static constexpr patchbay::UnreadableWay unreadableWays[]{
                patchbay::UnreadableWay::EveryWay, patchbay::UnreadableWay::FirstWay,
                patchbay::UnreadableWay::LastWay, patchbay::UnreadableWay::KeepOut };

            int32_t selected{ 0 };

            for (size_t i = 0; i < std::size(unreadableWays); i++)
            {
                if (unreadableWays[i] == unreadable)
                {
                    selected = static_cast<int32_t>(i);
                }
            }

            auto unreadableBox = Choices(resources::GetString(L"LogicUnreadableHeader"),
                { resources::GetString(L"LogicUnreadableEvery"),
                  resources::GetString(isBranch ? L"LogicUnreadableYes" : L"LogicUnreadableFirstWay"),
                  resources::GetString(isBranch ? L"LogicUnreadableNo" : L"LogicUnreadableOtherwise"),
                  resources::GetString(L"LogicUnreadableKeepOut") },
                selected, L"unreadable");

            unreadableBox.SelectionChanged([change, isBranch](foundation::IInspectable const& sender, auto&&)
                {
                    auto const index = SelectedIndexOf(sender);

                    if (index < 0 || static_cast<size_t>(index) >= std::size(unreadableWays))
                    {
                        return;
                    }

                    auto const way = unreadableWays[static_cast<size_t>(index)];

                    change([&](patchbay::BlockSettings& s) { (isBranch ? s.Branch.Unreadable : s.Switch.Unreadable) = way; });
                });

            host.Children().Append(unreadableBox);
            host.Children().Append(Hint(resources::GetString(isBranch ? L"LogicUnreadableBranchHint" : L"LogicUnreadableSwitchHint")));

            auto bypassBox = Choices(resources::GetString(L"LogicBypassHeader"),
                { resources::GetString(L"LogicBypassEvery"),
                  resources::GetString(isBranch ? L"LogicBypassYes" : L"LogicBypassFirstWay") },
                bypass == patchbay::BypassWay::FirstWay ? 1 : 0, L"bypassWay");

            bypassBox.SelectionChanged([change, isBranch](foundation::IInspectable const& sender, auto&&)
                {
                    auto const index = SelectedIndexOf(sender);

                    if (index < 0)
                    {
                        return;
                    }

                    auto const way = index == 1 ? patchbay::BypassWay::FirstWay : patchbay::BypassWay::EveryWay;

                    change([&](patchbay::BlockSettings& s) { (isBranch ? s.Branch.Bypass : s.Switch.Bypass) = way; });
                });

            host.Children().Append(bypassBox);
        }

        // Any, then 1 to 16 on screen, stored from 0.
        controls::ComboBox SixteenOrAny(
            _In_ wchar_t const* headerKey,
            _In_ wchar_t const* itemFormat,
            _In_ int32_t value,
            _In_ std::wstring const& tag)
        {
            std::vector<winrt::hstring> items{ resources::GetString(L"ParameterAny") };

            for (int32_t i = 0; i < 16; i++)
            {
                items.push_back(resources::FormatString(itemFormat, i + 1));
            }

            return Choices(resources::GetString(headerKey), items, value < 0 ? 0 : std::clamp(value, 0, 15) + 1, tag);
        }

        // The message that changes a memory, the same choices a gate has, without raw words.
        void AddTriggerControls(
            _In_ controls::StackPanel const& host,
            _In_ patchbay::GateTrigger const& trigger,
            _In_ Change const& change)
        {
            auto const pick = [](patchbay::BlockSettings& s) -> patchbay::GateTrigger& { return s.SetMemory.Trigger; };

            auto group = SixteenOrAny(L"GeneratorGroup", L"FilterGroupFormat", trigger.Group, L"triggerGroup");

            group.SelectionChanged([pick, change](foundation::IInspectable const& sender, auto&&)
                {
                    auto const index = SelectedIndexOf(sender);

                    if (index >= 0)
                    {
                        auto const value = static_cast<int8_t>(index - 1);
                        change([&](patchbay::BlockSettings& s) { pick(s).Group = value; });
                    }
                });

            auto const voice = trigger.Kind == patchbay::GateTriggerKind::NoteOn ||
                trigger.Kind == patchbay::GateTriggerKind::NoteOff ||
                trigger.Kind == patchbay::GateTriggerKind::ControlChange ||
                trigger.Kind == patchbay::GateTriggerKind::ProgramChange;

            if (!voice)
            {
                host.Children().Append(group);
                return;
            }

            auto channel = SixteenOrAny(L"GateChannel", L"FilterChannelFormat", trigger.Channel, L"triggerChannel");

            channel.SelectionChanged([pick, change](foundation::IInspectable const& sender, auto&&)
                {
                    auto const index = SelectedIndexOf(sender);

                    if (index >= 0)
                    {
                        auto const value = static_cast<int8_t>(index - 1);
                        change([&](patchbay::BlockSettings& s) { pick(s).Channel = value; });
                    }
                });

            host.Children().Append(Pair(group, channel));

            auto const numberKey = trigger.Kind == patchbay::GateTriggerKind::ControlChange ? L"GateController"
                : trigger.Kind == patchbay::GateTriggerKind::ProgramChange ? L"GateProgram" : L"GateNote";

            // Empty means any, which the file keeps as -1.
            auto number = patchbay::parts::NumberBox(resources::GetString(numberKey), 0, 127,
                trigger.Number < 0 ? std::numeric_limits<double>::quiet_NaN() : static_cast<double>(trigger.Number), 10);

            number.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            number.PlaceholderText(resources::GetString(L"ParameterAny"));
            number.Tag(winrt::box_value(winrt::hstring{ L"triggerNumber" }));

            number.ValueChanged([pick, change](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                {
                    auto const value = std::isnan(args.NewValue())
                        ? int16_t{ -1 }
                        : static_cast<int16_t>(std::clamp(std::lround(args.NewValue()), 0L, 127L));

                    change([&](patchbay::BlockSettings& s) { pick(s).Number = value; });
                });

            if (trigger.Kind != patchbay::GateTriggerKind::ControlChange)
            {
                host.Children().Append(number);
                return;
            }

            auto test = Choices(resources::GetString(L"GateValueTest"),
                { resources::GetString(L"GateTestAny"), resources::GetString(L"GateTestAtLeast"), resources::GetString(L"GateTestBelow") },
                static_cast<int32_t>(trigger.Test), L"triggerTest");

            test.SelectionChanged([pick, change](foundation::IInspectable const& sender, auto&&)
                {
                    auto const index = SelectedIndexOf(sender);

                    if (index >= 0)
                    {
                        auto const value = static_cast<patchbay::GateValueTest>(index);
                        change([&](patchbay::BlockSettings& s) { pick(s).Test = value; });
                    }
                });

            host.Children().Append(Pair(number, test));

            auto value = patchbay::parts::NumberBox(resources::GetString(L"GateValue"), 0, 127, trigger.Value, 10);
            value.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            value.Tag(winrt::box_value(winrt::hstring{ L"triggerValue" }));

            value.ValueChanged([pick, change](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                {
                    if (!std::isnan(args.NewValue()))
                    {
                        auto const number = static_cast<uint8_t>(std::clamp(std::lround(args.NewValue()), 0L, 127L));
                        change([&](patchbay::BlockSettings& s) { pick(s).Value = number; });
                    }
                });

            host.Children().Append(value);
        }

        // A case that is no longer there edits this instead, which changes nothing.
        patchbay::LogicCondition& CaseCondition(_In_ patchbay::BlockSettings& settings, _In_ int32_t id)
        {
            for (auto& entry : settings.Switch.Cases)
            {
                if (entry.Id == id)
                {
                    return entry.Condition;
                }
            }

            static thread_local patchbay::LogicCondition scratch{};
            scratch = settings.Switch.Cases.empty() ? patchbay::LogicCondition{} : settings.Switch.Cases.front().Condition;

            return scratch;
        }

        // The number a new case starts with: the lowest one no case takes yet, so a new way isn't
        // a copy of one above it, which would always win. A share has no next number.
        uint32_t NextCaseValue(_In_ patchbay::SwitchSettings const& settings) noexcept
        {
            uint32_t top{ 0 };

            switch (settings.Unit)
            {
            case patchbay::LogicUnit::Channel:
            case patchbay::LogicUnit::Group:
                top = 15;
                break;

            case patchbay::LogicUnit::Note:
                top = 127;
                break;

            case patchbay::LogicUnit::Number:
                // There are never more cases than this, so one of these is always free.
                top = static_cast<uint32_t>(patchbay::MaximumSwitchCases);
                break;

            default:
                return 0;
            }

            for (uint32_t value = 0; value <= top; value++)
            {
                auto const taken = std::any_of(settings.Cases.begin(), settings.Cases.end(), [value](patchbay::SwitchCase const& entry)
                    {
                        return entry.Condition.Test == patchbay::LogicTest::Is && entry.Condition.Value == value;
                    });

                if (!taken)
                {
                    return value;
                }
            }

            return 0;
        }
    }

    _Use_decl_annotations_
    void MainWindow::CollectPatchLogicNames(std::vector<std::wstring>& tags, std::vector<std::wstring>& memories) noexcept
    {
        tags.clear();
        memories.clear();

        try
        {
            auto const* patch = CurrentPatch();

            if (patch == nullptr)
            {
                return;
            }

            for (auto const& block : patch->Blocks)
            {
                patchbay::CollectLogicNames(block.Kind, block.Settings, tags, memories);
            }

            // One of each, however it is capitalized, in order.
            for (auto* names : { &tags, &memories })
            {
                std::vector<std::wstring> unique{};

                for (auto const& name : *names)
                {
                    auto const seen = std::any_of(unique.begin(), unique.end(),
                        [&name](std::wstring const& other) { return patchbay::SameLogicName(name, other); });

                    if (!seen)
                    {
                        unique.push_back(name);
                    }
                }

                std::sort(unique.begin(), unique.end(), [](std::wstring const& left, std::wstring const& right)
                    {
                        return _wcsicmp(left.c_str(), right.c_str()) < 0;
                    });

                *names = std::move(unique);
            }
        }
        catch (...)
        {
            tags.clear();
            memories.clear();
        }
    }

    _Use_decl_annotations_
    void MainWindow::BuildLogicStepSettings(patchbay::PatchBlock const& block, controls::StackPanel const& body) noexcept
    {
        try
        {
            controls::StackPanel host{};
            host.Spacing(10);

            body.Children().Append(host);

            FillLogicStepSettings(block, host);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the logic step's settings.")
    }

    _Use_decl_annotations_
    void MainWindow::FillLogicStepSettings(patchbay::PatchBlock const& block, controls::StackPanel const& host) noexcept
    {
        try
        {
            auto const blockId = block.Id;
            auto const& settings = block.Settings;
            auto weak = get_weak();

            Change const change = [weak, blockId](std::function<void(patchbay::BlockSettings&)> const& apply)
                -> std::optional<patchbay::BlockSettings>
                {
                    auto strong = weak.get();

                    if (strong == nullptr || strong->m_updatingStepSettings)
                    {
                        return std::nullopt;
                    }

                    return strong->ChangeStepSettings(blockId, apply);
                };

            // Weak, so the controls in the panel don't keep the panel alive.
            Refresh const refresh = [weak, blockId, weakHost = winrt::make_weak(host)](std::wstring const& focusTag)
                {
                    auto strong = weak.get();

                    if (strong == nullptr)
                    {
                        return;
                    }

                    // Once the control that made the change has finished with it.
                    strong->DispatcherQueue().TryEnqueue([weak, blockId, weakHost, focusTag]()
                        {
                            auto window = weak.get();
                            auto panel = weakHost.get();

                            // The inspector may show something else by now.
                            if (window == nullptr || panel == nullptr || !panel.IsLoaded())
                            {
                                return;
                            }

                            auto const* patch = window->CurrentPatch();
                            auto const* current = patch == nullptr ? nullptr : patch->FindBlock(blockId);

                            if (current == nullptr)
                            {
                                return;
                            }

                            panel.Children().Clear();
                            window->FillLogicStepSettings(*current, panel);

                            if (auto const target = FindTagged(panel, focusTag))
                            {
                                target.Loaded([weakTarget = winrt::make_weak(target)](auto&&, auto&&)
                                    {
                                        if (auto const control = weakTarget.get())
                                        {
                                            control.Focus(xaml::FocusState::Programmatic);
                                        }
                                    });
                            }
                        });
                };

            std::vector<std::wstring> tags{};
            std::vector<std::wstring> memories{};
            CollectPatchLogicNames(tags, memories);

            switch (block.Kind)
            {
            case patchbay::BlockKind::Branch:
            case patchbay::BlockKind::Switch:
            {
                auto const isBranch = block.Kind == patchbay::BlockKind::Branch;
                auto const& subject = isBranch ? settings.Branch.Subject : settings.Switch.Subject;
                auto const unit = isBranch ? settings.Branch.Unit : settings.Switch.Unit;
                auto const scale = isBranch ? settings.Branch.Scale : settings.Switch.Scale;

                AddSourceControls(host, resources::GetString(isBranch ? L"LogicBranchSubject" : L"LogicSwitchSubject"),
                    subject, false, scale, tags, memories,
                    [isBranch](patchbay::BlockSettings& s) -> patchbay::LogicSource& { return isBranch ? s.Branch.Subject : s.Switch.Subject; },
                    change, refresh, L"subject");

                // A part of the message has a unit of its own. A tag or a memory holds whatever it was given.
                if (subject.Kind != patchbay::LogicSourceKind::Part)
                {
                    AddUnitChoice(host, resources::GetString(L"LogicHoldsHeader"), unit, true,
                        [isBranch](patchbay::BlockSettings& s) -> patchbay::LogicUnit& { return isBranch ? s.Branch.Unit : s.Switch.Unit; },
                        change, refresh, L"holds");
                }

                if (unit == patchbay::LogicUnit::Value)
                {
                    AddScaleChoice(host, scale,
                        [isBranch](patchbay::BlockSettings& s) -> patchbay::ValueScale& { return isBranch ? s.Branch.Scale : s.Switch.Scale; },
                        change, refresh, L"subject");
                }

                if (isBranch)
                {
                    AddConditionControls(host, resources::GetString(L"LogicBranchTest"), settings.Branch.Condition, unit, scale, true,
                        [](patchbay::BlockSettings& s) -> patchbay::LogicCondition& { return s.Branch.Condition; },
                        change, refresh, L"condition");

                    host.Children().Append(Hint(resources::GetString(L"LogicBranchHint")));
                }
                else
                {
                    host.Children().Append(patchbay::parts::Heading(resources::GetString(L"LogicSwitchWays")));

                    auto const& cases = settings.Switch.Cases;

                    for (size_t i = 0; i < cases.size(); i++)
                    {
                        auto const caseId = cases[i].Id;
                        auto const caseTag = L"case" + std::to_wstring(caseId);

                        controls::StackPanel entry{};
                        entry.Spacing(6);

                        controls::Grid heading{};
                        heading.ColumnDefinitions().Append(controls::ColumnDefinition{});

                        controls::ColumnDefinition removeColumn{};
                        removeColumn.Width(xaml::GridLengthHelper::Auto());
                        heading.ColumnDefinitions().Append(removeColumn);

                        auto title = patchbay::parts::Heading(resources::FormatString(L"LogicSwitchWayFormat", static_cast<int>(i) + 1));
                        title.VerticalAlignment(xaml::VerticalAlignment::Center);
                        heading.Children().Append(title);

                        controls::HyperlinkButton remove{};
                        remove.Content(winrt::box_value(resources::GetString(L"TransformRemove")));
                        remove.Padding(xaml::ThicknessHelper::FromLengths(6, 2, 6, 2));
                        controls::Grid::SetColumn(remove, 1);

                        xaml::Automation::AutomationProperties::SetName(remove,
                            resources::FormatString(L"LogicSwitchRemoveWayFormat", static_cast<int>(i) + 1));

                        remove.Click([change, refresh, caseId](auto&&, auto&&)
                            {
                                if (change([caseId](patchbay::BlockSettings& s)
                                    {
                                        std::erase_if(s.Switch.Cases, [caseId](patchbay::SwitchCase const& c) { return c.Id == caseId; });
                                    }))
                                {
                                    refresh(L"addWay");
                                }
                            });

                        heading.Children().Append(remove);
                        entry.Children().Append(heading);

                        AddConditionControls(entry, resources::GetString(L"LogicSwitchCaseTest"), cases[i].Condition, unit, scale, false,
                            [caseId](patchbay::BlockSettings& s) -> patchbay::LogicCondition& { return CaseCondition(s, caseId); },
                            change, refresh, caseTag);

                        host.Children().Append(entry);
                    }

                    controls::StackPanel adders{};
                    adders.Orientation(controls::Orientation::Horizontal);
                    adders.Spacing(8);

                    controls::Button add{};
                    add.Content(winrt::box_value(resources::GetString(L"LogicSwitchAddWay")));
                    add.Tag(winrt::box_value(winrt::hstring{ L"addWay" }));
                    add.IsEnabled(cases.size() < patchbay::MaximumSwitchCases);

                    add.Click([change, refresh](auto&&, auto&&)
                        {
                            if (change([](patchbay::BlockSettings& s)
                                {
                                    auto const id = patchbay::NewSwitchCaseId(s.Switch);

                                    if (id < 1 || s.Switch.Cases.size() >= patchbay::MaximumSwitchCases)
                                    {
                                        return;
                                    }

                                    patchbay::SwitchCase entry{};
                                    entry.Id = id;
                                    entry.Condition.Test = patchbay::LogicTest::Is;
                                    entry.Condition.Value = NextCaseValue(s.Switch);

                                    s.Switch.Cases.push_back(std::move(entry));
                                }))
                            {
                                refresh(L"addWay");
                            }
                        });

                    adders.Children().Append(add);

                    // The usual reason for a Switch: a way for each channel or each group.
                    if (unit == patchbay::LogicUnit::Channel || unit == patchbay::LogicUnit::Group)
                    {
                        controls::Button each{};
                        each.Content(winrt::box_value(resources::GetString(
                            unit == patchbay::LogicUnit::Channel ? L"LogicSwitchEachChannel" : L"LogicSwitchEachGroup")));
                        each.Tag(winrt::box_value(winrt::hstring{ L"addEach" }));
                        each.IsEnabled(cases.size() < patchbay::MaximumSwitchCases);

                        each.Click([change, refresh](auto&&, auto&&)
                            {
                                if (change([](patchbay::BlockSettings& s)
                                    {
                                        for (uint32_t value = 0; value < 16; value++)
                                        {
                                            auto const taken = std::any_of(s.Switch.Cases.begin(), s.Switch.Cases.end(),
                                                [value](patchbay::SwitchCase const& entry)
                                                {
                                                    return entry.Condition.Test == patchbay::LogicTest::Is && entry.Condition.Value == value;
                                                });

                                            auto const id = patchbay::NewSwitchCaseId(s.Switch);

                                            if (taken || id < 1 || s.Switch.Cases.size() >= patchbay::MaximumSwitchCases)
                                            {
                                                continue;
                                            }

                                            patchbay::SwitchCase entry{};
                                            entry.Id = id;
                                            entry.Condition.Test = patchbay::LogicTest::Is;
                                            entry.Condition.Value = value;

                                            s.Switch.Cases.push_back(std::move(entry));
                                        }
                                    }))
                                {
                                    refresh(L"addEach");
                                }
                            });

                        adders.Children().Append(each);
                    }

                    host.Children().Append(adders);
                    host.Children().Append(Hint(resources::GetString(L"LogicSwitchHint")));
                }

                AddWayChoices(host, block.Kind,
                    isBranch ? settings.Branch.Unreadable : settings.Switch.Unreadable,
                    isBranch ? settings.Branch.Bypass : settings.Switch.Bypass,
                    change);

                m_stepSettingsFocus = FindTagged(host, L"subjectKind");
                break;
            }

            case patchbay::BlockKind::SetTag:
            {
                auto const& tag = settings.SetTag;

                host.Children().Append(NameBox(resources::GetString(L"LogicTagHeader"), tag.Tag, tags, L"tagName",
                    [change](std::wstring const& text)
                    {
                        auto const name = patchbay::LogicNameFrom(text);

                        change([&](patchbay::BlockSettings& s) { s.SetTag.Tag = name; });
                    }));

                AddSourceControls(host, resources::GetString(L"LogicSetToHeader"), tag.Value, true, tag.Scale, tags, memories,
                    [](patchbay::BlockSettings& s) -> patchbay::LogicSource& { return s.SetTag.Value; },
                    change, refresh, L"value");

                if (tag.Value.Kind == patchbay::LogicSourceKind::Number && tag.Value.Unit == patchbay::LogicUnit::Value)
                {
                    AddScaleChoice(host, tag.Scale, [](patchbay::BlockSettings& s) -> patchbay::ValueScale& { return s.SetTag.Scale; },
                        change, refresh, L"value");
                }

                host.Children().Append(Hint(resources::GetString(L"LogicSetTagHint")));

                m_stepSettingsFocus = FindTagged(host, L"tagName");
                break;
            }

            case patchbay::BlockKind::SetMemory:
            {
                auto const& memory = settings.SetMemory;

                host.Children().Append(NameBox(resources::GetString(L"LogicMemoryHeader"), memory.Memory, memories, L"memoryName",
                    [change](std::wstring const& text)
                    {
                        auto const name = patchbay::LogicNameFrom(text);

                        change([&](patchbay::BlockSettings& s) { s.SetMemory.Memory = name; });
                    }));

                // ------------------------------------------- when it changes
                static constexpr patchbay::GateTriggerKind triggerKinds[]{
                    patchbay::GateTriggerKind::NoteOn, patchbay::GateTriggerKind::NoteOff,
                    patchbay::GateTriggerKind::ControlChange, patchbay::GateTriggerKind::ProgramChange,
                    patchbay::GateTriggerKind::Start, patchbay::GateTriggerKind::Continue, patchbay::GateTriggerKind::Stop };

                int32_t when{ memory.EveryMessage ? 0 : -1 };

                for (size_t i = 0; i < std::size(triggerKinds) && !memory.EveryMessage; i++)
                {
                    if (triggerKinds[i] == memory.Trigger.Kind)
                    {
                        when = static_cast<int32_t>(i) + 1;
                    }
                }

                auto whenBox = Choices(resources::GetString(L"LogicWhenHeader"),
                    { resources::GetString(L"LogicWhenEveryMessage"), resources::GetString(L"GateKindNoteOn"),
                      resources::GetString(L"GateKindNoteOff"), resources::GetString(L"GateKindControlChange"),
                      resources::GetString(L"GateKindProgramChange"), resources::GetString(L"GateKindStart"),
                      resources::GetString(L"GateKindContinue"), resources::GetString(L"GateKindStop") },
                    when, L"when");

                whenBox.SelectionChanged([change, refresh](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const index = SelectedIndexOf(sender);

                        if (index < 0 || static_cast<size_t>(index) > std::size(triggerKinds))
                        {
                            return;
                        }

                        if (change([index](patchbay::BlockSettings& s)
                            {
                                s.SetMemory.EveryMessage = index == 0;

                                if (index > 0)
                                {
                                    s.SetMemory.Trigger.Kind = triggerKinds[static_cast<size_t>(index) - 1];
                                }
                            }))
                        {
                            refresh(L"when");
                        }
                    });

                host.Children().Append(whenBox);

                if (!memory.EveryMessage)
                {
                    AddTriggerControls(host, memory.Trigger, change);
                }

                // ------------------------------------------- what it does
                std::vector<winrt::hstring> actions{};

                for (auto const action : { patchbay::MemoryAction::Set, patchbay::MemoryAction::Toggle, patchbay::MemoryAction::StepUp,
                                           patchbay::MemoryAction::StepDown, patchbay::MemoryAction::Clear })
                {
                    actions.push_back(patchbay::DescribeMemoryAction(action));
                }

                auto actionBox = Choices(resources::GetString(L"LogicActionHeader"), actions, static_cast<int32_t>(memory.Action), L"action");

                actionBox.SelectionChanged([change, refresh](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const index = SelectedIndexOf(sender);

                        if (index < 0 || index > static_cast<int32_t>(patchbay::MemoryAction::Clear))
                        {
                            return;
                        }

                        auto const action = static_cast<patchbay::MemoryAction>(index);

                        if (change([action](patchbay::BlockSettings& s) { s.SetMemory.Action = action; }))
                        {
                            refresh(L"action");
                        }
                    });

                host.Children().Append(actionBox);

                switch (memory.Action)
                {
                case patchbay::MemoryAction::Set:
                    AddSourceControls(host, resources::GetString(L"LogicSetToHeader"), memory.Value, true, memory.Scale, tags, memories,
                        [](patchbay::BlockSettings& s) -> patchbay::LogicSource& { return s.SetMemory.Value; },
                        change, refresh, L"value");

                    if (memory.Value.Kind == patchbay::LogicSourceKind::Number && memory.Value.Unit == patchbay::LogicUnit::Value)
                    {
                        AddScaleChoice(host, memory.Scale, [](patchbay::BlockSettings& s) -> patchbay::ValueScale& { return s.SetMemory.Scale; },
                            change, refresh, L"value");
                    }
                    break;

                case patchbay::MemoryAction::Toggle:
                {
                    AddUnitChoice(host, resources::GetString(L"LogicHoldsHeader"), memory.Unit, true,
                        [](patchbay::BlockSettings& s) -> patchbay::LogicUnit& { return s.SetMemory.Unit; },
                        change, refresh, L"holds");

                    if (memory.Unit == patchbay::LogicUnit::Value)
                    {
                        AddScaleChoice(host, memory.Scale, [](patchbay::BlockSettings& s) -> patchbay::ValueScale& { return s.SetMemory.Scale; },
                            change, refresh, L"holds");
                    }

                    auto first = UnitBox(resources::GetString(L"LogicToggleFirst"), memory.First, memory.Unit, memory.Scale, L"first");
                    auto second = UnitBox(resources::GetString(L"LogicToggleSecond"), memory.Second, memory.Unit, memory.Scale, L"second");

                    OnUnitBoxChanged(first, memory.Unit, memory.Scale, [change](uint32_t stored)
                        {
                            change([stored](patchbay::BlockSettings& s) { s.SetMemory.First = stored; });
                        });

                    OnUnitBoxChanged(second, memory.Unit, memory.Scale, [change](uint32_t stored)
                        {
                            change([stored](patchbay::BlockSettings& s) { s.SetMemory.Second = stored; });
                        });

                    host.Children().Append(Pair(first, second));
                    host.Children().Append(Hint(resources::GetString(L"LogicToggleHint")));
                    break;
                }

                case patchbay::MemoryAction::StepUp:
                case patchbay::MemoryAction::StepDown:
                {
                    // Steps go one whole number at a time, so a value isn't one of the choices.
                    AddUnitChoice(host, resources::GetString(L"LogicHoldsHeader"), memory.Unit, false,
                        [](patchbay::BlockSettings& s) -> patchbay::LogicUnit& { return s.SetMemory.Unit; },
                        change, refresh, L"holds");

                    auto const unit = memory.Unit == patchbay::LogicUnit::Value ? patchbay::LogicUnit::Number : memory.Unit;

                    auto lowest = UnitBox(resources::GetString(L"LogicStepLowest"), memory.Lowest, unit, memory.Scale, L"lowest");
                    auto highest = UnitBox(resources::GetString(L"LogicStepHighest"), memory.Highest, unit, memory.Scale, L"highest");

                    for (auto const low : { true, false })
                    {
                        OnUnitBoxChanged(low ? lowest : highest, unit, memory.Scale, [change, refresh, low](uint32_t stored)
                            {
                                auto const changed = change([low, stored](patchbay::BlockSettings& s)
                                    {
                                        (low ? s.SetMemory.Lowest : s.SetMemory.Highest) = stored;
                                    });

                                // The ends are kept in order, so show them in order.
                                if (changed && (low ? changed->SetMemory.Lowest : changed->SetMemory.Highest) != stored)
                                {
                                    refresh(low ? L"lowest" : L"highest");
                                }
                            });
                    }

                    host.Children().Append(Pair(lowest, highest));

                    auto wraps = Check(resources::GetString(L"LogicStepWraps"), memory.Wraps);
                    wraps.Tag(winrt::box_value(winrt::hstring{ L"wraps" }));
                    wraps.Checked([change](auto&&, auto&&) { change([](patchbay::BlockSettings& s) { s.SetMemory.Wraps = true; }); });
                    wraps.Unchecked([change](auto&&, auto&&) { change([](patchbay::BlockSettings& s) { s.SetMemory.Wraps = false; }); });

                    host.Children().Append(wraps);
                    host.Children().Append(Hint(resources::GetString(L"LogicStepHint")));
                    break;
                }

                default:
                    break;
                }

                auto passes = Check(resources::GetString(L"LogicPassTriggers"), memory.PassesTriggers);
                passes.Tag(winrt::box_value(winrt::hstring{ L"passes" }));
                passes.Checked([change](auto&&, auto&&) { change([](patchbay::BlockSettings& s) { s.SetMemory.PassesTriggers = true; }); });
                passes.Unchecked([change](auto&&, auto&&) { change([](patchbay::BlockSettings& s) { s.SetMemory.PassesTriggers = false; }); });

                host.Children().Append(passes);
                host.Children().Append(Hint(resources::GetString(L"LogicSetMemoryHint")));

                m_stepSettingsFocus = FindTagged(host, L"memoryName");
                break;
            }

            case patchbay::BlockKind::PutValue:
            {
                auto const& put = settings.PutValue;

                AddPartControls(host, resources::GetString(L"LogicPutIntoHeader"), put.Target,
                    [](patchbay::BlockSettings& s) -> patchbay::PartPlace& { return s.PutValue.Target; },
                    change, refresh, L"target");

                AddSourceControls(host, resources::GetString(L"LogicPutWhatHeader"), put.Value, true, put.Scale, tags, memories,
                    [](patchbay::BlockSettings& s) -> patchbay::LogicSource& { return s.PutValue.Value; },
                    change, refresh, L"value");

                if (put.Value.Kind == patchbay::LogicSourceKind::Number && put.Value.Unit == patchbay::LogicUnit::Value)
                {
                    AddScaleChoice(host, put.Scale, [](patchbay::BlockSettings& s) -> patchbay::ValueScale& { return s.PutValue.Scale; },
                        change, refresh, L"value");
                }

                auto keepOut = Check(resources::GetString(L"LogicKeepOutWhenEmpty"), put.KeepsOutWhenEmpty);
                keepOut.Tag(winrt::box_value(winrt::hstring{ L"keepOut" }));
                keepOut.Checked([change](auto&&, auto&&) { change([](patchbay::BlockSettings& s) { s.PutValue.KeepsOutWhenEmpty = true; }); });
                keepOut.Unchecked([change](auto&&, auto&&) { change([](patchbay::BlockSettings& s) { s.PutValue.KeepsOutWhenEmpty = false; }); });

                host.Children().Append(keepOut);
                host.Children().Append(Hint(resources::GetString(L"LogicPutValueHint")));

                m_stepSettingsFocus = FindTagged(host, L"targetPart");
                break;
            }

            default:
                break;
            }
        }
        catch (...)
        {
            m_updatingStepSettings = false;
            MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(L"Unable to show the logic step's settings.");
        }
    }

    _Use_decl_annotations_
    winrt::hstring MainWindow::LogicStatusText(std::wstring const& blockId) noexcept
    {
        try
        {
            auto const* patch = CurrentPatch();
            auto const* block = patch == nullptr ? nullptr : patch->FindBlock(blockId);

            if (block == nullptr || !patchbay::IsLogicStep(block->Kind))
            {
                return {};
            }

            auto const& engine = patchbay::RouteEngine::Current();
            auto const& settings = block->Settings;

            std::wstring text{};

            auto const add = [&text](winrt::hstring const& line)
                {
                    if (!text.empty())
                    {
                        text += L'\n';
                    }

                    text += line;
                };

            // A memory lasts while the app runs, so it has a value to show even when the patch is off.
            auto const addMemory = [&](std::wstring const& name, patchbay::LogicUnit unit, patchbay::ValueScale scale)
                {
                    if (name.empty())
                    {
                        return;
                    }

                    auto const value = engine.MemoryValue(m_patchKey, name);

                    add(resources::FormatString(L"LogicMemoryNowFormat", name,
                        value.has_value() ? patchbay::DescribeLogicValue(*value, unit, scale) : resources::GetString(L"LogicValueEmpty")));
                };

            // How a source's value reads, for a memory it names.
            auto const unitOf = [](patchbay::LogicSource const& source)
                {
                    return source.Kind == patchbay::LogicSourceKind::Part ? patchbay::UnitOfPart(source.Place.Part)
                        : source.Kind == patchbay::LogicSourceKind::Number ? source.Unit
                        : patchbay::LogicUnit::Number;
                };

            switch (block->Kind)
            {
            case patchbay::BlockKind::Branch:
            case patchbay::BlockKind::Switch:
            {
                auto const isBranch = block->Kind == patchbay::BlockKind::Branch;
                auto const& subject = isBranch ? settings.Branch.Subject : settings.Switch.Subject;
                auto const unit = isBranch ? settings.Branch.Unit : settings.Switch.Unit;
                auto const scale = isBranch ? settings.Branch.Scale : settings.Switch.Scale;

                if (auto const last = engine.LastTestedValue(m_patchKey + L'|' + blockId))
                {
                    add(resources::FormatString(L"LogicLastTestedFormat", patchbay::DescribeLogicValue(*last, unit, scale)));
                }

                if (subject.Kind == patchbay::LogicSourceKind::Memory)
                {
                    addMemory(subject.Name, unit, scale);
                }

                break;
            }

            case patchbay::BlockKind::SetMemory:
            {
                auto const& memory = settings.SetMemory;

                addMemory(memory.Memory,
                    memory.Action == patchbay::MemoryAction::Set ? unitOf(memory.Value) : memory.Unit,
                    memory.Scale);
                break;
            }

            case patchbay::BlockKind::SetTag:
                if (settings.SetTag.Value.Kind == patchbay::LogicSourceKind::Memory)
                {
                    addMemory(settings.SetTag.Value.Name, patchbay::LogicUnit::Number, settings.SetTag.Scale);
                }
                break;

            case patchbay::BlockKind::PutValue:
                if (settings.PutValue.Value.Kind == patchbay::LogicSourceKind::Memory)
                {
                    addMemory(settings.PutValue.Value.Name, patchbay::UnitOfPart(settings.PutValue.Target.Part), settings.PutValue.Scale);
                }
                break;

            default:
                break;
            }

            return winrt::hstring{ text };
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to describe the logic step.")

        return {};
    }
}
