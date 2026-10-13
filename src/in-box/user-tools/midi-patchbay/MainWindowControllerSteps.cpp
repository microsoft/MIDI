// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The settings of the game controller steps, edited right in the inspector, and Learn. What a
// field shows depends on the message chosen above it, so a change to one of those shows the
// settings again and puts the focus back where it was.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "ControllerHub.h"
#include "HidHub.h"
#include "DialogParts.h"
#include "GeneralMidi.h"
#include "MessageText.h"
#include "StringResources.h"
#include "ThemeBrushes.h"

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

        using MessageOf = std::function<patchbay::ControllerMessage&(patchbay::BlockSettings&)>;

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

        void OnNumberChanged(_In_ controls::NumberBox const& box, _In_ std::function<void(double)> const& apply)
        {
            box.ValueChanged([apply](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                {
                    if (!std::isnan(args.NewValue()))
                    {
                        apply(args.NewValue());
                    }
                });
        }

        // A share of the whole range, shown as a percentage or as 0 to 127.
        controls::NumberBox ValueBox(
            _In_ winrt::hstring const& header,
            _In_ int32_t hundredths,
            _In_ patchbay::ValueScale scale,
            _In_ std::wstring const& tag,
            _In_ std::function<void(int32_t)> const& apply)
        {
            auto const percent = scale == patchbay::ValueScale::Percent;
            auto box = Number(header, 0, percent ? 100 : 127, patchbay::DisplayFromHundredths(hundredths, scale), tag);

            winrt::Windows::Globalization::NumberFormatting::DecimalFormatter formatter{};

            formatter.IntegerDigits(1);
            formatter.FractionDigits(percent ? 2 : 0);
            formatter.IsGrouped(false);

            box.NumberFormatter(formatter);
            box.Value(patchbay::DisplayFromHundredths(hundredths, scale));

            OnNumberChanged(box, [apply, scale](double value)
                {
                    apply(std::clamp(patchbay::HundredthsFromDisplay(value, scale), 0, patchbay::FullScaleHundredths));
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

        // Sixteen choices counted from 1 on screen and stored from 0.
        controls::ComboBox SixteenChoices(
            _In_ wchar_t const* headerKey,
            _In_ wchar_t const* itemFormat,
            _In_ int32_t selected,
            _In_ std::wstring const& tag)
        {
            std::vector<winrt::hstring> items{};

            for (int32_t i = 0; i < 16; i++)
            {
                items.push_back(resources::FormatString(itemFormat, i + 1));
            }

            return Choices(resources::GetString(headerKey), items, std::clamp(selected, 0, 15), tag);
        }

        controls::CheckBox CommittedCheck(
            _In_ winrt::hstring const& text,
            _In_ bool value,
            _In_ std::wstring const& tag,
            _In_ std::function<void(patchbay::BlockSettings&, bool)> const& apply,
            _In_ Change const& change,
            _In_ Refresh const& refresh)
        {
            auto check = Check(text, value);

            check.Tag(winrt::box_value(winrt::hstring{ tag }));

            auto const set = [apply, change, refresh, tag](bool on)
                {
                    auto const changed = change([&](patchbay::BlockSettings& s) { apply(s, on); });

                    if (changed && refresh)
                    {
                        refresh(tag);
                    }
                };

            check.Checked([set](auto&&, auto&&) { set(true); });
            check.Unchecked([set](auto&&, auto&&) { set(false); });

            return check;
        }

        // Group and channel side by side, or the group on its own for a message with no channel.
        void AddPlace(
            _In_ controls::StackPanel const& host,
            _In_ patchbay::ControllerMessage const& message,
            _In_ bool withChannel,
            _In_ MessageOf const& pick,
            _In_ Change const& change,
            _In_ std::wstring const& id)
        {
            auto group = SixteenChoices(L"GeneratorGroup", L"FilterGroupFormat", message.Group, id + L"Group");

            group.SelectionChanged([pick, change](foundation::IInspectable const& sender, auto&&)
                {
                    auto const index = SelectedIndexOf(sender);

                    if (index >= 0)
                    {
                        change([&](patchbay::BlockSettings& s) { pick(s).Group = static_cast<uint8_t>(index); });
                    }
                });

            if (!withChannel)
            {
                host.Children().Append(group);
                return;
            }

            auto channel = SixteenChoices(L"GeneratorChannel", L"FilterChannelFormat", message.Channel, id + L"Channel");

            channel.SelectionChanged([pick, change](foundation::IInspectable const& sender, auto&&)
                {
                    auto const index = SelectedIndexOf(sender);

                    if (index >= 0)
                    {
                        change([&](patchbay::BlockSettings& s) { pick(s).Channel = static_cast<uint8_t>(index); });
                    }
                });

            host.Children().Append(Pair(channel, group));
        }

        void AddProtocol(
            _In_ controls::StackPanel const& host,
            _In_ bool midi1,
            _In_ std::function<void(patchbay::BlockSettings&, bool)> const& apply,
            _In_ Change const& change,
            _In_ std::wstring const& tag)
        {
            auto box = Choices(resources::GetString(L"GeneratorProtocol"),
                { resources::GetString(L"GeneratorProtocolMidi2"), resources::GetString(L"GeneratorProtocolMidi1") },
                midi1 ? 1 : 0, tag);

            box.SelectionChanged([apply, change](foundation::IInspectable const& sender, auto&&)
                {
                    auto const index = SelectedIndexOf(sender);

                    if (index >= 0)
                    {
                        change([&](patchbay::BlockSettings& s) { apply(s, index == 1); });
                    }
                });

            host.Children().Append(box);
        }

        // Up to four words in hex, as one whole message. Nothing when they aren't.
        std::optional<std::vector<uint32_t>> ReadWords(_In_ std::wstring_view text)
        {
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

            patchbay::ControllerMessage check{};
            check.Kind = patchbay::ControllerMessageKind::Words;
            check.WordCount = static_cast<uint8_t>(words.size());
            std::copy(words.begin(), words.end(), check.Words.begin());

            // Built only when it is one whole message of its type.
            if (patchbay::BuildControllerWords(check, 0).OnCount == 0)
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

        // What a button sends when it's pressed, or held. The kind decides which fields show.
        void AddMessageControls(
            _In_ controls::StackPanel const& host,
            _In_ winrt::hstring const& header,
            _In_ patchbay::ControllerMessage const& message,
            _In_ patchbay::ValueScale scale,
            _In_ MessageOf const& pick,
            _In_ Change const& change,
            _In_ Refresh const& refresh,
            _In_ std::wstring const& id)
        {
            std::vector<winrt::hstring> names{};

            for (size_t i = 0; i < patchbay::ControllerMessageKindCount; i++)
            {
                names.push_back(patchbay::DescribeControllerMessageKind(static_cast<patchbay::ControllerMessageKind>(i)));
            }

            auto const kindTag = id + L"Kind";
            auto kindBox = Choices(header, names, static_cast<int32_t>(message.Kind), kindTag);

            kindBox.SelectionChanged([pick, change, refresh, kindTag](foundation::IInspectable const& sender, auto&&)
                {
                    auto const index = SelectedIndexOf(sender);

                    if (index < 0 || static_cast<size_t>(index) >= patchbay::ControllerMessageKindCount)
                    {
                        return;
                    }

                    auto const kind = static_cast<patchbay::ControllerMessageKind>(index);

                    auto const changed = change([&](patchbay::BlockSettings& s)
                        {
                            auto& target = pick(s);

                            if (target.Kind == kind)
                            {
                                return;
                            }

                            // A new kind starts fresh, in the same place.
                            auto fresh = patchbay::ControllerMessageFor(kind);

                            fresh.Group = target.Group;
                            fresh.Channel = target.Channel;
                            fresh.Midi1Protocol = target.Midi1Protocol;

                            target = fresh;
                        });

                    if (changed)
                    {
                        refresh(kindTag);
                    }
                });

            host.Children().Append(kindBox);

            auto const kind = message.Kind;
            auto const valueKind = patchbay::IsValueMessageKind(kind);
            auto const parameter = kind == patchbay::ControllerMessageKind::RegisteredController ||
                kind == patchbay::ControllerMessageKind::AssignableController;

            auto const setValue = [pick, change](int32_t hundredths)
                {
                    change([&](patchbay::BlockSettings& s) { pick(s).ValueHundredths = hundredths; });
                };

            auto const setNumber = [pick, change](uint32_t number)
                {
                    change([&](patchbay::BlockSettings& s) { pick(s).Number = number; });
                };

            switch (kind)
            {
            case patchbay::ControllerMessageKind::Note:
            {
                host.Children().Append(Pair(
                    NoteBox(resources::GetString(L"GateNote"), message.Number, id + L"Number", setNumber),
                    ValueBox(resources::GetString(L"ControllerVelocity"), message.ValueHundredths, scale, id + L"Value", setValue)));

                host.Children().Append(CommittedCheck(resources::GetString(L"ControllerNoteOff"), message.SendsRelease, id + L"Release",
                    [pick](patchbay::BlockSettings& s, bool on) { pick(s).SendsRelease = on; }, change, nullptr));
                break;
            }

            case patchbay::ControllerMessageKind::ControlChange:
            {
                auto number = Number(resources::GetString(L"GeneratorController"), 0, 127, (std::min)(message.Number, 127u), id + L"Number");

                OnNumberChanged(number, [setNumber](double value)
                    {
                        setNumber(static_cast<uint32_t>(std::clamp(std::lround(value), 0L, 127L)));
                    });

                host.Children().Append(Pair(number,
                    ValueBox(resources::GetString(L"ControllerValue"), message.ValueHundredths, scale, id + L"Value", setValue)));
                break;
            }

            case patchbay::ControllerMessageKind::PolyPressure:
                host.Children().Append(Pair(
                    NoteBox(resources::GetString(L"GateNote"), message.Number, id + L"Number", setNumber),
                    ValueBox(resources::GetString(L"ControllerValue"), message.ValueHundredths, scale, id + L"Value", setValue)));
                break;

            case patchbay::ControllerMessageKind::PitchBend:
            case patchbay::ControllerMessageKind::ChannelPressure:
                host.Children().Append(ValueBox(resources::GetString(L"ControllerValue"), message.ValueHundredths, scale, id + L"Value", setValue));
                break;

            case patchbay::ControllerMessageKind::RegisteredController:
            case patchbay::ControllerMessageKind::AssignableController:
            {
                auto const addPart = [pick, change, &id](wchar_t const* headerKey, bool isBank, uint32_t number)
                    {
                        auto box = Number(resources::GetString(headerKey), 0, 127, isBank ? (number >> 7) & 0x7F : number & 0x7F,
                            id + (isBank ? L"Bank" : L"Index"));

                        OnNumberChanged(box, [pick, change, isBank](double value)
                            {
                                auto const part = static_cast<uint32_t>(std::clamp(std::lround(value), 0L, 127L));

                                change([&](patchbay::BlockSettings& s)
                                    {
                                        auto& target = pick(s);
                                        auto bank = (target.Number >> 7) & 0x7F;
                                        auto index = target.Number & 0x7F;

                                        (isBank ? bank : index) = part;
                                        target.Number = (bank << 7) | index;
                                    });
                            });

                        return box;
                    };

                host.Children().Append(Pair(addPart(L"GeneratorBank", true, message.Number), addPart(L"GeneratorIndex", false, message.Number)));
                host.Children().Append(ValueBox(resources::GetString(L"ControllerValue"), message.ValueHundredths, scale, id + L"Value", setValue));
                break;
            }

            case patchbay::ControllerMessageKind::ProgramChange:
            {
                auto const stepTag = id + L"Step";
                auto step = Choices(resources::GetString(L"ControllerProgramStepHeader"),
                    { resources::GetString(L"ControllerProgramThis"), resources::GetString(L"ControllerProgramNext"),
                      resources::GetString(L"ControllerProgramPrevious") },
                    static_cast<int32_t>(message.Step), stepTag);

                step.SelectionChanged([pick, change, refresh, stepTag](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const index = SelectedIndexOf(sender);

                        if (index < 0 || index > static_cast<int32_t>(patchbay::ProgramStep::Previous))
                        {
                            return;
                        }

                        if (change([&](patchbay::BlockSettings& s) { pick(s).Step = static_cast<patchbay::ProgramStep>(index); }))
                        {
                            refresh(stepTag);
                        }
                    });

                host.Children().Append(step);

                if (message.Step == patchbay::ProgramStep::This)
                {
                    host.Children().Append(ProgramBox(resources::GetString(L"GateProgram"), message.Number, id + L"Number", setNumber));
                    break;
                }

                auto const addEnd = [pick, change](winrt::hstring const& header, uint8_t value, std::wstring const& tag, bool isLowest)
                    {
                        return ProgramBox(header, value, tag, [pick, change, isLowest](uint32_t program)
                            {
                                change([&](patchbay::BlockSettings& s)
                                    {
                                        (isLowest ? pick(s).LowestProgram : pick(s).HighestProgram) = static_cast<uint8_t>(program);
                                    });
                            });
                    };

                host.Children().Append(Pair(
                    addEnd(resources::GetString(L"ControllerLowestProgram"), message.LowestProgram, id + L"Lowest", true),
                    addEnd(resources::GetString(L"ControllerHighestProgram"), message.HighestProgram, id + L"Highest", false)));

                host.Children().Append(CommittedCheck(resources::GetString(L"ControllerWraps"), message.Wraps, id + L"Wraps",
                    [pick](patchbay::BlockSettings& s, bool on) { pick(s).Wraps = on; }, change, nullptr));

                host.Children().Append(Hint(resources::GetString(L"ControllerProgramShareHint")));
                break;
            }

            case patchbay::ControllerMessageKind::Words:
            {
                controls::TextBox words{};

                words.Header(winrt::box_value(resources::GetString(L"ControllerWordsHeader")));
                words.PlaceholderText(resources::GetString(L"ControllerWordsPlaceholder"));
                words.Text(winrt::hstring{ WordsText(message) });
                words.Tag(winrt::box_value(winrt::hstring{ id + L"Words" }));

                auto caption = Hint(resources::GetString(L"ControllerWordsHint"));

                auto const commit = [pick, change, caption](controls::TextBox const& box)
                    {
                        auto const read = ReadWords(std::wstring_view{ box.Text() });

                        if (!read.has_value())
                        {
                            caption.Text(resources::GetString(L"ControllerWordsNotUnderstood"));
                            return;
                        }

                        caption.Text(resources::GetString(L"ControllerWordsHint"));

                        auto const list = *read;

                        change([&](patchbay::BlockSettings& s)
                            {
                                auto& target = pick(s);

                                target.WordCount = static_cast<uint8_t>(list.size());
                                target.Words.fill(0);
                                std::copy(list.begin(), list.end(), target.Words.begin());
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
                return;
            }

            case patchbay::ControllerMessageKind::Start:
            case patchbay::ControllerMessageKind::Continue:
            case patchbay::ControllerMessageKind::Stop:
                AddPlace(host, message, false, pick, change, id);
                return;

            default:
                return;
            }

            // What letting go sends, for a value.
            if (valueKind)
            {
                host.Children().Append(CommittedCheck(resources::GetString(L"ControllerSendRelease"), message.SendsRelease, id + L"Release",
                    [pick](patchbay::BlockSettings& s, bool on) { pick(s).SendsRelease = on; }, change, refresh));

                if (message.SendsRelease)
                {
                    host.Children().Append(ValueBox(resources::GetString(L"ControllerReleaseValue"), message.ReleaseHundredths, scale,
                        id + L"ReleaseValue", [pick, change](int32_t hundredths)
                        {
                            change([&](patchbay::BlockSettings& s) { pick(s).ReleaseHundredths = hundredths; });
                        }));
                }
            }

            AddPlace(host, message, true, pick, change, id);

            if (parameter)
            {
                host.Children().Append(Hint(resources::GetString(L"GeneratorProtocolHint")));
                return;
            }

            AddProtocol(host, message.Midi1Protocol, [pick](patchbay::BlockSettings& s, bool midi1) { pick(s).Midi1Protocol = midi1; },
                change, id + L"Protocol");
        }

        bool ShowsValues(_In_ patchbay::ControllerMessage const& message) noexcept
        {
            return message.Kind == patchbay::ControllerMessageKind::Note || patchbay::IsValueMessageKind(message.Kind);
        }

        void AddScaleChoice(
            _In_ controls::StackPanel const& host,
            _In_ patchbay::ValueScale scale,
            _In_ std::function<patchbay::ValueScale&(patchbay::BlockSettings&)> const& pick,
            _In_ Change const& change,
            _In_ Refresh const& refresh)
        {
            auto box = Choices(resources::GetString(L"TransformValueScale"),
                { resources::GetString(L"TransformValueScaleSevenBit"), resources::GetString(L"TransformValueScalePercent") },
                scale == patchbay::ValueScale::Percent ? 1 : 0, L"scale");

            box.SelectionChanged([pick, change, refresh](foundation::IInspectable const& sender, auto&&)
                {
                    auto const index = SelectedIndexOf(sender);

                    if (index < 0)
                    {
                        return;
                    }

                    auto const value = index == 1 ? patchbay::ValueScale::Percent : patchbay::ValueScale::SevenBit;

                    if (change([&](patchbay::BlockSettings& s) { pick(s) = value; }))
                    {
                        refresh(L"scale");
                    }
                });

            host.Children().Append(box);
        }

        void AddButtonSettings(
            _In_ controls::StackPanel const& host,
            _In_ patchbay::ControllerButtonSettings const& button,
            _In_ Change const& change,
            _In_ Refresh const& refresh)
        {
            auto const scale = button.Scale;

            if (ShowsValues(button.Pressed) || (button.SendsWhenHeld() && ShowsValues(button.Held)))
            {
                AddScaleChoice(host, scale, [](patchbay::BlockSettings& s) -> patchbay::ValueScale& { return s.ControllerButton.Scale; },
                    change, refresh);
            }

            AddMessageControls(host, resources::GetString(L"ControllerWhenPressed"), button.Pressed, scale,
                [](patchbay::BlockSettings& s) -> patchbay::ControllerMessage& { return s.ControllerButton.Pressed; },
                change, refresh, L"pressed");

            host.Children().Append(CommittedCheck(resources::GetString(L"ControllerToggles"), button.Toggles, L"toggles",
                [](patchbay::BlockSettings& s, bool on) { s.ControllerButton.Toggles = on; }, change, refresh));

            // Each press switches it, so there is no holding it.
            if (button.Toggles)
            {
                return;
            }

            AddMessageControls(host, resources::GetString(L"ControllerWhenHeld"), button.Held, scale,
                [](patchbay::BlockSettings& s) -> patchbay::ControllerMessage& { return s.ControllerButton.Held; },
                change, refresh, L"held");

            if (button.Held.Kind == patchbay::ControllerMessageKind::Nothing)
            {
                return;
            }

            auto hold = Number(resources::GetString(L"ControllerHoldMilliseconds"),
                patchbay::MinimumHoldMilliseconds, patchbay::MaximumHoldMilliseconds, button.HoldMilliseconds, L"holdTime");

            hold.LargeChange(100);

            OnNumberChanged(hold, [change](double value)
                {
                    auto const milliseconds = static_cast<int32_t>(std::clamp(std::lround(value),
                        static_cast<long>(patchbay::MinimumHoldMilliseconds), static_cast<long>(patchbay::MaximumHoldMilliseconds)));

                    change([&](patchbay::BlockSettings& s) { s.ControllerButton.HoldMilliseconds = milliseconds; });
                });

            host.Children().Append(hold);

            host.Children().Append(CommittedCheck(resources::GetString(L"ControllerRepeats"), button.Repeats, L"repeats",
                [](patchbay::BlockSettings& s, bool on) { s.ControllerButton.Repeats = on; }, change, refresh));

            if (button.Repeats)
            {
                auto every = Number(resources::GetString(L"ControllerRepeatMilliseconds"),
                    patchbay::MinimumRepeatMilliseconds, patchbay::MaximumRepeatMilliseconds, button.RepeatMilliseconds, L"repeatTime");

                every.LargeChange(100);

                OnNumberChanged(every, [change](double value)
                    {
                        auto const milliseconds = static_cast<int32_t>(std::clamp(std::lround(value),
                            static_cast<long>(patchbay::MinimumRepeatMilliseconds), static_cast<long>(patchbay::MaximumRepeatMilliseconds)));

                        change([&](patchbay::BlockSettings& s) { s.ControllerButton.RepeatMilliseconds = milliseconds; });
                    });

                host.Children().Append(every);
            }

            host.Children().Append(CommittedCheck(resources::GetString(L"ControllerTapOrHold"), button.TapOrHold, L"tapOrHold",
                [](patchbay::BlockSettings& s, bool on) { s.ControllerButton.TapOrHold = on; }, change, nullptr));

            host.Children().Append(Hint(resources::GetString(L"ControllerTapOrHoldHint")));
        }

        void AddAxisSettings(
            _In_ controls::StackPanel const& host,
            _In_ patchbay::ControllerAxisSettings const& axis,
            _In_ Change const& change,
            _In_ Refresh const& refresh)
        {
            auto const& target = axis.Target;
            auto const scale = axis.Scale;

            // ------------------------------------------------------------ what it sends
            {
                std::vector<winrt::hstring> names{};
                int32_t selected{ 0 };

                for (size_t i = 0; i < std::size(midiapp::AllValueMessageKinds); i++)
                {
                    names.push_back(patchbay::DescribeValueMessageKind(midiapp::AllValueMessageKinds[i]));

                    if (midiapp::AllValueMessageKinds[i] == target.Kind)
                    {
                        selected = static_cast<int32_t>(i);
                    }
                }

                auto kind = Choices(resources::GetString(L"GeneratorMessage"), names, selected, L"axisKind");

                kind.SelectionChanged([change, refresh](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const index = SelectedIndexOf(sender);

                        if (index < 0 || static_cast<size_t>(index) >= std::size(midiapp::AllValueMessageKinds))
                        {
                            return;
                        }

                        auto const chosen = midiapp::AllValueMessageKinds[static_cast<size_t>(index)];

                        auto const changed = change([&](patchbay::BlockSettings& s)
                            {
                                auto& settings = s.ControllerAxis.Target;

                                if (settings.Kind == chosen)
                                {
                                    return;
                                }

                                settings.Kind = chosen;
                                settings.Number = chosen == midiapp::ValueMessageKind::ControlChange ? 1u
                                    : chosen == midiapp::ValueMessageKind::PolyPressure ? 60u
                                    : 0u;
                            });

                        if (changed)
                        {
                            refresh(L"axisKind");
                        }
                    });

                host.Children().Append(kind);

                auto const setNumber = [change](uint32_t number)
                    {
                        change([&](patchbay::BlockSettings& s) { s.ControllerAxis.Target.Number = number; });
                    };

                switch (target.Kind)
                {
                case midiapp::ValueMessageKind::ControlChange:
                {
                    auto number = Number(resources::GetString(L"GeneratorController"), 0, 127, (std::min)(target.Number, 127u), L"axisNumber");

                    OnNumberChanged(number, [setNumber](double value)
                        {
                            setNumber(static_cast<uint32_t>(std::clamp(std::lround(value), 0L, 127L)));
                        });

                    host.Children().Append(number);
                    break;
                }

                case midiapp::ValueMessageKind::PolyPressure:
                    host.Children().Append(NoteBox(resources::GetString(L"GateNote"), target.Number, L"axisNumber", setNumber));
                    break;

                case midiapp::ValueMessageKind::RegisteredController:
                case midiapp::ValueMessageKind::AssignableController:
                {
                    auto const addPart = [change](wchar_t const* headerKey, bool isBank, uint32_t number)
                        {
                            auto box = Number(resources::GetString(headerKey), 0, 127, isBank ? (number >> 7) & 0x7F : number & 0x7F,
                                isBank ? L"axisBank" : L"axisIndex");

                            OnNumberChanged(box, [change, isBank](double value)
                                {
                                    auto const part = static_cast<uint32_t>(std::clamp(std::lround(value), 0L, 127L));

                                    change([&](patchbay::BlockSettings& s)
                                        {
                                            auto& settings = s.ControllerAxis.Target;
                                            auto bank = (settings.Number >> 7) & 0x7F;
                                            auto index = settings.Number & 0x7F;

                                            (isBank ? bank : index) = part;
                                            settings.Number = (bank << 7) | index;
                                        });
                                });

                            return box;
                        };

                    host.Children().Append(Pair(addPart(L"GeneratorBank", true, target.Number), addPart(L"GeneratorIndex", false, target.Number)));
                    break;
                }

                default:
                    break;
                }

                auto channel = SixteenChoices(L"GeneratorChannel", L"FilterChannelFormat", target.Channel, L"axisChannel");

                channel.SelectionChanged([change](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const index = SelectedIndexOf(sender);

                        if (index >= 0)
                        {
                            change([&](patchbay::BlockSettings& s) { s.ControllerAxis.Target.Channel = static_cast<uint8_t>(index); });
                        }
                    });

                auto group = SixteenChoices(L"GeneratorGroup", L"FilterGroupFormat", target.Group, L"axisGroup");

                group.SelectionChanged([change](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const index = SelectedIndexOf(sender);

                        if (index >= 0)
                        {
                            change([&](patchbay::BlockSettings& s) { s.ControllerAxis.Target.Group = static_cast<uint8_t>(index); });
                        }
                    });

                host.Children().Append(Pair(channel, group));

                auto const parameter = target.Kind == midiapp::ValueMessageKind::RegisteredController ||
                    target.Kind == midiapp::ValueMessageKind::AssignableController;

                if (!parameter)
                {
                    AddProtocol(host, target.Midi1Protocol,
                        [](patchbay::BlockSettings& s, bool midi1) { s.ControllerAxis.Target.Midi1Protocol = midi1; }, change, L"axisProtocol");
                }

                host.Children().Append(Hint(resources::GetString(L"GeneratorProtocolHint")));
            }

            // ------------------------------------------------------------ the shape
            host.Children().Append(Heading(resources::GetString(L"ControllerShapeHeader")));

            AddScaleChoice(host, scale, [](patchbay::BlockSettings& s) -> patchbay::ValueScale& { return s.ControllerAxis.Scale; },
                change, refresh);

            auto const rests = patchbay::AxisRest(axis) != patchbay::ControlRest::None;

            if (rests)
            {
                auto deadZone = Number(resources::GetString(L"ControllerDeadZone"), 0,
                    patchbay::MaximumDeadZoneHundredths / 100.0, axis.DeadZoneHundredths / 100.0, L"deadZone");

                OnNumberChanged(deadZone, [change](double value)
                    {
                        auto const hundredths = std::clamp(static_cast<int32_t>(std::lround(value * 100.0)), 0, patchbay::MaximumDeadZoneHundredths);

                        change([&](patchbay::BlockSettings& s) { s.ControllerAxis.DeadZoneHundredths = hundredths; });
                    });

                host.Children().Append(deadZone);
                host.Children().Append(Hint(resources::GetString(L"ControllerDeadZoneHint")));
            }

            auto const& shape = axis.Shape;

            auto const shapeBox = [change, scale](wchar_t const* headerKey, int32_t hundredths, std::wstring const& tag,
                std::function<int32_t&(patchbay::ValueShape&)> const& field)
                {
                    return ValueBox(resources::GetString(headerKey), hundredths, scale, tag, [change, field](int32_t value)
                        {
                            change([&](patchbay::BlockSettings& s) { field(s.ControllerAxis.Shape) = value; });
                        });
                };

            host.Children().Append(Pair(
                shapeBox(L"TransformShapeInputLow", shape.InputMinimumHundredths, L"inputLow",
                    [](patchbay::ValueShape& s) -> int32_t& { return s.InputMinimumHundredths; }),
                shapeBox(L"TransformShapeInputHigh", shape.InputMaximumHundredths, L"inputHigh",
                    [](patchbay::ValueShape& s) -> int32_t& { return s.InputMaximumHundredths; })));

            host.Children().Append(Pair(
                shapeBox(L"TransformShapeOutputLow", shape.OutputMinimumHundredths, L"outputLow",
                    [](patchbay::ValueShape& s) -> int32_t& { return s.OutputMinimumHundredths; }),
                shapeBox(L"TransformShapeOutputHigh", shape.OutputMaximumHundredths, L"outputHigh",
                    [](patchbay::ValueShape& s) -> int32_t& { return s.OutputMaximumHundredths; })));

            auto curve = Choices(resources::GetString(L"TransformShapeCurve"),
                { resources::GetString(L"TransformShapeCurveLinear"), resources::GetString(L"TransformShapeCurveSlowRise"),
                  resources::GetString(L"TransformShapeCurveFastRise") },
                static_cast<int32_t>(shape.Curve), L"curve");

            curve.SelectionChanged([change](foundation::IInspectable const& sender, auto&&)
                {
                    auto const index = SelectedIndexOf(sender);

                    if (index >= 0 && index <= static_cast<int32_t>(patchbay::ValueCurve::FastRise))
                    {
                        change([&](patchbay::BlockSettings& s) { s.ControllerAxis.Shape.Curve = static_cast<patchbay::ValueCurve>(index); });
                    }
                });

            host.Children().Append(curve);

            host.Children().Append(CommittedCheck(resources::GetString(L"TransformShapeInvert"), shape.Invert, L"invert",
                [](patchbay::BlockSettings& s, bool on) { s.ControllerAxis.Shape.Invert = on; }, change, nullptr));

            // ------------------------------------------------------------ sending
            host.Children().Append(Heading(resources::GetString(L"ControllerSendingHeader")));

            auto interval = Number(resources::GetString(L"ControllerInterval"), 0,
                patchbay::MaximumAxisIntervalMilliseconds, axis.IntervalMilliseconds, L"interval");

            OnNumberChanged(interval, [change](double value)
                {
                    auto const milliseconds = static_cast<int32_t>(std::clamp(std::lround(value), 0L,
                        static_cast<long>(patchbay::MaximumAxisIntervalMilliseconds)));

                    change([&](patchbay::BlockSettings& s) { s.ControllerAxis.IntervalMilliseconds = milliseconds; });
                });

            host.Children().Append(interval);
            host.Children().Append(Hint(resources::GetString(L"ControllerIntervalHint")));

            if (rests)
            {
                host.Children().Append(CommittedCheck(resources::GetString(L"ControllerReturnToRest"), axis.ReturnsToRest, L"returnToRest",
                    [](patchbay::BlockSettings& s, bool on) { s.ControllerAxis.ReturnsToRest = on; }, change, nullptr));
            }
        }
    }

    _Use_decl_annotations_
    void MainWindow::BuildControllerStepSettings(patchbay::PatchBlock const& block, controls::StackPanel const& body) noexcept
    {
        try
        {
            controls::StackPanel host{};
            host.Spacing(10);

            body.Children().Append(host);

            m_controllerSettingsHost = winrt::make_weak(host);

            FillControllerStepSettings(block, host);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the game controller step's settings.")
    }

    _Use_decl_annotations_
    void MainWindow::FillControllerStepSettings(patchbay::PatchBlock const& block, controls::StackPanel const& host) noexcept
    {
        try
        {
            auto const blockId = block.Id;
            auto const& settings = block.Settings;
            auto const isButton = block.Kind == patchbay::BlockKind::ControllerButton;
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
                            window->FillControllerStepSettings(*current, panel);

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

            auto const player = isButton ? settings.ControllerButton.Player : settings.ControllerAxis.Player;
            auto const control = isButton ? settings.ControllerButton.Control : settings.ControllerAxis.Control;
            auto const readsDevice = isButton ? settings.ControllerButton.ReadsDevice() : settings.ControllerAxis.ReadsDevice();
            auto const& device = isButton ? settings.ControllerButton.Device : settings.ControllerAxis.Device;
            auto const deviceControl = isButton ? settings.ControllerButton.DeviceControl : settings.ControllerAxis.DeviceControl;
            auto const learning = !m_controllerLearnBlockId.empty() && m_controllerLearnBlockId == blockId;

            auto const present = patchbay::HidHub::Devices();

            std::vector<patchbay::HidDeviceId> presentIds{};

            for (auto const& info : present)
            {
                presentIds.push_back(info.Id);
            }

            auto const chosenDevice = readsDevice ? patchbay::ChooseHidDevice(presentIds, device) : -1;

            // ------------------------------------------------------------ which controller
            {
                struct Source
                {
                    uint8_t Player{ 0 };
                    patchbay::HidDeviceInfo Info{};
                };

                std::vector<Source> sources{};
                std::vector<winrt::hstring> names{};
                int32_t selected{ readsDevice ? -1 : static_cast<int32_t>(player) };

                for (uint8_t i = 0; i < patchbay::ControllerPlayerCount; i++)
                {
                    sources.push_back(Source{ i, {} });
                    names.push_back(patchbay::DescribeControllerPlayer(i));
                }

                for (size_t i = 0; i < present.size(); i++)
                {
                    auto name = patchbay::DescribeControllerDevice(present[i].Id);

                    // Two of the same are numbered in the order of their USB ports.
                    auto const same = std::count_if(present.begin(), present.begin() + static_cast<ptrdiff_t>(i),
                        [&present, i](patchbay::HidDeviceInfo const& other) { return other.Id.Name == present[i].Id.Name; });

                    if (same > 0)
                    {
                        name = resources::FormatString(L"ControllerDeviceNumberFormat", name, static_cast<int>(same) + 1);
                    }

                    if (static_cast<int32_t>(i) == chosenDevice)
                    {
                        selected = static_cast<int32_t>(sources.size());
                    }

                    sources.push_back(Source{ 0, present[i] });
                    names.push_back(name);
                }

                if (readsDevice && chosenDevice < 0)
                {
                    patchbay::HidDeviceInfo missing{};
                    missing.Id = device;

                    selected = static_cast<int32_t>(sources.size());
                    sources.push_back(Source{ 0, missing });
                    names.push_back(resources::FormatString(L"ControllerDeviceMissingSuffixFormat", patchbay::DescribeControllerDevice(device)));
                }

                auto playerBox = Choices(resources::GetString(L"ControllerPlayerHeader"), names, selected, L"player");

                playerBox.SelectionChanged([change, refresh, isButton, sources](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const index = SelectedIndexOf(sender);

                        if (index < 0 || static_cast<size_t>(index) >= sources.size())
                        {
                            return;
                        }

                        auto const& source = sources[static_cast<size_t>(index)];

                        auto const changed = change([&](patchbay::BlockSettings& s)
                            {
                                auto& chosenPlayer = isButton ? s.ControllerButton.Player : s.ControllerAxis.Player;
                                auto& chosen = isButton ? s.ControllerButton.Device : s.ControllerAxis.Device;
                                auto& chosenControl = isButton ? s.ControllerButton.DeviceControl : s.ControllerAxis.DeviceControl;

                                if (source.Info.Id.IsEmpty())
                                {
                                    chosen = {};
                                    chosenPlayer = source.Player;
                                    return;
                                }

                                if (chosen == source.Info.Id)
                                {
                                    return;
                                }

                                chosen = source.Info.Id;
                                chosenControl = isButton ? patchbay::HidButtonControl(1) : patchbay::HidAxisControl(patchbay::HidAxis::X);

                                // The first axis the device has, for a stick or trigger.
                                for (size_t a = 0; !isButton && a < patchbay::HidAxisCount; a++)
                                {
                                    if (source.Info.Axes.test(a))
                                    {
                                        chosenControl = patchbay::HidAxisControl(static_cast<patchbay::HidAxis>(a));
                                        break;
                                    }
                                }
                            });

                        // Each controller has controls of its own.
                        if (changed)
                        {
                            refresh(L"player");
                        }
                    });

                // The same Learn as the filter steps: down while it listens.
                primitives::ToggleButton learn{};

                learn.Content(winrt::box_value(resources::GetString(L"FilterLearn")));
                learn.IsChecked(learning);
                learn.Tag(winrt::box_value(winrt::hstring{ L"learn" }));
                learn.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                learn.VerticalAlignment(xaml::VerticalAlignment::Bottom);

                // Click rather than Checked, so showing the settings again does not loop back here.
                learn.Click([weak, blockId, isButton, refresh](auto&&, auto&&)
                    {
                        auto strong = weak.get();

                        if (strong == nullptr)
                        {
                            return;
                        }

                        if (!strong->m_controllerLearnBlockId.empty() && strong->m_controllerLearnBlockId == blockId)
                        {
                            strong->StopControllerLearn();
                        }
                        else
                        {
                            strong->StartControllerLearn(blockId, isButton);
                        }

                        refresh(L"learn");
                    });

                host.Children().Append(Pair(playerBox, learn));

                if (learning)
                {
                    host.Children().Append(Hint(resources::GetString(isButton ? L"ControllerLearnButtonHint" : L"ControllerLearnAxisHint")));
                }

                m_controllerStatusText = controls::TextBlock{};
                m_controllerStatusText.Text(ControllerStatusText(blockId));
                m_controllerStatusText.TextWrapping(xaml::TextWrapping::Wrap);
                m_controllerStatusText.FontSize(12);
                m_controllerStatusText.Foreground(patchbay::ThemeBrushes::Current().Get(L"TextFillColorSecondaryBrush"));

                host.Children().Append(m_controllerStatusText);
                host.Children().Append(Hint(resources::GetString(readsDevice ? L"ControllerDeviceHint" : L"ControllerPlayerHint")));
            }

            // ------------------------------------------------------------ which control
            if (readsDevice)
            {
                patchbay::HidDeviceInfo layout{};

                if (chosenDevice >= 0)
                {
                    layout = present[static_cast<size_t>(chosenDevice)];
                }
                else
                {
                    // Not plugged in: what most joysticks and gamepads have.
                    layout.ButtonCount = 32;
                    layout.HasHat = true;

                    for (auto const axis : { patchbay::HidAxis::X, patchbay::HidAxis::Y, patchbay::HidAxis::Z, patchbay::HidAxis::Rx,
                                             patchbay::HidAxis::Ry, patchbay::HidAxis::Rz, patchbay::HidAxis::Slider })
                    {
                        layout.Axes.set(static_cast<size_t>(axis));
                    }
                }

                std::vector<patchbay::HidControl> choices{};

                if (isButton)
                {
                    for (uint16_t b = 1; b <= (std::max)(layout.ButtonCount, uint16_t{ 1 }); b++)
                    {
                        choices.push_back(patchbay::HidButtonControl(b));
                    }

                    if (layout.HasHat)
                    {
                        for (auto const kind : { patchbay::HidControlKind::HatUp, patchbay::HidControlKind::HatDown,
                                                 patchbay::HidControlKind::HatLeft, patchbay::HidControlKind::HatRight })
                        {
                            choices.push_back(patchbay::HidControl{ kind, 0 });
                        }
                    }
                }

                for (size_t a = 0; a < patchbay::HidAxisCount; a++)
                {
                    if (!layout.Axes.test(a))
                    {
                        continue;
                    }

                    auto const number = static_cast<uint16_t>(a);

                    if (isButton)
                    {
                        choices.push_back(patchbay::HidControl{ patchbay::HidControlKind::AxisLow, number });
                        choices.push_back(patchbay::HidControl{ patchbay::HidControlKind::AxisHigh, number });
                    }
                    else
                    {
                        choices.push_back(patchbay::HidControl{ patchbay::HidControlKind::Axis, number });
                    }
                }

                if (std::find(choices.begin(), choices.end(), deviceControl) == choices.end())
                {
                    choices.push_back(deviceControl);
                }

                std::vector<winrt::hstring> names{};
                int32_t selected{ -1 };

                for (size_t i = 0; i < choices.size(); i++)
                {
                    names.push_back(patchbay::DescribeHidControl(choices[i]));

                    if (choices[i] == deviceControl)
                    {
                        selected = static_cast<int32_t>(i);
                    }
                }

                auto controlBox = Choices(resources::GetString(L"ControllerControlHeader"), names, selected, L"control");

                controlBox.SelectionChanged([change, isButton, choices](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const index = SelectedIndexOf(sender);

                        if (index < 0 || static_cast<size_t>(index) >= choices.size())
                        {
                            return;
                        }

                        auto const chosen = choices[static_cast<size_t>(index)];

                        change([&](patchbay::BlockSettings& s)
                            {
                                (isButton ? s.ControllerButton.DeviceControl : s.ControllerAxis.DeviceControl) = chosen;
                            });
                    });

                host.Children().Append(controlBox);
            }
            else
            {
                std::vector<patchbay::ControllerControl> choices{};
                std::vector<winrt::hstring> names{};
                int32_t selected{ -1 };

                for (size_t i = 0; i < patchbay::ControllerControlCount; i++)
                {
                    auto const candidate = static_cast<patchbay::ControllerControl>(i);

                    if (isButton ? !patchbay::CanBePressed(candidate) : !patchbay::CanSlide(candidate))
                    {
                        continue;
                    }

                    if (candidate == control)
                    {
                        selected = static_cast<int32_t>(choices.size());
                    }

                    choices.push_back(candidate);
                    names.push_back(patchbay::DescribeControllerControl(candidate));
                }

                auto controlBox = Choices(resources::GetString(L"ControllerControlHeader"), names, selected, L"control");

                controlBox.SelectionChanged([change, isButton, choices](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const index = SelectedIndexOf(sender);

                        if (index < 0 || static_cast<size_t>(index) >= choices.size())
                        {
                            return;
                        }

                        auto const chosen = choices[static_cast<size_t>(index)];

                        change([&](patchbay::BlockSettings& s)
                            {
                                (isButton ? s.ControllerButton.Control : s.ControllerAxis.Control) = chosen;
                            });
                    });

                host.Children().Append(controlBox);

                if (isButton)
                {
                    host.Children().Append(Hint(resources::GetString(L"ControllerAdaptiveHint")));
                }
            }

            if (isButton)
            {
                AddButtonSettings(host, settings.ControllerButton, change, refresh);
            }
            else
            {
                AddAxisSettings(host, settings.ControllerAxis, change, refresh);
            }

            m_stepSettingsFocus = FindTagged(host, L"player");
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the game controller step's settings.")
    }

    _Use_decl_annotations_
    void MainWindow::StartControllerLearn(std::wstring const& blockId, bool forButton) noexcept
    {
        try
        {
            StopControllerLearn();

            auto weak = get_weak();
            auto dispatcher = DispatcherQueue();

            // Whichever controller moves first wins, and the two hubs have threads of their own.
            auto found = std::make_shared<std::atomic<bool>>(false);

            // Where each controller was when Learn started, kept by the hub's thread alone.
            struct Seen
            {
                std::array<patchbay::ControllerReading, patchbay::ControllerPlayerCount> Before{};
                std::array<bool, patchbay::ControllerPlayerCount> Have{};
            };

            auto seen = std::make_shared<Seen>();

            m_controllerLearnToken = patchbay::ControllerHub::Current().Listen(patchbay::ControllerHub::AnyPlayer,
                [weak, dispatcher, blockId, forButton, seen, found](uint8_t player, uint64_t, uint64_t, patchbay::ControllerReading const& reading)
                {
                    if (found->load() || player >= patchbay::ControllerPlayerCount)
                    {
                        return;
                    }

                    if (!seen->Have[player] || !reading.Connected)
                    {
                        seen->Before[player] = reading;
                        seen->Have[player] = reading.Connected;
                        return;
                    }

                    auto const moved = patchbay::FindMovedControl(seen->Before[player], reading, forButton);

                    if (!moved.has_value() || found->exchange(true))
                    {
                        return;
                    }

                    patchbay::LearnedControl learned{};
                    learned.Player = player;
                    learned.Control = *moved;

                    dispatcher.TryEnqueue([weak, blockId, learned]()
                        {
                            if (auto strong = weak.get())
                            {
                                strong->OnControllerLearned(blockId, learned);
                            }
                        });
                });

            // Each HID game controller, by path, with where its controls rest, learned as reports arrive.
            auto devices = std::make_shared<std::vector<std::pair<std::wstring, patchbay::HidReading>>>();

            m_controllerHidLearnToken = patchbay::HidHub::Current().ListenToAll(
                [weak, dispatcher, blockId, forButton, devices, found](patchbay::HidDeviceId const& device, patchbay::HidReading const& reading)
                {
                    if (found->load())
                    {
                        return;
                    }

                    auto it = std::find_if(devices->begin(), devices->end(),
                        [&device](auto const& entry) { return entry.first == device.Path; });

                    if (it == devices->end())
                    {
                        devices->emplace_back(device.Path, reading);
                        return;
                    }

                    auto& before = it->second;

                    patchbay::SettleHidLearnBaseline(before, reading);

                    auto const moved = patchbay::FindMovedHidControl(before, reading, forButton);

                    if (!moved.has_value() || found->exchange(true))
                    {
                        return;
                    }

                    patchbay::LearnedControl learned{};
                    learned.Device = device;
                    learned.DeviceControl = *moved;

                    dispatcher.TryEnqueue([weak, blockId, learned]()
                        {
                            if (auto strong = weak.get())
                            {
                                strong->OnControllerLearned(blockId, learned);
                            }
                        });
                });

            if (m_controllerLearnToken != 0 || m_controllerHidLearnToken != 0)
            {
                m_controllerLearnBlockId = blockId;
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to start learning a game controller control.")
    }

    void MainWindow::StopControllerLearn() noexcept
    {
        if (m_controllerLearnToken != 0)
        {
            patchbay::ControllerHub::Current().StopListening(m_controllerLearnToken);
        }

        if (m_controllerHidLearnToken != 0)
        {
            patchbay::HidHub::Current().StopListening(m_controllerHidLearnToken);
        }

        m_controllerLearnToken = 0;
        m_controllerHidLearnToken = 0;
        m_controllerLearnBlockId.clear();
    }

    _Use_decl_annotations_
    void MainWindow::OnControllerLearned(std::wstring const& blockId, patchbay::LearnedControl const& learned) noexcept
    {
        try
        {
            if (m_controllerLearnBlockId.empty() || m_controllerLearnBlockId != blockId)
            {
                return;
            }

            StopControllerLearn();

            auto const* patch = CurrentPatch();
            auto const* block = patch == nullptr ? nullptr : patch->FindBlock(blockId);

            if (block == nullptr)
            {
                return;
            }

            auto const isButton = block->Kind == patchbay::BlockKind::ControllerButton;

            ChangeStepSettings(blockId, [&learned, isButton](patchbay::BlockSettings& s)
                {
                    auto& player = isButton ? s.ControllerButton.Player : s.ControllerAxis.Player;
                    auto& control = isButton ? s.ControllerButton.Control : s.ControllerAxis.Control;
                    auto& device = isButton ? s.ControllerButton.Device : s.ControllerAxis.Device;
                    auto& deviceControl = isButton ? s.ControllerButton.DeviceControl : s.ControllerAxis.DeviceControl;

                    device = learned.Device;

                    if (learned.Device.IsEmpty())
                    {
                        player = learned.Player;
                        control = learned.Control;
                    }
                    else
                    {
                        deviceControl = learned.DeviceControl;
                    }
                });

            // Looked up again: committing the change can move the patch.
            auto const panel = m_controllerSettingsHost.get();
            auto const* changed = CurrentPatch();
            auto const* current = changed == nullptr ? nullptr : changed->FindBlock(blockId);

            if (panel == nullptr || current == nullptr || !panel.IsLoaded())
            {
                return;
            }

            panel.Children().Clear();
            FillControllerStepSettings(*current, panel);

            // On the control it found, so a screen reader says which one.
            if (auto const target = FindTagged(panel, L"control"))
            {
                target.Loaded([weakTarget = winrt::make_weak(target)](auto&&, auto&&)
                    {
                        if (auto const focused = weakTarget.get())
                        {
                            focused.Focus(xaml::FocusState::Programmatic);
                        }
                    });
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to use the learned game controller control.")
    }

    _Use_decl_annotations_
    winrt::hstring MainWindow::ControllerStatusText(std::wstring const& blockId) noexcept
    {
        try
        {
            auto const* patch = CurrentPatch();
            auto const* block = patch == nullptr ? nullptr : patch->FindBlock(blockId);

            if (block == nullptr)
            {
                return {};
            }

            auto const isButton = block->Kind == patchbay::BlockKind::ControllerButton;
            auto const& device = isButton ? block->Settings.ControllerButton.Device : block->Settings.ControllerAxis.Device;

            if (!device.IsEmpty())
            {
                std::vector<patchbay::HidDeviceId> present{};

                for (auto const& info : patchbay::HidHub::Devices())
                {
                    present.push_back(info.Id);
                }

                return resources::FormatString(patchbay::ChooseHidDevice(present, device) >= 0 ? L"ControllerConnectedFormat" : L"ControllerMissingFormat",
                    patchbay::DescribeControllerDevice(device));
            }

            auto& hub = patchbay::ControllerHub::Current();

            if (!hub.IsAvailable())
            {
                return resources::GetString(L"ControllerNoXInput");
            }

            auto const player = isButton ? block->Settings.ControllerButton.Player : block->Settings.ControllerAxis.Player;

            return resources::FormatString(hub.Read(player).Connected ? L"ControllerConnectedFormat" : L"ControllerMissingFormat",
                patchbay::DescribeControllerPlayer(player));
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to describe the game controller.")

        return {};
    }
}
