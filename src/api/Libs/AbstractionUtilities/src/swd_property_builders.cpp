// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

namespace Windows::Devices::Midi2::Internal
{
    _Use_decl_annotations_
    DEVPROPERTY BuildWStringDevProperty(DEVPROPKEY const key, std::wstring const& value)
    {
        auto cleanValue = internal::TrimmedWStringCopy(value);

        return DEVPROPERTY{ {key, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((cleanValue.length() + 1) * sizeof(WCHAR)), (PVOID)cleanValue.c_str() };
    }

    _Use_decl_annotations_
    DEVPROPERTY BuildUInt32DevProperty(DEVPROPKEY const key, uint32_t const& value)
    {

        return DEVPROPERTY{ {key, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_UINT32, static_cast<ULONG>(sizeof(value)), (PVOID)&value };
    }

    _Use_decl_annotations_
    DEVPROPERTY BuildByteDevProperty(DEVPROPKEY const key, uint8_t const& value)
    {

        return DEVPROPERTY{ {key, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BYTE, static_cast<ULONG>(sizeof(value)), (PVOID)&value };
    }

    _Use_decl_annotations_
    DEVPROPERTY BuildGuidDevProperty(DEVPROPKEY const key, GUID const& value)
    {

        return DEVPROPERTY{ {key, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_GUID, static_cast<ULONG>(sizeof(GUID)), (PVOID)&value };
    }

    _Use_decl_annotations_
    DEVPROPERTY BuildBooleanDevProperty(DEVPROPKEY const key, bool const& value)
    {
        DEVPROP_BOOLEAN propVal = value ? DEVPROP_TRUE : DEVPROP_FALSE;

        return DEVPROPERTY{ {key, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(propVal)), (PVOID)&propVal };

    }

    _Use_decl_annotations_
    DEVPROPERTY BuildEmptyDevProperty(DEVPROPKEY const key)
    {
        return DEVPROPERTY{ {key, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_EMPTY, 0, nullptr };

    }





}
