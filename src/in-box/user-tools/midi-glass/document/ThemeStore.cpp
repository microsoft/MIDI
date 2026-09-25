// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include "ThemeStore.h"
#include "JsonText.h"
#include "LayoutStore.h"

#include <windows.h>

// windows.h defines this as GetObjectW, which turns IJsonValue::GetObject() into a compile error.
#undef GetObject

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>

namespace glass
{
    namespace
    {
        namespace mjson = winrt::Windows::Data::Json;

        constexpr wchar_t CommentText[] =
            L"Windows MIDI Glass theme. Written by the MIDI Glass app. The MIDI service does not read this file.";

        constexpr wchar_t KeyComment[] = L"_comment";
        constexpr wchar_t KeyFileVersion[] = L"fileVersion";
        constexpr wchar_t KeyName[] = L"name";
        constexpr wchar_t KeyHueSlots[] = L"hueSlots";
        constexpr wchar_t KeyDeck[] = L"deck";
        constexpr wchar_t KeyKind[] = L"kind";
        constexpr wchar_t KeyColor[] = L"color";
        constexpr wchar_t KeyGradientEnd[] = L"gradientEndColor";
        constexpr wchar_t KeyImage[] = L"image";
        constexpr wchar_t KeyCornerRadius[] = L"cornerRadius";
        constexpr wchar_t KeyGlassTint[] = L"glassTintPercent";
        constexpr wchar_t KeyGlassColor[] = L"glassColor";
        constexpr wchar_t KeyGlowStrength[] = L"glowStrength";
        constexpr wchar_t KeyLabels[] = L"labels";
        constexpr wchar_t KeyFillAtRest[] = L"fillAtRest";
        constexpr wchar_t KeyTrackColor[] = L"trackColor";
        constexpr wchar_t KeyPlateColor[] = L"plateColor";
        constexpr wchar_t KeyRim[] = L"rim";
        constexpr wchar_t KeyNeutralRim[] = L"neutralRimColor";
        constexpr wchar_t KeyValueStrip[] = L"valueStrip";
        constexpr wchar_t KeyValueIndicator[] = L"valueIndicator";
        constexpr wchar_t KeyLampCount[] = L"lampCount";
        constexpr wchar_t KeyMinimumLampRing[] = L"minimumLampRingSize";
        constexpr wchar_t KeyPlateSheen[] = L"plateSheenPercent";
        constexpr wchar_t KeyPlateElevation[] = L"plateElevation";
        constexpr wchar_t KeyPipeFalloff[] = L"pipeFalloff";
        constexpr wchar_t KeyThumb[] = L"thumb";
        constexpr wchar_t KeyThumbColor[] = L"thumbColor";
        constexpr wchar_t KeyThumbEnd[] = L"thumbEndColor";

        template <typename TEnum>
        struct EnumName
        {
            TEnum Value{};
            std::wstring_view Name{};
        };

        constexpr EnumName<DeckKind> DeckKindNames[]
        {
            { DeckKind::SolidColor, L"solidColor" },
            { DeckKind::Gradient, L"gradient" },
            { DeckKind::Image, L"image" },
        };

        constexpr EnumName<LabelPlacement> LabelNames[]
        {
            { LabelPlacement::Inside, L"inside" },
            { LabelPlacement::Below, L"below" },
            { LabelPlacement::None, L"none" },
        };

        constexpr EnumName<RimSource> RimNames[]
        {
            { RimSource::ControlHue, L"controlHue" },
            { RimSource::NeutralEdge, L"neutralEdge" },
            { RimSource::None, L"none" },
        };

        constexpr EnumName<ValueStripPlacement> StripNames[]
        {
            { ValueStripPlacement::Bottom, L"bottom" },
            { ValueStripPlacement::Top, L"top" },
            { ValueStripPlacement::None, L"none" },
        };

        constexpr EnumName<ValueIndicatorStyle> IndicatorNames[]
        {
            { ValueIndicatorStyle::SolidArc, L"solidArc" },
            { ValueIndicatorStyle::SegmentedLamps, L"segmentedLamps" },
        };

        constexpr EnumName<ThumbStyle> ThumbNames[]
        {
            { ThumbStyle::None, L"none" },
            { ThumbStyle::Neutral, L"neutral" },
            { ThumbStyle::Hue, L"hue" },
        };

