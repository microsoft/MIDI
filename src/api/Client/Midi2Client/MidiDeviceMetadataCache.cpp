// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiDeviceMetadataCache.h"
#include "MidiDeviceMetadataCache.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    void MidiDeviceMetadataCache::AddOrUpdateData(
        winrt::hstring const& /*deviceId*/,
        winrt::hstring const& /*key*/,
        winrt::hstring const& /*data*/,
        winrt::Windows::Foundation::DateTime const& /*expirationTime*/
        ) 
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiDeviceMetadataCache::RemoveData(
        winrt::hstring const& /*deviceId*/,
        winrt::hstring const& /*key*/
        ) 
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    winrt::hstring MidiDeviceMetadataCache::GetData(
        winrt::hstring const& /*deviceId*/,
        winrt::hstring const& /*key*/
        ) 
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    bool MidiDeviceMetadataCache::IsDataPresent(
        winrt::hstring const& /*deviceId*/,
        winrt::hstring const& /*key*/
        ) 
    {
        throw hresult_not_implemented();
    }

}
