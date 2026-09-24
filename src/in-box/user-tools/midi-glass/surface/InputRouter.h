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

        // A button or a pad went down or came up, and a toggle changed state.
        std::function<void(size_t itemIndex, bool isOn)> Switched{};

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

            double StartValue{ 0.0 };
            double StartY{ 0.0 };
            double Value{ 0.0 };
        };

        void OnPressed(_In_ size_t index, _In_ xaml::Input::PointerRoutedEventArgs const& args);
        void OnMoved(_In_ size_t index, _In_ xaml::Input::PointerRoutedEventArgs const& args);
        void OnReleased(_In_ size_t index, _In_ xaml::Input::PointerRoutedEventArgs const& args);
        void OnCaptureLost(_In_ size_t index);

        void Publish(_In_ Binding& binding, _In_ double value, _In_ bool isFinal);

        std::vector<Binding> m_bindings{};
        int32_t m_heldCount{ 0 };
        bool m_viewMode{ false };
    };
}
