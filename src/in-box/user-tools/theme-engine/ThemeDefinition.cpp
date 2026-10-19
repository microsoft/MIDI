// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "ThemeDefinition.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <format>
#include <functional>
#include <set>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

// windows.h, when something has brought it in, turns IJsonValue::GetObject into GetObjectW.
#undef GetObject

namespace themeengine
{
    namespace
    {
        namespace wdj = winrt::Windows::Data::Json;

        constexpr size_t MaximumJsonLength = 1024 * 1024;
        constexpr size_t MaximumEntries = 4096;
        constexpr size_t MaximumProblems = 200;

        // The names an expression can always use, which a theme can't take for a color of its own.
        constexpr std::wstring_view TintName = L"tint";
        constexpr std::wstring_view TrackName = L"track";

        bool IsReservedName(std::wstring_view name) noexcept
        {
            return name == TintName || name == TrackName || name == L"transparent" || name == L"white" || name == L"black";
        }

        bool IsComment(std::wstring_view key) noexcept
        {
            return key.starts_with(L"$") || key.starts_with(L"//");
        }

        struct Loader
        {
            std::vector<std::wstring>& Problems;

            void Problem(std::wstring_view path, std::wstring_view message)
            {
                if (Problems.size() < MaximumProblems)
                {
                    Problems.push_back(std::format(L"{}: {}", path, message));
                }
            }
        };

        std::wstring Join(std::wstring_view path, std::wstring_view key)
        {
            return path.empty() ? std::wstring{ key } : std::format(L"{}.{}", path, key);
        }

        // A mode as its file writes it, before it takes anything from the other mode.
        struct RawMode
        {
            ThemeMode Mode{};
            std::wstring Extends{};
            std::map<std::wstring, std::wstring, std::less<>> PartExtends{};
        };

        bool ReadStyle(Loader& loader, wdj::JsonObject const& object, std::wstring const& path, StyleSpec& style,
            std::wstring* extends, std::map<std::wstring, StyleSpec, std::less<>>* states)
        {
            auto ok = true;

            for (auto const& pair : object)
            {
                std::wstring const key{ pair.Key() };
                auto const value = pair.Value();
                auto const where = Join(path, key);

                if (IsComment(key))
                {
                    continue;
                }

                if (key == L"extends" && extends != nullptr)
                {
                    if (value.ValueType() != wdj::JsonValueType::String)
                    {
                        loader.Problem(where, L"Name the part this one starts from.");
                        ok = false;
                        continue;
                    }

                    *extends = value.GetString();
                    continue;
                }

                if (key == L"states" && states != nullptr)
                {
                    if (value.ValueType() != wdj::JsonValueType::Object)
                    {
                        loader.Problem(where, L"States are an object of state names, each with the properties it changes.");
                        ok = false;
                        continue;
                    }

                    for (auto const& state : value.GetObject())
                    {
                        std::wstring const stateName{ state.Key() };

                        if (IsComment(stateName))
                        {
                            continue;
                        }

                        if (state.Value().ValueType() != wdj::JsonValueType::Object)
                        {
                            loader.Problem(Join(where, stateName), L"A state is an object of the properties it changes.");
                            ok = false;
                            continue;
                        }

                        StyleSpec stateStyle{};
                        ok = ReadStyle(loader, state.Value().GetObject(), Join(where, stateName), stateStyle, nullptr, nullptr) && ok;
                        (*states)[stateName] = std::move(stateStyle);
                    }

                    continue;
                }

                std::wstring error{};

                switch (value.ValueType())
                {
                case wdj::JsonValueType::String:
                    if (!SetStyleProperty(style, key, value.GetString(), error))
                    {
                        loader.Problem(where, error);
                        ok = false;
                    }
                    break;

                case wdj::JsonValueType::Number:
                    if (!SetStyleNumber(style, key, value.GetNumber(), error))
                    {
                        loader.Problem(where, error);
                        ok = false;
                    }
                    break;

                case wdj::JsonValueType::Boolean:
                    if (key == L"italic")
                    {
                        style.Italic = value.GetBoolean();
                    }
                    else
                    {
                        style.Numbers.insert_or_assign(key, value.GetBoolean() ? 1.0 : 0.0);
                    }
                    break;

                default:
                    loader.Problem(where, L"A property is a string, a number or true or false.");
                    ok = false;
                    break;
                }
            }

            return ok;
        }

