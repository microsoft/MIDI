// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The settings of the message converter, edited right in the inspector: a list of rows, each
// with what it looks for and what it turns that into. What a row shows depends on the kinds
// chosen in it, so a change to one of those shows the settings again and puts the focus back.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "DialogParts.h"
#include "GeneralMidi.h"
#include "MessageText.h"
#include "StringResources.h"

#include <cwctype>

using namespace winrt::Microsoft::UI::Xaml;

namespace patchbay = ::midipatchbay;
namespace resources = ::midipatchbay::resources;

namespace winrt::midipatchbay::implementation
{
    namespace
    {
        using patchbay::parts::Check;
        using patchbay::parts::Heading;
        using patchbay::parts::Hint;

        using Change = std::function<std::optional<patchbay::BlockSettings>(std::function<void(patchbay::BlockSettings&)> const&)>;

        // Shows the settings again, then puts the focus on the control with this tag.
        using Refresh = std::function<void(std::wstring const&)>;

        // Changes one row, when it is still there.
        std::optional<patchbay::BlockSettings> EditRow(
            _In_ Change const& change,
            _In_ size_t index,
            _In_ std::function<void(patchbay::ConverterRow&)> const& edit)
        {
            return change([index, &edit](patchbay::BlockSettings& s)
                {
                    if (index < s.Converter.Rows.size())
                    {
                        edit(s.Converter.Rows[index]);
                    }
                });
        }

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
            box.Tag(winrt::box_value(winrt::hstring{ tag }));

            return box;
        }

        int32_t SelectedIndexOf(_In_ foundation::IInspectable const& sender)
        {
            auto const box = sender.try_as<controls::ComboBox>();

            return box == nullptr ? -1 : box.SelectedIndex();
        }

