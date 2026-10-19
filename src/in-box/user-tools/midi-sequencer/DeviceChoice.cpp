// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "DeviceChoice.h"
#include "DeviceChoice.g.cpp"

namespace midisequencer
{
    namespace
    {
        bool IsSvg(std::wstring const& path) noexcept
        {
            constexpr std::wstring_view extension{ L".svg" };

            if (path.size() < extension.size())
            {
                return false;
            }

            return ::CompareStringOrdinal(path.data() + path.size() - extension.size(), static_cast<int>(extension.size()),
                extension.data(), static_cast<int>(extension.size()), TRUE) == CSTR_EQUAL;
        }
    }

    _Use_decl_annotations_
    media::ImageSource DevicePicture(std::wstring const& path, int32_t size) noexcept
    {
        if (path.empty() || size <= 0)
        {
            return nullptr;
        }

        try
        {
            // Never destroyed: XAML objects mustn't be released after XAML has shut down, which
            // is when a static's destructor would run.
            static auto* const cache = new std::unordered_map<std::wstring, media::ImageSource>();

            // Twice the size, so a picture stays sharp up to 200% scaling.
            auto const pixels = size * 2;
            auto const key = std::to_wstring(pixels) + L"|" + path;

            if (auto const found = cache->find(key); found != cache->end())
            {
                return found->second;
            }

            foundation::Uri const uri{ L"file:///" + winrt::hstring{ path } };
            media::ImageSource source{ nullptr };

            // BitmapImage can't draw SVG, and the transports' default pictures are SVG.
            if (IsSvg(path))
            {
                media::Imaging::SvgImageSource svg{};
                svg.RasterizePixelHeight(pixels);
                svg.UriSource(uri);
                source = svg;
            }
            else
            {
                media::Imaging::BitmapImage bitmap{};
                bitmap.DecodePixelHeight(pixels);
                bitmap.UriSource(uri);
                source = bitmap;
            }

            // A PC has a handful of pictures. This only stops paths that keep changing from
            // growing it without end.
            if (cache->size() >= 256)
            {
                cache->clear();
            }

            cache->emplace(key, source);
            return source;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return nullptr;
    }
}