        template <typename Read>
        void ReadSection(Loader& loader, wdj::IJsonValue const& value, std::wstring const& path, Read&& read)
        {
            if (value.ValueType() != wdj::JsonValueType::Object)
            {
                loader.Problem(path, L"This section is an object.");
                return;
            }

            size_t count{ 0 };

            for (auto const& pair : value.GetObject())
            {
                std::wstring const key{ pair.Key() };

                if (IsComment(key))
                {
                    continue;
                }

                if (++count > MaximumEntries)
                {
                    loader.Problem(path, L"This section has too many entries.");
                    return;
                }

                read(key, pair.Value(), Join(path, key));
            }
        }

        RawMode ReadMode(Loader& loader, wdj::JsonObject const& object, std::wstring const& path)
        {
            RawMode raw{};
            auto& mode = raw.Mode;

            auto const readColors = [&loader](std::map<std::wstring, ColorExpression, std::less<>>& target)
            {
                return [&loader, &target](std::wstring const& key, wdj::IJsonValue const& value, std::wstring const& where)
                {
                    if (value.ValueType() != wdj::JsonValueType::String)
                    {
                        loader.Problem(where, L"A color is a string, such as \"#E4E3DF\" or \"oklch(from tint 0.9 0.01 h)\".");
                        return;
                    }

                    if (!IsName(key))
                    {
                        loader.Problem(where, L"A color's name is letters, digits, '-', '_' and '.', starting with a letter.");
                        return;
                    }

                    if (IsReservedName(key))
                    {
                        loader.Problem(where, L"This name is taken: tint, track, transparent, white and black mean the same in every theme.");
                        return;
                    }

                    std::wstring error{};

                    if (auto parsed = ColorExpression::Parse(value.GetString(), error); parsed.has_value())
                    {
                        target.insert_or_assign(key, std::move(*parsed));
                    }
                    else
                    {
                        loader.Problem(where, error);
                    }
                };
            };

            auto const readNumbers = [&loader](std::map<std::wstring, double, std::less<>>& target)
            {
                return [&loader, &target](std::wstring const& key, wdj::IJsonValue const& value, std::wstring const& where)
                {
                    if (value.ValueType() != wdj::JsonValueType::Number || !std::isfinite(value.GetNumber()))
                    {
                        loader.Problem(where, L"This is a number.");
                        return;
                    }

                    target.insert_or_assign(key, value.GetNumber());
                };
            };

            for (auto const& pair : object)
            {
                std::wstring const key{ pair.Key() };
                auto const value = pair.Value();
                auto const where = Join(path, key);

                if (IsComment(key))
                {
                    continue;
                }

                if (key == L"extends")
                {
                    if (value.ValueType() != wdj::JsonValueType::String)
                    {
                        loader.Problem(where, L"Name the mode this one starts from: \"light\" or \"dark\".");
                        continue;
                    }

                    raw.Extends = value.GetString();
                }
                else if (key == L"tokens")
                {
                    ReadSection(loader, value, where, readColors(mode.Tokens));
                }
                else if (key == L"tinted")
                {
                    ReadSection(loader, value, where, readColors(mode.Tinted));
                }
                else if (key == L"numbers")
                {
                    ReadSection(loader, value, where, readNumbers(mode.Numbers));
                }
                else if (key == L"metrics")
                {
                    ReadSection(loader, value, where, readNumbers(mode.Metrics));
                }
                else if (key == L"fonts")
                {
                    ReadSection(loader, value, where, [&loader, &mode](std::wstring const& name, wdj::IJsonValue const& font, std::wstring const& at)
                    {
                        if (font.ValueType() != wdj::JsonValueType::String || font.GetString().empty())
                        {
                            loader.Problem(at, L"A font is the name of a font family.");
                            return;
                        }

                        mode.Fonts.insert_or_assign(name, std::wstring{ font.GetString() });
                    });
                }
                else if (key == L"shadows")
                {
                    ReadSection(loader, value, where, [&loader, &mode](std::wstring const& name, wdj::IJsonValue const& shadow, std::wstring const& at)
                    {
                        if (shadow.ValueType() != wdj::JsonValueType::String)
                        {
                            loader.Problem(at, L"A shadow is a string, written as CSS writes box-shadow.");
                            return;
                        }

                        std::vector<ShadowLayer> layers{};
                        std::wstring error{};

                        if (!ParseShadows(shadow.GetString(), layers, error))
                        {
                            loader.Problem(at, error);
                            return;
                        }

                        mode.Shadows.insert_or_assign(name, std::move(layers));
                    });
                }
                else if (key == L"parts")
                {
                    ReadSection(loader, value, where, [&loader, &raw](std::wstring const& name, wdj::IJsonValue const& part, std::wstring const& at)
                    {
                        if (part.ValueType() != wdj::JsonValueType::Object)
                        {
                            loader.Problem(at, L"A part is an object of CSS properties.");
                            return;
                        }

                        PartSpec spec{};
                        std::wstring extends{};

                        ReadStyle(loader, part.GetObject(), at, spec.Base, &extends, &spec.States);

                        if (!extends.empty())
                        {
                            raw.PartExtends.insert_or_assign(name, extends);
                        }

                        raw.Mode.Parts.insert_or_assign(name, std::move(spec));
                    });
                }
                else if (key == L"resources")
                {
                    ReadSection(loader, value, where, [&loader, &mode](std::wstring const& name, wdj::IJsonValue const& resource, std::wstring const& at)
                    {
                        if (resource.ValueType() != wdj::JsonValueType::String)
                        {
                            loader.Problem(at, L"A resource is a color or a linear gradient.");
                            return;
                        }

                        std::vector<BackgroundLayer> layers{};
                        std::wstring error{};

                        if (!ParseBackground(resource.GetString(), layers, error))
                        {
                            loader.Problem(at, error);
                            return;
                        }

                        if (layers.size() != 1 || (layers[0].Kind != BackgroundKind::Solid && layers[0].Kind != BackgroundKind::Linear))
                        {
                            loader.Problem(at, L"A resource is one color or one linear gradient.");
                            return;
                        }

                        mode.Resources.insert_or_assign(name, std::move(layers[0]));
                    });
                }
                else if (key == L"trackColors")
                {
                    ReadSection(loader, value, where, [&loader, &mode](std::wstring const& name, wdj::IJsonValue const& color, std::wstring const& at)
                    {
                        uint32_t rgb{};
                        auto const isHex = name.size() == 7 && name[0] == L'#' &&
                            std::all_of(name.begin() + 1, name.end(), [](wchar_t c) { return std::iswxdigit(c) != 0; });

                        if (!isHex || color.ValueType() != wdj::JsonValueType::String)
                        {
                            loader.Problem(at, L"A track color is the app's color as \"#RRGGBB\" and the color the theme shows it as.");
                            return;
                        }

                        rgb = static_cast<uint32_t>(std::wcstoul(name.c_str() + 1, nullptr, 16));
                        std::wstring error{};

                        if (auto parsed = ColorExpression::Parse(color.GetString(), error); parsed.has_value())
                        {
                            mode.TrackColors.insert_or_assign(rgb, std::move(*parsed));
                        }
                        else
                        {
                            loader.Problem(at, error);
                        }
                    });
                }
                else
                {
                    loader.Problem(where, L"This isn't a section a theme has. A mode has extends, tokens, tinted, numbers, shadows, fonts, metrics, parts, resources and trackColors.");
                }
            }

            return raw;
        }

