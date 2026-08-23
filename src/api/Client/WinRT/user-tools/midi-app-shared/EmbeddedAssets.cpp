// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "EmbeddedAssets.h"

#include <shlwapi.h>
#include <shcore.h>

namespace streams = ::winrt::Windows::Storage::Streams;
namespace imaging = ::winrt::Microsoft::UI::Xaml::Media::Imaging;

// the linker supplies this; it is the HINSTANCE of the module holding the resource
extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace midiapp
{
    _Use_decl_annotations_
    void EmbeddedAssets::SetSvgFromResource(
        winrt::Microsoft::UI::Xaml::Controls::Image const& target,
        uint16_t const resourceId) noexcept
    {
        try
        {
            if (target == nullptr)
            {
                return;
            }

            auto const instance = reinterpret_cast<HINSTANCE>(&__ImageBase);

            auto const found = ::FindResourceW(instance, MAKEINTRESOURCEW(resourceId), RT_RCDATA);

            if (found == nullptr)
            {
                return;
            }

            auto const sizeBytes = ::SizeofResource(instance, found);
            auto const loaded = ::LoadResource(instance, found);

            if (sizeBytes == 0 || loaded == nullptr)
            {
                return;
            }

            auto const bytes = ::LockResource(loaded);

            if (bytes == nullptr)
            {
                return;
            }

            // A resource is already in memory and stays mapped for the life of the process, but
            // SHCreateMemStream copies rather than aliasing it, which keeps the stream's lifetime
            // independent of anything here.
            wil::com_ptr_nothrow<IStream> memory{
                ::SHCreateMemStream(static_cast<BYTE const*>(bytes), sizeBytes) };

            if (!memory)
            {
                return;
            }

            winrt::com_ptr<::IUnknown> unknown{};

            if (FAILED(::CreateRandomAccessStreamOverStream(
                memory.get(),
                BSOS_DEFAULT,
                winrt::guid_of<streams::IRandomAccessStream>(),
                unknown.put_void())))
            {
                return;
            }

            imaging::SvgImageSource source{};

            auto const scale = target.XamlRoot() != nullptr ? target.XamlRoot().RasterizationScale() : 1.0;

            auto const width = target.Width();
            auto const height = target.Height();

            // Without this the art renders at whatever tiny size a 100% x 100% svg reports, and
            // Stretch has nothing to scale up from.
            if (width > 0 && height > 0)
            {
                source.RasterizePixelWidth(width * scale);
                source.RasterizePixelHeight(height * scale);
            }

            // Deliberately not awaited. The caller is on the UI thread and the image element
            // redraws itself once the decode lands.
            source.SetSourceAsync(unknown.as<streams::IRandomAccessStream>());

            target.Source(source);
        }
        catch (...)
        {
        }
    }
}
