// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ThemeXaml.h"

#include <cmath>
#include <set>

namespace themeengine
{
    namespace
    {
        namespace wux = ::winrt::Microsoft::UI::Xaml;
        namespace wuxm = ::winrt::Microsoft::UI::Xaml::Media;
        namespace wf = ::winrt::Windows::Foundation;

        constexpr double Pi = 3.14159265358979323846;
        constexpr wchar_t Separator = L'\x1F';

        wux::ResourceDictionary ThemeDictionary(wux::ResourceDictionary const& host, std::wstring_view name)
        {
            auto const themes = host.ThemeDictionaries();
            auto const key = winrt::box_value(winrt::hstring{ name });

            if (!themes.HasKey(key))
            {
                return nullptr;
            }

            return themes.Lookup(key).try_as<wux::ResourceDictionary>();
        }

        // ResourceDictionary.Insert fails on a key that's already there, so this replaces instead.
        void Put(wux::ResourceDictionary const& dictionary, std::wstring const& key, wf::IInspectable const& value)
        {
            auto const boxed = winrt::box_value(winrt::hstring{ key });

            if (dictionary.HasKey(boxed))
            {
                dictionary.Remove(boxed);
            }

            dictionary.Insert(boxed, value);
        }

        winrt::Windows::UI::Color FirstColor(ResolvedBackground const& value) noexcept
        {
            if (value.Kind == BackgroundKind::Solid || value.Stops.empty())
            {
                return ToWindowsColor(value.Solid);
            }

            return ToWindowsColor(value.Stops.front().Value);
        }

        void SetGradient(wuxm::LinearGradientBrush const& brush, ResolvedBackground const& value)
        {
            auto const stops = brush.GradientStops();
            stops.Clear();

            if (value.Kind == BackgroundKind::Solid || value.Stops.empty())
            {
                for (auto const offset : { 0.0, 1.0 })
                {
                    wuxm::GradientStop stop{};
                    stop.Color(ToWindowsColor(value.Solid));
                    stop.Offset(offset);
                    stops.Append(stop);
                }

                return;
            }

            for (auto const& resolved : value.Stops)
            {
                wuxm::GradientStop stop{};
                stop.Color(ToWindowsColor(resolved.Value));
                stop.Offset(resolved.Position);
                stops.Append(stop);
            }

            auto const [start, end] = GradientPoints(value.Angle);
            brush.StartPoint(start);
            brush.EndPoint(end);
        }

        wuxm::Brush MakeBrush(ResolvedBackground const& value)
        {
            if (value.Kind == BackgroundKind::Linear)
            {
                wuxm::LinearGradientBrush brush{};
                SetGradient(brush, value);
                return brush;
            }

            return wuxm::SolidColorBrush{ FirstColor(value) };
        }
    }

    _Use_decl_annotations_
    winrt::Windows::UI::Color ToWindowsColor(Color const& color) noexcept
    {
        auto const argb = color.ToArgb();
        return { static_cast<uint8_t>(argb >> 24), static_cast<uint8_t>(argb >> 16), static_cast<uint8_t>(argb >> 8), static_cast<uint8_t>(argb) };
    }

    _Use_decl_annotations_
    Color FromWindowsColor(winrt::Windows::UI::Color const& color) noexcept
    {
        return Color::FromRgb8(color.R, color.G, color.B, color.A / 255.0);
    }

    _Use_decl_annotations_
    std::pair<wf::Point, wf::Point> GradientPoints(double cssAngle) noexcept
    {
        // CSS: 0deg points up and the angle turns clockwise; the gradient line runs through the
        // middle and is just long enough to reach the corners.
        auto const radians = cssAngle * Pi / 180.0;
        auto const dx = std::sin(radians);
        auto const dy = -std::cos(radians);
        auto const half = (std::abs(dx) + std::abs(dy)) / 2.0;

        wf::Point const start{ static_cast<float>(0.5 - dx * half), static_cast<float>(0.5 - dy * half) };
        wf::Point const end{ static_cast<float>(0.5 + dx * half), static_cast<float>(0.5 + dy * half) };
        return { start, end };
    }

