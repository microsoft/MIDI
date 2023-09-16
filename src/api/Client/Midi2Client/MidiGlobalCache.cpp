// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiGlobalCache.h"
#include "MidiGlobalCache.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    void MidiGlobalCache::AddOrUpdateData(
        winrt::hstring const& /*key*/,
        winrt::hstring const& /*data*/,
        foundation::DateTime const& /*expirationTime*/)
    {
        // TODO
        throw hresult_not_implemented();

    }

    _Use_decl_annotations_
    void MidiGlobalCache::RemoveData(
        winrt::hstring const& /*key*/)
    {
        // TODO
        throw hresult_not_implemented();

    }

    _Use_decl_annotations_
    winrt::hstring MidiGlobalCache::GetData(
        winrt::hstring const& /*key*/)
    {
        // TODO

        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    bool MidiGlobalCache::IsDataPresent(
        winrt::hstring const& /*key*/)
    {
        // TODO

        throw hresult_not_implemented();
    }
}
