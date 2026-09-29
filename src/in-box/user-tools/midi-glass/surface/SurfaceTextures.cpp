// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "SurfaceTextures.h"
#include "SurfaceColors.h"
#include "ThemeStore.h"
#include "resource.h"

#include <shcore.h>
#include <shlwapi.h>
#include <wincodec.h>

#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Windows.Storage.Streams.h>

#include <cmath>
#include <filesystem>
#include <fstream>

using namespace winrt::Microsoft::UI::Composition;
using namespace winrt::Windows::Foundation::Numerics;

namespace glass
{
    namespace
    {
        struct BuiltInPicture
        {
            wchar_t const* Name;
            int32_t ResourceId;
        };

        // The pictures the shipped themes name. They live inside the exe, like the themes do, so
        // nothing on disk can take a shipped theme's wall away.
        constexpr BuiltInPicture BuiltInPictures[]
        {
            { L"Off-world Colonies wall.png", IDR_THEME_OFFWORLD_WALL },
            { L"Off-world Colonies stains.png", IDR_THEME_OFFWORLD_STAINS },
        };

        // A picture repeated across a page has no need to be bigger than this, and a stranger's
        // theme naming a huge one should not cost the memory to find that out.
        constexpr uint32_t MaximumPictureSide = 2048;

        // A picture file larger than this is not read.
        constexpr uint64_t MaximumPictureBytes = 32ull * 1024ull * 1024ull;

        // GUID_WICPixelFormat32bppBGRA, GUID_ContainerFormatPng and CLSID_WICImagingFactory, written
        // out so no import library is needed for them.
        constexpr GUID StraightBgra{ 0x6fddc324, 0x4e03, 0x4bfe, { 0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0f } };
        constexpr GUID PngContainer{ 0x1b7cfaf4, 0x713f, 0x473c, { 0xbb, 0xcd, 0x61, 0x37, 0x42, 0x5f, 0xae, 0xaf } };
        constexpr CLSID ImagingFactoryClass{ 0xcacaf262, 0x9370, 0x4615, { 0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a } };

        // The fine grain's tile. Large enough that the eye does not find the repeat.
        constexpr int32_t FineGrainTile = 192;

        // A tile placed this many times at most, whatever size the page is.
        constexpr int32_t MaximumTiles = 1200;

        wil::com_ptr<IWICImagingFactory> ImagingFactory() noexcept
        {
            try
            {
                return wil::CoCreateInstance<IWICImagingFactory>(ImagingFactoryClass, CLSCTX_INPROC_SERVER);
            }
            catch (...)
            {
                return nullptr;
            }
        }

        // The picture's size, read from its encoded bytes, which are kept for the page to decode.
        std::shared_ptr<TextureImage const> FromEncoded(_Inout_ std::vector<uint8_t>& bytes) noexcept
        {
            try
            {
                if (bytes.empty() || bytes.size() > MaximumPictureBytes)
                {
                    return nullptr;
                }

                auto const factory = ImagingFactory();

                if (!factory)
                {
                    return nullptr;
                }

                UINT width{ 0 };
                UINT height{ 0 };

                {
                    wil::com_ptr<IWICStream> stream;
                    THROW_IF_FAILED(factory->CreateStream(stream.put()));
                    THROW_IF_FAILED(stream->InitializeFromMemory(bytes.data(), static_cast<DWORD>(bytes.size())));

                    wil::com_ptr<IWICBitmapDecoder> decoder;
                    THROW_IF_FAILED(factory->CreateDecoderFromStream(
                        stream.get(), nullptr, WICDecodeMetadataCacheOnDemand, decoder.put()));

                    wil::com_ptr<IWICBitmapFrameDecode> frame;
                    THROW_IF_FAILED(decoder->GetFrame(0, frame.put()));
                    THROW_IF_FAILED(frame->GetSize(&width, &height));
                }

                if (width == 0 || height == 0 || width > MaximumPictureSide || height > MaximumPictureSide)
                {
                    return nullptr;
                }

                auto image = std::make_shared<TextureImage>();

                image->Width = static_cast<int32_t>(width);
                image->Height = static_cast<int32_t>(height);
                image->Encoded = std::move(bytes);

                return image;
            }
            catch (...)
            {
                return nullptr;
            }
        }