        template <typename TEnum, size_t N>
        std::wstring_view NameOf(_In_ EnumName<TEnum> const (&table)[N], _In_ TEnum value) noexcept
        {
            for (auto const& entry : table)
            {
                if (entry.Value == value)
                {
                    return entry.Name;
                }
            }

            return table[0].Name;
        }

        template <typename TEnum, size_t N>
        TEnum ValueOf(
            _In_ EnumName<TEnum> const (&table)[N],
            _In_ std::wstring_view name,
            _In_ TEnum fallback) noexcept
        {
            for (auto const& entry : table)
            {
                if (entry.Name == name)
                {
                    return entry.Value;
                }
            }

            return fallback;
        }

        std::wstring ReadString(_In_ mjson::JsonObject const& object, _In_ std::wstring_view key) noexcept
        {
            try
            {
                winrt::hstring const name{ key };

                if (!object.HasKey(name))
                {
                    return {};
                }

                auto const value = object.Lookup(name);

                if (value == nullptr || value.ValueType() != mjson::JsonValueType::String)
                {
                    return {};
                }

                return midiapp::SanitizeStoredString(std::wstring{ value.GetString() });
            }
            catch (...)
            {
                return {};
            }
        }

        double ReadNumber(
            _In_ mjson::JsonObject const& object,
            _In_ std::wstring_view key,
            _In_ double fallback,
            _In_ double lowest,
            _In_ double highest) noexcept
        {
            try
            {
                winrt::hstring const name{ key };

                if (!object.HasKey(name))
                {
                    return fallback;
                }

                auto const value = object.Lookup(name);

                if (value == nullptr || value.ValueType() != mjson::JsonValueType::Number)
                {
                    return fallback;
                }

                auto const number = value.GetNumber();

                if (!std::isfinite(number) || number < lowest || number > highest)
                {
                    return fallback;
                }

                return number;
            }
            catch (...)
            {
                return fallback;
            }
        }

        ThemeColor ReadColor(
            _In_ mjson::JsonObject const& object,
            _In_ std::wstring_view key,
            _In_ ThemeColor const& fallback) noexcept
        {
            ThemeColor parsed{};

            return TryParseColor(ReadString(object, key), parsed) ? parsed : fallback;
        }
    }

    _Use_decl_annotations_
    std::wstring ColorToText(ThemeColor const& color) noexcept
    {
        try
        {
            if (color.A == 255)
            {
                return std::format(L"#{:02X}{:02X}{:02X}", color.R, color.G, color.B);
            }

            return std::format(L"#{:02X}{:02X}{:02X}{:02X}", color.A, color.R, color.G, color.B);
        }
        catch (...)
        {
            return L"#000000";
        }
    }

    _Use_decl_annotations_
    bool TryParseColor(std::wstring_view text, ThemeColor& color) noexcept
    {
        color = {};

        if (text.size() < 2 || text[0] != L'#')
        {
            return false;
        }

        auto const digits = text.substr(1);

        if (digits.size() != 6 && digits.size() != 8)
        {
            return false;
        }

        auto const nibble = [](wchar_t ch) noexcept -> int32_t
            {
                if (ch >= L'0' && ch <= L'9') { return ch - L'0'; }
                if (ch >= L'A' && ch <= L'F') { return ch - L'A' + 10; }
                if (ch >= L'a' && ch <= L'f') { return ch - L'a' + 10; }
                return -1;
            };

        uint8_t bytes[4]{};

        for (size_t i = 0; i < digits.size(); i += 2)
        {
            auto const high = nibble(digits[i]);
            auto const low = nibble(digits[i + 1]);

            if (high < 0 || low < 0)
            {
                return false;
            }

            bytes[i / 2] = static_cast<uint8_t>((high << 4) | low);
        }

        if (digits.size() == 6)
        {
            color = { bytes[0], bytes[1], bytes[2], 255 };
        }
        else
        {
            color = { bytes[1], bytes[2], bytes[3], bytes[0] };
        }

        return true;
    }

