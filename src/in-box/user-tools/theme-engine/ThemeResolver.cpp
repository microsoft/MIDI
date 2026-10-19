// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "ThemeResolver.h"

#include <format>
#include <set>

namespace themeengine
{
    namespace
    {
        constexpr int32_t MaximumDepth = 32;
        constexpr size_t MaximumCachedParts = 8192;

        constexpr wchar_t Separator = L'\x1F';
    }

    // Looks names up in the theme: its colors, "tint", "track", and its numbers.
    class ResolvedTheme::Scope final : public ExpressionScope
    {
    public:
        Scope(ResolvedTheme const& theme, std::optional<Color> track, int32_t depth = 0) noexcept :
            m_theme(theme), m_track(track), m_depth(depth)
        {
        }

        std::optional<Color> NamedColor(std::wstring_view name) const noexcept override
        {
            if (name == L"track")
            {
                return m_track.value_or(Color{ 0, 0, 0, 1 });
            }

            if (name == L"tint")
            {
                return m_theme.m_tint.value_or(Color{ 0, 0, 0, 1 });
            }

            if (auto const found = m_theme.m_tokens.find(name); found != m_theme.m_tokens.end())
            {
                return found->second;
            }

            // One that depends on the track, or that hasn't been worked out yet.
            auto const expression = m_theme.FindExpression(name);

            if (expression == nullptr)
            {
                return std::nullopt;
            }

            if (m_depth > MaximumDepth)
            {
                return Color{ 0, 0, 0, 0 };
            }

            Scope const deeper{ m_theme, m_track, m_depth + 1 };
            return expression->Evaluate(deeper);
        }

        std::optional<double> NamedNumber(std::wstring_view name) const noexcept override
        {
            if (m_theme.m_mode == nullptr)
            {
                return std::nullopt;
            }

            auto const found = m_theme.m_mode->Numbers.find(name);
            return found != m_theme.m_mode->Numbers.end() ? std::optional<double>{ found->second } : std::nullopt;
        }

    private:
        ResolvedTheme const& m_theme;
        std::optional<Color> m_track{};
        int32_t m_depth{ 0 };
    };

    _Use_decl_annotations_
    std::shared_ptr<ResolvedTheme const> ResolvedTheme::Resolve(std::shared_ptr<ThemeDefinition const> theme, bool dark, std::optional<Color> tint)
    {
        std::shared_ptr<ResolvedTheme> result{ new ResolvedTheme() };

        if (theme == nullptr)
        {
            theme = std::make_shared<ThemeDefinition>();
        }

        result->m_theme = std::move(theme);
        result->m_mode = &result->m_theme->Mode(dark);
        result->m_dark = dark;

        if (tint.has_value() && result->m_theme->WindowColor == WindowColorUse::Material)
        {
            result->m_tint = Color{ tint->R, tint->G, tint->B, 1.0 };
        }

        // Which colors depend on the track, directly or through another color.
        std::set<std::wstring, std::less<>> dynamic{};
        std::map<std::wstring, bool, std::less<>> known{};

        std::function<bool(std::wstring_view, int32_t)> usesTrack = [&](std::wstring_view name, int32_t depth) -> bool
        {
            if (auto const found = known.find(name); found != known.end())
            {
                return found->second;
            }

            auto const expression = result->FindExpression(name);

            if (expression == nullptr || depth > MaximumDepth)
            {
                return false;
            }

            known.emplace(std::wstring{ name }, false);

            std::vector<std::wstring> colors{};
            std::vector<std::wstring> numbers{};
            expression->CollectNames(colors, numbers);

            auto uses = false;

            for (auto const& next : colors)
            {
                if (next == L"track" || (next != L"tint" && usesTrack(next, depth + 1)))
                {
                    uses = true;
                    break;
                }
            }

            known.insert_or_assign(std::wstring{ name }, uses);
            return uses;
        };

        for (auto const& [name, expression] : result->m_mode->Tokens)
        {
            if (usesTrack(name, 0))
            {
                dynamic.insert(name);
            }
        }

        for (auto const& [name, expression] : result->m_mode->Tokens)
        {
            if (dynamic.contains(name))
            {
                continue;
            }

            Scope const scope{ *result, std::nullopt };
            auto const value = result->FindExpression(name)->Evaluate(scope);
            result->m_tokens.insert_or_assign(name, value);
        }

        return result;
    }

    _Use_decl_annotations_
    ColorExpression const* ResolvedTheme::FindExpression(std::wstring_view name) const noexcept
    {
        if (m_mode == nullptr)
        {
            return nullptr;
        }

        if (m_tint.has_value())
        {
            if (auto const tinted = m_mode->Tinted.find(name); tinted != m_mode->Tinted.end())
            {
                return &tinted->second;
            }
        }

        auto const found = m_mode->Tokens.find(name);
        return found != m_mode->Tokens.end() ? &found->second : nullptr;
    }

