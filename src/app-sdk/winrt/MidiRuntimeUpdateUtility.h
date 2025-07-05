// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.Update.MidiRuntimeUpdateUtility.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::Update::implementation
{
    struct MidiRuntimeUpdateUtility
    {
        MidiRuntimeUpdateUtility() = default;

        static collections::IVectorView<midi2::Utilities::Update::MidiRuntimeRelease> GetAllAvailableReleases();

        static midi2::Utilities::Update::MidiRuntimeRelease GetNewestAvailableRelease();
        static midi2::Utilities::Update::MidiRuntimeRelease GetNewestAvailableRelease(_In_ midi2::Utilities::Update::MidiRuntimeUpdateReleaseType releaseType);
        static midi2::Utilities::Update::MidiRuntimeRelease GetNewestAvailableRelease(_In_ uint16_t specificMajorVersion, _In_ midi2::Utilities::Update::MidiRuntimeUpdateReleaseType releaseType);

    private:
        static midi2::Utilities::Update::MidiRuntimeRelease ParseRuntimeReleaseFromJsonObject(_In_ json::JsonObject obj);
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::Update::factory_implementation
{
    struct MidiRuntimeUpdateUtility : MidiRuntimeUpdateUtilityT<MidiRuntimeUpdateUtility, implementation::MidiRuntimeUpdateUtility, winrt::static_lifetime>
    {
    };
}