        // The other mode's look, with this one's on top. Its "tinted" stays its own.
        RawMode Inherit(RawMode const& base, RawMode const& own)
        {
            RawMode result = base;
            auto& mode = result.Mode;

            mode.Tinted = own.Mode.Tinted;

            for (auto const& [name, value] : own.Mode.Tokens) { mode.Tokens.insert_or_assign(name, value); }
            for (auto const& [name, value] : own.Mode.Numbers) { mode.Numbers.insert_or_assign(name, value); }
            for (auto const& [name, value] : own.Mode.Shadows) { mode.Shadows.insert_or_assign(name, value); }
            for (auto const& [name, value] : own.Mode.Fonts) { mode.Fonts.insert_or_assign(name, value); }
            for (auto const& [name, value] : own.Mode.Metrics) { mode.Metrics.insert_or_assign(name, value); }
            for (auto const& [name, value] : own.Mode.Resources) { mode.Resources.insert_or_assign(name, value); }
            for (auto const& [rgb, value] : own.Mode.TrackColors) { mode.TrackColors.insert_or_assign(rgb, value); }
            for (auto const& [name, value] : own.PartExtends) { result.PartExtends.insert_or_assign(name, value); }

            for (auto const& [name, part] : own.Mode.Parts)
            {
                auto found = mode.Parts.find(name);

                if (found == mode.Parts.end())
                {
                    mode.Parts.emplace(name, part);
                    continue;
                }

                found->second.Base.MergeFrom(part.Base);

                for (auto const& [state, style] : part.States)
                {
                    found->second.States[state].MergeFrom(style);
                }
            }

            result.Extends.clear();
            return result;
        }

