// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Controls a MIDI remote control rule turned off or put out of sight, as the surface draws them
// and as a finger finds them.
//
// A control that is off is drawn faint and takes no input, but still follows what a device tells
// it, so it shows the right thing the moment it comes back on. It has no strike through it: that
// look means the device it sends to is missing, which is a different thing to fix.

#include "pch.h"
#include "SurfaceRenderer.h"
#include "InputRouter.h"
#include "GlassControl.h"

using namespace winrt::Microsoft::UI::Xaml::Hosting;

namespace glass
{
    namespace
    {
        constexpr uint8_t ItemUnavailable = 0x1;
        constexpr uint8_t ItemDisabled = 0x2;
        constexpr uint8_t ItemHidden = 0x4;

        // Struck through and dimmed when its device is missing, never hidden: a layout with a
        // missing device still has to be editable.
        constexpr float UnavailableOpacity = 0.55f;

        // Fainter than a missing device, so the two read as different things across a room.
        constexpr float DisabledOpacity = 0.3f;

        // Through the element's composition visual rather than its XAML properties, so this
        // never overwrites an opacity or a visibility the renderer gave the element itself.
        void ApplyTo(_In_ xaml::UIElement const& element, _In_ float opacity, _In_ bool visible) noexcept
        {
            if (element == nullptr)
            {
                return;
            }

            try
            {
                auto const visual = ElementCompositionPreview::GetElementVisual(element);

                visual.Opacity(opacity);
                visual.IsVisible(visible);
            }
            catch (...)
            {
            }
        }
    }

    _Use_decl_annotations_
    void SurfaceRenderer::SetControlState(size_t itemIndex, bool enabled, bool shown) noexcept
    {
        if (itemIndex >= m_visuals.size())
        {
            return;
        }

        try
        {
            if (m_itemStates.size() != m_visuals.size())
            {
                m_itemStates.assign(m_visuals.size(), 0);
            }

            auto state = static_cast<uint8_t>(m_itemStates[itemIndex] & ItemUnavailable);

            if (!enabled)
            {
                state |= ItemDisabled;
            }

            if (!shown)
            {
                state |= ItemHidden;
            }

            m_itemStates[itemIndex] = state;

            // Off is off for a screen reader too, and out of sight is out of its tree.
            if (itemIndex < m_elements.size() && m_elements[itemIndex] != nullptr)
            {
                auto const& element = m_elements[itemIndex];

                element.IsEnabled(enabled);
                element.Visibility(shown ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            }

            ApplyItemLook(itemIndex);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void SurfaceRenderer::ApplyItemLook(size_t itemIndex) noexcept
    {
        if (itemIndex >= m_visuals.size() || itemIndex >= m_itemStates.size())
        {
            return;
        }

        auto const state = m_itemStates[itemIndex];
        auto const visible = (state & ItemHidden) == 0;

        auto opacity = 1.0f;

        if ((state & ItemUnavailable) != 0)
        {
            opacity = UnavailableOpacity;
        }

        if ((state & ItemDisabled) != 0)
        {
            opacity = std::min(opacity, DisabledOpacity);
        }

        try
        {
            if (auto const& root = m_visuals[itemIndex].Root; root != nullptr)
            {
                root.Opacity(opacity);
                root.IsVisible(visible);
            }

            // Everything drawn beside the control rather than inside it goes with it.
            auto const each = [&](auto const& list)
                {
                    if (itemIndex < list.size())
                    {
                        ApplyTo(list[itemIndex], opacity, visible);
                    }
                };

            each(m_labels);
            each(m_labelGlows);
            each(m_valueTexts);
            each(m_pictures);
            each(m_detentTexts);
            each(m_beatTexts);
            each(m_tempoTexts);
            each(m_elapsedTexts);

            if (itemIndex < m_padNames.size())
            {
                ApplyTo(m_padNames[itemIndex].Host, opacity, visible);
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    bool SurfaceRenderer::TakesHitsAt(size_t itemIndex) const noexcept
    {
        auto const kind = KindAt(itemIndex);

        return kind != ControlKind::Panel && kind != ControlKind::Line && kind != ControlKind::Placeholder;
    }

    _Use_decl_annotations_
    void InputRouter::ReleaseItem(size_t itemIndex)
    {
        for (size_t index = 0; index < m_bindings.size(); ++index)
        {
            auto& binding = m_bindings[index];

            if (binding.ItemIndex != itemIndex)
            {
                continue;
            }

            if (binding.PlaysPads)
            {
                for (auto const& finger : binding.PadFingers)
                {
                    if (finger.PointerId != 0)
                    {
                        ReleasePad(binding, finger.PointerId);
                    }
                }
            }
            else if (binding.PlaysStrings)
            {
                for (auto const& finger : binding.FretFingers)
                {
                    if (finger.PointerId != 0)
                    {
                        ReleaseFret(binding, finger.PointerId);
                    }
                }
            }
            else
            {
                // The same as the pointer being lost: the touch ends, a held button lets go, and
                // a fader sends where it was left.
                OnCaptureLost(index);
            }

            if (binding.Element != nullptr)
            {
                binding.Element.ReleasePointerCaptures();
            }

            return;
        }
    }
}
