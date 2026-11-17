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
    void ThemeBrushes::AddSource(wuxc::Panel const& source) noexcept
    {
        try
        {
            if (source != nullptr)
            {
                m_sources.push_back(source);
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    _Use_decl_annotations_
    void ThemeBrushes::RemoveSource(wuxc::Panel const& source) noexcept
    {
        try
        {
            m_sources.erase(std::remove(m_sources.begin(), m_sources.end(), source), m_sources.end());

            if (m_sources.empty())
            {
                m_brushes.clear();
            }
            else
            {
                // The colors may have come from the window that just closed.
                Refresh();
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
        if (auto const found = m_brushes.find(key); found != m_brushes.end())
        {
            return found->second;
        }

        auto const source = FindSource(key);

        if (source == nullptr)
        {
            // A key missing from the window's brush source panel. It still draws, but in the
            // Windows theme rather than the tool's.
            LOG_HR_MSG(HRESULT_FROM_WIN32(ERROR_NOT_FOUND), "Theme brush is not published by any window: %ls", key);
            return wux::Application::Current().Resources().Lookup(winrt::box_value(key)).as<wuxm::Brush>();
        }

        wuxm::SolidColorBrush brush{};
        CopyInto(source, brush);
        m_brushes.emplace(key, brush);

        return brush;
    }

    void ThemeBrushes::Refresh() noexcept
    {
        for (auto const& [key, brush] : m_brushes)
        {
            try
            {
                if (auto const source = FindSource(key))
                {
                    CopyInto(source, brush);
                }
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }
        }
    }

    _Use_decl_annotations_
    wuxc::Border ThemeBrushes::FindSource(std::wstring_view key) const
    {
        for (auto source = m_sources.rbegin(); source != m_sources.rend(); ++source)
        {
            for (auto const& child : source->Children())
            {
                if (auto const border = child.try_as<wuxc::Border>(); border != nullptr && border.Name() == key)
                {
                    return border;
                }
            }
        }

        return nullptr;
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