        // A part that extends another starts from all of that one, states included.
        void ApplyPartExtends(Loader& loader, RawMode& raw, std::wstring const& path)
        {
            std::map<std::wstring, PartSpec, std::less<>> finished{};

            std::function<PartSpec const*(std::wstring const&, std::set<std::wstring>&)> finish =
                [&](std::wstring const& name, std::set<std::wstring>& visiting) -> PartSpec const*
            {
                if (auto done = finished.find(name); done != finished.end())
                {
                    return &done->second;
                }

                auto const own = raw.Mode.Parts.find(name);

                if (own == raw.Mode.Parts.end())
                {
                    return nullptr;
                }

                auto const extends = raw.PartExtends.find(name);

                if (extends == raw.PartExtends.end())
                {
                    return &finished.emplace(name, own->second).first->second;
                }

                if (!visiting.insert(name).second)
                {
                    loader.Problem(Join(Join(path, L"parts"), name), L"These parts extend each other in a circle.");
                    return &finished.emplace(name, own->second).first->second;
                }

                auto const base = finish(extends->second, visiting);
                visiting.erase(name);

                if (base == nullptr)
                {
                    loader.Problem(Join(Join(path, L"parts"), name), std::format(L"It extends \"{}\", which isn't a part of this theme.", extends->second));
                    return &finished.emplace(name, own->second).first->second;
                }

                PartSpec merged = *base;
                merged.Base.MergeFrom(own->second.Base);

                for (auto const& [state, style] : own->second.States)
                {
                    merged.States[state].MergeFrom(style);
                }

                return &finished.emplace(name, std::move(merged)).first->second;
            };

            for (auto const& [name, part] : raw.Mode.Parts)
            {
                std::set<std::wstring> visiting{};
                finish(name, visiting);
            }

            raw.Mode.Parts = std::move(finished);
        }

        // Named shadows can name each other, and a part's shadow can name one.
        void ExpandShadows(Loader& loader, ThemeMode& mode, std::wstring const& path)
        {
            std::map<std::wstring, std::vector<ShadowLayer>, std::less<>> expanded{};

            std::function<std::vector<ShadowLayer> const*(std::wstring const&, std::set<std::wstring>&)> expand =
                [&](std::wstring const& name, std::set<std::wstring>& visiting) -> std::vector<ShadowLayer> const*
            {
                if (auto done = expanded.find(name); done != expanded.end())
                {
                    return &done->second;
                }

                auto const own = mode.Shadows.find(name);

                if (own == mode.Shadows.end())
                {
                    return nullptr;
                }

                if (!visiting.insert(name).second)
                {
                    loader.Problem(Join(Join(path, L"shadows"), name), L"These shadows name each other in a circle.");
                    return nullptr;
                }

                std::vector<ShadowLayer> layers{};

                for (auto const& layer : own->second)
                {
                    if (layer.Reference.empty())
                    {
                        layers.push_back(layer);
                        continue;
                    }

                    if (auto const named = expand(layer.Reference, visiting); named != nullptr)
                    {
                        layers.insert(layers.end(), named->begin(), named->end());
                    }
                    else if (mode.Shadows.find(layer.Reference) == mode.Shadows.end())
                    {
                        loader.Problem(Join(Join(path, L"shadows"), name), std::format(L"\"{}\" isn't one of the theme's shadows.", layer.Reference));
                    }
                }

                visiting.erase(name);
                return &expanded.emplace(name, std::move(layers)).first->second;
            };

            for (auto const& [name, layers] : mode.Shadows)
            {
                std::set<std::wstring> visiting{};
                expand(name, visiting);
            }

            mode.Shadows = expanded;

            auto const expandStyle = [&](StyleSpec& style, std::wstring const& where)
            {
                if (!style.Shadow.has_value())
                {
                    return;
                }

                std::vector<ShadowLayer> layers{};

                for (auto const& layer : *style.Shadow)
                {
                    if (layer.Reference.empty())
                    {
                        layers.push_back(layer);
                    }
                    else if (auto const named = expanded.find(layer.Reference); named != expanded.end())
                    {
                        layers.insert(layers.end(), named->second.begin(), named->second.end());
                    }
                    else
                    {
                        loader.Problem(where, std::format(L"\"{}\" isn't one of the theme's shadows.", layer.Reference));
                    }
                }

                style.Shadow = std::move(layers);
            };

            for (auto& [name, part] : mode.Parts)
            {
                auto const where = Join(Join(path, L"parts"), name);
                expandStyle(part.Base, Join(where, L"box-shadow"));

                for (auto& [state, style] : part.States)
                {
                    expandStyle(style, Join(Join(Join(where, L"states"), state), L"box-shadow"));
                }
            }
        }

