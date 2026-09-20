// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ThemeBrushes.h"

namespace midipatchbay
{
    ThemeBrushes& ThemeBrushes::Current() noexcept
    {
        static ThemeBrushes instance{};

        return instance;
    }

    _Use_decl_annotations_
    void ThemeBrushes::Initialize(controls::Panel const& source) noexcept
    {
        try
        {
            m_sources.clear();

            if (source == nullptr)
            {
                return;
            }

            for (auto const& child : source.Children())
            {
                if (auto const border = child.try_as<controls::Border>())
                {
                    m_sources.emplace_back(std::wstring{ border.Name() }, border);
                }
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    _Use_decl_annotations_
    media::Brush ThemeBrushes::Get(std::wstring_view name) const noexcept
    {
        try
        {
            for (auto const& [key, border] : m_sources)
            {
                if (key == name)
                {
                    // Read rather than cache: XAML replaces this when the theme changes.
                    return border.Background();
                }
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return nullptr;
    }
}
