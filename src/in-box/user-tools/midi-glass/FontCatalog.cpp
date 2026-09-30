// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The fonts a label can be set in, read from DirectWrite, which is what XAML draws text with, so
// a family this says is installed is one XAML will find.

#include "pch.h"
#include "FontCatalog.h"
#include "LayoutModel.h"

#include <dwrite.h>

namespace midiglass::fonts
{
    namespace
    {
        std::mutex& CollectionLock() noexcept
        {
            static std::mutex lock{};
            return lock;
        }

        wil::com_ptr<IDWriteFontCollection>& CachedCollection() noexcept
        {
            static wil::com_ptr<IDWriteFontCollection> collection{};
            return collection;
        }

        wil::com_ptr<IDWriteFontCollection> SystemCollection(_In_ bool refresh)
        {
            std::scoped_lock guard{ CollectionLock() };

            auto& cached = CachedCollection();

            if (cached == nullptr || refresh)
            {
                wil::com_ptr<IDWriteFactory> factory{};

                THROW_IF_FAILED(::DWriteCreateFactory(
                    DWRITE_FACTORY_TYPE_SHARED,
                    __uuidof(IDWriteFactory),
                    reinterpret_cast<IUnknown**>(factory.put())));

                wil::com_ptr<IDWriteFontCollection> collection{};

                THROW_IF_FAILED(factory->GetSystemFontCollection(collection.put(), refresh ? TRUE : FALSE));

                cached = collection;
            }

            return cached;
        }

        // The English name where the family has one. A layout goes to PCs whose Windows speaks
        // other languages, and every one of them knows a family by its English name as well.
        std::wstring StoredName(_In_ IDWriteFontFamily* family)
        {
            wil::com_ptr<IDWriteLocalizedStrings> names{};

            if (FAILED(family->GetFamilyNames(names.put())) || names == nullptr)
            {
                return {};
            }

            UINT32 index{ 0 };
            BOOL exists{ FALSE };

            if (FAILED(names->FindLocaleName(L"en-us", &index, &exists)) || !exists)
            {
                index = 0;
            }

            UINT32 length{ 0 };

            if (FAILED(names->GetStringLength(index, &length)) || length == 0 || length > 256)
            {
                return {};
            }

            std::wstring name(static_cast<size_t>(length) + 1, L'\0');

            if (FAILED(names->GetString(index, name.data(), length + 1)))
            {
                return {};
            }

            name.resize(length);

            return name;
        }
    }

    std::vector<std::wstring> const& InBoxFamilies() noexcept
    {
        static std::vector<std::wstring> const families
        {
            L"Segoe UI Variable Text",
            L"Segoe UI Variable Display",
            L"Segoe UI",
            L"Bahnschrift",
            L"Cascadia Mono",
            L"Consolas",
            L"Segoe UI Emoji",
        };

        return families;
    }

    _Use_decl_annotations_
    std::vector<std::wstring> InstalledFamilies(bool refresh)
    {
        std::vector<std::wstring> families{};

        try
        {
            auto const collection = SystemCollection(refresh);

            auto const count = collection->GetFontFamilyCount();

            families.reserve(count);

            for (UINT32 index = 0; index < count; ++index)
            {
                wil::com_ptr<IDWriteFontFamily> family{};

                if (FAILED(collection->GetFontFamily(index, family.put())) || family == nullptr)
                {
                    continue;
                }

                auto name = StoredName(family.get());

                // A name a layout could not store is not offered, and a family whose name starts
                // with @ is the sideways copy of an East Asian font for vertical text.
                if (!glass::IsSafeFontFamilyName(name) || name.front() == L'@')
                {
                    continue;
                }

                families.push_back(std::move(name));
            }

            auto const lessThan = [](std::wstring const& left, std::wstring const& right)
                {
                    return ::CompareStringOrdinal(
                        left.c_str(), static_cast<int>(left.size()),
                        right.c_str(), static_cast<int>(right.size()), TRUE) == CSTR_LESS_THAN;
                };

            auto const same = [](std::wstring const& left, std::wstring const& right)
                {
                    return ::CompareStringOrdinal(
                        left.c_str(), static_cast<int>(left.size()),
                        right.c_str(), static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
                };

            std::sort(families.begin(), families.end(), lessThan);
            families.erase(std::unique(families.begin(), families.end(), same), families.end());
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();

            // The families that are always there are better than an empty list.
            families = InBoxFamilies();
        }

        return families;
    }

    _Use_decl_annotations_
    bool IsInstalled(std::wstring const& family) noexcept
    {
        if (family.empty())
        {
            return true;
        }

        try
        {
            auto const collection = SystemCollection(false);

            UINT32 index{ 0 };
            BOOL exists{ FALSE };

            if (FAILED(collection->FindFamilyName(family.c_str(), &index, &exists)))
            {
                return true;
            }

            return exists != FALSE;
        }
        catch (...)
        {
            return true;
        }
    }

    _Use_decl_annotations_
    media::FontFamily FamilyFor(std::wstring const& family)
    {
        if (family.empty() || !glass::IsSafeFontFamilyName(family))
        {
            return media::FontFamily{ DefaultFamily };
        }

        return media::FontFamily{ winrt::hstring{ family + L", " + DefaultFamily } };
    }
}