        // Every name an expression uses has to be there, and "tint" only where a window color is.
        void CheckNames(Loader& loader, ThemeMode const& mode, ColorExpression const& expression, std::wstring const& where, bool allowTint, bool allowTrack)
        {
            std::vector<std::wstring> colors{};
            std::vector<std::wstring> numbers{};
            expression.CollectNames(colors, numbers);

            for (auto const& name : colors)
            {
                if (name == TintName)
                {
                    if (!allowTint)
                    {
                        loader.Problem(where, L"\"tint\" can only be used in \"tinted\", which is used only when there's a window color.");
                    }
                }
                else if (name == TrackName)
                {
                    if (!allowTrack)
                    {
                        loader.Problem(where, L"\"track\" can't be used here: a resource has one color for the whole window.");
                    }
                }
                else if (mode.Tokens.find(name) == mode.Tokens.end())
                {
                    loader.Problem(where, std::format(L"\"{}\" isn't one of the theme's colors.", name));
                }
            }

            for (auto const& name : numbers)
            {
                if (mode.Numbers.find(name) == mode.Numbers.end())
                {
                    loader.Problem(where, std::format(L"\"{}\" isn't one of the theme's numbers.", name));
                }
            }
        }

        bool UsesTrack(ThemeMode const& mode, ColorExpression const& expression, bool tinted, std::set<std::wstring>& visiting)
        {
            std::vector<std::wstring> colors{};
            std::vector<std::wstring> numbers{};
            expression.CollectNames(colors, numbers);

            for (auto const& name : colors)
            {
                if (name == TrackName)
                {
                    return true;
                }

                if (!visiting.insert(name).second)
                {
                    continue;
                }

                auto const* source = &mode.Tokens;

                if (tinted && mode.Tinted.find(name) != mode.Tinted.end())
                {
                    source = &mode.Tinted;
                }

                if (auto const found = source->find(name); found != source->end() && UsesTrack(mode, found->second, tinted, visiting))
                {
                    return true;
                }
            }

            return false;
        }

