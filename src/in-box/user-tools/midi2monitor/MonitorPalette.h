// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midi2monitor
{
    // Brushes are declared in Styles\MonitorStyles.xaml so that light and dark each get their
    // own values. This resolves them once per theme rather than per row.
    class MonitorPalette
    {
    public:
        static media::Brush MessageTypeBackground(uint32_t colorIndex) noexcept;
        static media::Brush MessageTypeForeground(uint32_t colorIndex) noexcept;
        static media::Brush RowBackground(bool alternate) noexcept;

        // called when the app theme changes so the next set of rows picks up new brushes
        static void Invalidate() noexcept;

        static constexpr uint32_t MessageTypeColorCount = 8;
    };
}
