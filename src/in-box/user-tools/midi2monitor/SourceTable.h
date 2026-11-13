// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MessageCapture.h"
#include "MessageStore.h"

namespace midi2monitor
{
    // Advanced mode: what a record's SourceSlot means. A slot is handed out the first time an
    // endpoint is used in this window and is never reused while the window is open, so a record
    // captured an hour ago still names the right endpoint after the source list changes.
    //
    // Each checked group gets a color, handed out in the order groups are checked.
    //
    // Read and written on the UI thread, and read by Save to File and copy. A lock keeps the two
    // apart.
    class SourceTable
    {
    public:
        // The slot for an endpoint, made the first time. NoSourceSlot when every slot is in use.
        static uint16_t SlotFor(_In_ std::wstring const& endpointDeviceId, _In_ winrt::hstring const& displayName) noexcept;

        // NoSourceSlot when this window has never used the endpoint.
        static uint16_t FindSlot(_In_ std::wstring const& endpointDeviceId) noexcept;

        static std::wstring EndpointDeviceId(_In_ uint16_t slot) noexcept;
        static winrt::hstring EndpointName(_In_ uint16_t slot) noexcept;

        // groupNumber counts from 1. Empty when the endpoint gives the group no name.
        static void GroupName(_In_ uint16_t slot, _In_ uint8_t groupNumber, _In_ winrt::hstring const& name) noexcept;
        static winrt::hstring GroupName(_In_ uint16_t slot, _In_ uint8_t groupNumber) noexcept;

        // The color for a group of a source, given out the first time it's asked for. A message
        // without a group uses groupNumber 0.
        static uint32_t ColorIndex(_In_ uint16_t slot, _In_ uint8_t groupNumber) noexcept;

        // "MiniLab 3", "1 · MIDI" for the From column, and "MiniLab 3 1 · MIDI" for copying.
        static winrt::hstring GroupText(_In_ uint16_t slot, _In_ uint8_t groupNumber) noexcept;
        static winrt::hstring Describe(_In_ uint16_t slot, _In_ uint8_t groupNumber) noexcept;
    };
}
