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
        constexpr wchar_t KeyTouchFill[] = L"touchFillPercent";
        constexpr wchar_t KeyTrackColor[] = L"trackColor";
        constexpr wchar_t KeyInkColor[] = L"inkColor";
        constexpr wchar_t KeyPlateColor[] = L"plateColor";
        constexpr wchar_t KeyRim[] = L"rim";
        constexpr wchar_t KeyNeutralRim[] = L"neutralRimColor";
        constexpr wchar_t KeyValueStrip[] = L"valueStrip";
        constexpr wchar_t KeyValueIndicator[] = L"valueIndicator";
        constexpr wchar_t KeyLampCount[] = L"lampCount";
        constexpr wchar_t KeyMinimumLampRing[] = L"minimumLampRingSize";
        constexpr wchar_t KeyPlateSheen[] = L"plateSheenPercent";
        constexpr wchar_t KeyPlateElevation[] = L"plateElevation";
        constexpr wchar_t KeyShadowSpread[] = L"shadowSpread";
        constexpr wchar_t KeyShadowColor[] = L"shadowColor";
        constexpr wchar_t KeyRimStrength[] = L"rimStrengthPercent";
        constexpr wchar_t KeyPipeFalloff[] = L"pipeFalloff";
        constexpr wchar_t KeyThumb[] = L"thumb";
        constexpr wchar_t KeyThumbColor[] = L"thumbColor";
        constexpr wchar_t KeyThumbEnd[] = L"thumbEndColor";
        constexpr wchar_t KeyBloomColor[] = L"bloomColor";
        constexpr wchar_t KeyRestingGlow[] = L"restingGlowPercent";
        constexpr wchar_t KeyPersistence[] = L"persistenceMilliseconds";
        constexpr wchar_t KeyPlateSheenColor[] = L"plateSheenColor";
        constexpr wchar_t KeyPlateEnd[] = L"plateEndColor";
        constexpr wchar_t KeyArcTrack[] = L"arcTrackColor";
        constexpr wchar_t KeyValueFadesToLight[] = L"valueFadesToLight";
        constexpr wchar_t KeyMeterSlots[] = L"meterSlots";
        constexpr wchar_t KeyOverlay[] = L"deckOverlay";
        constexpr wchar_t KeyScanLinePitch[] = L"scanLinePitch";
        constexpr wchar_t KeyScanLineStrength[] = L"scanLineStrength";
        constexpr wchar_t KeyScanLineColor[] = L"scanLineColor";
        constexpr wchar_t KeyVignettePercent[] = L"vignettePercent";
        constexpr wchar_t KeyVignetteColor[] = L"vignetteColor";
        constexpr wchar_t KeyFaceplatePercent[] = L"faceplateSheenPercent";
        constexpr wchar_t KeyFaceplateColor[] = L"faceplateSheenColor";
        constexpr wchar_t KeyGrainPercent[] = L"grainPercent";
        constexpr wchar_t KeyGrainColor[] = L"grainColor";
        constexpr wchar_t KeySwitchFillAtRest[] = L"switchFillAtRest";
        constexpr wchar_t KeyFillWhenOn[] = L"fillWhenOnPercent";
        constexpr wchar_t KeyPointerColor[] = L"pointerColor";
        constexpr wchar_t KeyCapLineColor[] = L"capLineColor";
        constexpr wchar_t KeyNeutralColor[] = L"neutralColor";
        constexpr wchar_t KeySectionHeader[] = L"sectionHeader";
        constexpr wchar_t KeySectionNameInHue[] = L"sectionNameInHue";
        constexpr wchar_t KeyPanelFill[] = L"panelFill";
        constexpr wchar_t KeyPanelColor[] = L"panelColor";
        constexpr wchar_t KeyPanelEnd[] = L"panelEndColor";
        constexpr wchar_t KeyPanelOutline[] = L"panelOutlineColor";
        constexpr wchar_t KeyKnobFace[] = L"knobFaceColor";
        constexpr wchar_t KeyKnobFaceEnd[] = L"knobFaceEndColor";
        constexpr wchar_t KeyKnobCap[] = L"knobCapColor";
        constexpr wchar_t KeyKnobCapEnd[] = L"knobCapEndColor";
        constexpr wchar_t KeyKnobCapSize[] = L"knobCapSizePercent";
        constexpr wchar_t KeyKnobTicks[] = L"knobTickCount";
        constexpr wchar_t KeyNamesInsideSwitches[] = L"namesInsideSwitches";
        constexpr wchar_t KeyOnLift[] = L"onLiftPercent";
        constexpr wchar_t KeyLampColor[] = L"lampColor";
        constexpr wchar_t KeyPlateShade[] = L"plateShadePercent";
        constexpr wchar_t KeyPlateHighlight[] = L"plateHighlightPercent";
        constexpr wchar_t KeyFaderPlate[] = L"faderPlate";
        constexpr wchar_t KeyFaderFill[] = L"faderFillPercent";
        constexpr wchar_t KeyValueColor[] = L"valueColor";
        constexpr wchar_t KeyRecessShade[] = L"recessShadePercent";
        constexpr wchar_t KeyWellColor[] = L"wellColor";
        constexpr wchar_t KeyThumbShadow[] = L"thumbShadowPercent";
        constexpr wchar_t KeyCapLineWide[] = L"capLineWide";
        constexpr wchar_t KeyKeyWhite[] = L"keyWhiteColor";
        constexpr wchar_t KeyKeyBlack[] = L"keyBlackColor";
        constexpr wchar_t KeyInsetPanelColor[] = L"insetPanelColor";
        constexpr wchar_t KeyInsetPanelEnd[] = L"insetPanelEndColor";
        constexpr wchar_t KeyPanelElevation[] = L"panelElevation";
        constexpr wchar_t KeySectionInk[] = L"sectionInkColor";
        constexpr wchar_t KeyGrainStreak[] = L"grainStreak";
        constexpr wchar_t KeyPointerOnCap[] = L"pointerOnCap";
        constexpr wchar_t KeyArcGlow[] = L"arcGlow";
        constexpr wchar_t KeyArcTrackHue[] = L"arcTrackHuePercent";
        constexpr wchar_t KeyLampShape[] = L"lampShape";
        constexpr wchar_t KeySwitchRim[] = L"switchRimStrengthPercent";
        constexpr wchar_t KeySwitchRestingGlow[] = L"switchRestingGlowPercent";
        constexpr wchar_t KeyPadFillAtRest[] = L"padFillAtRest";
        constexpr wchar_t KeyPadFillWhenOn[] = L"padFillWhenOnPercent";
        constexpr wchar_t KeyNeutralCaps[] = L"neutralCaps";
        constexpr wchar_t KeyFaderScale[] = L"faderScalePercent";
        constexpr wchar_t KeyRuleColor[] = L"ruleColor";
        constexpr wchar_t KeyRuleFades[] = L"ruleFades";
        constexpr wchar_t KeyImageRepeats[] = L"imageRepeats";
        constexpr wchar_t KeyGrainStyle[] = L"grainStyle";
        constexpr wchar_t KeyRainPercent[] = L"rainPercent";
        constexpr wchar_t KeyRainColor[] = L"rainColor";
        constexpr wchar_t KeyRainSpeed[] = L"rainSpeed";
        constexpr wchar_t KeyStripeColors[] = L"stripeColors";
        constexpr wchar_t KeyStripeWidth[] = L"stripeWidth";
        constexpr wchar_t KeyRimThickness[] = L"rimThickness";
        constexpr wchar_t KeyArcThickness[] = L"arcThickness";
        constexpr wchar_t KeyArcRoundEnds[] = L"arcRoundEnds";
        constexpr wchar_t KeySwitchShape[] = L"switchShape";
        constexpr wchar_t KeyKeycapTop[] = L"keycapTopColor";
        constexpr wchar_t KeyKeycapTopEnd[] = L"keycapTopEndColor";
        constexpr wchar_t KeySwitchNames[] = L"switchNames";
        constexpr wchar_t KeyPressTravel[] = L"pressTravelPixels";
        constexpr wchar_t KeyLampPosition[] = L"lampPosition";
        constexpr wchar_t KeyLampHolder[] = L"lampHolderColor";
        constexpr wchar_t KeyLampGlow[] = L"lampGlowPercent";
        constexpr wchar_t KeyKnobKnurls[] = L"knobKnurlCount";
        constexpr wchar_t KeyPanelRecess[] = L"panelRecessPercent";
        constexpr wchar_t KeyRecessLip[] = L"recessLipColor";
        constexpr wchar_t KeyWellFillsControl[] = L"wellFillsControl";
        constexpr wchar_t KeyWellGloss[] = L"wellGlossPercent";
        constexpr wchar_t KeyNeonLetters[] = L"neonLetters";
        constexpr wchar_t KeyValueCore[] = L"valueCorePercent";
        constexpr wchar_t KeyGlowFall[] = L"glowFallPixels";
        constexpr wchar_t KeyFlarePercent[] = L"flarePercent";
        constexpr wchar_t KeyFlareColor[] = L"flareColor";
        constexpr wchar_t KeySectionTexture[] = L"sectionTexture";
        constexpr wchar_t KeySectionTexturePercent[] = L"sectionTexturePercent";
        constexpr wchar_t KeyMeterUnlit[] = L"meterUnlitColor";
        constexpr wchar_t KeyOnInk[] = L"onInkColor";
        constexpr wchar_t KeyRestTintOnPlate[] = L"restTintOnPlate";

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
            { LabelPlacement::Above, L"above" },
        };

        constexpr EnumName<SectionHeaderStyle> SectionHeaderNames[]
        {
            { SectionHeaderStyle::Caption, L"caption" },
            { SectionHeaderStyle::FilledBar, L"filledBar" },
            { SectionHeaderStyle::Notched, L"notched" },
            { SectionHeaderStyle::Centered, L"centered" },
        };

        constexpr EnumName<LampStyle> LampStyleNames[]
        {
            { LampStyle::Bar, L"bar" },
            { LampStyle::Dot, L"dot" },
        };

        constexpr EnumName<LampPlacement> LampPlacementNames[]
        {
            { LampPlacement::TopCenter, L"topCenter" },
            { LampPlacement::TopRight, L"topRight" },
        };

        constexpr EnumName<SwitchNamePlacement> SwitchNameNames[]
        {
            { SwitchNamePlacement::Center, L"center" },
            { SwitchNamePlacement::TopLeft, L"topLeft" },
        };

        constexpr EnumName<SwitchShapeStyle> SwitchShapeNames[]
        {
            { SwitchShapeStyle::Plate, L"plate" },
            { SwitchShapeStyle::Keycap, L"keycap" },
        };

        constexpr EnumName<GrainStyle> GrainStyleNames[]
        {
            { GrainStyle::Speckle, L"speckle" },
            { GrainStyle::Brushed, L"brushed" },
            { GrainStyle::Fine, L"fine" },
        };

        constexpr EnumName<PanelFillStyle> PanelFillNames[]
        {
            { PanelFillStyle::Plate, L"plate" },
            { PanelFillStyle::Color, L"color" },
            { PanelFillStyle::None, L"none" },
        };

        constexpr EnumName<FaderPlateStyle> FaderPlateNames[]
        {
            { FaderPlateStyle::Full, L"full" },
            { FaderPlateStyle::Strip, L"strip" },
            { FaderPlateStyle::None, L"none" },
            { FaderPlateStyle::Frame, L"frame" },
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

        // A bare file name, never a path. A theme from a stranger naming
        // "..\..\Windows\System32\something" is the whole reason a picture is not a path.
        bool IsBareFileName(_In_ std::wstring const& name) noexcept
        {
            return name.find(L'\\') == std::wstring::npos &&
                name.find(L'/') == std::wstring::npos &&
                name.find(L':') == std::wstring::npos &&
                name != L"..";
        }

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

        bool ReadBoolean(
            _In_ mjson::JsonObject const& object,
            _In_ std::wstring_view key,
            _In_ bool fallback) noexcept
        {
            try
            {
                winrt::hstring const name{ key };

                if (!object.HasKey(name))
                {
                    return fallback;
                }

                auto const value = object.Lookup(name);

                return (value != nullptr && value.ValueType() == mjson::JsonValueType::Boolean)
                    ? value.GetBoolean()
                    : fallback;
            }
            catch (...)
            {
                return fallback;
            }
        }

        // A child object, or null when the file does not have one. Every nested block in a theme
        // file is optional, so a theme written before the block existed still loads.
        mjson::JsonObject ReadObject(
            _In_ mjson::JsonObject const& root,
            _In_ std::wstring_view key) noexcept
        {
            try
            {
                winrt::hstring const name{ key };

                if (!root.HasKey(name))
                {
                    return nullptr;
                }

                auto const value = root.Lookup(name);

                return (value != nullptr && value.ValueType() == mjson::JsonValueType::Object)
                    ? value.GetObject()
                    : nullptr;
            }
            catch (...)
            {
                return nullptr;
            }
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
    Theme ReadThemeObject(mjson::JsonObject const& object) noexcept
    {
        Theme theme{};

        try
        {
            if (object == nullptr)
            {
                return BuiltInThemes()[0];
            }

            auto const& root = object;

            theme.Name = ReadString(root, KeyName);

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

                // A bare file name inside the shared assets folder, never a path out of it.
                auto const image = ReadString(deck, KeyImage);

                if (IsBareFileName(image))
                {
                    theme.Deck.ImageFileName = image;
                }

                theme.Deck.ImageRepeats = ReadBoolean(deck, KeyImageRepeats, false);
            }

            theme.CornerRadius = static_cast<int32_t>(ReadNumber(root, KeyCornerRadius, base.CornerRadius, 0, 128));
            theme.GlassTintPercent = static_cast<int32_t>(ReadNumber(root, KeyGlassTint, base.GlassTintPercent, 0, 100));
            theme.GlowStrength = static_cast<int32_t>(ReadNumber(root, KeyGlowStrength, base.GlowStrength, 0, 100));
            theme.Labels = ValueOf(LabelNames, ReadString(root, KeyLabels), base.Labels);
            theme.FillAtRest = ReadNumber(root, KeyFillAtRest, base.FillAtRest, 0.0, 1.0);
            theme.TouchFillPercent = static_cast<int32_t>(
                ReadNumber(root, KeyTouchFill, base.TouchFillPercent, 0, 100));
            theme.TrackColor = ReadColor(root, KeyTrackColor, base.TrackColor);
            theme.InkColor = ReadColor(root, KeyInkColor, base.InkColor);
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
            theme.ShadowSpread = static_cast<int32_t>(
                ReadNumber(root, KeyShadowSpread, base.ShadowSpread, 0, 64));
            theme.ShadowColor = ReadColor(root, KeyShadowColor, base.ShadowColor);
            theme.RimStrengthPercent = static_cast<int32_t>(
                ReadNumber(root, KeyRimStrength, base.RimStrengthPercent, 0, 100));
            theme.PipeFalloff = ReadNumber(root, KeyPipeFalloff, base.PipeFalloff, 0.0, 1.0);
            theme.Thumb = ValueOf(ThumbNames, ReadString(root, KeyThumb), base.Thumb);
            theme.ThumbColor = ReadColor(root, KeyThumbColor, base.ThumbColor);
            theme.ThumbEndColor = ReadColor(root, KeyThumbEnd, base.ThumbEndColor);
            theme.GlassColor = ReadColor(root, KeyGlassColor, base.GlassColor);

            theme.BloomColor = ReadColor(root, KeyBloomColor, base.BloomColor);
            theme.RestingGlowPercent = static_cast<int32_t>(
                ReadNumber(root, KeyRestingGlow, base.RestingGlowPercent, 0, 100));
            theme.PersistenceMilliseconds = static_cast<int32_t>(
                ReadNumber(root, KeyPersistence, base.PersistenceMilliseconds, 0, 10000));
            theme.PlateSheenColor = ReadColor(root, KeyPlateSheenColor, base.PlateSheenColor);
            theme.PlateEndColor = ReadColor(root, KeyPlateEnd, base.PlateEndColor);
            theme.ArcTrackColor = ReadColor(root, KeyArcTrack, base.ArcTrackColor);
            theme.ValueFadesToLight = ReadBoolean(root, KeyValueFadesToLight, base.ValueFadesToLight);

            // Below zero means follow FillAtRest, so the bound starts there rather than at zero.
            theme.SwitchFillAtRest = ReadNumber(root, KeySwitchFillAtRest, base.SwitchFillAtRest, -1.0, 1.0);
            theme.FillWhenOnPercent = static_cast<int32_t>(
                ReadNumber(root, KeyFillWhenOn, base.FillWhenOnPercent, 0, 100));
            theme.PointerColor = ReadColor(root, KeyPointerColor, base.PointerColor);
            theme.CapLineColor = ReadColor(root, KeyCapLineColor, base.CapLineColor);
            theme.NeutralColor = ReadColor(root, KeyNeutralColor, base.NeutralColor);
            theme.SectionHeader = ValueOf(
                SectionHeaderNames, ReadString(root, KeySectionHeader), base.SectionHeader);

            // Everything the hardware panel comps asked for. Each one is optional, and a file
            // written before it existed gets what the theme did before it existed.
            theme.SectionNameInHue = ReadBoolean(root, KeySectionNameInHue, base.SectionNameInHue);
            theme.PanelFill = ValueOf(PanelFillNames, ReadString(root, KeyPanelFill), base.PanelFill);
            theme.PanelColor = ReadColor(root, KeyPanelColor, base.PanelColor);
            theme.PanelEndColor = ReadColor(root, KeyPanelEnd, base.PanelEndColor);
            theme.PanelOutlineColor = ReadColor(root, KeyPanelOutline, base.PanelOutlineColor);
            theme.KnobFaceColor = ReadColor(root, KeyKnobFace, base.KnobFaceColor);
            theme.KnobFaceEndColor = ReadColor(root, KeyKnobFaceEnd, base.KnobFaceEndColor);
            theme.KnobCapColor = ReadColor(root, KeyKnobCap, base.KnobCapColor);
            theme.KnobCapEndColor = ReadColor(root, KeyKnobCapEnd, base.KnobCapEndColor);
            theme.KnobCapSizePercent = static_cast<int32_t>(
                ReadNumber(root, KeyKnobCapSize, base.KnobCapSizePercent, 5, 100));
            theme.KnobTickCount = static_cast<int32_t>(
                ReadNumber(root, KeyKnobTicks, base.KnobTickCount, 0, 64));
            theme.NamesInsideSwitches = ReadBoolean(root, KeyNamesInsideSwitches, base.NamesInsideSwitches);
            theme.OnLiftPercent = static_cast<int32_t>(
                ReadNumber(root, KeyOnLift, base.OnLiftPercent, -100, 100));
            theme.LampColor = ReadColor(root, KeyLampColor, base.LampColor);
            theme.PlateShadePercent = static_cast<int32_t>(
                ReadNumber(root, KeyPlateShade, base.PlateShadePercent, 0, 100));
            theme.PlateHighlightPercent = static_cast<int32_t>(
                ReadNumber(root, KeyPlateHighlight, base.PlateHighlightPercent, 0, 100));
            theme.FaderPlate = ValueOf(FaderPlateNames, ReadString(root, KeyFaderPlate), base.FaderPlate);
            theme.FaderFillPercent = static_cast<int32_t>(
                ReadNumber(root, KeyFaderFill, base.FaderFillPercent, 0, 100));
            theme.ValueColor = ReadColor(root, KeyValueColor, base.ValueColor);
            theme.RecessShadePercent = static_cast<int32_t>(
                ReadNumber(root, KeyRecessShade, base.RecessShadePercent, 0, 100));
            theme.WellColor = ReadColor(root, KeyWellColor, base.WellColor);
            theme.ThumbShadowPercent = static_cast<int32_t>(
                ReadNumber(root, KeyThumbShadow, base.ThumbShadowPercent, 0, 100));
            theme.CapLineWide = ReadBoolean(root, KeyCapLineWide, base.CapLineWide);
            theme.KeyWhiteColor = ReadColor(root, KeyKeyWhite, base.KeyWhiteColor);
            theme.KeyBlackColor = ReadColor(root, KeyKeyBlack, base.KeyBlackColor);

            // What the two printed panels asked for. Below zero means "follow" on every one that
            // takes a number, so the bound starts there.
            theme.InsetPanelColor = ReadColor(root, KeyInsetPanelColor, base.InsetPanelColor);
            theme.InsetPanelEndColor = ReadColor(root, KeyInsetPanelEnd, base.InsetPanelEndColor);
            theme.PanelElevation = static_cast<int32_t>(
                ReadNumber(root, KeyPanelElevation, base.PanelElevation, -1, 100));
            theme.SectionInkColor = ReadColor(root, KeySectionInk, base.SectionInkColor);
            theme.PointerOnCap = ReadBoolean(root, KeyPointerOnCap, base.PointerOnCap);
            theme.ArcGlow = ReadBoolean(root, KeyArcGlow, base.ArcGlow);
            theme.ArcTrackHuePercent = static_cast<int32_t>(
                ReadNumber(root, KeyArcTrackHue, base.ArcTrackHuePercent, 0, 100));
            theme.LampShape = ValueOf(LampStyleNames, ReadString(root, KeyLampShape), base.LampShape);
            theme.SwitchRimStrengthPercent = static_cast<int32_t>(
                ReadNumber(root, KeySwitchRim, base.SwitchRimStrengthPercent, -1, 100));
            theme.SwitchRestingGlowPercent = static_cast<int32_t>(
                ReadNumber(root, KeySwitchRestingGlow, base.SwitchRestingGlowPercent, -1, 100));
            theme.PadFillAtRest = ReadNumber(root, KeyPadFillAtRest, base.PadFillAtRest, -1.0, 1.0);
            theme.PadFillWhenOnPercent = static_cast<int32_t>(
                ReadNumber(root, KeyPadFillWhenOn, base.PadFillWhenOnPercent, -1, 100));
            theme.NeutralCaps = ReadBoolean(root, KeyNeutralCaps, base.NeutralCaps);
            theme.FaderScalePercent = static_cast<int32_t>(
                ReadNumber(root, KeyFaderScale, base.FaderScalePercent, 0, 100));
            theme.RuleColor = ReadColor(root, KeyRuleColor, base.RuleColor);
            theme.RuleFades = ReadBoolean(root, KeyRuleFades, base.RuleFades);

            // What the five comps of the fifth round asked for.
            theme.StripeColors = base.StripeColors;

            if (auto const stripes = [&root]() -> mjson::JsonArray
                {
                    winrt::hstring const name{ KeyStripeColors };

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
                theme.StripeColors.fill(ThemeColor{ 0, 0, 0, 0 });

                for (uint32_t i = 0; i < stripes.Size() && i < static_cast<uint32_t>(MaximumStripeCount); ++i)
                {
                    auto const value = stripes.GetAt(i);
                    ThemeColor parsed{};

                    // A stripe that is not a color ends the list, the way alpha 0 does.
                    if (value == nullptr || value.ValueType() != mjson::JsonValueType::String ||
                        !TryParseColor(std::wstring{ value.GetString() }, parsed))
                    {
                        break;
                    }

                    theme.StripeColors[i] = parsed;
                }
            }

            theme.StripeWidth = static_cast<int32_t>(ReadNumber(root, KeyStripeWidth, base.StripeWidth, 1, 16));
            theme.RimThickness = static_cast<int32_t>(ReadNumber(root, KeyRimThickness, base.RimThickness, 1, 6));
            theme.ArcThickness = static_cast<int32_t>(ReadNumber(root, KeyArcThickness, base.ArcThickness, 0, 12));
            theme.ArcRoundEnds = ReadBoolean(root, KeyArcRoundEnds, base.ArcRoundEnds);
            theme.SwitchShape = ValueOf(SwitchShapeNames, ReadString(root, KeySwitchShape), base.SwitchShape);
            theme.KeycapTopColor = ReadColor(root, KeyKeycapTop, base.KeycapTopColor);
            theme.KeycapTopEndColor = ReadColor(root, KeyKeycapTopEnd, base.KeycapTopEndColor);
            theme.SwitchNames = ValueOf(SwitchNameNames, ReadString(root, KeySwitchNames), base.SwitchNames);
            theme.PressTravelPixels = static_cast<int32_t>(
                ReadNumber(root, KeyPressTravel, base.PressTravelPixels, 0, 8));
            theme.LampPosition = ValueOf(LampPlacementNames, ReadString(root, KeyLampPosition), base.LampPosition);
            theme.LampHolderColor = ReadColor(root, KeyLampHolder, base.LampHolderColor);
            theme.LampGlowPercent = static_cast<int32_t>(ReadNumber(root, KeyLampGlow, base.LampGlowPercent, -1, 100));
            theme.KnobKnurlCount = static_cast<int32_t>(ReadNumber(root, KeyKnobKnurls, base.KnobKnurlCount, 0, 120));
            theme.PanelRecessPercent = static_cast<int32_t>(
                ReadNumber(root, KeyPanelRecess, base.PanelRecessPercent, 0, 100));
            theme.RecessLipColor = ReadColor(root, KeyRecessLip, base.RecessLipColor);
            theme.WellFillsControl = ReadBoolean(root, KeyWellFillsControl, base.WellFillsControl);
            theme.WellGlossPercent = static_cast<int32_t>(ReadNumber(root, KeyWellGloss, base.WellGlossPercent, 0, 100));
            theme.NeonLetters = ReadBoolean(root, KeyNeonLetters, base.NeonLetters);
            theme.ValueCorePercent = static_cast<int32_t>(ReadNumber(root, KeyValueCore, base.ValueCorePercent, 0, 100));
            theme.GlowFallPixels = static_cast<int32_t>(ReadNumber(root, KeyGlowFall, base.GlowFallPixels, 0, 32));
            theme.FlarePercent = static_cast<int32_t>(ReadNumber(root, KeyFlarePercent, base.FlarePercent, 0, 100));
            theme.FlareColor = ReadColor(root, KeyFlareColor, base.FlareColor);

            if (auto const texture = ReadString(root, KeySectionTexture); IsBareFileName(texture))
            {
                theme.SectionTexture = texture;
            }

            theme.SectionTexturePercent = static_cast<int32_t>(
                ReadNumber(root, KeySectionTexturePercent, base.SectionTexturePercent, 0, 100));
            theme.MeterUnlitColor = ReadColor(root, KeyMeterUnlit, base.MeterUnlitColor);
            theme.OnInkColor = ReadColor(root, KeyOnInk, base.OnInkColor);
            theme.RestTintOnPlate = ReadBoolean(root, KeyRestTintOnPlate, base.RestTintOnPlate);

            // A resource key rather than a sentence, so it is translated like everything else. A
            // theme from a stranger has no business naming one of ours, so it is dropped.
            theme.CautionResourceKey.clear();

            theme.MeterSlots = base.MeterSlots;

            if (auto const zones = [&root]() -> mjson::JsonArray
                {
                    winrt::hstring const name{ KeyMeterSlots };

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
                for (uint32_t i = 0; i < zones.Size() && i < MeterZoneCount; ++i)
                {
                    auto const value = zones.GetAt(i);

                    if (value == nullptr || value.ValueType() != mjson::JsonValueType::Number)
                    {
                        continue;
                    }

                    auto const slot = static_cast<int32_t>(value.GetNumber());

                    if (slot >= 0 && slot < ThemeHueSlotCount)
                    {
                        theme.MeterSlots[i] = slot;
                    }
                }
            }

            theme.Overlay = base.Overlay;

            if (auto const overlay = ReadObject(root, KeyOverlay))
            {
                theme.Overlay.ScanLinePitch = static_cast<int32_t>(
                    ReadNumber(overlay, KeyScanLinePitch, base.Overlay.ScanLinePitch, 0, 64));
                theme.Overlay.ScanLineStrength = static_cast<int32_t>(
                    ReadNumber(overlay, KeyScanLineStrength, base.Overlay.ScanLineStrength, 0, 100));
                theme.Overlay.ScanLineColor =
                    ReadColor(overlay, KeyScanLineColor, base.Overlay.ScanLineColor);
                theme.Overlay.VignettePercent = static_cast<int32_t>(
                    ReadNumber(overlay, KeyVignettePercent, base.Overlay.VignettePercent, 0, 100));
                theme.Overlay.VignetteColor =
                    ReadColor(overlay, KeyVignetteColor, base.Overlay.VignetteColor);
                theme.Overlay.FaceplateSheenPercent = static_cast<int32_t>(
                    ReadNumber(overlay, KeyFaceplatePercent, base.Overlay.FaceplateSheenPercent, 0, 100));
                theme.Overlay.FaceplateSheenColor =
                    ReadColor(overlay, KeyFaceplateColor, base.Overlay.FaceplateSheenColor);
                theme.Overlay.GrainPercent = static_cast<int32_t>(
                    ReadNumber(overlay, KeyGrainPercent, base.Overlay.GrainPercent, 0, 100));
                theme.Overlay.GrainColor =
                    ReadColor(overlay, KeyGrainColor, base.Overlay.GrainColor);
                theme.Overlay.GrainStreak = static_cast<int32_t>(
                    ReadNumber(overlay, KeyGrainStreak, base.Overlay.GrainStreak, 1, 64));
                theme.Overlay.Grain = ValueOf(
                    GrainStyleNames, ReadString(overlay, KeyGrainStyle), base.Overlay.Grain);
                theme.Overlay.RainPercent = static_cast<int32_t>(
                    ReadNumber(overlay, KeyRainPercent, base.Overlay.RainPercent, 0, 100));
                theme.Overlay.RainColor = ReadColor(overlay, KeyRainColor, base.Overlay.RainColor);
                theme.Overlay.RainSpeed = static_cast<int32_t>(
                    ReadNumber(overlay, KeyRainSpeed, base.Overlay.RainSpeed, 0, 600));
            }
        }
        catch (...)
        {
            // A half read theme is still a usable one, because every field started at Studio
            // Dark's. Two colors set and the rest of the file garbage still gives a legible
            // surface rather than six black slots on a black deck.
        }

        // A theme read from a file is never built in, whatever the file claims. Otherwise a
        // shared file could make itself unoverwritable on somebody else's PC.
        theme.IsBuiltIn = false;

        return theme;
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

            auto const version = static_cast<uint32_t>(
                ReadNumber(root, KeyFileVersion, ThemeFileVersion, 1, 0x7FFFFFFF));

            result.IsFromNewerVersion = version > ThemeFileVersion;
            result.Value = ReadThemeObject(root);
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
    void WriteThemeBody(JsonTextWriter& writer, Theme const& theme) noexcept
    {
        try
        {
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
            writer.Write(KeyImageRepeats, theme.Deck.ImageRepeats);
            writer.EndObject();

            writer.Write(KeyCornerRadius, static_cast<int64_t>(theme.CornerRadius));
            writer.Write(KeyGlassTint, static_cast<int64_t>(theme.GlassTintPercent));
            writer.Write(KeyGlowStrength, static_cast<int64_t>(theme.GlowStrength));
            writer.Write(KeyLabels, NameOf(LabelNames, theme.Labels));
            writer.Write(KeyFillAtRest, theme.FillAtRest);
            writer.Write(KeyTouchFill, static_cast<int64_t>(theme.TouchFillPercent));
            writer.Write(KeyTrackColor, ColorToText(theme.TrackColor));
            writer.Write(KeyInkColor, ColorToText(theme.InkColor));
            writer.Write(KeyPlateColor, ColorToText(theme.PlateColor));
            writer.Write(KeyRim, NameOf(RimNames, theme.Rim));
            writer.Write(KeyNeutralRim, ColorToText(theme.NeutralRimColor));
            writer.Write(KeyValueStrip, NameOf(StripNames, theme.ValueStrip));
            writer.Write(KeyValueIndicator, NameOf(IndicatorNames, theme.ValueIndicator));
            writer.Write(KeyLampCount, static_cast<int64_t>(theme.LampCount));
            writer.Write(KeyMinimumLampRing, static_cast<int64_t>(theme.MinimumLampRingSize));
            writer.Write(KeyPlateSheen, static_cast<int64_t>(theme.PlateSheenPercent));
            writer.Write(KeyPlateElevation, static_cast<int64_t>(theme.PlateElevation));
            writer.Write(KeyShadowSpread, static_cast<int64_t>(theme.ShadowSpread));
            writer.Write(KeyShadowColor, ColorToText(theme.ShadowColor));
            writer.Write(KeyRimStrength, static_cast<int64_t>(theme.RimStrengthPercent));
            writer.Write(KeyPipeFalloff, theme.PipeFalloff);
            writer.Write(KeyThumb, NameOf(ThumbNames, theme.Thumb));
            writer.Write(KeyThumbColor, ColorToText(theme.ThumbColor));
            writer.Write(KeyThumbEnd, ColorToText(theme.ThumbEndColor));
            writer.Write(KeyGlassColor, ColorToText(theme.GlassColor));
            writer.Write(KeyBloomColor, ColorToText(theme.BloomColor));
            writer.Write(KeyRestingGlow, static_cast<int64_t>(theme.RestingGlowPercent));
            writer.Write(KeyPersistence, static_cast<int64_t>(theme.PersistenceMilliseconds));
            writer.Write(KeyPlateSheenColor, ColorToText(theme.PlateSheenColor));
            writer.Write(KeyPlateEnd, ColorToText(theme.PlateEndColor));
            writer.Write(KeyArcTrack, ColorToText(theme.ArcTrackColor));
            writer.Write(KeyValueFadesToLight, theme.ValueFadesToLight);
            writer.Write(KeySwitchFillAtRest, theme.SwitchFillAtRest);
            writer.Write(KeyFillWhenOn, static_cast<int64_t>(theme.FillWhenOnPercent));
            writer.Write(KeyPointerColor, ColorToText(theme.PointerColor));
            writer.Write(KeyCapLineColor, ColorToText(theme.CapLineColor));
            writer.Write(KeyNeutralColor, ColorToText(theme.NeutralColor));
            writer.Write(KeySectionHeader, NameOf(SectionHeaderNames, theme.SectionHeader));
            writer.Write(KeySectionNameInHue, theme.SectionNameInHue);
            writer.Write(KeyPanelFill, NameOf(PanelFillNames, theme.PanelFill));
            writer.Write(KeyPanelColor, ColorToText(theme.PanelColor));
            writer.Write(KeyPanelEnd, ColorToText(theme.PanelEndColor));
            writer.Write(KeyPanelOutline, ColorToText(theme.PanelOutlineColor));
            writer.Write(KeyKnobFace, ColorToText(theme.KnobFaceColor));
            writer.Write(KeyKnobFaceEnd, ColorToText(theme.KnobFaceEndColor));
            writer.Write(KeyKnobCap, ColorToText(theme.KnobCapColor));
            writer.Write(KeyKnobCapEnd, ColorToText(theme.KnobCapEndColor));
            writer.Write(KeyKnobCapSize, static_cast<int64_t>(theme.KnobCapSizePercent));
            writer.Write(KeyKnobTicks, static_cast<int64_t>(theme.KnobTickCount));
            writer.Write(KeyNamesInsideSwitches, theme.NamesInsideSwitches);
            writer.Write(KeyOnLift, static_cast<int64_t>(theme.OnLiftPercent));
            writer.Write(KeyLampColor, ColorToText(theme.LampColor));
            writer.Write(KeyPlateShade, static_cast<int64_t>(theme.PlateShadePercent));
            writer.Write(KeyPlateHighlight, static_cast<int64_t>(theme.PlateHighlightPercent));
            writer.Write(KeyFaderPlate, NameOf(FaderPlateNames, theme.FaderPlate));
            writer.Write(KeyFaderFill, static_cast<int64_t>(theme.FaderFillPercent));
            writer.Write(KeyValueColor, ColorToText(theme.ValueColor));
            writer.Write(KeyRecessShade, static_cast<int64_t>(theme.RecessShadePercent));
            writer.Write(KeyWellColor, ColorToText(theme.WellColor));
            writer.Write(KeyThumbShadow, static_cast<int64_t>(theme.ThumbShadowPercent));
            writer.Write(KeyCapLineWide, theme.CapLineWide);
            writer.Write(KeyKeyWhite, ColorToText(theme.KeyWhiteColor));
            writer.Write(KeyKeyBlack, ColorToText(theme.KeyBlackColor));
            writer.Write(KeyInsetPanelColor, ColorToText(theme.InsetPanelColor));
            writer.Write(KeyInsetPanelEnd, ColorToText(theme.InsetPanelEndColor));
            writer.Write(KeyPanelElevation, static_cast<int64_t>(theme.PanelElevation));
            writer.Write(KeySectionInk, ColorToText(theme.SectionInkColor));
            writer.Write(KeyPointerOnCap, theme.PointerOnCap);
            writer.Write(KeyArcGlow, theme.ArcGlow);
            writer.Write(KeyArcTrackHue, static_cast<int64_t>(theme.ArcTrackHuePercent));
            writer.Write(KeyLampShape, NameOf(LampStyleNames, theme.LampShape));
            writer.Write(KeySwitchRim, static_cast<int64_t>(theme.SwitchRimStrengthPercent));
            writer.Write(KeySwitchRestingGlow, static_cast<int64_t>(theme.SwitchRestingGlowPercent));
            writer.Write(KeyPadFillAtRest, theme.PadFillAtRest);
            writer.Write(KeyPadFillWhenOn, static_cast<int64_t>(theme.PadFillWhenOnPercent));
            writer.Write(KeyNeutralCaps, theme.NeutralCaps);
            writer.Write(KeyFaderScale, static_cast<int64_t>(theme.FaderScalePercent));
            writer.Write(KeyRuleColor, ColorToText(theme.RuleColor));
            writer.Write(KeyRuleFades, theme.RuleFades);

            writer.BeginArray(KeyStripeColors);

            for (int32_t stripe = 0; stripe < StripeCount(theme); ++stripe)
            {
                writer.WriteArrayString(ColorToText(theme.StripeColors[static_cast<size_t>(stripe)]));
            }

            writer.EndArray();

            writer.Write(KeyStripeWidth, static_cast<int64_t>(theme.StripeWidth));
            writer.Write(KeyRimThickness, static_cast<int64_t>(theme.RimThickness));
            writer.Write(KeyArcThickness, static_cast<int64_t>(theme.ArcThickness));
            writer.Write(KeyArcRoundEnds, theme.ArcRoundEnds);
            writer.Write(KeySwitchShape, NameOf(SwitchShapeNames, theme.SwitchShape));
            writer.Write(KeyKeycapTop, ColorToText(theme.KeycapTopColor));
            writer.Write(KeyKeycapTopEnd, ColorToText(theme.KeycapTopEndColor));
            writer.Write(KeySwitchNames, NameOf(SwitchNameNames, theme.SwitchNames));
            writer.Write(KeyPressTravel, static_cast<int64_t>(theme.PressTravelPixels));
            writer.Write(KeyLampPosition, NameOf(LampPlacementNames, theme.LampPosition));
            writer.Write(KeyLampHolder, ColorToText(theme.LampHolderColor));
            writer.Write(KeyLampGlow, static_cast<int64_t>(theme.LampGlowPercent));
            writer.Write(KeyKnobKnurls, static_cast<int64_t>(theme.KnobKnurlCount));
            writer.Write(KeyPanelRecess, static_cast<int64_t>(theme.PanelRecessPercent));
            writer.Write(KeyRecessLip, ColorToText(theme.RecessLipColor));
            writer.Write(KeyWellFillsControl, theme.WellFillsControl);
            writer.Write(KeyWellGloss, static_cast<int64_t>(theme.WellGlossPercent));
            writer.Write(KeyNeonLetters, theme.NeonLetters);
            writer.Write(KeyValueCore, static_cast<int64_t>(theme.ValueCorePercent));
            writer.Write(KeyGlowFall, static_cast<int64_t>(theme.GlowFallPixels));
            writer.Write(KeyFlarePercent, static_cast<int64_t>(theme.FlarePercent));
            writer.Write(KeyFlareColor, ColorToText(theme.FlareColor));
            writer.Write(KeySectionTexture, theme.SectionTexture);
            writer.Write(KeySectionTexturePercent, static_cast<int64_t>(theme.SectionTexturePercent));
            writer.Write(KeyMeterUnlit, ColorToText(theme.MeterUnlitColor));
            writer.Write(KeyOnInk, ColorToText(theme.OnInkColor));
            writer.Write(KeyRestTintOnPlate, theme.RestTintOnPlate);

            writer.BeginArray(KeyMeterSlots);

            for (auto const slot : theme.MeterSlots)
            {
                writer.WriteArrayValue(static_cast<int64_t>(slot));
            }

            writer.EndArray();

            writer.BeginObject(KeyOverlay);
            writer.Write(KeyScanLinePitch, static_cast<int64_t>(theme.Overlay.ScanLinePitch));
            writer.Write(KeyScanLineStrength, static_cast<int64_t>(theme.Overlay.ScanLineStrength));
            writer.Write(KeyScanLineColor, ColorToText(theme.Overlay.ScanLineColor));
            writer.Write(KeyVignettePercent, static_cast<int64_t>(theme.Overlay.VignettePercent));
            writer.Write(KeyVignetteColor, ColorToText(theme.Overlay.VignetteColor));
            writer.Write(KeyFaceplatePercent, static_cast<int64_t>(theme.Overlay.FaceplateSheenPercent));
            writer.Write(KeyFaceplateColor, ColorToText(theme.Overlay.FaceplateSheenColor));
            writer.Write(KeyGrainPercent, static_cast<int64_t>(theme.Overlay.GrainPercent));
            writer.Write(KeyGrainColor, ColorToText(theme.Overlay.GrainColor));
            writer.Write(KeyGrainStreak, static_cast<int64_t>(theme.Overlay.GrainStreak));
            writer.Write(KeyGrainStyle, NameOf(GrainStyleNames, theme.Overlay.Grain));
            writer.Write(KeyRainPercent, static_cast<int64_t>(theme.Overlay.RainPercent));
            writer.Write(KeyRainColor, ColorToText(theme.Overlay.RainColor));
            writer.Write(KeyRainSpeed, static_cast<int64_t>(theme.Overlay.RainSpeed));
            writer.EndObject();
        }
        catch (...)
        {
        }
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

            WriteThemeBody(writer, theme);

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
    std::wstring ThemePicturePath(std::wstring const& fileName) noexcept
    {
        try
        {
            if (fileName.empty())
            {
                return {};
            }

            std::filesystem::path const name{ fileName };

            if (name.has_parent_path() || name.has_root_name() || !name.has_filename())
            {
                return {};
            }

            auto const extension = name.extension().wstring();

            if (_wcsicmp(extension.c_str(), L".png") != 0 &&
                _wcsicmp(extension.c_str(), L".jpg") != 0 &&
                _wcsicmp(extension.c_str(), L".jpeg") != 0)
            {
                return {};
            }

            auto const folder = ThemesFolder();

            if (folder.empty())
            {
                return {};
            }

            auto const full = std::filesystem::path{ folder } / name;

            std::error_code ignored{};

            return std::filesystem::is_regular_file(full, ignored) ? full.wstring() : std::wstring{};
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::wstring DeckImagePath(ThemeDeck const& deck) noexcept
    {
        return ThemePicturePath(deck.ImageFileName);
    }

    _Use_decl_annotations_
    std::wstring CopyDeckImageToThemes(std::wstring const& sourcePath) noexcept
    {
        try
        {
            auto const folder = ThemesFolder();

            if (folder.empty())
            {
                return {};
            }

            // Copied the way a layout's background is, with the themes folder standing in for the
            // layout's own folder: a taken name gets a number rather than an overwrite.
            return CopyBackgroundImageBeside(
                sourcePath, (std::filesystem::path{ folder } / ThemeFolderName).wstring());
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
            // go back to, so the ones that ship are never written over.
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

                if (HasFileExtension(name, ThemeFileExtension) || HasFileExtension(name, LegacyThemeFileExtension))
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
                auto const clash = FindBuiltInTheme(read.Value.Name) != nullptr ||
                    std::any_of(themes.begin(), themes.end(),
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

    _Use_decl_annotations_
    Theme ResolveDocumentTheme(LayoutDocument const& document) noexcept
    {
        // What the layout carries wins. It was edited on purpose, and it is what made the
        // layout worth sending to somebody.
        if (document.HasOwnTheme)
        {
            return document.OwnTheme;
        }

        try
        {
            auto const themes = AllThemes();
            auto const wanted = CurrentThemeName(document.ThemeName);

            for (auto const& theme : themes)
            {
                if (theme.Name == wanted)
                {
                    return theme;
                }
            }

            if (!themes.empty())
            {
                return themes[0];
            }
        }
        catch (...)
        {
        }

        return BuiltInThemes()[0];
    }
}
