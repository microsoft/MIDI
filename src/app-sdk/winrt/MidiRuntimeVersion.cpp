// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiRuntimeVersion.h"
#include "Utilities.RuntimeInformation.MidiRuntimeVersion.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::RuntimeInformation::implementation
{
    _Use_decl_annotations_
    void MidiRuntimeVersion::InternalInitialize(
        uint16_t major,
        uint16_t minor,
        uint16_t patch,
        uint16_t buildNumber,
        winrt::hstring previewSuffix,
        winrt::hstring versionFull
    ) noexcept
    {
        m_major = major;
        m_minor = minor;
        m_patch = patch;
        m_buildNumber = buildNumber;
        m_previewSuffix = previewSuffix;
        m_versionFull = versionFull;
    }

    _Use_decl_annotations_
    bool MidiRuntimeVersion::IsGreaterThan(
        midi2::Utilities::RuntimeInformation::MidiRuntimeVersion const& versionToCompare) const noexcept
    {
        // both are null, so just return null
        if (versionToCompare == nullptr)
        {
            return true;
        }

        // nothing is null beyond this point

        if (m_major < versionToCompare.Major())
        {
            return false;
        }
        else if (m_major > versionToCompare.Major())
        {
            return true;
        }

        // Major is equal beyond this point

        if (m_minor < versionToCompare.Minor())
        {
            return false;
        }
        else if (m_minor > versionToCompare.Minor())
        {
            return true;
        }

        // Major and Minor are equal beyond this point

        if (m_patch < versionToCompare.Patch())
        {
            return false;
        }
        else if (m_patch > versionToCompare.Patch())
        {
            return true;
        }

        // Major, Minor, and Patch are all equal
        // compare build numbers. Per semver, this should only be for previews, but we'll ignore that here

        if (m_buildNumber < versionToCompare.BuildNumber())
        {
            return false;
        }
        else if (m_buildNumber > versionToCompare.BuildNumber())
        {
            return true;
        }

        return false;

    }
    
    winrt::hstring MidiRuntimeVersion::ToString() const noexcept
    {
        if (!m_versionFull.empty())
        {
            return m_versionFull;
        }
        else
        {

            if (m_previewSuffix != L"")
            {
                auto versionString = std::format(L"{}.{}.{}", m_major, m_minor, m_patch);
                versionString += L"-" + m_previewSuffix;

                return versionString.c_str();
            }
            else
            {
                auto versionString = std::format(L"{}.{}.{}.{}", m_major, m_minor, m_patch, m_buildNumber);
                
                return versionString.c_str();
            }
        }
    }
}