        // A list that changes what the row shows, so a choice shows the settings again.
        void OnChoice(
            _In_ controls::ComboBox const& box,
            _In_ Change const& change,
            _In_ Refresh const& refresh,
            _In_ size_t index,
            _In_ std::function<void(patchbay::ConverterRow&, int32_t)> const& apply)
        {
            auto const tag = std::wstring{ winrt::unbox_value_or<winrt::hstring>(box.Tag(), winrt::hstring{}) };

            box.SelectionChanged([change, refresh, index, apply, tag](foundation::IInspectable const& sender, auto&&)
                {
                    auto const chosen = SelectedIndexOf(sender);

                    if (chosen < 0)
                    {
                        return;
                    }

                    if (EditRow(change, index, [&](patchbay::ConverterRow& row) { apply(row, chosen); }) && refresh)
                    {
                        refresh(tag);
                    }
                });
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

        controls::NumberBox Number(
            _In_ winrt::hstring const& header,
            _In_ double minimum,
            _In_ double maximum,
            _In_ double value,
            _In_ std::wstring const& tag)
        {
            auto box = patchbay::parts::NumberBox(header, minimum, maximum, value, 10);

            box.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            box.Tag(winrt::box_value(winrt::hstring{ tag }));

            return box;
        }

        void OnWholeNumber(_In_ controls::NumberBox const& box, _In_ uint32_t highest, _In_ std::function<void(uint32_t)> const& apply)
        {
            box.ValueChanged([highest, apply](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                {
                    if (!std::isnan(args.NewValue()))
                    {
                        apply(static_cast<uint32_t>(std::clamp(std::llround(args.NewValue()), 0LL, static_cast<long long>(highest))));
                    }
                });
        }

        // A share of the whole range, shown as a percentage or as 0 to 127.
        controls::NumberBox ValueBox(
            _In_ winrt::hstring const& header,
            _In_ int32_t hundredths,
            _In_ patchbay::ValueScale scale,
            _In_ std::wstring const& tag,
            _In_ int32_t lowestHundredths,
            _In_ std::function<void(int32_t)> const& apply)
        {
            auto const percent = scale == patchbay::ValueScale::Percent;
            auto box = Number(header, patchbay::DisplayFromHundredths(lowestHundredths, scale), percent ? 100 : 127,
                patchbay::DisplayFromHundredths(hundredths, scale), tag);

            winrt::Windows::Globalization::NumberFormatting::DecimalFormatter formatter{};

            formatter.IntegerDigits(1);
            formatter.FractionDigits(percent ? 2 : 0);
            formatter.IsGrouped(false);

            box.NumberFormatter(formatter);
            box.Value(patchbay::DisplayFromHundredths(hundredths, scale));

            box.ValueChanged([apply, scale, lowestHundredths](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                {
                    if (!std::isnan(args.NewValue()))
                    {
                        apply(std::clamp(patchbay::HundredthsFromDisplay(args.NewValue(), scale), lowestHundredths, patchbay::FullScaleHundredths));
                    }
                });

            return box;
        }

        // A note number with its name, so nobody has to work out which C it is.
        controls::NumberBox NoteBox(
            _In_ winrt::hstring const& header,
            _In_ uint32_t note,
            _In_ std::wstring const& tag,
            _In_ std::function<void(uint32_t)> const& apply)
        {
            auto const shown = (std::min)(note, 127u);
            auto box = Number(header, 0, 127, shown, tag);

            box.Description(winrt::box_value(patchbay::DescribeNote(static_cast<uint8_t>(shown))));

            box.ValueChanged([apply](controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args)
                {
                    if (std::isnan(args.NewValue()))
                    {
                        return;
                    }

                    auto const value = static_cast<uint32_t>(std::clamp(std::lround(args.NewValue()), 0L, 127L));

                    sender.Description(winrt::box_value(patchbay::DescribeNote(static_cast<uint8_t>(value))));
                    apply(value);
                });

            return box;
        }

        // A program number with the General MIDI name an instrument without a list would play.
        controls::NumberBox ProgramBox(
            _In_ winrt::hstring const& header,
            _In_ uint32_t program,
            _In_ std::wstring const& tag,
            _In_ std::function<void(uint32_t)> const& apply)
        {
            auto const shown = (std::min)(program, 127u);
            auto box = Number(header, 0, 127, shown, tag);

            box.Description(winrt::box_value(winrt::hstring{ midiapp::GeneralMidiProgramName(static_cast<uint8_t>(shown)) }));

            box.ValueChanged([apply](controls::NumberBox const& sender, controls::NumberBoxValueChangedEventArgs const& args)
                {
                    if (std::isnan(args.NewValue()))
                    {
                        return;
                    }

                    auto const value = static_cast<uint32_t>(std::clamp(std::lround(args.NewValue()), 0L, 127L));

                    sender.Description(winrt::box_value(winrt::hstring{ midiapp::GeneralMidiProgramName(static_cast<uint8_t>(value)) }));
                    apply(value);
                });

            return box;
        }

        // A note, a controller or a program, the way each is best typed.
        controls::NumberBox NumberFor(
            _In_ patchbay::ConverterInput input,
            _In_ winrt::hstring const& header,
            _In_ uint32_t number,
            _In_ std::wstring const& tag,
            _In_ std::function<void(uint32_t)> const& apply)
        {
            if (input == patchbay::ConverterInput::Note || input == patchbay::ConverterInput::PolyPressure)
            {
                return NoteBox(header, number, tag, apply);
            }

            if (input == patchbay::ConverterInput::ProgramChange)
            {
                return ProgramBox(header, number, tag, apply);
            }

            auto box = Number(header, 0, 127, (std::min)(number, 127u), tag);
            OnWholeNumber(box, 127, apply);

            return box;
        }

        controls::CheckBox CommittedCheck(
            _In_ winrt::hstring const& text,
            _In_ bool value,
            _In_ std::wstring const& tag,
            _In_ Change const& change,
            _In_ Refresh const& refresh,
            _In_ size_t index,
            _In_ std::function<void(patchbay::ConverterRow&, bool)> const& apply)
        {
            auto check = Check(text, value);

            check.Tag(winrt::box_value(winrt::hstring{ tag }));

            auto const set = [change, refresh, index, apply, tag](bool on)
                {
                    if (EditRow(change, index, [&](patchbay::ConverterRow& row) { apply(row, on); }) && refresh)
                    {
                        refresh(tag);
                    }
                };

            check.Checked([set](auto&&, auto&&) { set(true); });
            check.Unchecked([set](auto&&, auto&&) { set(false); });

            return check;
        }

        // Up to four words in hex, as one whole message that has a group. Nothing when they aren't.
        std::optional<std::vector<uint32_t>> ReadWords(_In_ std::wstring_view text)
        {
            constexpr uint8_t sizes[16] = { 1, 1, 1, 2, 2, 4, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4 };

            std::vector<uint32_t> words{};
            std::wstring token{};

            auto const flush = [&]() -> bool
                {
                    if (token.empty())
                    {
                        return true;
                    }

                    if (token.size() > 2 && token[0] == L'0' && (token[1] == L'x' || token[1] == L'X'))
                    {
                        token.erase(0, 2);
                    }

                    if (token.empty() || token.size() > 8 || words.size() >= patchbay::MaximumControllerWords ||
                        !std::all_of(token.begin(), token.end(), [](wchar_t c) { return std::iswxdigit(c) != 0; }))
                    {
                        return false;
                    }

                    words.push_back(static_cast<uint32_t>(std::wcstoul(token.c_str(), nullptr, 16)));
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

            if (!flush() || words.empty())
            {
                return std::nullopt;
            }

            auto const type = words[0] >> 28;

            // Utility and stream messages have no group to take from the message they replace.
            if (words.size() != sizes[type] || type == 0x0 || type == 0xF)
            {
                return std::nullopt;
            }

            return words;
        }

        std::wstring WordsText(_In_ patchbay::ControllerMessage const& message)
        {
            std::wstring text{};

            for (uint8_t i = 0; i < message.WordCount && i < patchbay::MaximumControllerWords; i++)
            {
                if (!text.empty())
                {
                    text += L' ';
                }

                text += std::format(L"{:08X}", message.Words[i]);
            }

            return text;
        }

        // Any channel, or one of the sixteen, counted from 1 on screen and stored from 0.
        std::vector<winrt::hstring> ChannelItems(_In_ wchar_t const* firstKey)
        {
            std::vector<winrt::hstring> items{ resources::GetString(firstKey) };

            for (int32_t i = 0; i < 16; i++)
            {
                items.push_back(resources::FormatString(L"FilterChannelFormat", i + 1));
            }

            return items;
        }

        // ------------------------------------------------------------------ what a row looks for

        void AddFrom(
            _In_ controls::StackPanel const& host,
            _In_ patchbay::ConverterRow const& row,
            _In_ size_t index,
            _In_ patchbay::ValueScale scale,
            _In_ Change const& change,
            _In_ Refresh const& refresh,
            _In_ std::wstring const& id)
        {
            std::vector<winrt::hstring> kinds{};

            for (size_t i = 0; i < patchbay::ConverterInputCount; i++)
            {
                kinds.push_back(patchbay::DescribeConverterInput(static_cast<patchbay::ConverterInput>(i)));
            }

            auto kind = Choices(resources::GetString(L"ConverterFromHeader"), kinds, static_cast<int32_t>(row.From), id + L"From");

            OnChoice(kind, change, refresh, index, [](patchbay::ConverterRow& r, int32_t chosen)
                {
                    if (static_cast<size_t>(chosen) < patchbay::ConverterInputCount)
                    {
                        r.From = static_cast<patchbay::ConverterInput>(chosen);
                    }
                });

            auto channel = Choices(resources::GetString(L"GeneratorChannel"), ChannelItems(L"ConverterAnyChannel"),
                row.FromChannel < 0 ? 0 : row.FromChannel + 1, id + L"FromChannel");

            OnChoice(channel, change, nullptr, index, [](patchbay::ConverterRow& r, int32_t chosen)
                {
                    r.FromChannel = static_cast<int8_t>(chosen <= 0 ? -1 : (std::min)(chosen - 1, 15));
                });

            host.Children().Append(Pair(kind, channel));

            if (patchbay::ConverterInputHasNumber(row.From))
            {
                auto const isNote = row.From == patchbay::ConverterInput::Note || row.From == patchbay::ConverterInput::PolyPressure;
                auto const isProgram = row.From == patchbay::ConverterInput::ProgramChange;

                auto const lowestKey = isNote ? L"ConverterLowestNote" : isProgram ? L"ControllerLowestProgram" : L"ConverterLowestController";
                auto const highestKey = isNote ? L"ConverterHighestNote" : isProgram ? L"ControllerHighestProgram" : L"ConverterHighestController";

                auto const edit = [change, index](bool isLowest, uint32_t number)
                    {
                        EditRow(change, index, [isLowest, number](patchbay::ConverterRow& r)
                            {
                                (isLowest ? r.Lowest : r.Highest) = static_cast<uint8_t>(number);
                            });
                    };

                host.Children().Append(Pair(
                    NumberFor(row.From, resources::GetString(lowestKey), row.Lowest, id + L"Lowest",
                        [edit](uint32_t number) { edit(true, number); }),
                    NumberFor(row.From, resources::GetString(highestKey), row.Highest, id + L"Highest",
                        [edit](uint32_t number) { edit(false, number); })));
            }

            if (!patchbay::ConverterInputIsValue(row.From))
            {
                return;
            }

            auto readAs = Choices(resources::GetString(L"ConverterReadAs"),
                { resources::GetString(L"ConverterReadAsValue"), resources::GetString(L"ConverterReadAsButton") },
                row.AsButton ? 1 : 0, id + L"ReadAs");

            OnChoice(readAs, change, refresh, index, [](patchbay::ConverterRow& r, int32_t chosen) { r.AsButton = chosen == 1; });

            host.Children().Append(readAs);

            if (!row.AsButton)
            {
                return;
            }

            host.Children().Append(ValueBox(resources::GetString(L"ConverterPressedAt"), row.PressHundredths, scale, id + L"Press",
                patchbay::HundredthsFromSevenBit(1), [change, index](int32_t hundredths)
                {
                    EditRow(change, index, [hundredths](patchbay::ConverterRow& r) { r.PressHundredths = hundredths; });
                }));

            host.Children().Append(Hint(resources::GetString(L"ConverterPressedAtHint")));
        }

        // ------------------------------------------------------------------ what it becomes

        void AddTo(
            _In_ controls::StackPanel const& host,
            _In_ patchbay::ConverterRow const& row,
            _In_ size_t index,
            _In_ patchbay::ValueScale scale,
            _In_ Change const& change,
            _In_ Refresh const& refresh,
            _In_ std::wstring const& id)
        {
            auto const& to = row.To;

            // Every kind but Nothing: a converter always sends something.
            std::vector<winrt::hstring> kinds{};

            for (size_t i = 1; i < patchbay::ControllerMessageKindCount; i++)
            {
                kinds.push_back(patchbay::DescribeControllerMessageKind(static_cast<patchbay::ControllerMessageKind>(i)));
            }

            auto kind = Choices(resources::GetString(L"ConverterToHeader"), kinds, static_cast<int32_t>(to.Kind) - 1, id + L"To");

            OnChoice(kind, change, refresh, index, [](patchbay::ConverterRow& r, int32_t chosen)
                {
                    auto const chosenKind = static_cast<patchbay::ControllerMessageKind>(chosen + 1);

                    if (static_cast<size_t>(chosen) + 1 >= patchbay::ControllerMessageKindCount || r.To.Kind == chosenKind)
                    {
                        return;
                    }

                    // A new kind starts fresh, on the same channel and in the same format.
                    auto fresh = patchbay::ControllerMessageFor(chosenKind);
                    auto const maximum = patchbay::ControllerMessageNumberMaximum(chosenKind);

                    fresh.Channel = r.To.Channel;
                    fresh.Midi1Protocol = r.To.Midi1Protocol;

                    if (maximum != 0 && patchbay::ControllerMessageNumberMaximum(r.To.Kind) != 0)
                    {
                        fresh.Number = (std::min)(r.To.Number, maximum);
                    }

                    r.To = fresh;
                });

            auto const channelKind = patchbay::IsChannelMessageKind(to.Kind);

            if (channelKind)
            {
                auto channel = Choices(resources::GetString(L"GeneratorChannel"), ChannelItems(L"ConverterSameChannel"),
                    row.SameChannel ? 0 : static_cast<int32_t>(to.Channel & 0x0F) + 1, id + L"ToChannel");

                OnChoice(channel, change, nullptr, index, [](patchbay::ConverterRow& r, int32_t chosen)
                    {
                        r.SameChannel = chosen <= 0;

                        if (chosen > 0)
                        {
                            r.To.Channel = static_cast<uint8_t>((std::min)(chosen - 1, 15));
                        }
                    });

                host.Children().Append(Pair(kind, channel));
            }
            else
            {
                host.Children().Append(kind);
            }

            auto const setNumber = [change, index](uint32_t number)
                {
                    EditRow(change, index, [number](patchbay::ConverterRow& r) { r.To.Number = number; });
                };

            // ---- the program a program change sends
            if (to.Kind == patchbay::ControllerMessageKind::ProgramChange)
            {
                auto step = Choices(resources::GetString(L"ControllerProgramStepHeader"),
                    { resources::GetString(L"ControllerProgramThis"), resources::GetString(L"ControllerProgramNext"),
                      resources::GetString(L"ControllerProgramPrevious") },
                    static_cast<int32_t>(to.Step), id + L"Step");

                OnChoice(step, change, refresh, index, [](patchbay::ConverterRow& r, int32_t chosen)
                    {
                        if (chosen <= static_cast<int32_t>(patchbay::ProgramStep::Previous))
                        {
                            r.To.Step = static_cast<patchbay::ProgramStep>(chosen);
                        }
                    });

                host.Children().Append(step);

                if (to.Step != patchbay::ProgramStep::This)
                {
                    auto const edge = [change, index](bool isLowest, uint32_t program)
                        {
                            EditRow(change, index, [isLowest, program](patchbay::ConverterRow& r)
                                {
                                    (isLowest ? r.To.LowestProgram : r.To.HighestProgram) = static_cast<uint8_t>(program);
                                });
                        };

                    host.Children().Append(Pair(
                        ProgramBox(resources::GetString(L"ControllerLowestProgram"), to.LowestProgram, id + L"LowestProgram",
                            [edge](uint32_t program) { edge(true, program); }),
                        ProgramBox(resources::GetString(L"ControllerHighestProgram"), to.HighestProgram, id + L"HighestProgram",
                            [edge](uint32_t program) { edge(false, program); })));

                    host.Children().Append(CommittedCheck(resources::GetString(L"ControllerWraps"), to.Wraps, id + L"Wraps",
                        change, nullptr, index, [](patchbay::ConverterRow& r, bool on) { r.To.Wraps = on; }));

                    host.Children().Append(Hint(resources::GetString(L"ConverterProgramPlaceHint")));
                }
            }

            // ---- the number
            auto const numbered = patchbay::ControllerMessageNumberMaximum(to.Kind) != 0 &&
                (to.Kind != patchbay::ControllerMessageKind::ProgramChange || to.Step == patchbay::ProgramStep::This);

            if (numbered)
            {
                auto const canKeep = patchbay::ConverterInputHasNumber(row.From);

                if (canKeep)
                {
                    host.Children().Append(CommittedCheck(resources::GetString(L"ConverterSameNumber"), row.SameNumber, id + L"SameNumber",
                        change, refresh, index, [](patchbay::ConverterRow& r, bool on)
                        {
                            r.SameNumber = on;

                            // Start from the number the row listens for, so a note-to-controller row reads the same.
                            if (!on)
                            {
                                r.To.Number = (std::min)(static_cast<uint32_t>((std::min)(r.Lowest, r.Highest)),
                                    patchbay::ControllerMessageNumberMaximum(r.To.Kind));
                            }
                        }));
                }

                if (!canKeep || !row.SameNumber)
                {
                    switch (to.Kind)
                    {
                    case patchbay::ControllerMessageKind::Note:
                    case patchbay::ControllerMessageKind::PolyPressure:
                        host.Children().Append(NoteBox(resources::GetString(L"GateNote"), to.Number, id + L"Number", setNumber));
                        break;

                    case patchbay::ControllerMessageKind::ProgramChange:
                        host.Children().Append(ProgramBox(resources::GetString(L"GateProgram"), to.Number, id + L"Number", setNumber));
                        break;

                    case patchbay::ControllerMessageKind::RegisteredController:
                    case patchbay::ControllerMessageKind::AssignableController:
                    {
                        auto const addPart = [change, index, &id](wchar_t const* headerKey, bool isBank, uint32_t number)
                            {
                                auto box = Number(resources::GetString(headerKey), 0, 127,
                                    isBank ? (number >> 7) & 0x7F : number & 0x7F, id + (isBank ? L"Bank" : L"Index"));

                                OnWholeNumber(box, 127, [change, index, isBank](uint32_t part)
                                    {
                                        EditRow(change, index, [isBank, part](patchbay::ConverterRow& r)
                                            {
                                                auto bank = (r.To.Number >> 7) & 0x7F;
                                                auto parameterIndex = r.To.Number & 0x7F;

                                                (isBank ? bank : parameterIndex) = part;
                                                r.To.Number = (bank << 7) | parameterIndex;
                                            });
                                    });

                                return box;
                            };

                        host.Children().Append(Pair(addPart(L"GeneratorBank", true, to.Number), addPart(L"GeneratorIndex", false, to.Number)));
                        break;
                    }

                    default:
                    {
                        auto number = Number(resources::GetString(L"GeneratorController"), 0, 127, (std::min)(to.Number, 127u), id + L"Number");
                        OnWholeNumber(number, 127, setNumber);

                        host.Children().Append(number);
                        break;
                    }
                    }
                }
            }

            // ---- the value
            auto const isNote = to.Kind == patchbay::ControllerMessageKind::Note;

            if (isNote || patchbay::IsValueMessageKind(to.Kind))
            {
                auto const canKeep = patchbay::ConverterInputHasValue(row.From);

                if (canKeep)
                {
                    host.Children().Append(CommittedCheck(resources::GetString(L"ConverterSameValue"), row.SameValue, id + L"SameValue",
                        change, refresh, index, [](patchbay::ConverterRow& r, bool on) { r.SameValue = on; }));
                }

                if (!canKeep || !row.SameValue)
                {
                    host.Children().Append(ValueBox(resources::GetString(isNote ? L"ControllerVelocity" : L"ControllerValue"),
                        to.ValueHundredths, scale, id + L"Value", 0, [change, index](int32_t hundredths)
                        {
                            EditRow(change, index, [hundredths](patchbay::ConverterRow& r) { r.To.ValueHundredths = hundredths; });
                        }));
                }
            }

            // ---- a message typed in
            if (to.Kind == patchbay::ControllerMessageKind::Words)
            {
                controls::TextBox words{};

                words.Header(winrt::box_value(resources::GetString(L"ControllerWordsHeader")));
                words.PlaceholderText(resources::GetString(L"ControllerWordsPlaceholder"));
                words.Text(winrt::hstring{ WordsText(to) });
                words.Tag(winrt::box_value(winrt::hstring{ id + L"Words" }));

                auto caption = Hint(resources::GetString(L"ConverterWordsHint"));

                auto const commit = [change, index, caption](controls::TextBox const& box)
                    {
                        auto const read = ReadWords(std::wstring_view{ box.Text() });

                        if (!read.has_value())
                        {
                            caption.Text(resources::GetString(L"ConverterWordsNotUnderstood"));
                            return;
                        }

                        caption.Text(resources::GetString(L"ConverterWordsHint"));

                        auto const list = *read;

                        EditRow(change, index, [&list](patchbay::ConverterRow& r)
                            {
                                r.To.WordCount = static_cast<uint8_t>(list.size());
                                r.To.Words.fill(0);
                                std::copy(list.begin(), list.end(), r.To.Words.begin());
                            });
                    };

                words.LostFocus([commit](foundation::IInspectable const& sender, auto&&)
                    {
                        if (auto const box = sender.try_as<controls::TextBox>())
                        {
                            commit(box);
                        }
                    });

                words.KeyDown([commit](foundation::IInspectable const& sender, input::KeyRoutedEventArgs const& args)
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

                host.Children().Append(words);
                host.Children().Append(caption);
            }

            // ---- the format
            if (channelKind)
            {
                if (to.Kind == patchbay::ControllerMessageKind::RegisteredController ||
                    to.Kind == patchbay::ControllerMessageKind::AssignableController)
                {
                    host.Children().Append(Hint(resources::GetString(L"GeneratorProtocolHint")));
                }
                else
                {
                    auto protocol = Choices(resources::GetString(L"GeneratorProtocol"),
                        { resources::GetString(L"ConverterSameProtocol"), resources::GetString(L"GeneratorProtocolMidi2"),
                          resources::GetString(L"GeneratorProtocolMidi1") },
                        row.SameProtocol ? 0 : (to.Midi1Protocol ? 2 : 1), id + L"Protocol");

                    OnChoice(protocol, change, nullptr, index, [](patchbay::ConverterRow& r, int32_t chosen)
                        {
                            r.SameProtocol = chosen <= 0;
                            r.To.Midi1Protocol = chosen == 2;
                        });

                    host.Children().Append(protocol);
                }
            }

            // ---- letting go
            if (patchbay::ConverterRowCanLetGo(row) && patchbay::ConverterOutputCanLetGo(to.Kind))
            {
                auto release = Choices(resources::GetString(L"ConverterReleaseHeader"),
                    { resources::GetString(isNote ? L"ConverterReleaseNoteOff" : L"ConverterReleaseValue"),
                      resources::GetString(L"ConverterReleaseNothing"), resources::GetString(L"ControllerToggles") },
                    static_cast<int32_t>(row.Release), id + L"Release");

                OnChoice(release, change, refresh, index, [](patchbay::ConverterRow& r, int32_t chosen)
                    {
                        if (chosen <= static_cast<int32_t>(patchbay::ConverterRelease::Toggle))
                        {
                            r.Release = static_cast<patchbay::ConverterRelease>(chosen);
                        }
                    });

                host.Children().Append(release);

                if (!isNote && row.Release != patchbay::ConverterRelease::Nothing)
                {
                    host.Children().Append(ValueBox(resources::GetString(L"ControllerReleaseValue"), to.ReleaseHundredths, scale,
                        id + L"ReleaseValue", 0, [change, index](int32_t hundredths)
                        {
                            EditRow(change, index, [hundredths](patchbay::ConverterRow& r) { r.To.ReleaseHundredths = hundredths; });
                        }));
                }
            }
            else if (isNote && !patchbay::ConverterRowCanLetGo(row))
            {
                host.Children().Append(Hint(resources::GetString(L"ConverterNoteEndsHint")));
            }
        }
    }

    _Use_decl_annotations_
    void MainWindow::BuildConverterStepSettings(patchbay::PatchBlock const& block, controls::StackPanel const& body) noexcept
    {
        try
        {
            controls::StackPanel host{};
            host.Spacing(10);

            body.Children().Append(host);

            FillConverterStepSettings(block, host);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the message converter's settings.")
    }

    _Use_decl_annotations_
    void MainWindow::FillConverterStepSettings(patchbay::PatchBlock const& block, controls::StackPanel const& host) noexcept
    {
        try
        {
            auto const blockId = block.Id;
            auto const& settings = block.Settings.Converter;
            auto const scale = settings.Scale;
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
                            window->FillConverterStepSettings(*current, panel);

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

            auto const& rows = settings.Rows;

            if (!rows.empty())
            {
                auto box = Choices(resources::GetString(L"TransformValueScale"),
                    { resources::GetString(L"TransformValueScaleSevenBit"), resources::GetString(L"TransformValueScalePercent") },
                    scale == patchbay::ValueScale::Percent ? 1 : 0, L"scale");

                box.SelectionChanged([change, refresh](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const index = SelectedIndexOf(sender);

                        if (index < 0)
                        {
                            return;
                        }

                        auto const value = index == 1 ? patchbay::ValueScale::Percent : patchbay::ValueScale::SevenBit;

                        if (change([value](patchbay::BlockSettings& s) { s.Converter.Scale = value; }))
                        {
                            refresh(L"scale");
                        }
                    });

                host.Children().Append(box);
            }

            for (size_t i = 0; i < rows.size(); i++)
            {
                auto const id = L"row" + std::to_wstring(i);

                controls::StackPanel entry{};
                entry.Spacing(8);

                controls::Grid heading{};
                heading.ColumnDefinitions().Append(controls::ColumnDefinition{});

                controls::ColumnDefinition removeColumn{};
                removeColumn.Width(xaml::GridLengthHelper::Auto());
                heading.ColumnDefinitions().Append(removeColumn);

                auto title = Heading(resources::FormatString(L"ConverterRowHeadingFormat", static_cast<int>(i) + 1));
                title.VerticalAlignment(xaml::VerticalAlignment::Center);
                heading.Children().Append(title);

                controls::HyperlinkButton remove{};
                remove.Content(winrt::box_value(resources::GetString(L"TransformRemove")));
                remove.Padding(xaml::ThicknessHelper::FromLengths(6, 2, 6, 2));
                controls::Grid::SetColumn(remove, 1);

                xaml::Automation::AutomationProperties::SetName(remove,
                    resources::FormatString(L"ConverterRemoveRowFormat", static_cast<int>(i) + 1));

                remove.Click([change, refresh, i](auto&&, auto&&)
                    {
                        if (change([i](patchbay::BlockSettings& s)
                            {
                                if (i < s.Converter.Rows.size())
                                {
                                    s.Converter.Rows.erase(s.Converter.Rows.begin() + static_cast<ptrdiff_t>(i));
                                }
                            }))
                        {
                            refresh(L"addRow");
                        }
                    });

                heading.Children().Append(remove);
                entry.Children().Append(heading);

                auto summary = Hint(patchbay::DescribeConverterRow(rows[i]));
                entry.Children().Append(summary);

                // Keeps the row's own line current as its fields change.
                Change const rowChange = [change, i, weakSummary = winrt::make_weak(summary)](
                    std::function<void(patchbay::BlockSettings&)> const& apply) -> std::optional<patchbay::BlockSettings>
                    {
                        auto const changed = change(apply);

                        if (auto const line = weakSummary.get(); line != nullptr && changed && i < changed->Converter.Rows.size())
                        {
                            line.Text(patchbay::DescribeConverterRow(changed->Converter.Rows[i]));
                        }

                        return changed;
                    };

                AddFrom(entry, rows[i], i, scale, rowChange, refresh, id);
                AddTo(entry, rows[i], i, scale, rowChange, refresh, id);

                host.Children().Append(entry);
            }

            controls::Button add{};
            add.Content(winrt::box_value(resources::GetString(L"ConverterAddRow")));
            add.Tag(winrt::box_value(winrt::hstring{ L"addRow" }));
            add.IsEnabled(rows.size() < patchbay::MaximumConverterRows);

            add.Click([change, refresh](auto&&, auto&&)
                {
                    if (change([](patchbay::BlockSettings& s)
                        {
                            if (s.Converter.Rows.size() < patchbay::MaximumConverterRows)
                            {
                                s.Converter.Rows.push_back(patchbay::DefaultConverterRow());
                            }
                        }))
                    {
                        refresh(L"addRow");
                    }
                });

            host.Children().Append(add);
            host.Children().Append(Hint(resources::GetString(L"ConverterRowsHint")));

            m_stepSettingsFocus = rows.empty() ? controls::Control{ add } : FindTagged(host, L"row0From");
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to fill in the message converter's settings.")
    }
}
