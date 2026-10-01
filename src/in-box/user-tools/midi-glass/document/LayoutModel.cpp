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
#include <cstdio>
#include <unordered_set>

namespace glass
{
    _Use_decl_annotations_
    ControlGroup* Page::FindGroup(std::wstring const& id) noexcept
    {
        auto it = std::find_if(Groups.begin(), Groups.end(),
            [&id](ControlGroup const& group) { return group.Id == id; });

        return it == Groups.end() ? nullptr : &(*it);
    }

    _Use_decl_annotations_
    ControlGroup const* Page::FindGroup(std::wstring const& id) const noexcept
    {
        auto it = std::find_if(Groups.begin(), Groups.end(),
            [&id](ControlGroup const& group) { return group.Id == id; });

        return it == Groups.end() ? nullptr : &(*it);
    }

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
    DeviceProtocol LayoutDocument::ProtocolOf(std::wstring const& deviceName) const noexcept
    {
        auto const* const device = FindDevice(deviceName);

        return device == nullptr ? DeviceProtocol::Midi2 : device->Protocol;
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
    PictureRect PictureCropRect(
        Picture const& picture,
        double controlWidth,
        double controlHeight,
        double naturalWidth,
        double naturalHeight) noexcept
    {
        auto const available = PictureRect{
            0.0, 0.0, std::max(controlWidth, 1.0), std::max(controlHeight, 1.0) };

        auto const zoom = std::clamp(picture.Zoom, MinimumPictureZoom, MaximumPictureZoom);

        // Nothing is known about the file yet, so it simply covers the control. A video looks
        // like this for the moment between the element appearing and the first frame arriving.
        if (naturalWidth <= 0.0 || naturalHeight <= 0.0)
        {
            return PictureRect{ 0.0, 0.0, available.Width * zoom, available.Height * zoom };
        }

        auto const byWidth = available.Width / naturalWidth;
        auto const byHeight = available.Height / naturalHeight;

        auto width = available.Width * zoom;
        auto height = available.Height * zoom;

        switch (picture.Fit)
        {
        case BackgroundFit::Centered:
            width = naturalWidth * zoom;
            height = naturalHeight * zoom;
            break;

        case BackgroundFit::Stretch:
            // Its shape is thrown away on purpose, so both sides follow the control.
            break;

        case BackgroundFit::Uniform:
        {
            auto const scale = std::min(byWidth, byHeight) * zoom;

            width = naturalWidth * scale;
            height = naturalHeight * scale;
            break;
        }

        default:
        {
            // Fill and Tiled both cover the control and cut off what does not fit.
            auto const scale = std::max(byWidth, byHeight) * zoom;

            width = naturalWidth * scale;
            height = naturalHeight * scale;
            break;
        }
        }

        // Where the named point of the picture would put it, then held inside the control.
        // Sliding a zoomed picture right up to its own edge is reasonable; sliding it past the
        // edge and leaving a bare strip down one side of the control is not, so the pan runs
        // out at the point where the picture stops covering.
        auto const place = [](double centerFraction, double rendered, double space)
            {
                if (rendered <= space)
                {
                    // Smaller than the control, so the same number says where it sits instead:
                    // 0 against the left or top, 1 against the right or bottom.
                    return (space - rendered) * std::clamp(centerFraction, 0.0, 1.0);
                }

                return std::clamp(
                    space * 0.5 - rendered * std::clamp(centerFraction, 0.0, 1.0),
                    space - rendered,
                    0.0);
            };

        return PictureRect{
            place(picture.CenterX, width, available.Width),
            place(picture.CenterY, height, available.Height),
            std::max(width, 1.0),
            std::max(height, 1.0) };
    }

    _Use_decl_annotations_
    PictureRect VisiblePictureRect(
        PictureRect const& content,
        double controlWidth,
        double controlHeight) noexcept
    {
        auto const left = std::max(content.X, 0.0);
        auto const top = std::max(content.Y, 0.0);
        auto const right = std::min(content.X + content.Width, std::max(controlWidth, 0.0));
        auto const bottom = std::min(content.Y + content.Height, std::max(controlHeight, 0.0));

        if (!(right > left) || !(bottom > top))
        {
            return PictureRect{};
        }

        return PictureRect{ left, top, right - left, bottom - top };
    }

    _Use_decl_annotations_
    VideoRange VideoPlayRange(Picture const& picture, double durationSeconds) noexcept
    {
        auto const finiteOrZero = [](double value) noexcept
            {
                return std::isfinite(value) ? std::clamp(value, 0.0, MaximumVideoSeconds) : 0.0;
            };

        auto const duration = finiteOrZero(durationSeconds);
        auto start = finiteOrZero(picture.VideoStartSeconds);
        auto end = finiteOrZero(picture.VideoEndSeconds);

        // Not opened yet. The points are taken at their word, and a stop that is not after the
        // start means the end of the file, whenever that turns out to be.
        if (duration <= 0.0)
        {
            return VideoRange{ start, end > start ? end : 0.0 };
        }

        if (start >= duration)
        {
            start = 0.0;
        }

        if (end <= start || end > duration)
        {
            end = duration;
        }

        // Too short to be worth playing, so the start gives way. A clip shorter than the minimum
        // plays whole.
        if (end - start < MinimumVideoPlaySeconds)
        {
            start = std::max(0.0, end - MinimumVideoPlaySeconds);
        }

        return VideoRange{ start, end };
    }

    _Use_decl_annotations_
    double VideoRangeFraction(VideoRange const& range, double seconds) noexcept
    {
        auto const length = range.Length();

        if (length <= 0.0 || !std::isfinite(seconds))
        {
            return 0.0;
        }

        return std::clamp((seconds - range.StartSeconds) / length, 0.0, 1.0);
    }

    _Use_decl_annotations_
    double VideoRangeSeconds(VideoRange const& range, double fraction) noexcept
    {
        auto const clamped = std::isfinite(fraction) ? std::clamp(fraction, 0.0, 1.0) : 0.0;

        return range.StartSeconds + (range.Length() * clamped);
    }

    _Use_decl_annotations_
    std::wstring FormatVideoTime(double seconds)
    {
        auto const safe = std::isfinite(seconds) ? std::clamp(seconds, 0.0, MaximumVideoSeconds) : 0.0;
        auto const tenths = static_cast<long long>(std::llround(safe * 10.0));
        auto const whole = tenths / 10;

        wchar_t text[32]{};

        if (whole >= 3600)
        {
            swprintf_s(text, L"%lld:%02lld:%02lld.%lld",
                whole / 3600, (whole / 60) % 60, whole % 60, tenths % 10);
        }
        else
        {
            swprintf_s(text, L"%lld:%02lld.%lld", whole / 60, whole % 60, tenths % 10);
        }

        return text;
    }

    _Use_decl_annotations_
    int32_t SwitchPositionCount(Control const& control) noexcept
    {
        return std::clamp(
            static_cast<int32_t>(control.Switch.Positions.size()),
            MinimumSwitchPositions,
            MaximumSwitchPositions);
    }

    _Use_decl_annotations_
    int32_t SwitchPositionAt(double value, int32_t positions) noexcept
    {
        auto const count = std::max(positions, MinimumSwitchPositions);

        if (!std::isfinite(value))
        {
            return 0;
        }

        return std::clamp(
            static_cast<int32_t>(std::lround(std::clamp(value, 0.0, 1.0) * (count - 1))),
            0,
            count - 1);
    }

    _Use_decl_annotations_
    double SwitchValueOf(int32_t position, int32_t positions) noexcept
    {
        auto const count = std::max(positions, MinimumSwitchPositions);

        return static_cast<double>(std::clamp(position, 0, count - 1)) / static_cast<double>(count - 1);
    }

    _Use_decl_annotations_
    bool HasBankAndIndex(MessageKind kind) noexcept
    {
        return kind == MessageKind::RegisteredController || kind == MessageKind::AssignedController;
    }

    _Use_decl_annotations_
    uint32_t ControllerBank(uint32_t number) noexcept
    {
        return (number >> 7) & 0x7F;
    }

    _Use_decl_annotations_
    uint32_t ControllerIndex(uint32_t number) noexcept
    {
        return number & 0x7F;
    }

    _Use_decl_annotations_
    uint32_t ControllerNumber(uint32_t bank, uint32_t index) noexcept
    {
        return ((std::min)(bank, 127u) << 7) | (std::min)(index, 127u);
    }

    _Use_decl_annotations_
    std::wstring FormatMessageNumber(MessageKind kind, uint32_t number)
    {
        if (HasBankAndIndex(kind))
        {
            return std::to_wstring(ControllerBank(number)) + L":" + std::to_wstring(ControllerIndex(number));
        }

        return std::to_wstring(number);
    }

    _Use_decl_annotations_
    bool SendsToADevice(MessageKind kind) noexcept
    {
        return kind != MessageKind::Sequence &&
            kind != MessageKind::GoToPage &&
            kind != MessageKind::HoldLayer;
    }

    _Use_decl_annotations_
    bool CarriesAChannel(MessageKind kind) noexcept
    {
        switch (kind)
        {
        case MessageKind::Note:
        case MessageKind::ControlChange:
        case MessageKind::ProgramChange:
        case MessageKind::PitchBend:
        case MessageKind::ChannelPressure:
        case MessageKind::PerNoteController:
        case MessageKind::RegisteredController:
        case MessageKind::AssignedController:
            return true;

        default:
            return false;
        }
    }

    _Use_decl_annotations_
    bool SendsAsMidi1(ControlMessage const& message, DeviceProtocol protocol) noexcept
    {
        switch (message.Kind)
        {
        case MessageKind::Note:
        case MessageKind::ControlChange:
        case MessageKind::ProgramChange:
        case MessageKind::PitchBend:
        case MessageKind::ChannelPressure:
            return message.UseMidi1Protocol || protocol != DeviceProtocol::Midi2;

        default:
            return false;
        }
    }

    _Use_decl_annotations_
    int32_t RawValueMaximum(ControlMessage const& message, DeviceProtocol protocol) noexcept
    {
        if (protocol == DeviceProtocol::Midi2 && !message.UseMidi1Protocol)
        {
            return 0;
        }

        switch (message.Kind)
        {
        case MessageKind::Note:
        case MessageKind::ControlChange:
        case MessageKind::ChannelPressure:
            return 127;

        case MessageKind::PitchBend:
        case MessageKind::RegisteredController:
        case MessageKind::AssignedController:
            return 16383;

        default:
            return 0;
        }
    }

    namespace
    {
        // The largest number the field this row's value lands in can hold on the wire.
        double WireMaximum(_In_ ControlMessage const& message, _In_ DeviceProtocol protocol) noexcept
        {
            if (SendsAsMidi1(message, protocol))
            {
                return message.Kind == MessageKind::PitchBend ? 16383.0 : 127.0;
            }

            return message.Kind == MessageKind::Note ? 65535.0 : 4294967295.0;
        }

        double ShareOf(
            _In_ double value,
            _In_ ValueScaling scaling,
            _In_ ControlMessage const& message,
            _In_ DeviceProtocol protocol) noexcept
        {
            if (!std::isfinite(value))
            {
                return 0.0;
            }

            auto const share = scaling == ValueScaling::Absolute
                ? value / WireMaximum(message, protocol)
                : value;

            return std::clamp(share, 0.0, 1.0);
        }
    }

    _Use_decl_annotations_
    double ShownValue(MessageValue const& end, ControlMessage const& message, DeviceProtocol protocol) noexcept
    {
        auto const share = ShareOf(end.Value, end.Scaling, message, protocol);
        auto const raw = RawValueMaximum(message, protocol);

        return raw > 0
            ? std::round(share * raw)
            : std::round(share * 1000.0) / 10.0;
    }

    _Use_decl_annotations_
    MessageValue ValueFromShown(double shown, ControlMessage const& message, DeviceProtocol protocol) noexcept
    {
        if (!std::isfinite(shown))
        {
            shown = 0.0;
        }

        auto const raw = RawValueMaximum(message, protocol);

        auto const share = raw > 0
            ? std::round(shown) / raw
            : shown / 100.0;

        return { std::clamp(share, 0.0, 1.0), ValueScaling::Fraction };
    }

    _Use_decl_annotations_
    void ShareExactValues(ControlMessage& message, DeviceProtocol protocol) noexcept
    {
        for (auto* const end : { &message.Minimum, &message.Maximum })
        {
            if (end->Scaling == ValueScaling::Absolute)
            {
                *end = { ShareOf(end->Value, ValueScaling::Absolute, message, protocol), ValueScaling::Fraction };
            }
        }

        auto& detents = message.Detents;

        if (detents.Scaling == ValueScaling::Absolute)
        {
            detents.Step = ShareOf(detents.Step, ValueScaling::Absolute, message, protocol);

            for (auto& stop : detents.Stops)
            {
                stop = ShareOf(stop, ValueScaling::Absolute, message, protocol);
            }

            detents.Scaling = ValueScaling::Fraction;
        }
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

    _Use_decl_annotations_
    std::vector<std::pair<std::wstring, std::wstring>> RegroupCopies(std::vector<Control>& copies)
    {
        std::vector<std::pair<std::wstring, std::wstring>> renamed{};

        for (auto& copy : copies)
        {
            if (copy.GroupId.empty())
            {
                continue;
            }

            auto found = std::find_if(renamed.begin(), renamed.end(),
                [&copy](auto const& pair) { return pair.first == copy.GroupId; });

            if (found == renamed.end())
            {
                renamed.emplace_back(copy.GroupId, LayoutDocument::NewId());
                found = std::prev(renamed.end());
            }

            copy.GroupId = found->second;
        }

        // A copy of one member of a group is a control on its own, not a group of one.
        for (auto& copy : copies)
        {
            if (!copy.GroupId.empty() &&
                std::count_if(copies.begin(), copies.end(),
                    [&copy](Control const& other) { return other.GroupId == copy.GroupId; }) < 2)
            {
                copy.GroupId.clear();
            }
        }

        std::erase_if(renamed, [&copies](auto const& pair)
            {
                return std::none_of(copies.begin(), copies.end(),
                    [&pair](Control const& copy) { return copy.GroupId == pair.second; });
            });

        return renamed;
    }

    _Use_decl_annotations_
    std::wstring NameForCopiedGroup(std::wstring const& name, Page const& page)
    {
        auto const taken = [&page](std::wstring const& candidate)
            {
                return std::any_of(page.Groups.begin(), page.Groups.end(),
                    [&candidate](ControlGroup const& group) { return group.Name == candidate; });
            };

        if (name.empty() || !taken(name))
        {
            return name;
        }

        auto digits = name.size();

        while (digits > 0 && name[digits - 1] >= L'0' && name[digits - 1] <= L'9')
        {
            --digits;
        }

        std::wstring base{ name };
        uint64_t number{ 1 };

        // Nine digits is more than anybody numbers a bank with, and cannot overflow.
        if (digits < name.size() && name.size() - digits <= 9)
        {
            base = name.substr(0, digits);
            number = 0;

            for (auto index = digits; index < name.size(); ++index)
            {
                number = number * 10 + static_cast<uint64_t>(name[index] - L'0');
            }
        }
        else
        {
            base += L' ';
        }

        // A page holds fewer groups than this, so one of these is always free.
        for (uint64_t next = number + 1; next <= number + MaximumControlsPerPage + 1; ++next)
        {
            auto candidate = base + std::to_wstring(next);

            if (candidate.size() > MaximumStringLength)
            {
                break;
            }

            if (!taken(candidate))
            {
                return candidate;
            }
        }

        return name;
    }

    _Use_decl_annotations_
    void NameCopiedGroups(
        Page& page,
        std::vector<ControlGroup> originals,
        std::vector<std::pair<std::wstring, std::wstring>> const& regrouped)
    {
        for (auto const& [from, to] : regrouped)
        {
            auto const original = std::find_if(originals.begin(), originals.end(),
                [&from](ControlGroup const& group) { return group.Id == from; });

            if (original == originals.end() || page.FindGroup(to) != nullptr)
            {
                continue;
            }

            page.Groups.push_back({ to, NameForCopiedGroup(original->Name, page), original->Unknown });
        }
    }

    _Use_decl_annotations_
    void PruneControlGroups(Page& page) noexcept
    {
        std::erase_if(page.Groups, [&page](ControlGroup const& group)
            {
                return group.Id.empty() ||
                    (group.Name.empty() && group.Unknown == nullptr) ||
                    std::none_of(page.Controls.begin(), page.Controls.end(),
                        [&group](Control const& control) { return control.GroupId == group.Id; });
            });
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

                if (!IsSlotInRange(control.HueSlot))
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

    _Use_decl_annotations_
    bool PanelIsFilled(Control const& panel, Theme const& theme) noexcept
    {
        switch (panel.Style)
        {
        case ControlStyleOverride::Outline:
        case ControlStyleOverride::Bare:
            return false;

        case ControlStyleOverride::Solid:
            return true;

        default:
            return theme.PanelFill != PanelFillStyle::None;
        }
    }

    _Use_decl_annotations_
    std::vector<PanelFootprint> PanelFootprints(Page const& page, Theme const& theme) noexcept
    {
        std::vector<PanelFootprint> panels{};

        try
        {
            for (size_t order = 0; order < page.Controls.size(); ++order)
            {
                auto const& control = page.Controls[order];

                if (control.Kind != ControlKind::Panel || !PanelIsFilled(control, theme))
                {
                    continue;
                }

                PanelFootprint footprint{ control.X, control.Y, control.Width, control.Height, order, false };

                // Its middle rather than its whole rectangle, so an inset drawn a pixel over the
                // edge of the section it belongs to is still that section's inset.
                footprint.IsInset = SurfaceAt(
                    panels,
                    control.X + control.Width * 0.5,
                    control.Y + control.Height * 0.5,
                    order) != PrintSurface::Deck;

                panels.push_back(footprint);
            }
        }
        catch (...)
        {
        }

        return panels;
    }

    _Use_decl_annotations_
    PrintSurface SurfaceAt(std::vector<PanelFootprint> const& panels, double x, double y, size_t order) noexcept
    {
        for (auto walk = panels.rbegin(); walk != panels.rend(); ++walk)
        {
            if (walk->Order >= order)
            {
                continue;
            }

            if (x >= walk->X && x < walk->X + walk->Width &&
                y >= walk->Y && y < walk->Y + walk->Height)
            {
                return walk->IsInset ? PrintSurface::Inset : PrintSurface::Section;
            }
        }

        return PrintSurface::Deck;
    }

    _Use_decl_annotations_
    std::wstring PageTabTarget(Control const& control)
    {
        for (auto const& message : control.Messages)
        {
            if (message.Kind == MessageKind::GoToPage && !message.TargetPageId.empty())
            {
                return message.TargetPageId;
            }
        }

        return {};
    }

    _Use_decl_annotations_
    bool IsSafeFontFamilyName(std::wstring_view name) noexcept
    {
        // Longer than any family name on a PC, and short enough that nobody can use it to carry
        // anything else.
        constexpr size_t MaximumFontFamilyLength = 128;

        if (name.empty() || name.size() > MaximumFontFamilyLength)
        {
            return false;
        }

        // A path, a link or a font file is written with these, and a comma makes a list of
        // families. None of them is part of a family's own name.
        for (auto const ch : name)
        {
            if (ch < L' ' || ch == L'\\' || ch == L'/' || ch == L':' || ch == L'#' || ch == L',' ||
                ch == L'%' || ch == L'?' || ch == L'*' || ch == L'"' || ch == L'<' || ch == L'>' || ch == L'|')
            {
                return false;
            }
        }

        return name.find_first_not_of(L' ') != std::wstring_view::npos;
    }
}
