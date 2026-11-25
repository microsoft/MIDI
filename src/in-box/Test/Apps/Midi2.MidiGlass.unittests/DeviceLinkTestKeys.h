// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "DeviceLinkIdentity.h"

#include <combaseapi.h>

#include <memory>
#include <string>

// A device key of its own for a test, with a name nothing else uses, deleted when it's done.
struct DeviceLinkTestKey
{
    DeviceLinkTestKey()
    {
        GUID id{};
        ::CoCreateGuid(&id);

        wchar_t text[64]{};
        ::StringFromGUID2(id, text, static_cast<int>(std::size(text)));

        Name = std::wstring{ L"Windows MIDI Services test device link " } + text;

        HRESULT error{ E_FAIL };
        Identity = midiapp::DeviceIdentity::OpenOrCreate(Name, error);
    }

    ~DeviceLinkTestKey()
    {
        Identity.reset();
        midiapp::DeviceIdentity::DeleteKey(Name);
    }

    DeviceLinkTestKey(DeviceLinkTestKey const&) = delete;
    DeviceLinkTestKey& operator=(DeviceLinkTestKey const&) = delete;

    std::wstring Name{};
    std::unique_ptr<midiapp::DeviceIdentity> Identity{};
};
