// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ThemeBrushes.h"

namespace midiapp
{
    namespace
    {
        namespace wux = winrt::Microsoft::UI::Xaml;
        namespace wuxc = winrt::Microsoft::UI::Xaml::Controls;
        namespace wuxm = winrt::Microsoft::UI::Xaml::Media;
    }

    ThemeBrushes& ThemeBrushes::Current() noexcept
    {
        static ThemeBrushes instance{};

        return instance;
    }

    _Use_decl_annotations_
    void ThemeBrushes::Initialize(wuxc::Panel const& source) noexcept
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
                if (auto const border = child.try_as<wuxc::Border>())
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
    wuxm::Brush ThemeBrushes::Get(wchar_t const* key)
    {
        auto const found = m_entries.find(key);

        if (found == m_entries.end())
        {
            // A key missing from the window's brush source panel. It still draws, but in the
            // Windows theme rather than the tool's.
            LOG_HR_MSG(HRESULT_FROM_WIN32(ERROR_NOT_FOUND), "Theme brush is not published by the window: %ls", key);
            return wux::Application::Current().Resources().Lookup(winrt::box_value(key)).as<wuxm::Brush>();
        }

        auto& entry = found->second;

        if (entry.Brush == nullptr)
        {
            entry.Brush = wuxm::SolidColorBrush{};
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
    void ThemeBrushes::CopyInto(wuxc::Border const& source, wuxm::SolidColorBrush const& target)
    {
        // Read every time rather than cached: XAML replaces this brush when the theme changes.
        if (auto const brush = source.Background().try_as<wuxm::SolidColorBrush>())
        {
            target.Color(brush.Color());
            target.Opacity(brush.Opacity());
        }
    }
}