// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, so the unit tests compile it unchanged.

#include "RepeatPlan.h"

#include <algorithm>
#include <cmath>

namespace glass
{
    namespace
    {
        constexpr uint32_t ChannelWrap = 16;
        constexpr uint32_t GroupWrap = 16;
        constexpr uint32_t NumberWrap = 128;

        int32_t WrapIndex(_In_ int32_t base, _In_ int32_t offset, _In_ int32_t wrap) noexcept
        {
            auto value = (base + offset) % wrap;

            if (value < 0)
            {
                value += wrap;
            }

            return value;
        }

        uint32_t WrapNumber(_In_ uint32_t base, _In_ int32_t offset, _In_ uint32_t wrap) noexcept
        {
            auto const value = static_cast<int64_t>(base) + offset;
            auto const span = static_cast<int64_t>(wrap);

            auto wrapped = value % span;

            if (wrapped < 0)
            {
                wrapped += span;
            }

            return static_cast<uint32_t>(wrapped);
        }

        bool StepsThisMessage(_In_ RepeatField field, _In_ MessageKind kind) noexcept
        {
            switch (field)
            {
            case RepeatField::ControllerNumber:
                return kind == MessageKind::ControlChange ||
                    kind == MessageKind::RegisteredController ||
                    kind == MessageKind::AssignedController;

            case RepeatField::NoteNumber:
                return kind == MessageKind::Note || kind == MessageKind::PerNoteController;

            default:
                return false;
            }
        }

        void StepMessage(_Inout_ ControlMessage& message, _In_ RepeatField field, _In_ int32_t offset) noexcept
        {
            switch (field)
            {
            case RepeatField::Channel:
                message.ChannelIndex = WrapIndex(message.ChannelIndex, offset, ChannelWrap);
                break;

            case RepeatField::Group:
                // All groups stays all groups. Stepping it would silently bind the copy to one.
                if (message.GroupIndex != AllGroups)
                {
                    message.GroupIndex = WrapIndex(message.GroupIndex, offset, GroupWrap);
                }
                break;

            case RepeatField::ControllerNumber:
            case RepeatField::NoteNumber:
                if (StepsThisMessage(field, message.Kind))
                {
                    message.Number = WrapNumber(message.Number, offset, NumberWrap);
                }
                break;

            case RepeatField::Nothing:
                break;
            }
        }

        // A fader bank that follows the DAW needs its feedback stepped as well, or copy two
        // sends on channel two and listens on channel one.
        void StepFeedback(_Inout_ FeedbackBinding& feedback, _In_ RepeatField field, _In_ int32_t offset) noexcept
        {
            if (!feedback.Enabled)
            {
                return;
            }

            switch (field)
            {
            case RepeatField::Channel:
                feedback.ChannelIndex = WrapIndex(feedback.ChannelIndex, offset, ChannelWrap);
                break;

            case RepeatField::Group:
                if (feedback.GroupIndex != AllGroups)
                {
                    feedback.GroupIndex = WrapIndex(feedback.GroupIndex, offset, GroupWrap);
                }
                break;

            case RepeatField::ControllerNumber:
            case RepeatField::NoteNumber:
                if (StepsThisMessage(field, feedback.Kind))
                {
                    feedback.Number = WrapNumber(feedback.Number, offset, NumberWrap);
                }
                break;

            case RepeatField::Nothing:
                break;
            }
        }

        struct Extent
        {
            double Left{ 0.0 };
            double Top{ 0.0 };
            double Right{ 0.0 };
            double Bottom{ 0.0 };
        };

        Extent MeasureExtent(_In_ std::vector<Control> const& controls) noexcept
        {
            Extent extent{ controls[0].X, controls[0].Y, controls[0].X + controls[0].Width, controls[0].Y + controls[0].Height };

            for (auto const& control : controls)
            {
                extent.Left = std::min(extent.Left, control.X);
                extent.Top = std::min(extent.Top, control.Y);
                extent.Right = std::max(extent.Right, control.X + control.Width);
                extent.Bottom = std::max(extent.Bottom, control.Y + control.Height);
            }

            return extent;
        }
    }

    _Use_decl_annotations_
    std::wstring FormatRepeatLabel(std::wstring const& pattern, int32_t number)
    {
        std::wstring const token{ L"{n}" };
        std::wstring const replacement{ std::to_wstring(number) };

        std::wstring result{};
        size_t position{ 0 };

        for (;;)
        {
            auto const found = pattern.find(token, position);

            if (found == std::wstring::npos)
            {
                result.append(pattern, position, std::wstring::npos);
                break;
            }

            result.append(pattern, position, found - position);
            result.append(replacement);

            position = found + token.size();
        }

        return result;
    }

    _Use_decl_annotations_
    RepeatResult BuildRepeat(std::vector<Control> const& source, RepeatOptions const& options)
    {
        RepeatResult result{};

        if (source.empty())
        {
            return result;
        }

        result.UpdatedSource = source;

        if (!options.LabelPattern.empty())
        {
            for (auto& control : result.UpdatedSource)
            {
                control.Label = FormatRepeatLabel(options.LabelPattern, 1);
            }
        }

        auto const copies = std::clamp(options.Copies, 0, MaximumRepeatCopies);

        if (copies == 0)
        {
            return result;
        }

        auto const extent = MeasureExtent(source);

        auto const strideX = extent.Right - extent.Left + options.Gap;
        auto const strideY = extent.Bottom - extent.Top + options.Gap;

        result.Copies.reserve(source.size() * static_cast<size_t>(copies));

        for (int32_t copy = 1; copy <= copies; ++copy)
        {
            double offsetX{ 0.0 };
            double offsetY{ 0.0 };

            switch (options.Direction)
            {
            case RepeatDirection::Right: offsetX = strideX * copy; break;
            case RepeatDirection::Left:  offsetX = -strideX * copy; break;
            case RepeatDirection::Down:  offsetY = strideY * copy; break;
            case RepeatDirection::Up:    offsetY = -strideY * copy; break;
            }

            auto const step = options.Step * copy;

            for (auto const& original : source)
            {
                auto control = original;

                control.Id = LayoutDocument::NewId();
                control.X = original.X + offsetX;
                control.Y = original.Y + offsetY;

                if (!options.LabelPattern.empty())
                {
                    control.Label = FormatRepeatLabel(options.LabelPattern, copy + 1);
                }

                for (auto& message : control.Messages)
                {
                    StepMessage(message, options.Field, step);
                }

                StepFeedback(control.Feedback, options.Field, step);

                result.Copies.push_back(std::move(control));
            }
        }

        return result;
    }
}
