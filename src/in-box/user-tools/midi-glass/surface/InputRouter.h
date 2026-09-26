// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "SurfaceRenderer.h"
#include "InputRules.h"

namespace glass
{
    // Pointer, pen and touch to a control to a value.
    //
    // Handlers are attached to each control element rather than to the page, so XAML routes the
    // press and per-pointer capture keeps each finger on the control it landed on. That is what
    // gives multi-touch - two fingers on two faders, which is the entire point of the product -
    // without the surface ever having to treat two pointers as a gesture.
    //
    // Everything here runs on the UI thread and calls straight back out, because the message is
    // built and sent in the pointer handler. Queuing it to a render tick would turn a quarter of
    // a millisecond into as much as seventeen, measured, for nothing.
    class InputRouter
    {
    public:
        // A continuous control moved. Final is set on the last event of a gesture, which is what
        // lets a throttled control finish on the value the finger left it at.
        std::function<void(size_t itemIndex, double value, bool isFinal)> ValueChanged{};

        // A button or a pad went down or came up, and a toggle changed state. Velocity is how
        // hard it was hit, which is 1.0 for everything that does not measure it.
        std::function<void(size_t itemIndex, bool isOn, double velocity)> Switched{};

        // The other axis of a two axis control. Separate from ValueChanged so every control
        // that only has one value pays nothing for the ones that have two.
        std::function<void(size_t itemIndex, double value, bool isFinal)> ValueYChanged{};

        // A key on a piano keyboard went down or came up, with how hard it was hit. Key is
        // counted from the leftmost drawn, not from note zero.
        std::function<void(size_t itemIndex, int32_t key, double velocity, bool isDown)> KeyChanged{};

        // Where the nearest stop is, or the position unchanged. The engine owns the stops; the
        // surface only has to put the finger on one.
        std::function<double(size_t itemIndex, double position)> Snap{};

        // Touched and released, for the bloom.
        std::function<void(size_t itemIndex, bool isTouched)> TouchChanged{};

        void Attach(_In_ SurfaceRenderer& renderer);
        void Detach();

        // Nothing is left held when a window closes or a page changes.
        void ReleaseAll();

        bool IsAnythingHeld() const noexcept { return m_heldCount > 0; }

        // While View mode is on, the surface sends nothing. A pinch on a control surface is
        // ambiguous - two fingers might be two fingers on two faders - so zoom and pan live
        // behind an explicit switch rather than being guessed at.
        void SetViewMode(_In_ bool viewMode) noexcept { m_viewMode = viewMode; }
        bool IsViewMode() const noexcept { return m_viewMode; }

    private:
        struct Binding
        {
            GlassControlElement Element{ nullptr };
            size_t ItemIndex{ 0 };
            ControlKind Kind{ ControlKind::Knob };

            xaml::Input::PointerEventHandler PressedHandler{ nullptr };
            xaml::Input::PointerEventHandler MovedHandler{ nullptr };
            xaml::Input::PointerEventHandler ReleasedHandler{ nullptr };
            xaml::Input::PointerEventHandler CaptureLostHandler{ nullptr };

            winrt::event_token PressedToken{};
            winrt::event_token MovedToken{};
            winrt::event_token ReleasedToken{};
            winrt::event_token CaptureLostToken{};

            // 0 means nothing is holding this control. A second finger on a control somebody is
            // already using is ignored rather than fought over.
            uint32_t PointerId{ 0 };

            // Where it goes when the finger comes off, and whether it goes there at all.
            bool ReturnsToRest{ false };
            double RestValue{ 0.0 };
            double RestValueY{ 0.0 };

            double StartValue{ 0.0 };
            double StartY{ 0.0 };
            double StartX{ 0.0 };
            double Value{ 0.0 };
            double ValueY{ 0.0 };

            // A platter: where the hand is on its face, and how far it has pushed it round
            // since it landed.
            double StartAngle{ 0.0 };
            double TurnedDegrees{ 0.0 };
            double TurnDegreesForFullRange{ 180.0 };

            // Which way a finger drags this control up. Knobs and encoders only.
            DragAxis Drag{ DragAxis::Vertical };

            // A pad that takes its velocity from how hard it was hit.
            bool VelocityFromTouch{ false };

            // Whether a press has to be held or latches. Per control rather than per kind,
            // because an LFO is one or the other depending on what the customer asked for.
            bool Momentary{ false };
            bool Toggling{ false };

            // A copy rather than a pointer into the document, because the document can be
            // edited underneath a gesture and a keyboard has to keep playing the key it started.
            KeyboardSpec Keyboard{};

            int32_t PressedKey{ -1 };
        };

        void OnPressed(_In_ size_t index, _In_ xaml::Input::PointerRoutedEventArgs const& args);
        void OnMoved(_In_ size_t index, _In_ xaml::Input::PointerRoutedEventArgs const& args);
        void OnReleased(_In_ size_t index, _In_ xaml::Input::PointerRoutedEventArgs const& args);
        void OnCaptureLost(_In_ size_t index);

        void Publish(_In_ Binding& binding, _In_ double value, _In_ bool isFinal);
        void PublishY(_In_ Binding& binding, _In_ double value, _In_ bool isFinal);

        // A touch anywhere on a keyboard, and the same when it moves: sliding off one key and
        // onto the next releases the first and plays the second, the way a finger dragged along
        // a real keyboard does.
        void TouchKeyboard(
            _In_ Binding& binding,
            _In_ double x,
            _In_ double y,
            _In_ bool down);

        std::vector<Binding> m_bindings{};
        SurfaceRenderer* m_renderer{ nullptr };
        int32_t m_heldCount{ 0 };
        bool m_viewMode{ false };
    };
}
