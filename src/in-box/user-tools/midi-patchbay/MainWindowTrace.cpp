// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The trace: messages followed through the patch without a device, and shown on the canvas one
// at a time. Nothing is sent anywhere, so it works on a patch that isn't routing and on devices
// that aren't plugged in.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "DialogParts.h"
#include "LogicSteps.h"
#include "MessageText.h"
#include "StringResources.h"

using namespace winrt::Microsoft::UI::Xaml;

namespace patchbay = ::midipatchbay;
namespace resources = ::midipatchbay::resources;

namespace winrt::midipatchbay::implementation
{
    namespace
    {
        using patchbay::parts::Check;
        using patchbay::parts::Hint;

        // The kinds in the order the list shows them. Words is the last, after the kinds that
        // MakeTraceMessage makes.
        constexpr int32_t TraceKindWordsIndex = 6;

        // What a trace starts with when nothing has been traced yet: a note played and let go.
        std::vector<patchbay::TraceMessage> FirstTraceMessages()
        {
            return {
                patchbay::MakeTraceMessage(patchbay::TraceMessageKind::NoteOn, 0, 0, 60, 100, false),
                patchbay::MakeTraceMessage(patchbay::TraceMessageKind::NoteOff, 0, 0, 60, 0, false) };
        }

        std::wstring WordsText(_In_ patchbay::TraceMessage const& message)
        {
            std::wstring text{};

            for (uint8_t i = 0; i < message.WordCount && i < patchbay::MaximumUmpWords; i++)
            {
                wchar_t buffer[12]{};
                swprintf_s(buffer, L"%08X", message.Words[i]);

                if (!text.empty())
                {
                    text += L' ';
                }

                text += buffer;
            }

            return text;
        }

        patchbay::LogicValue PartOf(_In_ patchbay::TraceMessage const& message, _In_ patchbay::MessagePart part) noexcept
        {
            patchbay::PartPlace place{};
            place.Part = part;

            return patchbay::ReadPart(place, message.Words.data(), message.WordCount);
        }

        // "Note on C4, velocity 100, channel 1", the way a musician says it.
        winrt::hstring DescribeTraceMessage(_In_ patchbay::TraceMessage const& message)
        {
            if (message.WordCount == 0)
            {
                return {};
            }

            auto const type = message.Words[0] >> 28;

            if (type == 0x1)
            {
                return patchbay::DescribeSystemMessage(static_cast<uint8_t>((message.Words[0] >> 16) & 0xFF));
            }

            if (type != 0x2 && type != 0x4)
            {
                return resources::FormatString(L"TraceWordsFormat", WordsText(message));
            }

            auto const status = static_cast<uint8_t>((message.Words[0] >> 20) & 0x0F);
            auto const name = patchbay::DescribeChannelVoiceStatus(status);
            auto const channel = static_cast<int32_t>((message.Words[0] >> 16) & 0x0F) + 1;

            auto const seven = [&message](patchbay::MessagePart part)
                {
                    return patchbay::DescribeLogicValue(PartOf(message, part), patchbay::LogicUnit::Value, patchbay::ValueScale::SevenBit);
                };

            auto const whole = [&message](patchbay::MessagePart part)
                {
                    return patchbay::WholeOf(PartOf(message, part));
                };

            switch (status)
            {
            case 0x8:
            case 0x9:
                return resources::FormatString(L"TraceNoteFormat", name,
                    patchbay::DescribeNote(static_cast<uint8_t>(whole(patchbay::MessagePart::Note) & 0x7F)),
                    seven(patchbay::MessagePart::Velocity), channel);

            case 0xB:
                return resources::FormatString(L"TraceControlFormat", name,
                    whole(patchbay::MessagePart::ControllerNumber), seven(patchbay::MessagePart::ControllerValue), channel);

            case 0xC:
                return resources::FormatString(L"TraceNumberFormat", name, whole(patchbay::MessagePart::Program), channel);

            case 0xD:
                return resources::FormatString(L"TraceNumberFormat", name, seven(patchbay::MessagePart::Pressure), channel);

            case 0xE:
                return resources::FormatString(L"TraceNumberFormat", name,
                    patchbay::FieldValueOf(PartOf(message, patchbay::MessagePart::PitchBend), 14), channel);

            default:
                return resources::FormatString(L"TraceChannelFormat", name, channel);
            }
        }

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

