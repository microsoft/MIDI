// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

void MidiApiTests::TestIsSystemProvidedMatchesServingCopy()
{
    // The factory's code lives in whichever copy of the DLL served the class.
    auto const factory = winrt::get_activation_factory<MidiApi>();

    HMODULE servingModule{};
    VERIFY_WIN32_BOOL_SUCCEEDED(::GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        *reinterpret_cast<LPCWSTR*>(winrt::get_abi(factory)),
        &servingModule));

    wchar_t modulePath[MAX_PATH * 4]{};
    VERIFY_IS_GREATER_THAN(::GetModuleFileNameW(servingModule, modulePath, ARRAYSIZE(modulePath)), static_cast<DWORD>(0));

    wchar_t systemFolder[MAX_PATH]{};
    VERIFY_IS_GREATER_THAN(::GetSystemDirectoryW(systemFolder, ARRAYSIZE(systemFolder)), static_cast<UINT>(0));

    std::wstring_view const path{ modulePath };
    auto const folder = path.substr(0, path.find_last_of(L'\\'));

    bool const servedFromSystemFolder =
        ::CompareStringOrdinal(folder.data(), static_cast<int>(folder.size()), systemFolder, -1, TRUE) == CSTR_EQUAL;

    LOG_OUTPUT(L"Serving copy: %s", modulePath);

    VERIFY_ARE_EQUAL(MidiApi::IsSystemProvided(), servedFromSystemFolder);
}
