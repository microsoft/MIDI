// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <string>

namespace midiglass
{
    // midiglass --run "<layout file>" opens a runtime window as well as the library. The command
    // line is read before XAML starts, so the answer is parked here for the app to pick up once
    // there is somewhere to put a window.
    std::wstring const& PendingRunLayoutPath() noexcept;
    void SetPendingRunLayoutPath(_In_ std::wstring path) noexcept;
}
