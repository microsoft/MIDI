// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Root and scale pickers on a running surface: a pick redraws every pad grid that has a key in
// the key picked, and moves every picker on the page to show it. Nothing is sent from here.

#include "pch.h"
#include "SurfaceRenderer.h"
#include "PadGrid.h"

namespace glass
{
    _Use_decl_annotations_
    std::vector<std::pair<size_t, double>> SurfaceRenderer::ApplyPickedKey(
        PickedKey const& picked,
        LayoutDocument const& document,
        Theme const& theme)
    {
        m_pickedKey = picked;

        std::vector<std::pair<size_t, double>> pickers{};

        auto const shown = ShownKey(document, picked);

        for (size_t item = 0; item < m_visuals.size(); ++item)
        {
            auto const* const control = document.ControlAtIndex(ControlIndexOf(item));

            if (control == nullptr)
            {
                continue;
            }

            // The grid's own spec goes in. The picked key is laid over it as it is drawn.
            if (IsPadGrid(control->Kind) && control->Pads.KeyRoot != NoKey)
            {
                ResizeItem(item, *control, theme);
            }
            else if (IsKeyPicker(*control))
            {
                auto const value = PickerValue(*control, shown);

                SetValue(item, value);
                pickers.emplace_back(item, value);
            }
        }

        return pickers;
    }
}
