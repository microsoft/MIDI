// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Not every tool's pch pulls in the input types, and this header uses them directly.
#include <initializer_list>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>

namespace midiapp
{
    // ========================================================================================
    // Spin buttons for touch only
    //
    // A NumberBox in Compact mode opens a pop-up with large up and down buttons beside the box
    // each time the box gets focus. That pop-up is made for a finger. Customers using a mouse
    // find it confusing, and it covers whatever is next to the box. A mouse already has the
    // wheel (over a box that has focus) and the keyboard has the arrow keys, so a box set up
    // here shows the pop-up only when a finger or a pen uses it. For a mouse or the keyboard it
    // has no spin buttons at all.
    //
    // No setting is needed, because every pointer event says what kind of device sent it. The
    // NumberBox opens its pop-up when it gets focus, and only if it's in Compact mode at that
    // moment (WinUI's NumberBox.cpp, OnNumberBoxGotFocus). A pointer enters a box before it
    // presses it, a finger as it touches down, and focus moves after the press, so the mode is
    // set on PointerEntered and again on PointerPressed. Both are watched with handledEventsToo
    // because the text box inside the NumberBox handles the press itself.
    //
    // Inline mode isn't used for the mouse: its buttons need a column about 72 pixels wide,
    // which most of the tools' boxes don't have.
    // ========================================================================================

    inline void SetSpinButtonsForDevice(
        _In_ winrt::Microsoft::UI::Xaml::Controls::NumberBox const& box,
        _In_ winrt::Microsoft::UI::Input::PointerDeviceType device) noexcept
    {
        try
        {
            namespace controls = winrt::Microsoft::UI::Xaml::Controls;

            auto const mode = device == winrt::Microsoft::UI::Input::PointerDeviceType::Mouse
                ? controls::NumberBoxSpinButtonPlacementMode::Hidden
                : controls::NumberBoxSpinButtonPlacementMode::Compact;

            // A pop-up that's already open stays until the box loses focus, which closes it.
            if (box.SpinButtonPlacementMode() != mode)
            {
                box.SpinButtonPlacementMode(mode);
            }
        }
        catch (...)
        {
        }
    }

    // Call once, after creating the box, instead of setting its SpinButtonPlacementMode.
    inline void UseTouchSpinButtons(_In_ winrt::Microsoft::UI::Xaml::Controls::NumberBox const& box)
    {
        namespace xaml = winrt::Microsoft::UI::Xaml;

        box.SpinButtonPlacementMode(xaml::Controls::NumberBoxSpinButtonPlacementMode::Hidden);

        // Holds nothing, so it can't keep the box alive.
        xaml::Input::PointerEventHandler const handler{ [](winrt::Windows::Foundation::IInspectable const& sender, xaml::Input::PointerRoutedEventArgs const& args)
        {
            try
            {
                if (auto const numberBox = sender.try_as<xaml::Controls::NumberBox>())
                {
                    SetSpinButtonsForDevice(numberBox, args.Pointer().PointerDeviceType());
                }
            }
            catch (...)
            {
            }
        } };

        box.AddHandler(xaml::UIElement::PointerEnteredEvent(), winrt::box_value(handler), true);
        box.AddHandler(xaml::UIElement::PointerPressedEvent(), winrt::box_value(handler), true);
    }

    // The same for several boxes, such as the ones a window's XAML names. Their XAML should say
    // Hidden rather than Compact, so none of them shows the pop-up before this is called.
    inline void UseTouchSpinButtons(_In_ std::initializer_list<winrt::Microsoft::UI::Xaml::Controls::NumberBox> boxes)
    {
        for (auto const& box : boxes)
        {
            if (box != nullptr)
            {
                UseTouchSpinButtons(box);
            }
        }
    }
}