        std::shared_ptr<TextureImage const> ReadPictureFile(_In_ std::wstring const& path) noexcept
        {
            try
            {
                std::error_code error{};
                auto const size = std::filesystem::file_size(std::filesystem::path{ path }, error);

                if (error || size == 0 || size > MaximumPictureBytes)
                {
                    return nullptr;
                }

                std::ifstream file{ std::filesystem::path{ path }, std::ios::binary };

                if (!file)
                {
                    return nullptr;
                }

                std::vector<uint8_t> bytes(static_cast<size_t>(size));
                file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                bytes.resize(static_cast<size_t>(file.gcount()));

                return FromEncoded(bytes);
            }
            catch (...)
            {
                return nullptr;
            }
        }

        std::shared_ptr<TextureImage const> ReadPictureResource(_In_ int32_t resourceId) noexcept
        {
            try
            {
                auto const module = ::GetModuleHandleW(nullptr);
                auto const found = ::FindResourceW(module, MAKEINTRESOURCEW(resourceId), RT_RCDATA);

                if (found == nullptr)
                {
                    return nullptr;
                }

                auto const size = ::SizeofResource(module, found);
                auto const loaded = ::LoadResource(module, found);
                auto const bytes = loaded != nullptr ? static_cast<uint8_t const*>(::LockResource(loaded)) : nullptr;

                if (bytes == nullptr || size == 0)
                {
                    return nullptr;
                }

                std::vector<uint8_t> copy(bytes, bytes + size);

                return FromEncoded(copy);
            }
            catch (...)
            {
                return nullptr;
            }
        }

        // Straight BGRA pixels, top row first, as a PNG.
        std::vector<uint8_t> EncodePng(
            _In_ int32_t width,
            _In_ int32_t height,
            _In_ std::vector<uint8_t> const& bgra)
        {
            auto const factory = ImagingFactory();

            if (!factory)
            {
                return {};
            }

            wil::com_ptr<IStream> stream;
            stream.attach(::SHCreateMemStream(nullptr, 0));

            if (!stream)
            {
                return {};
            }

            wil::com_ptr<IWICBitmapEncoder> encoder;
            THROW_IF_FAILED(factory->CreateEncoder(PngContainer, nullptr, encoder.put()));
            THROW_IF_FAILED(encoder->Initialize(stream.get(), WICBitmapEncoderNoCache));

            wil::com_ptr<IWICBitmapFrameEncode> frame;
            THROW_IF_FAILED(encoder->CreateNewFrame(frame.put(), nullptr));
            THROW_IF_FAILED(frame->Initialize(nullptr));
            THROW_IF_FAILED(frame->SetSize(static_cast<UINT>(width), static_cast<UINT>(height)));

            auto format = StraightBgra;
            THROW_IF_FAILED(frame->SetPixelFormat(&format));

            if (format != StraightBgra)
            {
                return {};
            }

            THROW_IF_FAILED(frame->WritePixels(
                static_cast<UINT>(height),
                static_cast<UINT>(width) * 4,
                static_cast<UINT>(bgra.size()),
                const_cast<BYTE*>(bgra.data())));
            THROW_IF_FAILED(frame->Commit());
            THROW_IF_FAILED(encoder->Commit());

            STATSTG stat{};
            THROW_IF_FAILED(stream->Stat(&stat, STATFLAG_NONAME));
            THROW_IF_FAILED(stream->Seek(LARGE_INTEGER{}, STREAM_SEEK_SET, nullptr));

            std::vector<uint8_t> bytes(static_cast<size_t>(stat.cbSize.QuadPart));

            ULONG read{ 0 };
            THROW_IF_FAILED(stream->Read(bytes.data(), static_cast<ULONG>(bytes.size()), &read));
            bytes.resize(read);

            return bytes;
        }

