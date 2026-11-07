// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MmcssTasks.h"
#include "StringResources.h"

#include <avrt.h>

namespace miditroubleshooter
{
    namespace
    {
        constexpr wchar_t SystemProfileKey[] = LR"(SOFTWARE\Microsoft\Windows NT\CurrentVersion\Multimedia\SystemProfile)";
        constexpr wchar_t TasksKey[] = LR"(SOFTWARE\Microsoft\Windows NT\CurrentVersion\Multimedia\SystemProfile\Tasks)";

        constexpr wchar_t ValueMaxThreads[] = L"MaxThreads";
        constexpr DWORD DefaultMaxThreadsPerProcess{ 32 };

        constexpr wchar_t ValuePriority[] = L"Priority";
        constexpr wchar_t ValueSchedulingCategory[] = L"Scheduling Category";
        constexpr wchar_t ValueSfioPriority[] = L"SFIO Priority";
        constexpr wchar_t ValueClockRate[] = L"Clock Rate";

        // The values Windows ships with. They have not changed since MMCSS was introduced.
        struct TaskDefaults
        {
            wchar_t const* Name;
            DWORD Priority;
            wchar_t const* SchedulingCategory;
            wchar_t const* SfioPriority;
            DWORD ClockRate;
        };

        constexpr TaskDefaults Tasks[] =
        {
            { L"Audio", 6, L"Medium", L"Normal", 10000 },
            { L"Pro Audio", 1, L"High", L"Normal", 10000 },
        };

        std::optional<DWORD> TryReadDword(_In_ HKEY const key, _In_ wchar_t const* const valueName) noexcept
        {
            try
            {
                return wil::reg::try_get_value_dword(key, valueName);
            }
            catch (...)
            {
                return std::nullopt;
            }
        }

        std::optional<DWORD> TryReadDword(
            _In_ HKEY const key,
            _In_ wchar_t const* const subKey,
            _In_ wchar_t const* const valueName) noexcept
        {
            try
            {
                return wil::reg::try_get_value_dword(key, subKey, valueName);
            }
            catch (...)
            {
                return std::nullopt;
            }
        }

        std::optional<std::wstring> TryReadString(_In_ HKEY const key, _In_ wchar_t const* const valueName) noexcept
        {
            try
            {
                return wil::reg::try_get_value_string(key, valueName);
            }
            catch (...)
            {
                return std::nullopt;
            }
        }

        bool EqualsIgnoreCase(_In_ std::optional<std::wstring> const& actual, _In_ wchar_t const* const expected) noexcept
        {
            return actual.has_value() && ::_wcsicmp(actual->c_str(), expected) == 0;
        }

        // Registry value names and data are shown as they appear in the registry, so they are
        // not localized.
        void AppendSetting(_Inout_ std::wstring& text, _In_ wchar_t const* const name, _In_ std::wstring const& value) noexcept
        {
            try
            {
                if (!text.empty())
                {
                    text += L", ";
                }

                text += name;
                text += L' ';
                text += value;
            }
            catch (...)
            {
            }
        }

        std::wstring DescribeDefaults(_In_ TaskDefaults const& defaults) noexcept
        {
            std::wstring text{};

            AppendSetting(text, ValuePriority, std::to_wstring(defaults.Priority));
            AppendSetting(text, ValueSchedulingCategory, defaults.SchedulingCategory);
            AppendSetting(text, ValueSfioPriority, defaults.SfioPriority);
            AppendSetting(text, ValueClockRate, std::to_wstring(defaults.ClockRate));

            return text;
        }

        // Asks MMCSS for the task the same way an app does. This is the real test: the
        // registry can look right while Windows still has the task dropped, because MMCSS
        // reads the profiles only when Windows starts.
        DWORD TryRegisterWithTask(_In_ wchar_t const* const taskName) noexcept
        {
            DWORD taskIndex{ 0 };

            auto const handle = ::AvSetMmThreadCharacteristicsW(taskName, &taskIndex);

            if (handle == nullptr)
            {
                auto const error = ::GetLastError();
                return error == ERROR_SUCCESS ? static_cast<DWORD>(ERROR_GEN_FAILURE) : error;
            }

            ::AvRevertMmThreadCharacteristics(handle);

            return ERROR_SUCCESS;
        }

