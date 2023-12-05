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
    winrt::hstring ResourceManager::GetHString(UINT resourceId)
    {
        // TODO: Consider if we should pre-load all these strings

        wchar_t* resourcePointer{ NULL };
        
        // 0 cch to just get a read-only pointer to the string resource
        int charCount = LoadStringW((HINSTANCE)NULL, resourceId, reinterpret_cast<LPWSTR>(&resourcePointer), 0);

        if (charCount > 0)
        {
            return winrt::hstring(resourcePointer, static_cast<winrt::hstring::size_type>(charCount));
        }
        else
        {
            // resource Id doesn't exist or the string resource is empty

            return winrt::hstring{ };
        }
    }
}