    _Use_decl_annotations_
    ThemeReadResult ReadThemeFromJson(std::wstring_view json) noexcept
    {
        ThemeReadResult result{};

        try
        {
            if (json.empty())
            {
                result.Detail = L"The file is empty.";
                return result;
            }

            mjson::JsonObject root{ nullptr };

            if (!mjson::JsonObject::TryParse(winrt::hstring{ json }, root) || root == nullptr)
            {
                result.Detail = L"The file is not valid JSON.";
                return result;
            }

            auto& theme = result.Value;

            auto const version = static_cast<uint32_t>(
                ReadNumber(root, KeyFileVersion, ThemeFileVersion, 1, 0x7FFFFFFF));

            result.IsFromNewerVersion = version > ThemeFileVersion;

            theme.Name = ReadString(root, KeyName);

            // A theme read from a file is never built in, whatever the file claims. Otherwise a
            // shared file could make itself unoverwritable on somebody else's PC.
            theme.IsBuiltIn = false;

            // Starting from Studio Dark means a half written theme file still produces something
            // usable rather than six black slots on a black deck.
            auto const& base = BuiltInThemes()[0];
            theme.HueSlots = base.HueSlots;

            if (auto const slots = [&root]() -> mjson::JsonArray
                {
                    winrt::hstring const name{ KeyHueSlots };

                    if (!root.HasKey(name))
                    {
                        return nullptr;
                    }

                    auto const value = root.Lookup(name);

                    return (value != nullptr && value.ValueType() == mjson::JsonValueType::Array)
                        ? value.GetArray()
                        : nullptr;
                }())
            {
                for (uint32_t i = 0; i < slots.Size() && i < ThemeHueSlotCount; ++i)
                {
                    auto const value = slots.GetAt(i);

                    if (value != nullptr && value.ValueType() == mjson::JsonValueType::String)
                    {
                        ThemeColor parsed{};

                        if (TryParseColor(std::wstring{ value.GetString() }, parsed))
                        {
                            theme.HueSlots[i] = parsed;
                        }
                    }
                }
            }

            theme.Deck = base.Deck;

            if (auto const deck = [&root]() -> mjson::JsonObject
                {
                    winrt::hstring const name{ KeyDeck };

                    if (!root.HasKey(name))
                    {
                        return nullptr;
                    }

                    auto const value = root.Lookup(name);

                    return (value != nullptr && value.ValueType() == mjson::JsonValueType::Object)
                        ? value.GetObject()
                        : nullptr;
                }())
            {
                theme.Deck.Kind = ValueOf(DeckKindNames, ReadString(deck, KeyKind), DeckKind::SolidColor);
                theme.Deck.Color = ReadColor(deck, KeyColor, base.Deck.Color);
                theme.Deck.GradientEndColor = ReadColor(deck, KeyGradientEnd, theme.Deck.Color);

                // A bare file name inside the shared assets folder, never a path out of it. A
                // theme from a stranger naming "..\..\Windows\System32\something" is the whole
                // reason this is not a path.
                auto const image = ReadString(deck, KeyImage);

                if (image.find(L'\\') == std::wstring::npos &&
                    image.find(L'/') == std::wstring::npos &&
                    image.find(L':') == std::wstring::npos &&
                    image != L".." )
                {
                    theme.Deck.ImageFileName = image;
                }
            }

            theme.CornerRadius = static_cast<int32_t>(ReadNumber(root, KeyCornerRadius, base.CornerRadius, 0, 128));
            theme.GlassTintPercent = static_cast<int32_t>(ReadNumber(root, KeyGlassTint, base.GlassTintPercent, 0, 100));
            theme.GlowStrength = static_cast<int32_t>(ReadNumber(root, KeyGlowStrength, base.GlowStrength, 0, 100));
            theme.Labels = ValueOf(LabelNames, ReadString(root, KeyLabels), base.Labels);
            theme.FillAtRest = ReadNumber(root, KeyFillAtRest, base.FillAtRest, 0.0, 1.0);
            theme.TrackColor = ReadColor(root, KeyTrackColor, base.TrackColor);
            theme.PlateColor = ReadColor(root, KeyPlateColor, base.PlateColor);
            theme.Rim = ValueOf(RimNames, ReadString(root, KeyRim), base.Rim);
            theme.NeutralRimColor = ReadColor(root, KeyNeutralRim, base.NeutralRimColor);
            theme.ValueStrip = ValueOf(StripNames, ReadString(root, KeyValueStrip), base.ValueStrip);
            theme.ValueIndicator = ValueOf(IndicatorNames, ReadString(root, KeyValueIndicator), base.ValueIndicator);
            theme.LampCount = static_cast<int32_t>(ReadNumber(root, KeyLampCount, base.LampCount, 2, 128));
            theme.MinimumLampRingSize = static_cast<int32_t>(
                ReadNumber(root, KeyMinimumLampRing, base.MinimumLampRingSize, 8, 512));

            theme.PlateSheenPercent = static_cast<int32_t>(
                ReadNumber(root, KeyPlateSheen, base.PlateSheenPercent, 0, 100));
            theme.PlateElevation = static_cast<int32_t>(
                ReadNumber(root, KeyPlateElevation, base.PlateElevation, 0, 100));
            theme.PipeFalloff = ReadNumber(root, KeyPipeFalloff, base.PipeFalloff, 0.0, 1.0);
            theme.Thumb = ValueOf(ThumbNames, ReadString(root, KeyThumb), base.Thumb);
            theme.ThumbColor = ReadColor(root, KeyThumbColor, base.ThumbColor);
            theme.ThumbEndColor = ReadColor(root, KeyThumbEnd, base.ThumbEndColor);
            theme.GlassColor = ReadColor(root, KeyGlassColor, base.GlassColor);

            result.Succeeded = true;
        }
        catch (...)
        {
            result.Succeeded = false;
            result.Detail = L"The file could not be read.";
        }

        return result;
    }

