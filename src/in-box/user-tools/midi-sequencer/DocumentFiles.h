// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// File helpers defined in MainWindowDocument.cpp, for the window's other files that read and write files.

#include <sal.h>

#include <cstdint>
#include <string>
#include <vector>

#include "SequenceModel.h"

namespace winrt::midisequencer::implementation::documentfiles
{
    bool ReadAllBytes(_In_ std::wstring const& path, _Out_ std::vector<uint8_t>& bytes, _In_ uint64_t maximum) noexcept;
    bool WriteAllBytes(_In_ std::wstring const& path, _In_reads_bytes_(size) void const* data, _In_ size_t size) noexcept;
    std::wstring FileStem(_In_ std::wstring const& path);
    std::wstring SafeFileName(_In_ std::wstring name);
    bool IsDefaultTempo(_In_ ::midisequencer::Sequence const& sequence) noexcept;
}
