// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#pragma once

#include "SurfaceModel.h"

namespace gspike
{
    // Every renderer answers the same questions. The window layer does not know which one it has.
    struct ISurfaceRenderer
    {
        virtual ~ISurfaceRenderer() = default;

        virtual std::wstring_view Name() const noexcept = 0;

        // Create the page inside the host panel. The host is sized to the page already.
        virtual void Build(winrt::xaml::Controls::Panel const& host, PageModel const& page) = 0;

        virtual void Teardown() = 0;

        // The hot path. Called once per changed control per frame, and once per pointer move.
        // Must not allocate.
        virtual void SetValue(uint32_t index, float value, float bloom) noexcept = 0;

        // Page coordinates in, control index out, -1 for a miss.
        virtual int32_t HitTest(float x, float y) const noexcept = 0;

        // How many XAML elements the page cost, counted by walking the visual tree.
        virtual uint32_t XamlElementCount() const = 0;

        // Whether a screen reader would find a control here without more work being done.
        virtual bool HasAutomationPeers() const noexcept = 0;

        // Re-read the theme. This is the Patchbay trap: anything built in code does not re-theme
        // itself, and an app-level resource lookup ignores an element-level theme override.
        virtual void ApplyTheme(bool alternate) = 0;

        // Read the color actually in use by the last control, so a theme swap is proven to have
        // landed rather than assumed to have.
        virtual bool VerifyTheme(bool alternate) const = 0;
    };

    std::unique_ptr<ISurfaceRenderer> MakeXamlElementRenderer();
    std::unique_ptr<ISurfaceRenderer> MakeCompositionRenderer();
    std::unique_ptr<ISurfaceRenderer> MakeHybridRenderer();
}