        void Validate(Loader& loader, ThemeMode const& mode, std::wstring const& path)
        {
            for (auto const& [name, expression] : mode.Tokens)
            {
                CheckNames(loader, mode, expression, Join(Join(path, L"tokens"), name), false, true);
            }

            for (auto const& [name, expression] : mode.Tinted)
            {
                auto const where = Join(Join(path, L"tinted"), name);

                if (mode.Tokens.find(name) == mode.Tokens.end())
                {
                    loader.Problem(where, L"A tinted color replaces one of the theme's colors, so it needs one of the same name in \"tokens\".");
                }

                CheckNames(loader, mode, expression, where, true, true);
            }

            for (auto const& [name, layers] : mode.Shadows)
            {
                for (auto const& layer : layers)
                {
                    CheckNames(loader, mode, layer.Color, Join(Join(path, L"shadows"), name), false, true);
                }
            }

            for (auto const& [name, part] : mode.Parts)
            {
                auto const where = Join(Join(path, L"parts"), name);
                part.Base.ForEachColor([&](ColorExpression const& color) { CheckNames(loader, mode, color, where, false, true); });

                for (auto const& [state, style] : part.States)
                {
                    auto const stateWhere = Join(Join(where, L"states"), state);
                    style.ForEachColor([&](ColorExpression const& color) { CheckNames(loader, mode, color, stateWhere, false, true); });
                }
            }

            for (auto const& [rgb, color] : mode.TrackColors)
            {
                CheckNames(loader, mode, color, Join(Join(path, L"trackColors"), std::format(L"#{:06X}", rgb)), false, false);
            }

            for (auto const& [name, resource] : mode.Resources)
            {
                auto const where = Join(Join(path, L"resources"), name);
                std::vector<ColorExpression const*> colors{ &resource.Color };

                for (auto const& stop : resource.Stops)
                {
                    colors.push_back(&stop.Color);
                }

                for (auto const* color : colors)
                {
                    CheckNames(loader, mode, *color, where, false, false);

                    for (auto const tinted : { false, true })
                    {
                        std::set<std::wstring> visiting{};

                        if (UsesTrack(mode, *color, tinted, visiting))
                        {
                            loader.Problem(where, L"A resource can't use a color that depends on the track being drawn.");
                            break;
                        }
                    }
                }
            }

            // No color can depend on itself, with or without the window color.
            for (auto const tinted : { false, true })
            {
                std::map<std::wstring, int32_t, std::less<>> state{};

                std::function<bool(std::wstring const&)> visit = [&](std::wstring const& name) -> bool
                {
                    auto& mark = state[name];

                    if (mark == 1)
                    {
                        return false;
                    }

                    if (mark == 2)
                    {
                        return true;
                    }

                    mark = 1;

                    auto const* source = &mode.Tokens;

                    if (tinted && mode.Tinted.find(name) != mode.Tinted.end())
                    {
                        source = &mode.Tinted;
                    }

                    if (auto const found = source->find(name); found != source->end())
                    {
                        std::vector<std::wstring> colors{};
                        std::vector<std::wstring> numbers{};
                        found->second.CollectNames(colors, numbers);

                        for (auto const& next : colors)
                        {
                            if (mode.Tokens.find(next) != mode.Tokens.end() && !visit(next))
                            {
                                return false;
                            }
                        }
                    }

                    mark = 2;
                    return true;
                };

                for (auto const& [name, expression] : mode.Tokens)
                {
                    if (!visit(name))
                    {
                        loader.Problem(Join(Join(path, tinted ? L"tinted" : L"tokens"), name), L"This color depends on itself.");
                        break;
                    }
                }
            }
        }

        bool IsThemeId(std::wstring_view id) noexcept
        {
            if (id.empty() || id.size() > 64)
            {
                return false;
            }

            return std::all_of(id.begin(), id.end(), [](wchar_t c)
            {
                return (c >= L'a' && c <= L'z') || (c >= L'0' && c <= L'9') || c == L'-';
            });
        }
    }

    _Use_decl_annotations_
    bool ThemeDefinition::AllowsBackdrop(std::wstring_view backdrop) const noexcept
    {
        return std::find(Backdrops.begin(), Backdrops.end(), backdrop) != Backdrops.end();
    }

