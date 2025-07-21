// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "Utilities.RuntimeInformation.MidiRuntimeVersion.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::RuntimeInformation::implementation
{
    struct MidiRuntimeVersion : MidiRuntimeVersionT<MidiRuntimeVersion>
    {
        MidiRuntimeVersion() = default;

        uint16_t Major() { return m_major; }
        uint16_t Minor() { return m_minor; }
        uint16_t Patch() { return m_patch; }
        uint16_t BuildNumber() { return m_buildNumber; }
        winrt::hstring PreviewSuffix() { return m_previewSuffix; }
        
        bool IsGreaterThan(_In_ midi2::Utilities::RuntimeInformation::MidiRuntimeVersion const& versionToCompare) const noexcept;
        
        winrt::hstring ToString() const noexcept;

        void InternalInitialize(
            _In_ uint16_t major,
            _In_ uint16_t minor,
            _In_ uint16_t patch,
            _In_ uint16_t buildNumber,
            _In_ winrt::hstring previewSuffix,
            _In_ winrt::hstring versionFull
            ) noexcept;

    private:
        uint16_t m_major{ 0 };
        uint16_t m_minor{ 0 };
        uint16_t m_patch{ 0 };
        uint16_t m_buildNumber{ 0 };

        winrt::hstring m_previewSuffix{};

        winrt::hstring m_versionFull{};

    };
}