    _Use_decl_annotations_
    bool XamlThemeResources::Apply(wux::ResourceDictionary const& host, ResolvedTheme const& light, ResolvedTheme const& dark) noexcept
    {
        auto changedKeys = false;

        try
        {
            if (host == nullptr)
            {
                return false;
            }

            auto const lightValues = light.Resources();
            auto const darkValues = dark.Resources();

            struct Target
            {
                std::wstring Name{};
                wux::ResourceDictionary Dictionary{ nullptr };
                std::vector<ResolvedResource> const* Values{ nullptr };
            };

            std::vector<Target> targets{};

            if (auto dictionary = ThemeDictionary(host, L"Light"))
            {
                targets.push_back({ L"Light", dictionary, &lightValues });
            }

            for (auto const name : { L"Default", L"Dark" })
            {
                if (auto dictionary = ThemeDictionary(host, name))
                {
                    targets.push_back({ name, dictionary, &darkValues });
                }
            }

            for (auto const& target : targets)
            {
                std::set<std::wstring> keys{};

                for (auto const& resource : *target.Values)
                {
                    changedKeys = ApplyOne(target.Dictionary, target.Name, resource.Key, resource.Value) || changedKeys;
                    keys.insert(resource.Key);
                }

                // What an earlier theme set and this one leaves out goes back to how it was.
                auto const prefix = target.Name + Separator;

                for (auto it = m_originals.begin(); it != m_originals.end();)
                {
                    if (!it->first.starts_with(prefix))
                    {
                        ++it;
                        continue;
                    }

                    auto const key = it->first.substr(prefix.size());

                    if (keys.contains(key))
                    {
                        ++it;
                        continue;
                    }

                    changedKeys = Restore(target.Dictionary, key, it->second) || changedKeys;
                    it = m_originals.erase(it);
                }
            }
        }
        catch (...)
        {
        }

        return changedKeys;
    }

    _Use_decl_annotations_
    bool XamlThemeResources::ApplyOne(wux::ResourceDictionary const& dictionary, std::wstring const& dictionaryName, std::wstring const& key, ResolvedBackground const& value)
    {
        auto const boxedKey = winrt::box_value(winrt::hstring{ key });
        auto const existing = dictionary.HasKey(boxedKey) ? dictionary.Lookup(boxedKey) : nullptr;
        auto const id = dictionaryName + Separator + key;

        auto const solid = existing != nullptr ? existing.try_as<wuxm::SolidColorBrush>() : nullptr;
        auto const gradient = existing != nullptr ? existing.try_as<wuxm::LinearGradientBrush>() : nullptr;
        auto const boxedColor = existing != nullptr ? existing.try_as<wf::IReference<winrt::Windows::UI::Color>>() : nullptr;

        if (!m_originals.contains(id))
        {
            Original original{};
            original.WasThere = existing != nullptr;
            original.Value = existing;

            if (solid != nullptr)
            {
                original.SolidColor = solid.Color();
            }
            else if (gradient != nullptr)
            {
                for (auto const& stop : gradient.GradientStops())
                {
                    original.Stops.emplace_back(stop.Color(), stop.Offset());
                }

                original.StartPoint = gradient.StartPoint();
                original.EndPoint = gradient.EndPoint();
            }

            m_originals.emplace(id, std::move(original));
        }

        if (solid != nullptr)
        {
            solid.Color(FirstColor(value));
            return false;
        }

        if (gradient != nullptr)
        {
            SetGradient(gradient, value);
            return false;
        }

        if (boxedColor != nullptr)
        {
            Put(dictionary, key, winrt::box_value(FirstColor(value)));
            return true;
        }

        Put(dictionary, key, MakeBrush(value));
        return true;
    }

    _Use_decl_annotations_
    bool XamlThemeResources::Restore(wux::ResourceDictionary const& dictionary, std::wstring const& key, Original const& original)
    {
        auto const boxedKey = winrt::box_value(winrt::hstring{ key });

        if (!original.WasThere)
        {
            if (dictionary.HasKey(boxedKey))
            {
                dictionary.Remove(boxedKey);
            }

            return true;
        }

        auto const current = dictionary.HasKey(boxedKey) ? dictionary.Lookup(boxedKey) : nullptr;

        if (original.SolidColor.has_value())
        {
            if (auto const solid = current != nullptr ? current.try_as<wuxm::SolidColorBrush>() : nullptr)
            {
                solid.Color(*original.SolidColor);
                return false;
            }
        }
        else if (!original.Stops.empty())
        {
            if (auto const gradient = current != nullptr ? current.try_as<wuxm::LinearGradientBrush>() : nullptr)
            {
                auto const stops = gradient.GradientStops();
                stops.Clear();

                for (auto const& [color, offset] : original.Stops)
                {
                    wuxm::GradientStop stop{};
                    stop.Color(color);
                    stop.Offset(offset);
                    stops.Append(stop);
                }

                gradient.StartPoint(original.StartPoint);
                gradient.EndPoint(original.EndPoint);
                return false;
            }
        }

        Put(dictionary, key, original.Value);
        return true;
    }
}