    _Use_decl_annotations_
    bool ResolvedTheme::HasToken(std::wstring_view name) const noexcept
    {
        return FindExpression(name) != nullptr;
    }

    _Use_decl_annotations_
    Color ResolvedTheme::Token(std::wstring_view name, Color const& fallback) const
    {
        if (auto const found = m_tokens.find(name); found != m_tokens.end())
        {
            return found->second;
        }

        return Token(name, Color{ 0, 0, 0, 1 }, fallback);
    }

    _Use_decl_annotations_
    Color ResolvedTheme::Token(std::wstring_view name, Color const& track, Color const& fallback) const
    {
        if (auto const found = m_tokens.find(name); found != m_tokens.end())
        {
            return found->second;
        }

        auto const expression = FindExpression(name);

        if (expression == nullptr)
        {
            return fallback;
        }

        Scope const scope{ *this, track };
        return expression->Evaluate(scope);
    }

    _Use_decl_annotations_
    double ResolvedTheme::Number(std::wstring_view name, double fallback) const
    {
        auto const found = m_mode->Numbers.find(name);
        return found != m_mode->Numbers.end() ? found->second : fallback;
    }

    _Use_decl_annotations_
    double ResolvedTheme::Metric(std::wstring_view name, double fallback) const
    {
        auto const found = m_mode->Metrics.find(name);
        return found != m_mode->Metrics.end() ? found->second : fallback;
    }

    _Use_decl_annotations_
    std::wstring ResolvedTheme::Font(std::wstring_view name, std::wstring_view fallback) const
    {
        auto const found = m_mode->Fonts.find(name);
        return found != m_mode->Fonts.end() ? found->second : std::wstring{ fallback };
    }

    _Use_decl_annotations_
    Color ResolvedTheme::TrackColor(uint32_t rgb) const
    {
        rgb &= 0xFFFFFF;

        if (auto const found = m_mode->TrackColors.find(rgb); found != m_mode->TrackColors.end())
        {
            Scope const scope{ *this, std::nullopt };
            return found->second.Evaluate(scope);
        }

        return Color::FromArgb(0xFF000000u | rgb);
    }

    _Use_decl_annotations_
    bool ResolvedTheme::HasPart(std::wstring_view name) const noexcept
    {
        return m_mode->Parts.find(name) != m_mode->Parts.end();
    }

    _Use_decl_annotations_
    std::shared_ptr<ResolvedStyle const> ResolvedTheme::Part(
        std::wstring_view name,
        std::initializer_list<std::wstring_view> states,
        std::optional<Color> track) const
    {
        return Part(name, std::span<std::wstring_view const>{ states.begin(), states.size() }, track);
    }

    _Use_decl_annotations_
    std::shared_ptr<ResolvedStyle const> ResolvedTheme::Part(
        std::wstring_view name,
        std::span<std::wstring_view const> states,
        std::optional<Color> track) const
    {
        std::wstring key{ name };

        for (auto const state : states)
        {
            key.push_back(Separator);
            key.append(state);
        }

        key.push_back(Separator);

        if (track.has_value())
        {
            key.append(std::format(L"{:08X}", track->ToArgb()));
        }

        std::scoped_lock lock{ m_partsLock };

        if (auto const found = m_parts.find(key); found != m_parts.end())
        {
            return found->second;
        }

        auto const part = m_mode->Parts.find(name);

        if (part == m_mode->Parts.end())
        {
            auto empty = std::make_shared<ResolvedStyle const>();
            m_parts.emplace(std::move(key), empty);
            return empty;
        }

        StyleSpec spec = part->second.Base;

        for (auto const state : states)
        {
            if (auto const found = part->second.States.find(state); found != part->second.States.end())
            {
                spec.MergeFrom(found->second);
            }
        }

        Scope const scope{ *this, track };
        auto resolved = std::make_shared<ResolvedStyle const>(ResolveStyle(spec, scope));

        if (m_parts.size() >= MaximumCachedParts)
        {
            m_parts.clear();
        }

        m_parts.emplace(std::move(key), resolved);
        return resolved;
    }

    std::vector<ResolvedResource> ResolvedTheme::Resources() const
    {
        std::vector<ResolvedResource> resources{};
        Scope const scope{ *this, std::nullopt };

        for (auto const& [key, layer] : m_mode->Resources)
        {
            StyleSpec spec{};
            spec.Background = std::vector<BackgroundLayer>{ layer };
            auto const style = ResolveStyle(spec, scope);

            if (!style.Background.empty())
            {
                resources.push_back({ key, style.Background.front() });
            }
        }

        return resources;
    }
}