        controls::ComboBox Choices(
            _In_ winrt::hstring const& header,
            _In_ std::vector<winrt::hstring> const& items,
            _In_ int32_t selected)
        {
            controls::ComboBox box{};

            box.Header(winrt::box_value(header));
            box.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

            for (auto const& item : items)
            {
                box.Items().Append(winrt::box_value(item));
            }

            box.SelectedIndex(selected);

            return box;
        }

        std::vector<winrt::hstring> Sixteen(_In_ wchar_t const* itemFormat)
        {
            std::vector<winrt::hstring> items{};

            for (int32_t i = 0; i < 16; i++)
            {
                items.push_back(resources::FormatString(itemFormat, i + 1));
            }

            return items;
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnTraceBarCloseClick(controls::InfoBar const& sender, foundation::IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ClearTrace();
    }

    _Use_decl_annotations_
    void MainWindow::OnTracePreviousClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_traceIndex > 0)
        {
            m_traceIndex--;
            ShowTraceStep();
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnTraceNextClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_traceIndex + 1 < m_trace.Steps.size())
        {
            m_traceIndex++;
            ShowTraceStep();
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnTraceAgainClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ShowTraceDialogAsync();
    }

    winrt::fire_and_forget MainWindow::ShowTraceDialogAsync()
    {
        auto strong = get_strong();

        try
        {
            auto const* patch = CurrentPatch();

            if (patch == nullptr)
            {
                co_return;
            }

            // Messages come in from an endpoint with a link out of it.
            std::vector<std::pair<std::wstring, std::wstring>> sources{};

            for (auto const& endpoint : patch->Endpoints)
            {
                auto const linked = std::any_of(patch->Connections.begin(), patch->Connections.end(),
                    [&endpoint](patchbay::PatchConnection const& link) { return link.SourceId == endpoint.Id; });

                if (linked)
                {
                    sources.emplace_back(endpoint.Id, endpoint.DisplayName);
                }
            }

            if (sources.empty())
            {
                ShowStatus(resources::GetString(L"TraceNothingToTrace"), controls::InfoBarSeverity::Informational);
                co_return;
            }

            if (m_traceMessages.empty())
            {
                m_traceMessages = FirstTraceMessages();
            }

            auto const messages = std::make_shared<std::vector<patchbay::TraceMessage>>(m_traceMessages);
            auto const content = TraceDialogContent();
            auto const dialog = TraceDialog();

            content.Children().Clear();

            auto intro = Hint(resources::GetString(L"TraceIntro"));
            intro.FontSize(12);
            content.Children().Append(intro);

            // ------------------------------------------------- where they come in
            std::vector<winrt::hstring> sourceNames{};
            int32_t selectedSource{ 0 };

            for (size_t i = 0; i < sources.size(); i++)
            {
                sourceNames.push_back(winrt::hstring{ sources[i].second });

                if (sources[i].first == m_traceSourceId)
                {
                    selectedSource = static_cast<int32_t>(i);
                }
            }

            auto from = Choices(resources::GetString(L"TraceFrom"), sourceNames, selectedSource);
            auto group = Choices(resources::GetString(L"TraceGroup"), Sixteen(L"FilterGroupFormat"), m_traceGroup);

            content.Children().Append(Pair(from, group));

            // ------------------------------------------------- the messages, in order
            content.Children().Append(patchbay::parts::Heading(resources::GetString(L"TraceMessagesHeading")));

            controls::StackPanel list{};
            list.Spacing(2);
            content.Children().Append(list);

            auto const showList = std::make_shared<std::function<void()>>();

            // Weak inside, so the list's own buttons don't keep it alive.
            *showList = [list, messages, dialog, weakShow = std::weak_ptr<std::function<void()>>{ showList }]()
                {
                    list.Children().Clear();

                    if (messages->empty())
                    {
                        list.Children().Append(Hint(resources::GetString(L"TraceNoMessages")));
                    }

                    for (size_t i = 0; i < messages->size(); i++)
                    {
                        controls::Grid row{};
                        row.ColumnDefinitions().Append(controls::ColumnDefinition{});

                        controls::ColumnDefinition removeColumn{};
                        removeColumn.Width(xaml::GridLengthHelper::Auto());
                        row.ColumnDefinitions().Append(removeColumn);

                        controls::TextBlock text{};
                        text.Text(resources::FormatString(L"TraceListItemFormat", static_cast<int32_t>(i) + 1, DescribeTraceMessage((*messages)[i])));
                        text.VerticalAlignment(xaml::VerticalAlignment::Center);
                        text.TextWrapping(xaml::TextWrapping::Wrap);
                        row.Children().Append(text);

                        controls::HyperlinkButton remove{};
                        remove.Content(winrt::box_value(resources::GetString(L"TransformRemove")));
                        remove.Padding(xaml::ThicknessHelper::FromLengths(6, 2, 6, 2));
                        controls::Grid::SetColumn(remove, 1);

                        xaml::Automation::AutomationProperties::SetName(remove,
                            resources::FormatString(L"TraceRemoveMessageFormat", static_cast<int32_t>(i) + 1));

                        remove.Click([messages, i, weakShow](auto&&, auto&&)
                            {
                                if (i < messages->size())
                                {
                                    messages->erase(messages->begin() + static_cast<std::ptrdiff_t>(i));
                                }

                                if (auto const show = weakShow.lock())
                                {
                                    (*show)();
                                }
                            });

                        row.Children().Append(remove);
                        list.Children().Append(row);
                    }

                    dialog.IsPrimaryButtonEnabled(!messages->empty());
                };

            (*showList)();

            // ------------------------------------------------- one more
            controls::StackPanel adder{};
            adder.Spacing(8);

            auto kind = Choices(resources::GetString(L"TraceKind"),
                { resources::GetString(L"TraceKindNoteOn"), resources::GetString(L"TraceKindNoteOff"),
                  resources::GetString(L"TraceKindControlChange"), resources::GetString(L"TraceKindProgramChange"),
                  resources::GetString(L"TraceKindPitchBend"), resources::GetString(L"TraceKindChannelPressure"),
                  resources::GetString(L"TraceKindWords") },
                0);

            auto channel = Choices(resources::GetString(L"TraceChannel"), Sixteen(L"FilterChannelFormat"), 0);

            adder.Children().Append(Pair(kind, channel));

            auto number = patchbay::parts::NumberBox(resources::GetString(L"TraceNote"), 0, 127, 60, 12);
            number.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

            auto value = patchbay::parts::NumberBox(resources::GetString(L"TraceVelocity"), 0, 127, 100, 10);
            value.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

            adder.Children().Append(Pair(number, value));

            auto midi2 = Check(resources::GetString(L"TraceMidi2"), false);
            adder.Children().Append(midi2);

            controls::TextBox words{};
            words.Header(winrt::box_value(resources::GetString(L"TraceWords")));
            words.PlaceholderText(L"20903C64");
            words.FontFamily(media::FontFamily{ L"Consolas" });
            adder.Children().Append(words);

            auto wordsHint = Hint(resources::GetString(L"TraceWordsHint"));
            adder.Children().Append(wordsHint);

            controls::Button add{};
            add.Content(winrt::box_value(resources::GetString(L"TraceAdd")));
            adder.Children().Append(add);

            // The fields follow the kind. Not the kind box itself, which would keep itself alive.
            auto const fitFields = [channel, number, value, midi2, words, wordsHint](int32_t index)
                {
                    auto const isWords = index == TraceKindWordsIndex;
                    auto const shows = [](bool on) { return on ? xaml::Visibility::Visible : xaml::Visibility::Collapsed; };

                    auto const numberKey = index == 2 ? L"TraceController" : index == 3 ? L"TraceProgram" : L"TraceNote";
                    auto const valueKey = index == 2 ? L"TraceValue" : index == 4 ? L"TraceBend" : index == 5 ? L"TracePressure" : L"TraceVelocity";

                    number.Header(winrt::box_value(resources::GetString(numberKey)));
                    number.Visibility(shows(index >= 0 && index <= 3));

                    auto const bend = index == 4;

                    value.Header(winrt::box_value(resources::GetString(valueKey)));
                    value.Maximum(bend ? 16383.0 : 127.0);

                    // A bend starts in the middle, where it does nothing.
                    if (bend && (std::isnan(value.Value()) || value.Value() <= 127.0))
                    {
                        value.Value(8192);
                    }
                    else if (!bend && (std::isnan(value.Value()) || value.Value() > 127.0))
                    {
                        value.Value(100);
                    }

                    value.Visibility(shows(index == 0 || index == 1 || index == 2 || index == 4 || index == 5));
                    channel.Visibility(shows(!isWords));
                    midi2.Visibility(shows(!isWords));
                    words.Visibility(shows(isWords));
                    wordsHint.Visibility(shows(isWords));
                };

            kind.SelectionChanged([fitFields](foundation::IInspectable const& sender, auto&&)
                {
                    if (auto const box = sender.try_as<controls::ComboBox>())
                    {
                        fitFields(box.SelectedIndex());
                    }
                });

            fitFields(0);

            add.Click([kind, channel, number, value, midi2, words, wordsHint, messages, weakShow = std::weak_ptr<std::function<void()>>{ showList }](auto&&, auto&&)
                {
                    if (messages->size() >= patchbay::MaximumTraceMessages)
                    {
                        return;
                    }

                    auto const index = kind.SelectedIndex();

                    if (index < 0)
                    {
                        return;
                    }

                    if (index == TraceKindWordsIndex)
                    {
                        auto const read = patchbay::ReadTraceWords(std::wstring_view{ words.Text() });

                        if (!read.has_value())
                        {
                            wordsHint.Text(resources::GetString(L"TraceWordsNotUnderstood"));
                            return;
                        }

                        wordsHint.Text(resources::GetString(L"TraceWordsHint"));
                        messages->push_back(*read);
                    }
                    else
                    {
                        auto const whole = [](double shown, long top)
                            {
                                return std::isnan(shown) ? 0u : static_cast<uint32_t>(std::clamp(std::lround(shown), 0L, top));
                            };

                        messages->push_back(patchbay::MakeTraceMessage(
                            static_cast<patchbay::TraceMessageKind>(index),
                            0,
                            static_cast<uint8_t>(std::clamp(channel.SelectedIndex(), 0, 15)),
                            whole(number.Value(), 127),
                            whole(value.Value(), 16383),
                            midi2.IsChecked() != nullptr && midi2.IsChecked().Value()));
                    }

                    if (auto const show = weakShow.lock())
                    {
                        (*show)();
                    }
                });

            content.Children().Append(patchbay::parts::Card(adder));

            dialog.XamlRoot(Content().XamlRoot());

            auto const result = co_await dialog.ShowAsync();

            if (result != controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            auto const sourceIndex = from.SelectedIndex();

            if (sourceIndex < 0 || static_cast<size_t>(sourceIndex) >= sources.size())
            {
                co_return;
            }

            m_traceSourceId = sources[static_cast<size_t>(sourceIndex)].first;
            m_traceGroup = static_cast<uint8_t>(std::clamp(group.SelectedIndex(), 0, 15));
            m_traceMessages = *messages;

            // Looked up again: the patch can change while the dialog is open.
            auto const* current = CurrentPatch();

            if (current == nullptr)
            {
                co_return;
            }

            std::vector<patchbay::TraceMessage> grouped{};

            for (auto const& message : m_traceMessages)
            {
                grouped.push_back(patchbay::WithTraceGroup(message, m_traceGroup));
            }

            m_trace = patchbay::TracePatch(*current, m_traceSourceId, grouped);
            m_traceIndex = 0;

            ShowTraceStep();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to trace the messages.")
    }

    void MainWindow::ShowTraceStep() noexcept
    {
        try
        {
            auto const* patch = CurrentPatch();

            if (patch == nullptr)
            {
                ClearTrace();
                return;
            }

            TraceBar().Severity(controls::InfoBarSeverity::Informational);

            if (m_trace.Outcome != patchbay::TraceOutcome::Traced || m_trace.Steps.empty())
            {
                m_canvas.ClearTrace();

                TraceBar().Title(resources::GetString(L"TraceTitle"));
                TraceBar().Message(resources::GetString(m_trace.Outcome == patchbay::TraceOutcome::DoesNotRoute
                    ? L"TraceDoesNotRoute"
                    : L"TraceNothingFromSource"));
                TraceBar().Severity(controls::InfoBarSeverity::Warning);
                TracePreviousButton().IsEnabled(false);
                TraceNextButton().IsEnabled(false);
                TraceBar().IsOpen(true);
                return;
            }

            m_traceIndex = (std::min)(m_traceIndex, m_trace.Steps.size() - 1);

            auto const& step = m_trace.Steps[m_traceIndex];

            m_canvas.ShowTrace(step.Nodes, step.Links, step.KeptOut);

            patchbay::TraceMessage sent{};

            if (m_traceIndex < m_traceMessages.size())
            {
                sent = patchbay::WithTraceGroup(m_traceMessages[m_traceIndex], m_traceGroup);
            }

            TraceBar().Title(resources::FormatString(L"TraceTitleFormat",
                static_cast<int32_t>(m_traceIndex) + 1,
                static_cast<int32_t>(m_trace.Steps.size()),
                DescribeTraceMessage(sent)));

            auto const endpointName = [patch](std::wstring const& id)
                {
                    auto const* endpoint = patch->FindEndpoint(id);

                    return endpoint == nullptr ? id : endpoint->DisplayName;
                };

            std::wstring keptOut{};

            for (auto const& id : step.KeptOut)
            {
                if (auto const* block = patch->FindBlock(id))
                {
                    if (!keptOut.empty())
                    {
                        keptOut += L", ";
                    }

                    keptOut += patchbay::BlockDisplayName(*block);
                }
            }

            std::wstring text{};

            auto const add = [&text](winrt::hstring const& line)
                {
                    if (!text.empty())
                    {
                        text += L'\n';
                    }

                    text += line;
                };

            for (auto const& arrival : step.Arrivals)
            {
                auto const type = arrival.Message.WordCount == 0 ? 0u : arrival.Message.Words[0] >> 28;

                if (type == 0x0 || type == 0xF)
                {
                    add(resources::FormatString(L"TraceArrivalNoGroupFormat",
                        endpointName(arrival.EndpointId), DescribeTraceMessage(arrival.Message)));
                }
                else
                {
                    add(resources::FormatString(L"TraceArrivalFormat",
                        endpointName(arrival.EndpointId),
                        static_cast<int32_t>((arrival.Message.Words[0] >> 24) & 0x0F) + 1,
                        DescribeTraceMessage(arrival.Message)));
                }
            }

            if (step.Arrivals.empty())
            {
                add(keptOut.empty()
                    ? resources::GetString(L"TraceReachesNothing")
                    : resources::FormatString(L"TraceKeptOutFormat", keptOut));
            }
            else if (!keptOut.empty())
            {
                add(resources::FormatString(L"TraceAlsoKeptOutFormat", keptOut));
            }

            for (auto const& [name, memory] : step.Memories)
            {
                add(resources::FormatString(L"LogicMemoryNowFormat", name,
                    patchbay::DescribeLogicValue(memory, patchbay::LogicUnit::Number, patchbay::ValueScale::SevenBit)));
            }

            TraceBar().Message(winrt::hstring{ text });
            TracePreviousButton().IsEnabled(m_traceIndex > 0);
            TraceNextButton().IsEnabled(m_traceIndex + 1 < m_trace.Steps.size());
            TraceBar().IsOpen(true);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to show the trace.")
    }

    void MainWindow::ClearTrace() noexcept
    {
        try
        {
            m_trace = {};
            m_traceIndex = 0;
            m_canvas.ClearTrace();

            if (auto const bar = TraceBar())
            {
                bar.IsOpen(false);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to put the canvas back after a trace.")
    }
}
