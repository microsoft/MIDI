// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ThemeBrushes.h"

namespace midicalculator::ui
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
            m_entries.clear();

            if (source == nullptr)
            {
                return;
            }

            for (auto const& child : source.Children())
            {
                if (auto const border = child.try_as<controls::Border>())
                {
                    m_entries.emplace(std::wstring{ border.Name() }, Entry{ border, nullptr });
                }
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    _Use_decl_annotations_
    media::Brush ThemeBrushes::Get(wchar_t const* key)
    {
        auto const found = m_entries.find(key);

        if (found == m_entries.end())
        {
            // A key missing from ThemeBrushSource in MainWindow.xaml. It still draws, but in the
            // Windows theme rather than the app's.
            LOG_HR_MSG(HRESULT_FROM_WIN32(ERROR_NOT_FOUND), "Theme brush is not published by ThemeBrushSource: %ls", key);
            return xaml::Application::Current().Resources().Lookup(winrt::box_value(key)).as<media::Brush>();
        }

        auto& entry = found->second;

        if (entry.Brush == nullptr)
        {
            entry.Brush = media::SolidColorBrush{};
            CopyInto(entry.Source, entry.Brush);
        }

        return entry.Brush;
    }

    void ThemeBrushes::Refresh() noexcept
    {
        for (auto const& [key, entry] : m_entries)
        {
            try
            {
                if (entry.Brush != nullptr)
                {
                    CopyInto(entry.Source, entry.Brush);
                }
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }
        }
    }

    _Use_decl_annotations_
    void ThemeBrushes::CopyInto(controls::Border const& source, media::SolidColorBrush const& target)
    {
        // Read every time rather than cached: XAML replaces this brush when the theme changes.
        if (auto const brush = source.Background().try_as<media::SolidColorBrush>())
        {
            target.Color(brush.Color());
            target.Opacity(brush.Opacity());
        }
    }
}
