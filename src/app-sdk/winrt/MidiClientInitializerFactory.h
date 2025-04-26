// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

extern MidiClientInitializerSingleton g_midiClientInitializer;

// uuid
// famous phone number
// famous album
// famous musical (same letter/number swaps as others here)
// infamous synthesizer
// Web devs should have no problem decoding the last part
struct __declspec(uuid("18675309-5150-ca75-0b12-5648616c656e")) MidiClientInitializerFactory : winrt::implements<MidiClientInitializerFactory, IClassFactory>
{
    STDMETHOD(CreateInstance)(IUnknown* outer, GUID const& iid, void** result) noexcept final;

    STDMETHOD(LockServer)(BOOL) noexcept final
    {
        return S_OK;
    }
};

