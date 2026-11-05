// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <windows.h>
#include <werapi.h>

#include <cwchar>

namespace midiapp
{
    // Header only and plain Win32, so any tool can call it as the first thing in wWinMain, before
    // the Windows App SDK or anything else that could fail has loaded.
    //
    // It registers midicrashhandler.dll with Windows Error Reporting. When this process crashes,
    // WER loads that DLL into WerFault.exe, and it opens MIDI Troubleshooting and Repair on its
    // Crash dumps page. WER writes the dump itself, through the LocalDumps entry the Tools
    // installer adds for each tool. WER only loads a DLL that is also listed under
    // HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting\RuntimeExceptionHelperModules, which
    // the installer does as well, so on a PC without that entry this does nothing at all.
    //
    // MIDI Troubleshooting and Repair doesn't call this. It would only be opening itself again.
    namespace crashhandler
    {
        inline constexpr wchar_t HandlerFileName[] = L"midicrashhandler.dll";
        inline constexpr wchar_t TroubleshooterBuildFolder[] = L"miditroubleshooter";

        // WER hands this pointer back to the handler. The handler doesn't need anything from the
        // crashed process, but it gets something valid rather than null.
        inline DWORD RegistrationContext{ 0x4D494443 };   // 'MIDC'

        // WER keeps the path, so it has to outlive the call.
        inline wchar_t RegisteredHandlerPath[MAX_PATH]{};

        inline bool FileExists(_In_z_ wchar_t const* path) noexcept
        {
            auto const attributes = ::GetFileAttributesW(path);

            return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
        }

        // Cuts the last part off a path, in place, and returns it. Null when there's no
        // backslash left to cut at. Done by hand, because the shell path helpers are marked
        // deprecated in tools whose precompiled header has pathcch.h.
        inline wchar_t const* CutLastPart(_Inout_z_ wchar_t* path) noexcept
        {
            auto const slash = std::wcsrchr(path, L'\\');

            if (slash == nullptr)
            {
                return nullptr;
            }

            *slash = L'\0';

            return slash + 1;
        }

        // Installed, every tool and the handler are in the same Tools folder. In a development
        // build each tool has its own vsfiles-sdk\out\<tool>\<platform>\<configuration> folder,
        // and the handler is copied into the Troubleshooter's, next to the app it starts.
        inline bool FindHandler(_Out_writes_(cch) wchar_t* path, _In_ DWORD cch) noexcept
        {
            path[0] = L'\0';

            // <out>\<tool>\<platform>\<configuration>\<tool>.exe, cut up as it is walked
            wchar_t walk[MAX_PATH]{};
            auto const length = ::GetModuleFileNameW(nullptr, walk, ARRAYSIZE(walk));

            if (length == 0 || length >= ARRAYSIZE(walk) || CutLastPart(walk) == nullptr)
            {
                return false;
            }

            if (swprintf_s(path, cch, L"%s\\%s", walk, HandlerFileName) > 0 && FileExists(path))
            {
                return true;
            }

            path[0] = L'\0';

            auto const configuration = CutLastPart(walk);
            auto const platform = configuration != nullptr ? CutLastPart(walk) : nullptr;

            // past the tool's own folder
            if (platform == nullptr || CutLastPart(walk) == nullptr)
            {
                return false;
            }

            if (swprintf_s(path, cch, L"%s\\%s\\%s\\%s\\%s",
                walk, TroubleshooterBuildFolder, platform, configuration, HandlerFileName) > 0 &&
                FileExists(path))
            {
                return true;
            }

            path[0] = L'\0';
            return false;
        }
    }
    // Never fails the app: a tool that can't register still runs, and WER still writes its dump.
    inline void RegisterCrashHandler() noexcept
    {
        if (crashhandler::RegisteredHandlerPath[0] != L'\0')
        {
            return;
        }

        if (!crashhandler::FindHandler(crashhandler::RegisteredHandlerPath, ARRAYSIZE(crashhandler::RegisteredHandlerPath)))
        {
            return;
        }

        if (FAILED(::WerRegisterRuntimeExceptionModule(
            crashhandler::RegisteredHandlerPath,
            &crashhandler::RegistrationContext)))
        {
            crashhandler::RegisteredHandlerPath[0] = L'\0';
        }
    }
}
