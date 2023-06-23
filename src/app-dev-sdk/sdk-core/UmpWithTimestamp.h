// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "UmpWithTimestamp.g.h"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct UmpWithTimestamp : UmpWithTimestampT<UmpWithTimestamp>
    {
        UmpWithTimestamp() = default;

        uint32_t Timestamp();
        void Timestamp(uint32_t value);
        winrt::Microsoft::Devices::Midi2::Ump Ump();
        void Ump(winrt::Microsoft::Devices::Midi2::Ump const& value);
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct UmpWithTimestamp : UmpWithTimestampT<UmpWithTimestamp, implementation::UmpWithTimestamp>
    {
    };
}
