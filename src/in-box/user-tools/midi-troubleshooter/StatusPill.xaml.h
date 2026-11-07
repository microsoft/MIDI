// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "StatusPill.g.h"

namespace winrt::miditroubleshooter::implementation
{
    // Values for StatusPill.Severity, shared with the code that decides them.
    enum class PillSeverity : int32_t
    {
        Ok = 0,
        Warning = 1,
        Error = 2,
        Info = 3,
        Neutral = 4,
        Preview = 5
    };

    struct StatusPill : StatusPillT<StatusPill>
    {
        StatusPill();

        int32_t Severity() const noexcept { return m_severity; }
        void Severity(_In_ int32_t value) noexcept;

        winrt::hstring Text() const noexcept { return m_text; }
        void Text(_In_ winrt::hstring const& value) noexcept;

    private:
        void ApplyState() noexcept;

        int32_t m_severity{ static_cast<int32_t>(PillSeverity::Neutral) };
        winrt::hstring m_text{};
    };
}

namespace winrt::miditroubleshooter::factory_implementation
{
    struct StatusPill : StatusPillT<StatusPill, implementation::StatusPill>
    {
    };
}
