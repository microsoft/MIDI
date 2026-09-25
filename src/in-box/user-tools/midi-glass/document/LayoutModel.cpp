// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "LayoutModel.h"

#include <windows.h>
#include <combaseapi.h>

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace glass
{
    _Use_decl_annotations_
    Page* LayoutDocument::FindPage(std::wstring const& id) noexcept
    {
        auto it = std::find_if(Pages.begin(), Pages.end(),
            [&id](Page const& p) { return p.Id == id; });

        return it == Pages.end() ? nullptr : &(*it);
    }

    _Use_decl_annotations_
    Page const* LayoutDocument::FindPage(std::wstring const& id) const noexcept
    {
        auto it = std::find_if(Pages.begin(), Pages.end(),
            [&id](Page const& p) { return p.Id == id; });

        return it == Pages.end() ? nullptr : &(*it);
    }

    _Use_decl_annotations_
    Control* LayoutDocument::FindControl(std::wstring const& id) noexcept
    {
        for (auto& page : Pages)
        {
            auto it = std::find_if(page.Controls.begin(), page.Controls.end(),
                [&id](Control const& c) { return c.Id == id; });

            if (it != page.Controls.end())
            {
                return &(*it);
            }
        }

        return nullptr;
    }

    _Use_decl_annotations_
    Control const* LayoutDocument::FindControl(std::wstring const& id) const noexcept
    {
        for (auto const& page : Pages)
        {
            auto it = std::find_if(page.Controls.begin(), page.Controls.end(),
                [&id](Control const& c) { return c.Id == id; });

            if (it != page.Controls.end())
            {
                return &(*it);
            }
        }

        return nullptr;
    }

    _Use_decl_annotations_
    DeviceEntry const* LayoutDocument::FindDevice(std::wstring const& name) const noexcept
    {
        auto it = std::find_if(Devices.begin(), Devices.end(),
            [&name](DeviceEntry const& d) { return d.Name == name; });

        return it == Devices.end() ? nullptr : &(*it);
    }

    _Use_decl_annotations_
    Sequence const* LayoutDocument::FindSequence(std::wstring const& name) const noexcept
    {
        auto it = std::find_if(Sequences.begin(), Sequences.end(),
            [&name](Sequence const& s) { return s.Name == name; });

        return it == Sequences.end() ? nullptr : &(*it);
    }

    size_t LayoutDocument::ControlCount() const noexcept
    {
        size_t total{ 0 };

        for (auto const& page : Pages)
        {
            total += page.Controls.size();
        }

        return total;
    }

    _Use_decl_annotations_
    Control const* LayoutDocument::ControlAtIndex(size_t controlIndex) const noexcept
    {
        for (auto const& page : Pages)
        {
            if (controlIndex < page.Controls.size())
            {
                return &page.Controls[controlIndex];
            }

            controlIndex -= page.Controls.size();
        }

        return nullptr;
    }

    _Use_decl_annotations_
    int32_t DetentStopCount(Control const& control) noexcept
    {
        int32_t highest{ 0 };

        for (auto const& message : control.Messages)
        {
            auto const& detents = message.Detents;

            if (detents.Mode == DetentMode::ExplicitValues)
            {
                highest = std::max(
                    highest,
                    static_cast<int32_t>(std::min(detents.Stops.size(), MaximumDetentStops)));
            }
            else if (detents.Mode == DetentMode::EvenSteps && detents.Step > 0.0)
            {
                auto const span = std::abs(message.Maximum.Value - message.Minimum.Value);

                if (span > 0.0)
                {
                    auto const steps =
                        static_cast<int32_t>(std::floor(span / detents.Step)) + 1;

                    highest = std::max(
                        highest,
                        std::clamp(steps, 0, static_cast<int32_t>(MaximumDetentStops)));
                }
            }
        }

        return highest;
    }

    _Use_decl_annotations_
    std::vector<double> ParseStopList(std::wstring const& text) noexcept
    {
        std::vector<double> stops{};

        try
        {
            std::wstring token{};

            auto const flush = [&stops, &token]()
                {
                    if (token.empty() || stops.size() >= MaximumDetentStops)
                    {
                        token.clear();
                        return;
                    }

                    try
                    {
                        size_t consumed{ 0 };
                        auto const value = std::stod(token, &consumed);

                        if (consumed == token.size() && std::isfinite(value))
                        {
                            stops.push_back(value);
                        }
                    }
                    catch (...)
                    {
                    }

                    token.clear();
                };

            for (auto const character : text)
            {
                if (character == L',' || character == L';' || character == L' ' ||
                    character == L'\t' || character == L'\r' || character == L'\n')
                {
                    flush();
                }
                else
                {
                    token += character;
                }
            }

            flush();
        }
        catch (...)
        {
        }

        return stops;
    }

    _Use_decl_annotations_
    std::wstring FormatStopList(std::vector<double> const& stops) noexcept
    {
        try
        {
            std::wstring text{};

            for (auto const stop : stops)
            {
                if (!text.empty())
                {
                    text += L", ";
                }

                // Whole numbers without a decimal point, because a stop list is usually a row
                // of values out of a manual and "10.000000" is unreadable.
                if (stop == std::floor(stop) && std::abs(stop) < 1e15)
                {
                    text += std::to_wstring(static_cast<int64_t>(stop));
                }
                else
                {
                    auto number = std::to_wstring(stop);

                    while (number.size() > 1 && number.back() == L'0')
                    {
                        number.pop_back();
                    }

                    if (!number.empty() && number.back() == L'.')
                    {
                        number.pop_back();
                    }

                    text += number;
                }
            }

            return text;
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::vector<std::wstring> DetentStopLabels(Control const& control) noexcept
    {
        std::vector<std::wstring> labels{};

        try
        {
            // The message with the most stops is the one the control actually snaps to, so it
            // is the one whose numbers get printed.
            ControlMessage const* chosen{ nullptr };
            size_t most{ 0 };

            for (auto const& message : control.Messages)
            {
                size_t count{ 0 };

                if (message.Detents.Mode == DetentMode::ExplicitValues)
                {
                    count = std::min(message.Detents.Stops.size(), MaximumDetentStops);
                }
                else if (message.Detents.Mode == DetentMode::EvenSteps && message.Detents.Step > 0.0)
                {
                    auto const span = std::abs(message.Maximum.Value - message.Minimum.Value);

                    count = span > 0.0
                        ? static_cast<size_t>(std::floor(span / message.Detents.Step)) + 1
                        : 0;
                }

                if (count > most)
                {
                    most = count;
                    chosen = &message;
                }
            }

            if (chosen == nullptr || most < 2 || most > static_cast<size_t>(MaximumLabeledStops))
            {
                return labels;
            }

            auto const absolute =
                chosen->Detents.Scaling == ValueScaling::Absolute ||
                chosen->Minimum.Scaling == ValueScaling::Absolute ||
                chosen->Maximum.Scaling == ValueScaling::Absolute;

            auto const describe = [absolute](double value)
                {
                    if (absolute)
                    {
                        return FormatStopList({ value });
                    }

                    return std::to_wstring(static_cast<int32_t>(std::lround(value * 100.0))) + L" %";
                };

            if (chosen->Detents.Mode == DetentMode::ExplicitValues)
            {
                for (size_t index = 0; index < most; ++index)
                {
                    labels.push_back(describe(chosen->Detents.Stops[index]));
                }

                return labels;
            }

            // Measured from the minimum, so a range that does not start at zero still has a
            // stop on its own bottom end.
            auto const ascending = chosen->Maximum.Value >= chosen->Minimum.Value;
            auto const step = ascending ? chosen->Detents.Step : -chosen->Detents.Step;

            for (size_t index = 0; index < most; ++index)
            {
                labels.push_back(describe(chosen->Minimum.Value + step * static_cast<double>(index)));
            }
        }
        catch (...)
        {
            labels.clear();
        }

        return labels;
    }

    std::vector<Control const*> LayoutDocument::ControlsOutsidePage() const noexcept
    {
        std::vector<Control const*> outside{};

        for (auto const& page : Pages)
        {
            for (auto const& control : page.Controls)
            {
                // Any part hanging over an edge counts. A control half off the page is just as
                // unreachable at run time as one entirely off it.
                if (control.X < 0 ||
                    control.Y < 0 ||
                    control.X + control.Width > PageWidth ||
                    control.Y + control.Height > PageHeight)
                {
                    outside.push_back(&control);
                }
            }
        }

        return outside;
    }

    std::wstring LayoutDocument::NewId() noexcept
    {
        GUID value{};

        if (FAILED(::CoCreateGuid(&value)))
        {
            return {};
        }

        wchar_t buffer[40]{};

        if (::StringFromGUID2(value, buffer, ARRAYSIZE(buffer)) == 0)
        {
            return {};
        }

        std::wstring result{ buffer };

        // braces only add noise inside a file the app owns end to end
        std::erase(result, L'{');
        std::erase(result, L'}');

        return result;
    }

    _Use_decl_annotations_
    std::wstring SanitizeStoredString(std::wstring value) noexcept
    {
        return midiapp::SanitizeStoredString(std::move(value));
    }

    namespace
    {
        void CheckMessage(
            _In_ LayoutDocument const& document,
            _In_ std::wstring const& ownerId,
            _In_ ControlMessage const& message,
            _Inout_ std::vector<ValidationIssue>& issues) noexcept
        {
            if (!message.DeviceName.empty() && document.FindDevice(message.DeviceName) == nullptr)
            {
                issues.push_back({ ownerId, L"Sends to a device that is not in the device table: " + message.DeviceName });
            }

            if (message.GroupIndex != AllGroups &&
                (message.GroupIndex < 0 || message.GroupIndex >= MaximumGroupCount))
            {
                issues.push_back({ ownerId, L"Group is outside 1 to 16." });
            }

            if (message.ChannelIndex < 0 || message.ChannelIndex > 15)
            {
                issues.push_back({ ownerId, L"Channel is outside 1 to 16." });
            }

            if (message.Kind == MessageKind::Sequence &&
                document.FindSequence(message.SequenceName) == nullptr)
            {
                issues.push_back({ ownerId, L"Runs a sequence that does not exist: " + message.SequenceName });
            }

            if (message.Kind == MessageKind::GoToPage &&
                document.FindPage(message.TargetPageId) == nullptr)
            {
                issues.push_back({ ownerId, L"Switches to a page that does not exist." });
            }

            if (message.Kind == MessageKind::SystemExclusive && message.SystemExclusive.empty())
            {
                issues.push_back({ ownerId, L"System exclusive message carries no data." });
            }

            if (message.Kind == MessageKind::RawUmp &&
                (message.RawWords.empty() || message.RawWords.size() > 4))
            {
                issues.push_back({ ownerId, L"A universal packet is one to four words." });
            }
        }
    }

    _Use_decl_annotations_
    std::vector<ValidationIssue> Validate(LayoutDocument const& document) noexcept
    {
        std::vector<ValidationIssue> issues{};

        if (document.PageWidth <= 0 || document.PageHeight <= 0)
        {
            issues.push_back({ {}, L"The page has no size." });
        }

        if (document.CanvasWidth < document.PageWidth || document.CanvasHeight < document.PageHeight)
        {
            issues.push_back({ {}, L"The canvas is smaller than the page." });
        }

        if (document.Pages.empty())
        {
            issues.push_back({ {}, L"The layout has no pages." });
        }

        std::unordered_set<std::wstring> pageIds{};
        std::unordered_set<std::wstring> controlIds{};
        std::unordered_set<std::wstring> deviceNames{};

        for (auto const& device : document.Devices)
        {
            if (device.Name.empty())
            {
                issues.push_back({ {}, L"A device table entry has no name." });
            }
            else if (!deviceNames.insert(device.Name).second)
            {
                issues.push_back({ device.Name, L"Two device table entries share a name." });
            }
        }

        for (auto const& page : document.Pages)
        {
            if (page.Id.empty())
            {
                issues.push_back({ {}, L"A page has no id." });
            }
            else if (!pageIds.insert(page.Id).second)
            {
                issues.push_back({ page.Id, L"Two pages share an id." });
            }

            for (auto const& control : page.Controls)
            {
                if (control.Id.empty())
                {
                    issues.push_back({ page.Id, L"A control has no id." });
                }
                else if (!controlIds.insert(control.Id).second)
                {
                    issues.push_back({ control.Id, L"Two controls share an id." });
                }

                if (control.Width <= 0 || control.Height <= 0)
                {
                    issues.push_back({ control.Id, L"The control has no size." });
                }

                if (control.HueSlot != LiteralHue &&
                    (control.HueSlot < 0 || control.HueSlot >= HueSlotCount))
                {
                    issues.push_back({ control.Id, L"The hue slot is outside the theme." });
                }

                if (control.HueSlot == LiteralHue && control.LiteralColor.empty())
                {
                    issues.push_back({ control.Id, L"The control uses a literal color but none is set." });
                }

                for (auto const& message : control.Messages)
                {
                    CheckMessage(document, control.Id, message, issues);
                }

                if (control.Feedback.Enabled &&
                    !control.Feedback.DeviceName.empty() &&
                    document.FindDevice(control.Feedback.DeviceName) == nullptr)
                {
                    issues.push_back({ control.Id, L"Takes feedback from a device that is not in the device table." });
                }
            }
        }

        std::unordered_set<std::wstring> sequenceNames{};

        for (auto const& sequence : document.Sequences)
        {
            if (sequence.Name.empty())
            {
                issues.push_back({ {}, L"A sequence has no name." });
            }
            else if (!sequenceNames.insert(sequence.Name).second)
            {
                issues.push_back({ sequence.Name, L"Two sequences share a name." });
            }

            int32_t openRepeats{ 0 };

            for (auto const& step : sequence.Steps)
            {
                if (step.Kind == SequenceStepKind::RepeatBlockStart)
                {
                    ++openRepeats;
                }
                else if (step.Kind == SequenceStepKind::RepeatBlockEnd)
                {
                    --openRepeats;

                    if (openRepeats < 0)
                    {
                        issues.push_back({ sequence.Name, L"A repeat block ends before it starts." });
                        break;
                    }
                }
                else if (step.Kind == SequenceStepKind::SendMidiMessage)
                {
                    CheckMessage(document, sequence.Name, step.Message, issues);
                }
                else if (step.Kind == SequenceStepKind::SetControlValue &&
                    document.FindControl(step.TargetControlId) == nullptr)
                {
                    issues.push_back({ sequence.Name, L"Sets a control that does not exist." });
                }
            }

            if (openRepeats > 0)
            {
                issues.push_back({ sequence.Name, L"A repeat block is never closed." });
            }
        }

        if (document.Tempo.Kind == TempoSourceKind::FollowIncomingClock &&
            document.Tempo.DeviceName.empty())
        {
            issues.push_back({ {}, L"The tempo follows incoming clock but no device is named." });
        }

        return issues;
    }

    _Use_decl_annotations_
    std::vector<uint16_t> CollectGroupMasks(LayoutDocument const& document) noexcept
    {
        std::vector<uint16_t> masks(document.Devices.size(), uint16_t{ 0 });

        auto const indexOf = [&document](std::wstring const& name) noexcept -> int32_t
            {
                for (size_t i = 0; i < document.Devices.size(); ++i)
                {
                    if (document.Devices[i].Name == name)
                    {
                        return static_cast<int32_t>(i);
                    }
                }

                return -1;
            };

        auto const addMessage = [&](ControlMessage const& message) noexcept
            {
                auto const index = indexOf(message.DeviceName);

                if (index < 0)
                {
                    return;
                }

                // A message that names every group drives every group, which is the one case
                // where a panic has to be wide rather than precise.
                if (message.GroupIndex == AllGroups)
                {
                    masks[index] = 0xFFFF;
                }
                else
                {
                    masks[index] |= static_cast<uint16_t>(1u << (message.GroupIndex & 0x0F));
                }
            };

        for (auto const& page : document.Pages)
        {
            for (auto const& control : page.Controls)
            {
                for (auto const& message : control.Messages)
                {
                    addMessage(message);
                }
            }
        }

        // A sequence is a way of sending, not a different kind of destination, so the groups it
        // reaches count the same as a control's.
        for (auto const& sequence : document.Sequences)
        {
            for (auto const& step : sequence.Steps)
            {
                if (step.Kind == SequenceStepKind::SendMidiMessage ||
                    step.Kind == SequenceStepKind::SendSystemExclusive)
                {
                    addMessage(step.Message);
                }
            }
        }

        return masks;
    }
}
