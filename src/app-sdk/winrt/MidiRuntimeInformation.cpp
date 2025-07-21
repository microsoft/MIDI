// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiRuntimeInformation.h"
#include "Utilities.RuntimeInformation.MidiRuntimeInformation.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::RuntimeInformation::implementation
{
    midi2::Utilities::RuntimeInformation::MidiRuntimeVersion MidiRuntimeInformation::GetInstalledVersion()
    {
        auto version = winrt::make_self<MidiRuntimeVersion>();

        version->InternalInitialize(
            WINDOWS_MIDI_SERVICES_SDK_RUNTIME_BUILD_VERSION_MAJOR,
            WINDOWS_MIDI_SERVICES_SDK_RUNTIME_BUILD_VERSION_MINOR,
            WINDOWS_MIDI_SERVICES_SDK_RUNTIME_BUILD_VERSION_PATCH,
            WINDOWS_MIDI_SERVICES_SDK_RUNTIME_BUILD_VERSION_BUILD_NUMBER,
            WINDOWS_MIDI_SERVICES_SDK_RUNTIME_BUILD_PREVIEW,
            WINDOWS_MIDI_SERVICES_SDK_RUNTIME_BUILD_VERSION_FULL
        );

        return *version;
    }

    midi2::Utilities::RuntimeInformation::MidiRuntimeArchitecture MidiRuntimeInformation::GetInstalledArchitecture()
    {
#if defined(_M_ARM64EC) || defined(_M_ARM64)
        // Arm64 and Arm64EC use same Arm64X binaries
        return midi2::Utilities::RuntimeInformation::MidiRuntimeArchitecture::Arm64X;
#elif defined(_M_AMD64)
        // other 64 bit Intel/AMD
        return midi2::Utilities::RuntimeInformation::MidiRuntimeArchitecture::x64;
#endif

//        return midi2::Utilities::RuntimeInformation::MidiRuntimeArchitecture::Unknown;
    }

    midi2::Utilities::RuntimeInformation::MidiRuntimeReleaseTypes MidiRuntimeInformation::GetInstalledReleaseType()
    {
        if (WINDOWS_MIDI_SERVICES_SDK_RUNTIME_BUILD_IS_PREVIEW)
        {
            return midi2::Utilities::RuntimeInformation::MidiRuntimeReleaseTypes::Preview;
        }
        else
        {
            return midi2::Utilities::RuntimeInformation::MidiRuntimeReleaseTypes::Stable;
        }
    }
}