    _Use_decl_annotations_
    std::wstring WriteThemeToJson(Theme const& theme) noexcept
    {
        try
        {
            JsonTextWriter writer{};

            writer.BeginObject();

            writer.Write(KeyComment, CommentText);
            writer.Write(KeyFileVersion, static_cast<int64_t>(ThemeFileVersion));
            writer.Write(KeyName, theme.Name);

            writer.BeginArray(KeyHueSlots);

            for (auto const& slot : theme.HueSlots)
            {
                writer.WriteArrayString(ColorToText(slot));
            }

            writer.EndArray();

            writer.BeginObject(KeyDeck);
            writer.Write(KeyKind, NameOf(DeckKindNames, theme.Deck.Kind));
            writer.Write(KeyColor, ColorToText(theme.Deck.Color));
            writer.Write(KeyGradientEnd, ColorToText(theme.Deck.GradientEndColor));
            writer.Write(KeyImage, theme.Deck.ImageFileName);
            writer.EndObject();

            writer.Write(KeyCornerRadius, static_cast<int64_t>(theme.CornerRadius));
            writer.Write(KeyGlassTint, static_cast<int64_t>(theme.GlassTintPercent));
            writer.Write(KeyGlowStrength, static_cast<int64_t>(theme.GlowStrength));
            writer.Write(KeyLabels, NameOf(LabelNames, theme.Labels));
            writer.Write(KeyFillAtRest, theme.FillAtRest);
            writer.Write(KeyTrackColor, ColorToText(theme.TrackColor));
            writer.Write(KeyPlateColor, ColorToText(theme.PlateColor));
            writer.Write(KeyRim, NameOf(RimNames, theme.Rim));
            writer.Write(KeyNeutralRim, ColorToText(theme.NeutralRimColor));
            writer.Write(KeyValueStrip, NameOf(StripNames, theme.ValueStrip));
            writer.Write(KeyValueIndicator, NameOf(IndicatorNames, theme.ValueIndicator));
            writer.Write(KeyLampCount, static_cast<int64_t>(theme.LampCount));
            writer.Write(KeyMinimumLampRing, static_cast<int64_t>(theme.MinimumLampRingSize));
            writer.Write(KeyPlateSheen, static_cast<int64_t>(theme.PlateSheenPercent));
            writer.Write(KeyPlateElevation, static_cast<int64_t>(theme.PlateElevation));
            writer.Write(KeyPipeFalloff, theme.PipeFalloff);
            writer.Write(KeyThumb, NameOf(ThumbNames, theme.Thumb));
            writer.Write(KeyThumbColor, ColorToText(theme.ThumbColor));
            writer.Write(KeyThumbEnd, ColorToText(theme.ThumbEndColor));
            writer.Write(KeyGlassColor, ColorToText(theme.GlassColor));

            writer.EndObject();

            return writer.Text() + L"\n";
        }
        catch (...)
        {
            return {};
        }
    }

