// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

namespace WindowsMidiServicesInternal
{
    inline HMODULE GetCurrentModule()
    {
        HMODULE hModule = NULL;

        GetModuleHandleEx(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            (LPCTSTR)GetCurrentModule,
            &hModule);

        return hModule;
    }

    inline winrt::hstring ResourceGetHString(_In_ UINT resourceId)
    {
        // TODO: Consider if we should pre-load all these strings

        wchar_t* resourcePointer{ NULL };

        // 0 cch to just get a read-only pointer to the string resource
        int charCount = LoadStringW(
            GetCurrentModule(),
            resourceId,
            reinterpret_cast<LPWSTR>(&resourcePointer),
            0);

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

    inline std::wstring ResourceGetWString(_In_ UINT resourceId)
    {
        wchar_t* resourcePointer{ NULL };

        // 0 cch to just get a read-only pointer to the string resource
        int charCount = LoadStringW(
            GetCurrentModule(),
            resourceId,
            reinterpret_cast<LPWSTR>(&resourcePointer),
            0);

        if (charCount > 0)
        {
            return std::wstring(resourcePointer, charCount);
        }
        else
        {
            // resource Id doesn't exist or the string resource is empty

            return std::wstring{ };
        }
    }

    inline HRESULT ResourceCopyToCoString(_In_ UINT resourceId, LPWSTR* coString)
    {
        ATL::CComBSTR ccbstr;

        auto loadSuccess = ccbstr.LoadStringW(resourceId);

        if (loadSuccess)
        {
            wil::unique_cotaskmem_string tempString;
            tempString = wil::make_cotaskmem_string_nothrow(ccbstr);
            RETURN_IF_NULL_ALLOC(tempString.get());
            *coString = tempString.release();

            return S_OK;
        }
        else
        {
            return E_FAIL;
        }

    }



}