        int32_t BuiltInResource(_In_ std::wstring const& fileName) noexcept
        {
            for (auto const& picture : BuiltInPictures)
            {
                if (fileName == picture.Name)
                {
                    return picture.ResourceId;
                }
            }

            return 0;
        }

        uint32_t NextNoise(_Inout_ uint32_t& state) noexcept
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;

            return state;
        }
    }

    _Use_decl_annotations_
    bool IsBuiltInThemePicture(std::wstring const& fileName) noexcept
    {
        return BuiltInResource(fileName) != 0;
    }

    _Use_decl_annotations_
    std::shared_ptr<TextureImage const> LoadThemePicture(std::wstring const& fileName) noexcept
    {
        try
        {
            if (fileName.empty())
            {
                return nullptr;
            }

            static std::mutex lock{};
            static std::unordered_map<std::wstring, std::shared_ptr<TextureImage const>> cache{};

            // A file in the themes folder wins over one of the same name inside the app, and is
            // keyed by when it was written, so a picture changed on disk is read again.
            std::wstring key{};
            std::shared_ptr<TextureImage const> image{};

            auto const path = ThemePicturePath(fileName);

            if (!path.empty())
            {
                std::error_code ignored{};
                auto const written = std::filesystem::last_write_time(std::filesystem::path{ path }, ignored);

                key = path + L"|" + std::to_wstring(written.time_since_epoch().count());
            }
            else if (auto const resource = BuiltInResource(fileName); resource != 0)
            {
                key = L"built-in|" + fileName;
            }
            else
            {
                return nullptr;
            }

            {
                std::lock_guard guard{ lock };

                if (auto const found = cache.find(key); found != cache.end())
                {
                    return found->second;
                }
            }

            image = path.empty() ? ReadPictureResource(BuiltInResource(fileName)) : ReadPictureFile(path);

            if (image != nullptr)
            {
                std::lock_guard guard{ lock };
                cache.emplace(key, image);
            }

            return image;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    _Use_decl_annotations_
    std::shared_ptr<TextureImage const> FineGrainImage(Theme const& theme) noexcept
    {
        try
        {
            auto const strength = std::clamp(theme.Overlay.GrainPercent, 0, 100) / 10.0;

            if (strength <= 0.0)
            {
                return nullptr;
            }

            // What the grain lightens and darkens: the middle of the deck.
            auto const middle = BlendOver(theme.Deck.Color, theme.Deck.GradientEndColor, 0.5);
            auto const deck = (middle.R + middle.G + middle.B) / 3.0;

            ThemeColor light{ 255, 255, 255, 255 };

            if (theme.Overlay.GrainColor.A != 0)
            {
                light = theme.Overlay.GrainColor;
                light.A = 255;
            }

            auto const lightLevel = (light.R + light.G + light.B) / 3.0;

            auto const key = (static_cast<uint64_t>(theme.Overlay.GrainPercent) << 40) |
                (static_cast<uint64_t>(std::lround(deck)) << 32) |
                (static_cast<uint64_t>(light.R) << 16) |
                (static_cast<uint64_t>(light.G) << 8) |
                static_cast<uint64_t>(light.B);

            static std::mutex lock{};
            static std::unordered_map<uint64_t, std::shared_ptr<TextureImage const>> cache{};

            {
                std::lock_guard guard{ lock };

                if (auto const found = cache.find(key); found != cache.end())
                {
                    return found->second;
                }
            }

            constexpr int32_t size = FineGrainTile;

            // White noise, softened by a plus shaped blur with the middle four times its neighbors.
            // That leaves one pixel related to the next about the way stippled plastic and paper
            // are: the step from one pixel to its neighbor about 1.1 times the spread.
            std::vector<double> noise(static_cast<size_t>(size) * size);
            uint32_t state = 0x2545F491u;

            for (auto& value : noise)
            {
                value = (NextNoise(state) & 0xFFFFFF) / static_cast<double>(0x1000000);
            }

            auto const at = [&noise](int32_t x, int32_t y)
                {
                    x = (x + size) % size;
                    y = (y + size) % size;

                    return noise[static_cast<size_t>(y) * size + static_cast<size_t>(x)];
                };

            // The spread of the blurred noise: a uniform's, through the blur's weights.
            auto const spread = (1.0 / std::sqrt(12.0)) * std::sqrt(20.0 / 64.0);

            std::vector<uint8_t> bgra(static_cast<size_t>(size) * size * 4);

            for (int32_t y = 0; y < size; ++y)
            {
                for (int32_t x = 0; x < size; ++x)
                {
                    auto const blurred =
                        (4.0 * at(x, y) + at(x - 1, y) + at(x + 1, y) + at(x, y - 1) + at(x, y + 1)) / 8.0;

                    auto const levels = std::clamp((blurred - 0.5) / spread * strength, -4.0 * strength, 4.0 * strength);

                    auto* pixel = &bgra[(static_cast<size_t>(y) * size + static_cast<size_t>(x)) * 4];

                    if (levels >= 0.0)
                    {
                        // Toward the light by exactly this many levels, whatever the deck is.
                        auto const alpha = std::clamp(levels / std::max(1.0, lightLevel - deck), 0.0, 1.0);

                        pixel[0] = light.B;
                        pixel[1] = light.G;
                        pixel[2] = light.R;
                        pixel[3] = static_cast<uint8_t>(std::lround(255.0 * alpha));
                    }
                    else
                    {
                        auto const alpha = std::clamp(-levels / std::max(1.0, deck), 0.0, 1.0);

                        pixel[0] = 0;
                        pixel[1] = 0;
                        pixel[2] = 0;
                        pixel[3] = static_cast<uint8_t>(std::lround(255.0 * alpha));
                    }
                }
            }

            auto image = std::make_shared<TextureImage>();

            image->Width = size;
            image->Height = size;
            image->Encoded = EncodePng(size, size, bgra);

            if (image->Encoded.empty())
            {
                return nullptr;
            }

            std::lock_guard guard{ lock };
            cache.emplace(key, image);

            return image;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    _Use_decl_annotations_
    CompositionSurfaceBrush MakeTextureBrush(Compositor const& compositor, TextureImage const& image)
    {
        // Decoded by XAML. A second graphics device on XAML's compositor stops every window drawing.
        wil::com_ptr<IStream> bytes;
        bytes.attach(::SHCreateMemStream(image.Encoded.data(), static_cast<UINT>(image.Encoded.size())));
        THROW_HR_IF_NULL(E_OUTOFMEMORY, bytes);

        winrt::com_ptr<::IUnknown> unknown;
        THROW_IF_FAILED(::CreateRandomAccessStreamOverStream(bytes.get(), BSOS_DEFAULT, IID_PPV_ARGS(unknown.put())));

        auto const surface = winrt::Microsoft::UI::Xaml::Media::LoadedImageSurface::StartLoadFromStream(
            unknown.as<winrt::Windows::Storage::Streams::IRandomAccessStream>());

        return compositor.CreateSurfaceBrush(surface);
    }

    _Use_decl_annotations_
    ContainerVisual BuildTiles(
        Compositor const& compositor,
        CompositionBrush const& brush,
        float tileWidth,
        float tileHeight,
        float width,
        float height)
    {
        auto root = compositor.CreateContainerVisual();
        root.Size(float2{ width, height });

        if (brush == nullptr || tileWidth < 1.0f || tileHeight < 1.0f)
        {
            return root;
        }

        auto const across = static_cast<int32_t>(std::ceil(width / tileWidth));
        auto const down = static_cast<int32_t>(std::ceil(height / tileHeight));

        auto placed = 0;

        for (int32_t row = 0; row < down && placed < MaximumTiles; ++row)
        {
            for (int32_t column = 0; column < across && placed < MaximumTiles; ++column)
            {
                auto sprite = compositor.CreateSpriteVisual();
                sprite.Size(float2{ tileWidth, tileHeight });
                sprite.Offset(float3{ column * tileWidth, row * tileHeight, 0.0f });
                sprite.Brush(brush);

                root.Children().InsertAtTop(sprite);
                ++placed;
            }
        }

        return root;
    }
}