    std::wstring ThemesFolder() noexcept
    {
        try
        {
            auto const layouts = LayoutsFolder();

            if (layouts.empty())
            {
                return {};
            }

            std::filesystem::path folder{ layouts };
            folder /= ThemeFolderName;

            std::error_code ignored{};
            std::filesystem::create_directories(folder, ignored);

            return folder.wstring();
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    ThemeReadResult ReadThemeFile(std::wstring const& filePath) noexcept
    {
        ThemeReadResult result{};

        try
        {
            std::error_code ec{};

            auto const size = std::filesystem::file_size(std::filesystem::path{ filePath }, ec);

            if (ec || size > MaximumLayoutFileBytes)
            {
                result.Detail = L"The file could not be opened, or is too large to be a theme.";
                return result;
            }

            std::ifstream stream{ std::filesystem::path{ filePath }, std::ios::binary };

            if (!stream.is_open())
            {
                result.Detail = L"The file could not be opened.";
                return result;
            }

            std::string bytes(static_cast<size_t>(size), '\0');
            stream.read(bytes.data(), static_cast<std::streamsize>(size));
            stream.close();

            auto const needed = ::MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);

            if (needed <= 0)
            {
                result.Detail = L"The file is not valid UTF-8.";
                return result;
            }

            std::wstring text(static_cast<size_t>(needed), L'\0');

            ::MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), text.data(), needed);

            result = ReadThemeFromJson(text);
        }
        catch (...)
        {
            result.Succeeded = false;
            result.Detail = L"The file could not be read.";
        }

        return result;
    }

    _Use_decl_annotations_
    bool WriteThemeFile(Theme const& theme, std::wstring const& filePath) noexcept
    {
        try
        {
            if (filePath.empty() || theme.Name.empty())
            {
                return false;
            }

            // A customer who edited a shipped theme and then wanted it back would have nothing to
            // go back to, so the shipped nine are never written over.
            if (FindBuiltInTheme(theme.Name) != nullptr)
            {
                return false;
            }

            auto const text = WriteThemeToJson(theme);

            if (text.empty())
            {
                return false;
            }

            auto const folder = std::filesystem::path{ filePath }.parent_path();

            if (!folder.empty())
            {
                std::error_code ignored{};
                std::filesystem::create_directories(folder, ignored);
            }

            auto const needed = ::WideCharToMultiByte(
                CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);

            if (needed <= 0)
            {
                return false;
            }

            std::string bytes(static_cast<size_t>(needed), '\0');

            ::WideCharToMultiByte(
                CP_UTF8, 0, text.data(), static_cast<int>(text.size()), bytes.data(), needed, nullptr, nullptr);

            auto temporary = std::filesystem::path{ filePath };
            temporary += L".writing";

            {
                std::ofstream stream{ temporary, std::ios::binary | std::ios::trunc };

                if (!stream.is_open())
                {
                    return false;
                }

                stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));

                if (!stream.good())
                {
                    stream.close();
                    std::error_code ignored{};
                    std::filesystem::remove(temporary, ignored);
                    return false;
                }
            }

            std::error_code ec{};
            std::filesystem::rename(temporary, std::filesystem::path{ filePath }, ec);

            if (ec)
            {
                std::filesystem::remove(temporary, ec);
                return false;
            }

            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    std::vector<std::wstring> ListThemeFiles() noexcept
    {
        std::vector<std::wstring> files{};

        try
        {
            auto const folder = ThemesFolder();

            if (folder.empty())
            {
                return files;
            }

            std::error_code ec{};

            for (auto const& entry : std::filesystem::directory_iterator{ folder, ec })
            {
                if (!entry.is_regular_file(ec))
                {
                    continue;
                }

                auto const name = entry.path().filename().wstring();
                auto const tailLength = std::size(ThemeFileExtension) - 1;

                if (name.size() <= tailLength)
                {
                    continue;
                }

                auto const tail = name.substr(name.size() - tailLength);

                if (::CompareStringOrdinal(tail.c_str(), -1, ThemeFileExtension, -1, TRUE) == CSTR_EQUAL)
                {
                    files.push_back(entry.path().wstring());
                }
            }

            std::sort(files.begin(), files.end());
        }
        catch (...)
        {
        }

        return files;
    }

    std::vector<Theme> AllThemes() noexcept
    {
        auto themes = BuiltInThemes();

        try
        {
            for (auto const& file : ListThemeFiles())
            {
                auto const read = ReadThemeFile(file);

                if (!read.Succeeded || read.Value.Name.empty())
                {
                    continue;
                }

                // A customer theme named after a shipped one does not replace it. Two entries with
                // the same name in a picker is confusing; silently losing a shipped theme because
                // of a file somebody was sent is worse.
                auto const clash = std::any_of(themes.begin(), themes.end(),
                    [&read](Theme const& existing) { return existing.Name == read.Value.Name; });

                if (!clash)
                {
                    themes.push_back(read.Value);
                }
            }
        }
        catch (...)
        {
        }

        return themes;
    }
}
