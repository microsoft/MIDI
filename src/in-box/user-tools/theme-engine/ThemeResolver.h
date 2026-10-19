// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// A theme worked out for one look: light or dark, with or without the window color. Every color
// that doesn't depend on a track is worked out up front; a part is worked out the first time it's
// asked for in each set of states and track color, and kept.

#include <mutex>
#include <span>
#include <unordered_map>

#include "ThemeDefinition.h"

namespace themeengine
{
    struct ResolvedResource
    {
        std::wstring Key{};
        ResolvedBackground Value{};
    };

    class ResolvedTheme
    {
    public:
        // A window color only changes a theme whose window color is a material; it's ignored
        // otherwise. Never null.
        static std::shared_ptr<ResolvedTheme const> Resolve(
            _In_ std::shared_ptr<ThemeDefinition const> theme,
            _In_ bool dark,
            _In_ std::optional<Color> tint);

        ThemeDefinition const& Definition() const noexcept { return *m_theme; }
        std::shared_ptr<ThemeDefinition const> const& DefinitionPointer() const noexcept { return m_theme; }
        bool IsDark() const noexcept { return m_dark; }
        std::optional<Color> const& Tint() const noexcept { return m_tint; }

        bool HasToken(_In_ std::wstring_view name) const noexcept;

        // A color of the theme's own. One that depends on a track's color takes the track given,
        // or black when there's none.
        Color Token(_In_ std::wstring_view name, _In_ Color const& fallback = {}) const;
        Color Token(_In_ std::wstring_view name, _In_ Color const& track, _In_ Color const& fallback) const;

        double Number(_In_ std::wstring_view name, _In_ double fallback) const;
        double Metric(_In_ std::wstring_view name, _In_ double fallback) const;
        std::wstring Font(_In_ std::wstring_view name, _In_ std::wstring_view fallback) const;

        // A track's color (0xRRGGBB, the app's own) as this theme shows it.
        Color TrackColor(_In_ uint32_t rgb) const;

        bool HasPart(_In_ std::wstring_view name) const noexcept;

        // The part's look in the given states, later states winning, with the track's color when
        // the part uses one. An empty look when the theme has no such part. Never null.
        std::shared_ptr<ResolvedStyle const> Part(
            _In_ std::wstring_view name,
            _In_ std::span<std::wstring_view const> states = {},
            _In_ std::optional<Color> track = std::nullopt) const;

        std::shared_ptr<ResolvedStyle const> Part(
            _In_ std::wstring_view name,
            _In_ std::initializer_list<std::wstring_view> states,
            _In_ std::optional<Color> track = std::nullopt) const;

        std::vector<ResolvedResource> Resources() const;

    private:
        ResolvedTheme() = default;

        class Scope;

        ColorExpression const* FindExpression(_In_ std::wstring_view name) const noexcept;

        std::shared_ptr<ThemeDefinition const> m_theme{};
        ThemeMode const* m_mode{ nullptr };
        bool m_dark{ false };
        std::optional<Color> m_tint{};

        // the colors that don't depend on a track, worked out
        std::map<std::wstring, Color, std::less<>> m_tokens{};

        mutable std::mutex m_partsLock{};
        mutable std::unordered_map<std::wstring, std::shared_ptr<ResolvedStyle const>> m_parts{};
    };
}
