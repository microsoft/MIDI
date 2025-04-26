// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"

#include "MidiClientInitializerFactory.h"

//std::unique_ptr<MidiClientInitializer> g_clientInitializer{ nullptr };
MidiClientInitializerSingleton g_midiClientInitializer{ };



HRESULT
MidiClientInitializerFactory::CreateInstance(IUnknown* outer, GUID const& iid, void** result) noexcept
{
    *result = nullptr;

    try
    {
        // no aggregation support
        if (outer)
        {
            return CLASS_E_NOAGGREGATION;
        }

        // pull from the COM store
        auto initializer = g_midiClientInitializer.Get();

    //    initializer->AddRef();

        // get the requested interface
        HRESULT hrqi = initializer->QueryInterface(iid, result);
        RETURN_IF_FAILED(hrqi);

        return S_OK;
    }
    catch (...)
    {
        return winrt::to_hresult();
    }
}


