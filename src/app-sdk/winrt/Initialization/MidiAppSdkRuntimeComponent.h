// Copyright (c) Microsoft Corporation and Contributors
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================
// This file modified from
// https://github.com/microsoft/xlang/tree/master/src/UndockedRegFreeWinRT
// which retains its own copyrights as described in the xlang repo


#pragma once

// TODO: Components won't respect COM lifetime. workaround to get them in the COM list?

extern "C"
{
    typedef HRESULT(__stdcall* activation_factory_type)(HSTRING, IActivationFactory**);
}

// Intentionally no class factory cache here. That would be excessive since 
// other layers already cache.
struct MidiAppSdkRuntimeComponent
{
    std::wstring module_name{ };
    //std::wstring xmlns{ };
   // activation_factory_type get_activation_factory{};
    ABI::Windows::Foundation::ThreadingType threading_model{ ABI::Windows::Foundation::ThreadingType::ThreadingType_BOTH };

    ~MidiAppSdkRuntimeComponent()
    {
    //    get_activation_factory = nullptr;
    }

 //   HRESULT LoadModuleIfNeeded();
    HRESULT GetActivationFactory(_In_ HSTRING className, _In_ REFIID  iid, _Out_ void** factory);
};
