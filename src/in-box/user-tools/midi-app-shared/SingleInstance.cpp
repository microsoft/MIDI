// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "SingleInstance.h"

namespace midiapp
{
    HANDLE SingleInstance::s_instanceMutex{ nullptr };
    HANDLE SingleInstance::s_windowSection{ nullptr };
    void* SingleInstance::s_windowView{ nullptr };
    std::wstring SingleInstance::s_appKey{};
    HWND SingleInstance::s_publishedWindow{ nullptr };

    _Use_decl_annotations_
    std::wstring SingleInstance::MutexName(std::wstring const& appKey) noexcept
    {
        return L"Local\\Microsoft.WindowsMidiServices." + appKey + L".Instance";
    }

    _Use_decl_annotations_
    std::wstring SingleInstance::SectionName(std::wstring const& appKey) noexcept
    {
        return L"Local\\Microsoft.WindowsMidiServices." + appKey + L".Window";
    }

    _Use_decl_annotations_
    bool SingleInstance::AcquireOrActivateExisting(std::wstring const& appKey) noexcept
    {
        auto const mutexName = MutexName(appKey);

        s_appKey = appKey;

        s_instanceMutex = ::CreateMutexW(nullptr, TRUE, mutexName.c_str());

        if (s_instanceMutex == nullptr)
        {
            // Without the mutex there is no way to tell, and refusing to start would be worse
            // than the duplicate window this risks.
            return true;
        }

        if (::GetLastError() != ERROR_ALREADY_EXISTS)
        {
            // First one here. The section is created now so that a second instance arriving
            // before the window exists finds an empty handle rather than nothing at all.
            auto const sectionName = SectionName(appKey);

            s_windowSection = ::CreateFileMappingW(
                INVALID_HANDLE_VALUE,
                nullptr,
                PAGE_READWRITE,
                0,
                sizeof(uint64_t),
                sectionName.c_str());

            if (s_windowSection != nullptr)
            {
                s_windowView = ::MapViewOfFile(s_windowSection, FILE_MAP_WRITE, 0, 0, sizeof(uint64_t));

                if (s_windowView != nullptr)
                {
                    *static_cast<uint64_t*>(s_windowView) = 0;
                }
            }

            return true;
        }

        ::CloseHandle(s_instanceMutex);
        s_instanceMutex = nullptr;

        // Failing to raise the other window is not worth reporting. The customer still ends up
        // with one instance, which is the point.
        TryActivateExisting(appKey);

        return false;
    }

    _Use_decl_annotations_
    bool SingleInstance::TryActivateExisting(std::wstring const& appKey) noexcept
    {
        auto const window = FindExistingWindow(appKey);

        if (window == nullptr)
        {
            return false;
        }

        // One call rather than a show followed by a restore, because a tool which hides itself
        // in the notification area acts on the window becoming visible and would hide it again.
        if (::IsIconic(window))
        {
            ::ShowWindow(window, SW_RESTORE);
        }
        else if (!::IsWindowVisible(window))
        {
            // Naming the state is what keeps a window that was hidden while maximized maximized.
            ::ShowWindow(window, ::IsZoomed(window) ? SW_SHOWMAXIMIZED : SW_SHOW);
        }

        // This is best effort. Windows refuses a foreground change from a process which has not
        // been given the right, and the customer having just launched us is usually what grants
        // it. Failing means their window is restored but not raised, which is still better than
        // a second copy of the app.
        return ::SetForegroundWindow(window) != FALSE;
    }

    _Use_decl_annotations_
    HWND SingleInstance::FindExistingWindow(std::wstring const& appKey) noexcept
    {
        auto const sectionName = SectionName(appKey);

        wil::unique_handle section{ ::OpenFileMappingW(FILE_MAP_READ, FALSE, sectionName.c_str()) };

        if (!section)
        {
            return nullptr;
        }

        auto view = ::MapViewOfFile(section.get(), FILE_MAP_READ, 0, 0, sizeof(uint64_t));

        if (view == nullptr)
        {
            return nullptr;
        }

        auto const handleValue = *static_cast<uint64_t volatile*>(view);

        ::UnmapViewOfFile(view);

        auto const window = reinterpret_cast<HWND>(static_cast<ULONG_PTR>(handleValue));

        // The other instance may still be starting, or may have gone away between the mutex
        // check and here.
        return (window != nullptr && ::IsWindow(window)) ? window : nullptr;
    }

    _Use_decl_annotations_
    bool SingleInstance::IsRunning(std::wstring const& appKey) noexcept
    {
        auto const mutexName = MutexName(appKey);

        wil::unique_handle instanceMutex{ ::OpenMutexW(SYNCHRONIZE, FALSE, mutexName.c_str()) };

        if (instanceMutex)
        {
            return true;
        }

        // Anything other than "there is no such object" means it exists but this process cannot
        // have it, which still answers the question that was asked.
        return ::GetLastError() != ERROR_FILE_NOT_FOUND;
    }

    _Use_decl_annotations_
    void SingleInstance::PublishMainWindow(HWND const window) noexcept
    {
        s_publishedWindow = window;

        if (s_windowView == nullptr)
        {
            return;
        }

        *static_cast<uint64_t volatile*>(s_windowView) =
            static_cast<uint64_t>(reinterpret_cast<ULONG_PTR>(window));
    }

    bool SingleInstance::Reacquire() noexcept
    {
        if (s_instanceMutex != nullptr)
        {
            return true;
        }

        if (s_appKey.empty())
        {
            return false;
        }

        auto const window = s_publishedWindow;

        if (!AcquireOrActivateExisting(s_appKey))
        {
            return false;
        }

        if (window != nullptr)
        {
            PublishMainWindow(window);
        }

        return true;
    }

    void SingleInstance::Release() noexcept
    {
        if (s_windowView != nullptr)
        {
            ::UnmapViewOfFile(s_windowView);
            s_windowView = nullptr;
        }

        if (s_windowSection != nullptr)
        {
            ::CloseHandle(s_windowSection);
            s_windowSection = nullptr;
        }

        if (s_instanceMutex != nullptr)
        {
            ::ReleaseMutex(s_instanceMutex);
            ::CloseHandle(s_instanceMutex);
            s_instanceMutex = nullptr;
        }
    }
}
