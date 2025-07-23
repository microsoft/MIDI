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

        static collections::IVectorView<midi2::Utilities::Update::MidiRuntimeRelease> GetAllAvailableReleases() noexcept;

        static midi2::Utilities::Update::MidiRuntimeRelease GetHighestAvailableRelease() noexcept;

        static midi2::Utilities::Update::MidiRuntimeRelease GetHighestAvailableRelease(
            _In_ midi2::Utilities::RuntimeInformation::MidiRuntimeReleaseTypes inScopeReleaseTypes) noexcept;

        static midi2::Utilities::Update::MidiRuntimeRelease GetHighestAvailableRelease(
            _In_ uint16_t specificMajorVersion, 
            _In_ midi2::Utilities::RuntimeInformation::MidiRuntimeReleaseTypes inScopeReleaseTypes) noexcept;

        static bool IsReleaseNewerThanInstalled(
            _In_ midi2::Utilities::Update::MidiRuntimeRelease release) noexcept;

        static midi2::Utilities::RuntimeInformation::MidiRuntimeArchitecture InstalledRuntimeArchitecture();


    private:
        static midi2::Utilities::Update::MidiRuntimeRelease GetHigherReleaseValue(
            _In_ midi2::Utilities::Update::MidiRuntimeRelease const& releaseA,
            _In_ midi2::Utilities::Update::MidiRuntimeRelease const& releaseB
        ) noexcept;

        static collections::IVector<midi2::Utilities::Update::MidiRuntimeRelease> InternalGetAvailableReleases(
            _In_ bool restrictToMajorVersion,
            _In_ uint16_t majorVersion,
            _In_ midi2::Utilities::RuntimeInformation::MidiRuntimeReleaseTypes inScopeReleaseTypes
        ) noexcept;

        static midi2::Utilities::Update::MidiRuntimeRelease ParseRuntimeReleaseFromJsonObject(_In_ json::JsonObject const& obj) noexcept;
    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::Update::factory_implementation
{
    struct MidiRuntimeUpdateUtility : MidiRuntimeUpdateUtilityT<MidiRuntimeUpdateUtility, implementation::MidiRuntimeUpdateUtility, winrt::static_lifetime>
    {
    };
}
