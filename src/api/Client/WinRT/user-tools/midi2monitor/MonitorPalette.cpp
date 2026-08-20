// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MonitorPalette.h"

namespace midi2monitor
{
    namespace
    {
        std::unordered_map<std::wstring, media::Brush> g_brushCache{};

        media::Brush LookupBrush(std::wstring const& key) noexcept
        {
            try
            {
                auto const cached = g_brushCache.find(key);

                if (cached != g_brushCache.end())
                {
                    return cached->second;
                }

                auto application = xaml::Application::Current();

                if (application == nullptr)
                {
                    return nullptr;
                }

                auto resources = application.Resources();
                auto const boxedKey = winrt::box_value(winrt::hstring{ key });

                if (!resources.HasKey(boxedKey))
                {
                    return nullptr;
                }

                auto brush = resources.Lookup(boxedKey).try_as<media::Brush>();

                if (brush != nullptr)
                {
                    g_brushCache.emplace(key, brush);
                }

                return brush;
            }
            MIDI_MONITOR_CATCH_AND_LOG(L"Unable to resolve a palette brush.")

            return nullptr;
        }
    }

    void MonitorPalette::Invalidate() noexcept
    {
        g_brushCache.clear();
    }

    _Use_decl_annotations_
    media::Brush MonitorPalette::MessageTypeBackground(uint32_t colorIndex) noexcept
    {
        return LookupBrush(std::format(L"MidiMessageType{}BackgroundBrush", colorIndex % MessageTypeColorCount));
    }

    _Use_decl_annotations_
    media::Brush MonitorPalette::MessageTypeForeground(uint32_t colorIndex) noexcept
    {
        return LookupBrush(std::format(L"MidiMessageType{}ForegroundBrush", colorIndex % MessageTypeColorCount));
    }

    _Use_decl_annotations_
    media::Brush MonitorPalette::RowBackground(bool alternate) noexcept
    {
        return LookupBrush(alternate ? L"MidiRowAlternateBackgroundBrush" : L"MidiRowBackgroundBrush");
    }
}