    _Use_decl_annotations_
    ThemeLoadResult LoadTheme(std::wstring_view json)
    {
        ThemeLoadResult result{};
        Loader loader{ result.Problems };

        try
        {
            if (json.size() > MaximumJsonLength)
            {
                loader.Problem(L"theme", L"The file is too large to be a theme.");
                return result;
            }

            wdj::JsonObject root{ nullptr };

            if (!wdj::JsonObject::TryParse(winrt::hstring{ json }, root) || root == nullptr)
            {
                loader.Problem(L"theme", L"The file isn't JSON.");
                return result;
            }

            auto theme = std::make_shared<ThemeDefinition>();

            auto const version = root.HasKey(L"themeEngine") && root.GetNamedValue(L"themeEngine").ValueType() == wdj::JsonValueType::Number
                ? root.GetNamedNumber(L"themeEngine") : 0.0;

            if (version < 1)
            {
                loader.Problem(L"themeEngine", L"A theme says which version of the format it's in: \"themeEngine\": 1.");
            }
            else if (version > ThemeFormatVersion)
            {
                result.IsFromNewerVersion = true;
                loader.Problem(L"themeEngine", L"The theme was made for a newer version of the app.");
                return result;
            }

            auto const readString = [&root, &loader](std::wstring_view key, bool required) -> std::wstring
            {
                if (!root.HasKey(key))
                {
                    if (required)
                    {
                        loader.Problem(key, L"A theme needs this.");
                    }

                    return {};
                }

                auto const value = root.GetNamedValue(key);

                if (value.ValueType() != wdj::JsonValueType::String)
                {
                    loader.Problem(key, L"This is a string.");
                    return {};
                }

                return std::wstring{ value.GetString() };
            };

            theme->Id = readString(L"id", true);
            theme->Name = readString(L"name", true);
            theme->Description = readString(L"description", false);

            if (!theme->Id.empty() && !IsThemeId(theme->Id))
            {
                loader.Problem(L"id", L"An id is lowercase letters, digits and '-'.");
            }

            theme->Backdrops = { L"solid" };

            if (root.HasKey(L"backdrops"))
            {
                auto const value = root.GetNamedValue(L"backdrops");

                if (value.ValueType() != wdj::JsonValueType::Array || value.GetArray().Size() == 0)
                {
                    loader.Problem(L"backdrops", L"This lists the backdrops the theme can sit on: solid, mica and acrylic.");
                }
                else
                {
                    theme->Backdrops.clear();

                    for (auto const& item : value.GetArray())
                    {
                        std::wstring const backdrop = item.ValueType() == wdj::JsonValueType::String ? std::wstring{ item.GetString() } : std::wstring{};

                        if (backdrop != L"solid" && backdrop != L"mica" && backdrop != L"acrylic")
                        {
                            loader.Problem(L"backdrops", L"A backdrop is solid, mica or acrylic.");
                            continue;
                        }

                        theme->Backdrops.push_back(backdrop);
                    }
                }
            }

            if (auto const windowColor = readString(L"windowColor", false); !windowColor.empty())
            {
                if (windowColor == L"material") { theme->WindowColor = WindowColorUse::Material; }
                else if (windowColor == L"backdrop") { theme->WindowColor = WindowColorUse::Backdrop; }
                else { loader.Problem(L"windowColor", L"This is \"material\" or \"backdrop\"."); }
            }

            std::array<RawMode, 2> raw{};
            std::array<std::wstring, 2> const names{ L"light", L"dark" };

            for (size_t i = 0; i < raw.size(); ++i)
            {
                if (!root.HasKey(names[i]) || root.GetNamedValue(names[i]).ValueType() != wdj::JsonValueType::Object)
                {
                    loader.Problem(names[i], L"A theme has a light look and a dark look, each an object.");
                    continue;
                }

                raw[i] = ReadMode(loader, root.GetNamedObject(names[i]), names[i]);
            }

            for (size_t i = 0; i < raw.size(); ++i)
            {
                auto const& extends = raw[i].Extends;

                if (extends.empty())
                {
                    continue;
                }

                auto const other = 1 - i;

                if (extends != names[other])
                {
                    loader.Problem(Join(names[i], L"extends"), std::format(L"The {} look can only start from the {} one.", names[i], names[other]));
                    continue;
                }

                if (raw[other].Extends == names[i])
                {
                    loader.Problem(Join(names[i], L"extends"), L"The light and dark looks can't both start from each other.");
                    continue;
                }

                raw[i] = Inherit(raw[other], raw[i]);
            }

            for (size_t i = 0; i < raw.size(); ++i)
            {
                ApplyPartExtends(loader, raw[i], names[i]);
                ExpandShadows(loader, raw[i].Mode, names[i]);
                Validate(loader, raw[i].Mode, names[i]);
            }

            theme->Light = std::move(raw[0].Mode);
            theme->Dark = std::move(raw[1].Mode);

            if (result.Problems.empty())
            {
                result.Theme = std::move(theme);
            }
        }
        catch (...)
        {
            loader.Problem(L"theme", L"The theme couldn't be read.");
            result.Theme = nullptr;
        }

        return result;
    }
}
