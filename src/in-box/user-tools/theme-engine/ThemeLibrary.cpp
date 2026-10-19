// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include <windows.h>
#undef GetObject

#include "ThemeLibrary.h"

#include <format>

namespace themeengine
{
    _Use_decl_annotations_
    std::wstring Utf8ToWide(std::string_view text)
    {
        if (text.size() >= 3 && static_cast<uint8_t>(text[0]) == 0xEF && static_cast<uint8_t>(text[1]) == 0xBB && static_cast<uint8_t>(text[2]) == 0xBF)
        {
            text.remove_prefix(3);
        }

        if (text.empty() || text.size() > static_cast<size_t>(INT32_MAX))
        {
            return {};
        }

        auto const length = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);

        if (length <= 0)
        {
            return {};
        }

        std::wstring wide(static_cast<size_t>(length), L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
        return wide;
    }

    _Use_decl_annotations_
    ThemeLoadResult LoadThemeFromResource(uint16_t resourceId, void* module)
    {
        ThemeLoadResult result{};

        try
        {
            auto const instance = module != nullptr ? static_cast<HMODULE>(module) : ::GetModuleHandleW(nullptr);
            auto const found = ::FindResourceW(instance, MAKEINTRESOURCEW(resourceId), MAKEINTRESOURCEW(10)); // RT_RCDATA

            if (found == nullptr)
            {
                result.Problems.push_back(std::format(L"theme: There's no theme resource {}.", resourceId));
                return result;
            }

            auto const loaded = ::LoadResource(instance, found);
            auto const size = ::SizeofResource(instance, found);
            auto const bytes = loaded != nullptr ? static_cast<char const*>(::LockResource(loaded)) : nullptr;

            if (bytes == nullptr || size == 0)
            {
                result.Problems.push_back(std::format(L"theme: Theme resource {} is empty.", resourceId));
                return result;
            }

            return LoadTheme(Utf8ToWide({ bytes, size }));
        }
        catch (...)
        {
            result.Problems.push_back(std::format(L"theme: Theme resource {} couldn't be read.", resourceId));
            return result;
        }
    }

    _Use_decl_annotations_
    std::vector<std::shared_ptr<ThemeDefinition const>> LoadThemesFromResources(
        std::span<uint16_t const> resourceIds,
        std::function<void(uint16_t resourceId, std::wstring const& problem)> const& onProblem)
    {
        std::vector<std::shared_ptr<ThemeDefinition const>> themes{};

        for (auto const id : resourceIds)
        {
            auto const result = LoadThemeFromResource(id);

            if (onProblem)
            {
                for (auto const& problem : result.Problems)
                {
                    onProblem(id, problem);
                }
            }

            if (result.Theme != nullptr && FindTheme(themes, result.Theme->Id) != nullptr && FindTheme(themes, result.Theme->Id)->Id == result.Theme->Id)
            {
                if (onProblem)
                {
                    onProblem(id, std::format(L"id: Another theme already has the id \"{}\".", result.Theme->Id));
                }

                continue;
            }

            if (result.Theme != nullptr)
            {
                themes.push_back(result.Theme);
            }
        }

        return themes;
    }

    _Use_decl_annotations_
    std::shared_ptr<ThemeDefinition const> FindTheme(std::vector<std::shared_ptr<ThemeDefinition const>> const& themes, std::wstring_view id) noexcept
    {
        for (auto const& theme : themes)
        {
            if (theme != nullptr && theme->Id == id)
            {
                return theme;
            }
        }

        return themes.empty() ? nullptr : themes.front();
    }
}
