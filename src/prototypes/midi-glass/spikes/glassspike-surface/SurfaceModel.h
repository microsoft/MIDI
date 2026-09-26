// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#pragma once

// The page every renderer draws. Generated from a fixed seed so all three approaches are given
// byte-for-byte the same work to do and the only variable left is how it is painted.
namespace gspike
{
    enum class ControlKind
    {
        Fader = 0,
        Knob = 1,
        Pad = 2,
    };

    struct ControlDescriptor
    {
        ControlKind Kind{ ControlKind::Fader };
        float X{};
        float Y{};
        float Width{};
        float Height{};
        uint8_t HueSlot{};          // index into the theme's six hue slots
        float Value{};              // 0..1
        uint8_t Group{};
        uint8_t Channel{};
        uint8_t ControllerNumber{};
        bool Animated{ false };
    };

    struct PageModel
    {
        float Width{ 1280.0f };
        float Height{ 800.0f };
        std::vector<ControlDescriptor> Controls;
        std::vector<uint32_t> AnimatedIndices;
    };

    PageModel BuildPage(uint32_t controlCount, uint32_t animatedCount);

    // The six hue slots of Studio Dark, plus the deck and plate colors the spike draws on.
    winrt::Windows::UI::Color HueColor(uint8_t slot) noexcept;
    winrt::Windows::UI::Color DeckColor() noexcept;
    winrt::Windows::UI::Color PlateColor() noexcept;
    winrt::Windows::UI::Color TrackColor() noexcept;

    inline constexpr uint8_t HueSlotCount = 6;

    // Value 0..1 to the filled length of a fader slot or the swept angle of a knob arc.
    inline constexpr float PlateCornerRadius = 7.0f;
    inline constexpr float PlateInset = 4.0f;
    inline constexpr float PipeInset = 8.0f;
    inline constexpr float KnobArcThickness = 4.0f;
    inline constexpr float KnobSweepDegrees = 270.0f;
    inline constexpr float KnobStartDegrees = 135.0f;
}
