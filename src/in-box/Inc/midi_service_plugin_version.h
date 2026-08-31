
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <windows.h>
#include <string>
#include <memory>
#include <winver.h>
#include <vector>

#ifndef MIDI_SERVICE_PLUGIN_VERSION_H
#define MIDI_SERVICE_PLUGIN_VERSION_H

namespace WindowsMidiServicesInternal
{
    // via https://devblogs.microsoft.com/oldnewthing/20041025-00/?p=37483
    EXTERN_C IMAGE_DOS_HEADER __ImageBase;
    #define HINST_THIS_PLUGIN ((HINSTANCE)&__ImageBase)


    std::wstring GetCurrentModuleVersion()
    {
        WCHAR filename[MAX_PATH]{ 0 };
        auto size = GetModuleFileNameW(HINST_THIS_PLUGIN, filename, MAX_PATH);

        if (size > 0)
        {
            // we have our own file name, get the version info

            DWORD fileVersionInfoSize{ 0 };
            if ((fileVersionInfoSize = GetFileVersionInfoSizeW(filename, NULL)) > 0)
            {
                std::vector<byte> fileVersionInfoBuffer{};
                fileVersionInfoBuffer.resize(fileVersionInfoSize);

                if (GetFileVersionInfoW(
                    filename, 
                    0, 
                    static_cast<DWORD>(fileVersionInfoBuffer.size()), 
                    static_cast<LPVOID>(fileVersionInfoBuffer.data())))
                {
                    LPVOID versionStringResource{ nullptr };    // this is a pointer into the vector of data above
                    UINT versionStringResourceLength{ 0 };

                    if (VerQueryValueW(
                        static_cast<LPCVOID>(fileVersionInfoBuffer.data()),
                        L"\\StringFileInfo\\040904B0\\FileVersion",
                        &versionStringResource,
                        &versionStringResourceLength
                        ))
                    {
                        auto s = static_cast<wchar_t*>(versionStringResource);

                        auto ver = std::wstring{ s, versionStringResourceLength -1 };

                        return ver;
                    }
                }
            }
        }

        return L"";
    }


    std::wstring GetCurrentModuleVersionCompanyName()
    {
        WCHAR filename[MAX_PATH]{ 0 };
        auto size = GetModuleFileNameW(HINST_THIS_PLUGIN, filename, MAX_PATH);

        if (size > 0)
        {
            // we have our own file name, get the version info

            DWORD fileVersionInfoSize{ 0 };
            if ((fileVersionInfoSize = GetFileVersionInfoSizeW(filename, NULL)) > 0)
            {
                std::vector<byte> fileVersionInfoBuffer{};
                fileVersionInfoBuffer.resize(fileVersionInfoSize);

                if (GetFileVersionInfoW(
                    filename,
                    0,
                    static_cast<DWORD>(fileVersionInfoBuffer.size()),
                    static_cast<LPVOID>(fileVersionInfoBuffer.data())))
                {
                    LPVOID versionStringResource{ nullptr };    // this is a pointer into the vector of data above
                    UINT versionStringResourceLength{ 0 };

                    if (VerQueryValueW(
                        static_cast<LPCVOID>(fileVersionInfoBuffer.data()),
                        L"\\StringFileInfo\\040904B0\\CompanyName",
                        &versionStringResource,
                        &versionStringResourceLength
                    ))
                    {
                        auto s = static_cast<wchar_t*>(versionStringResource);

                        auto ver = std::wstring{ s, versionStringResourceLength - 1 };

                        return ver;
                    }
                }
            }
        }

        return L"";
    }


}



#endif