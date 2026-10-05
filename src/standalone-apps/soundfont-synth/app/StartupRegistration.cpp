// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "StartupRegistration.h"

namespace appmodel = ::winrt::Windows::ApplicationModel;

namespace midisoundfontsynth
{
    namespace
    {
        constexpr wchar_t RunKeyPath[] = LR"(Software\Microsoft\Windows\CurrentVersion\Run)";
        constexpr wchar_t RunValueName[] = L"MidiSoundFontSynth";

        std::wstring CurrentExecutablePath()
        {
            std::wstring buffer(MAX_PATH, L'\0');

            for (;;)
            {
                auto const length = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));

                if (length == 0)
                {
                    return {};
                }

                if (length < buffer.size())
                {
                    buffer.resize(length);
                    return buffer;
                }

                if (buffer.size() > 32768)
                {
                    return {};
                }

                buffer.resize(buffer.size() * 2);
            }
        }

        StartupState FromTaskState(appmodel::StartupTaskState state) noexcept
        {
            switch (state)
            {
            case appmodel::StartupTaskState::Enabled:
                return StartupState::On;

            case appmodel::StartupTaskState::DisabledByUser:
                return StartupState::OffByUser;

            case appmodel::StartupTaskState::DisabledByPolicy:
            case appmodel::StartupTaskState::EnabledByPolicy:
                return StartupState::ControlledByPolicy;

            default:
                return StartupState::Off;
            }
        }

        bool RunValueExists() noexcept
        {
            wil::unique_hkey key{};

            if (::RegOpenKeyExW(HKEY_CURRENT_USER, RunKeyPath, 0, KEY_QUERY_VALUE, key.put()) != ERROR_SUCCESS)
            {
                return false;
            }

            return ::RegQueryValueExW(key.get(), RunValueName, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
        }

        bool SetRunValue(bool enable) noexcept
        {
            try
            {
                wil::unique_hkey key{};

                if (::RegCreateKeyExW(
                    HKEY_CURRENT_USER, RunKeyPath, 0, nullptr, 0,
                    KEY_SET_VALUE, nullptr, key.put(), nullptr) != ERROR_SUCCESS)
                {
                    return false;
                }

                if (!enable)
                {
                    auto const result = ::RegDeleteValueW(key.get(), RunValueName);
                    return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
                }

                auto const path = CurrentExecutablePath();

                if (path.empty())
                {
                    return false;
                }

                // Quoted, because the path may contain spaces and an unquoted Run entry would be
                // read as a different executable plus arguments.
                auto const command = L'"' + path + L"\" --minimized";

                return ::RegSetValueExW(
                    key.get(), RunValueName, 0, REG_SZ,
                    reinterpret_cast<BYTE const*>(command.c_str()),
                    static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
            }
            catch (...)
            {
                return false;
            }
        }
    }

    bool StartupRegistration::IsPackaged() noexcept
    {
        UINT32 length{ 0 };

        return ::GetCurrentPackageFullName(&length, nullptr) != APPMODEL_ERROR_NO_PACKAGE;
    }

    winrt::Windows::Foundation::IAsyncOperation<int32_t> StartupRegistration::GetStateAsync()
    {
        try
        {
            if (!IsPackaged())
            {
                co_return static_cast<int32_t>(RunValueExists() ? StartupState::On : StartupState::Off);
            }

            auto const task = co_await appmodel::StartupTask::GetAsync(StartupTaskId);

            co_return static_cast<int32_t>(FromTaskState(task.State()));
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to read whether the app starts with Windows.")

        co_return static_cast<int32_t>(StartupState::Off);
    }

    winrt::Windows::Foundation::IAsyncOperation<int32_t> StartupRegistration::SetEnabledAsync(bool enable)
    {
        try
        {
            if (!IsPackaged())
            {
                if (SetRunValue(enable))
                {
                    co_return static_cast<int32_t>(enable ? StartupState::On : StartupState::Off);
                }

                co_return static_cast<int32_t>(RunValueExists() ? StartupState::On : StartupState::Off);
            }

            auto const task = co_await appmodel::StartupTask::GetAsync(StartupTaskId);

            if (!enable)
            {
                task.Disable();
                co_return static_cast<int32_t>(FromTaskState(task.State()));
            }

            auto const state = co_await task.RequestEnableAsync();

            co_return static_cast<int32_t>(FromTaskState(state));
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to change whether the app starts with Windows.")

        co_return static_cast<int32_t>(StartupState::Off);
    }
}
