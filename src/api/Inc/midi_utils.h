// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <wincrypt.h>
#include <SoftPub.h>
#include <mscat.h>
#include <wintrust.h>

#define GUID_STRING_LENGTH 39

const wchar_t appModelPath[]    = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\AppModelUnlock";
const wchar_t developerModeKey[] =  L"AllowDevelopmentWithoutDevLicense";

namespace WindowsMidiServicesInternal
{
    inline bool IsDeveloperModeEnabled()
    {
        DWORD developerMode {0};
        DWORD dataSize = sizeof(DWORD);

        // Skip check if developer mode is enabled.
        if (ERROR_SUCCESS == RegGetValue(HKEY_LOCAL_MACHINE, appModelPath, developerModeKey, RRF_RT_DWORD, NULL, &developerMode, &dataSize) && 
            developerMode == 1)
        {
            return true;
        }

        return false;
    }

    inline std::wstring GetFileNameFromCLSID(const std::wstring& clsidStr)
    {
        std::wstring subKey = L"CLSID\\" + clsidStr + L"\\InprocServer32";
        wchar_t filePath[MAX_PATH] {NULL};
        DWORD bufferSize = sizeof(filePath);

        if (ERROR_SUCCESS != RegGetValue(HKEY_CLASSES_ROOT, subKey.c_str(), NULL, RRF_RT_REG_SZ, NULL, &filePath, &bufferSize))
        {
            return L"";
        }

        return std::wstring(filePath);
    }

    inline HRESULT IsFileDigitallySigned(const std::wstring& filePath)
    {
        LONG status;
        WINTRUST_FILE_INFO fileData;
        WINTRUST_DATA winTrustData;
        GUID policyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;

        memset(&fileData, 0, sizeof(fileData));
        fileData.cbStruct = sizeof(WINTRUST_FILE_INFO);
        fileData.pcwszFilePath = filePath.c_str();

        memset(&winTrustData, 0, sizeof(winTrustData));
        winTrustData.cbStruct = sizeof(WINTRUST_DATA);
        winTrustData.dwUIChoice = WTD_UI_NONE;
        winTrustData.fdwRevocationChecks = WTD_REVOKE_NONE;
        winTrustData.dwUnionChoice = WTD_CHOICE_FILE;
        winTrustData.dwStateAction = WTD_STATEACTION_VERIFY;
        winTrustData.dwProvFlags = WTD_SAFER_FLAG;
        winTrustData.pFile = &fileData;

        status = WinVerifyTrust(NULL, &policyGUID, &winTrustData);

        winTrustData.dwStateAction = WTD_STATEACTION_CLOSE;
        WinVerifyTrust(NULL, &policyGUID, &winTrustData);

        return HRESULT_FROM_WIN32(status);
    }

    inline HRESULT IsComponentPermitted(GUID guid)
    {
        // If we are in developer mode, we allow loading of untrusted components.
        RETURN_HR_IF(S_OK, IsDeveloperModeEnabled());

        wchar_t guidString[GUID_STRING_LENGTH] {NULL};
        RETURN_HR_IF(E_INVALIDARG, 0 == StringFromGUID2(guid, guidString, _countof(guidString)));

        // otherwise, confirm the component is trusted.
        std::wstring fileName = GetFileNameFromCLSID(std::wstring(guidString));
        RETURN_IF_FAILED(IsFileDigitallySigned(fileName.c_str()));

        return S_OK;
    }
}
