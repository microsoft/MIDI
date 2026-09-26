// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "StringResources.h"

using namespace winrt::Microsoft::Windows::ApplicationModel::Resources;

namespace midiglass::resources
{
    namespace
    {
        std::once_flag g_loaderInitialized;
        ResourceLoader g_loader{ nullptr };

        ResourceLoader const& Loader() noexcept
        {
            std::call_once(g_loaderInitialized, []() noexcept
                {
                    try
                    {
                        g_loader = ResourceLoader();
                    }
                    MIDI_GLASS_CATCH_AND_LOG(L"Unable to create the resource loader. Falling back to resource keys.")
                });

            return g_loader;
        }
    }

    _Use_decl_annotations_
    winrt::hstring GetString(std::wstring_view resourceKey) noexcept
    {
        try
        {
            auto const& loader = Loader();

            if (loader != nullptr)
            {
                auto value = loader.GetString(resourceKey);

                if (!value.empty())
                {
                    return value;
                }
            }
        }
        catch (...)
        {
            // a missing resource is not worth an error event on every call
        }

        return winrt::hstring{ resourceKey };
    }

    _Use_decl_annotations_
    std::wstring DescribeFileSize(uint64_t bytes) noexcept
    {
        if (bytes < 1024)
        {
            return std::wstring{ FormatString(L"SizeBytesFormat", std::to_wstring(bytes)) };
        }

        if (bytes < 1024 * 1024)
        {
            return std::wstring{ FormatString(
                L"SizeKilobytesFormat", std::to_wstring((bytes + 512) / 1024)) };
        }

        if (bytes < 1024ull * 1024 * 1024)
        {
            return std::wstring{ FormatString(
                L"SizeMegabytesFormat", std::to_wstring((bytes + 512 * 1024) / (1024 * 1024))) };
        }

        auto const tenths = (bytes * 10 + 512ull * 1024 * 1024) / (1024ull * 1024 * 1024);

        return std::wstring{ FormatString(
            L"SizeGigabytesFormat",
            std::to_wstring(tenths / 10) + L"." + std::to_wstring(tenths % 10)) };
    }
}
