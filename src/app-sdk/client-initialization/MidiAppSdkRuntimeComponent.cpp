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

#include "pch.h"

HRESULT MidiAppSdkRuntimeComponent::LoadModule()
{
    if (handle == nullptr)
    {
        handle = LoadLibraryExW(module_name.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (handle == nullptr)
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }
        this->get_activation_factory = (activation_factory_type)GetProcAddress(handle, "DllGetActivationFactory");
        if (this->get_activation_factory == nullptr)
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }
    }
    return (handle != nullptr && this->get_activation_factory != nullptr) ? S_OK : E_FAIL;
}

_Use_decl_annotations_
HRESULT MidiAppSdkRuntimeComponent::GetActivationFactory(HSTRING className, REFIID  iid, void** factory)
{
    RETURN_IF_FAILED(LoadModule());

    IActivationFactory* ifactory = nullptr;
    HRESULT hr = this->get_activation_factory(className, &ifactory);
    // optimize for IActivationFactory?
    if (SUCCEEDED(hr))
    {
        hr = ifactory->QueryInterface(iid, factory);
        ifactory->Release();
    }
    return hr;
}