        RegistryEntryInfo ScanTask(_In_ TaskDefaults const& defaults) noexcept
        {
            RegistryEntryInfo entry{};

            try
            {
                entry.Name = defaults.Name;

                auto const missing = std::wstring{ ::miditroubleshooter::resources::GetString(L"RegistryValueMissing") };

                std::optional<DWORD> priority{};
                std::optional<std::wstring> schedulingCategory{};
                std::optional<std::wstring> sfioPriority{};
                std::optional<DWORD> clockRate{};

                auto const keyPath = std::wstring{ TasksKey } + L"\\" + defaults.Name;

                wil::unique_hkey key{};

                if (SUCCEEDED(wil::reg::open_unique_key_nothrow(
                    HKEY_LOCAL_MACHINE, keyPath.c_str(), key, wil::reg::key_access::read)))
                {
                    priority = TryReadDword(key.get(), ValuePriority);
                    schedulingCategory = TryReadString(key.get(), ValueSchedulingCategory);
                    sfioPriority = TryReadString(key.get(), ValueSfioPriority);
                    clockRate = TryReadDword(key.get(), ValueClockRate);
                }

                AppendSetting(entry.Value, ValuePriority, priority.has_value() ? std::to_wstring(*priority) : missing);
                AppendSetting(entry.Value, ValueSchedulingCategory, schedulingCategory.value_or(missing));
                AppendSetting(entry.Value, ValueSfioPriority, sfioPriority.value_or(missing));
                AppendSetting(entry.Value, ValueClockRate, clockRate.has_value() ? std::to_wstring(*clockRate) : missing);

                auto const matchesDefaults =
                    priority == defaults.Priority &&
                    EqualsIgnoreCase(schedulingCategory, defaults.SchedulingCategory) &&
                    EqualsIgnoreCase(sfioPriority, defaults.SfioPriority) &&
                    clockRate == defaults.ClockRate;

                auto const registerResult = TryRegisterWithTask(defaults.Name);
                auto const defaultsText = DescribeDefaults(defaults);

                if (registerResult != ERROR_SUCCESS && matchesDefaults)
                {
                    entry.Severity = EntrySeverity::Error;
                    entry.Comment = ::miditroubleshooter::resources::FormatString(
                        L"RegistryCommentMmcssNeedsRestartFormat", registerResult);
                }
                else if (registerResult != ERROR_SUCCESS)
                {
                    entry.Severity = EntrySeverity::Error;
                    entry.Comment = ::miditroubleshooter::resources::FormatString(
                        L"RegistryCommentMmcssUnusableFormat", registerResult, defaultsText);
                }
                else if (!matchesDefaults)
                {
                    entry.Severity = EntrySeverity::Warning;
                    entry.Comment = ::miditroubleshooter::resources::FormatString(
                        L"RegistryCommentMmcssChangedFormat", defaultsText);
                }
                else
                {
                    entry.Severity = EntrySeverity::Ok;
                    entry.Comment = ::miditroubleshooter::resources::GetString(L"RegistryCommentMmcssOk");
                }
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to check an MMCSS task.")

            return entry;
        }

        // MaxThreads changes how many threads one app can register with MMCSS. Windows doesn't
        // set it, and without it each app gets 32. Music apps with many MMCSS threads can hit that
        // limit, so it's worth showing whether someone changed it.
        RegistryEntryInfo ScanMaxThreads() noexcept
        {
            RegistryEntryInfo entry{};

            try
            {
                entry.Name = ValueMaxThreads;

                DWORD type{ REG_NONE };
                auto const present = ::RegGetValueW(
                    HKEY_LOCAL_MACHINE, SystemProfileKey, ValueMaxThreads,
                    RRF_RT_ANY, &type, nullptr, nullptr) == ERROR_SUCCESS;

                if (!present)
                {
                    entry.Value = ::miditroubleshooter::resources::GetString(L"RegistryValueNotSet");
                    entry.Comment = ::miditroubleshooter::resources::FormatString(
                        L"RegistryCommentMmcssMaxThreadsNotSetFormat", DefaultMaxThreadsPerProcess);
                    entry.Severity = EntrySeverity::Ok;
                    return entry;
                }

                std::optional<DWORD> value{};

                if (type == REG_DWORD)
                {
                    value = TryReadDword(HKEY_LOCAL_MACHINE, SystemProfileKey, ValueMaxThreads);
                }

                entry.Value = value.has_value() ?
                    std::to_wstring(*value) :
                    std::wstring{ ::miditroubleshooter::resources::GetString(L"RegistryValueWrongType") };

                entry.Comment = ::miditroubleshooter::resources::FormatString(
                    L"RegistryCommentMmcssMaxThreadsSetFormat", DefaultMaxThreadsPerProcess);
                entry.Severity = EntrySeverity::Warning;
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to check the MMCSS thread limit.")

            return entry;
        }
    }

    std::vector<RegistryEntryInfo> ScanMmcssTasks() noexcept
    {
        std::vector<RegistryEntryInfo> entries{};

        try
        {
            for (auto const& task : Tasks)
            {
                entries.push_back(ScanTask(task));
            }

            entries.push_back(ScanMaxThreads());
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to check the MMCSS tasks.")

        return entries;
    }
}
