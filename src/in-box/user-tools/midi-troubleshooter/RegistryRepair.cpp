// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "RegistryRepair.h"
#include "StringResources.h"
#include "SystemInfo.h"

namespace miditroubleshooter
{
    namespace
    {
        // Everything on this page is plain wide strings, so the resource helpers are wrapped
        // once here rather than converting at every call site.
        namespace res
        {
            inline std::wstring GetString(std::wstring_view resourceKey) noexcept
            {
                return std::wstring{ ::miditroubleshooter::resources::GetString(resourceKey) };
            }

            template <typename... TArgs>
            inline std::wstring FormatString(std::wstring_view resourceKey, TArgs&&... args) noexcept
            {
                return std::wstring{ ::miditroubleshooter::resources::FormatString(
                    resourceKey, std::forward<TArgs>(args)...) };
            }
        }

        constexpr wchar_t Drivers32Key[] = LR"(SOFTWARE\Microsoft\Windows NT\CurrentVersion\Drivers32)";
        constexpr wchar_t Drivers32WowKey[] = LR"(SOFTWARE\WOW6432Node\Microsoft\Windows NT\CurrentVersion\Drivers32)";
        constexpr wchar_t MidiRootKey[] = LR"(Software\Microsoft\Windows MIDI Services)";
        constexpr wchar_t TransportPluginsKey[] = LR"(Software\Microsoft\Windows MIDI Services\Transport Plugins)";

        constexpr wchar_t ValueUseLegacyMidi[] = L"UseLegacyMidi";
        constexpr wchar_t ValueMidisrvTransferComplete[] = L"MidisrvTransferComplete";

        constexpr wchar_t RequiredMidiDriver[] = L"wdmaud.drv";
        constexpr wchar_t RequiredMidi1Driver[] = L"wdmaud2.drv";

        // The service hosts these two itself, so they never appear under the MIDI root key but
        // are still part of a healthy installation.
        constexpr wchar_t MidisrvTransportClassId[] = L"{2BA15E4E-5417-4A66-85B8-2B2260EFBC84}";
        constexpr wchar_t DiagnosticsTransportClassId[] = L"{ac9b5417-3fe0-4e62-960f-034ee4235a1a}";

        std::wstring Lowered(_In_ std::wstring const& value) noexcept
        {
            std::wstring copy{ value };

            std::transform(copy.begin(), copy.end(), copy.begin(),
                [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

            return copy;
        }

        bool IsNumericSuffix(_In_ std::wstring const& valueName) noexcept
        {
            // everything after the leading "midi"
            if (valueName.size() <= 4)
            {
                return true;
            }

            auto const suffix = valueName.substr(4);

            return suffix.find_first_not_of(L"0123456789") == std::wstring::npos;
        }

        // Drivers other than the two required ones are allowed to sit in midi2..midi9. These
        // are the ones seen often enough to name, so a healthy machine does not look broken.
        bool IsKnownThirdPartyDriver(_In_ std::wstring const& loweredDriver, _Out_ std::wstring& comment) noexcept
        {
            if (loweredDriver == L"korgbm64.drv")
            {
                comment = res::GetString(L"RegistryCommentKorgBluetoothDriver");
                return true;
            }

            if (loweredDriver == L"midimapper.dll")
            {
                comment = res::GetString(L"RegistryCommentCoolSoftMidiMapper");
                return true;
            }

            if (loweredDriver == L"virtualmidisynth.dll")
            {
                comment = res::GetString(L"RegistryCommentCoolSoftVirtualMidiSynth");
                return true;
            }

            comment.clear();
            return false;
        }

        bool IsDeprecatedKorgUsbDriver(_In_ std::wstring const& loweredDriver) noexcept
        {
            return loweredDriver == L"korgum64.drv" ||
                loweredDriver == L"korgumds.drv" ||
                loweredDriver == L"korgumdd.drv";
        }

        std::optional<std::wstring> TryReadString(
            _In_ HKEY const key,
            _In_ wchar_t const* const valueName) noexcept
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

        // Reads the InprocServer32 default value for a class id, which is where a service
        // transport's DLL path lives.
        void ResolveComServer(_Inout_ TransportRegistrationInfo& info) noexcept
        {
            try
            {
                if (info.ClassId.empty())
                {
                    return;
                }

                auto const location = std::wstring{ L"CLSID\\" } + info.ClassId + L"\\InprocServer32";

                wil::unique_hkey serverKey{};

                if (FAILED(wil::reg::open_unique_key_nothrow(
                    HKEY_CLASSES_ROOT, location.c_str(), serverKey, wil::reg::key_access::read)))
                {
                    return;
                }

                info.ModuleRegistered = true;

                auto const path = TryReadString(serverKey.get(), nullptr);

                if (!path.has_value())
                {
                    return;
                }

                info.ModulePath = path.value();

                // A bare file name resolves through the normal search path, which for the
                // service means System32.
                auto resolved = info.ModulePath;

                if (resolved.find(L'\\') == std::wstring::npos)
                {
                    wchar_t systemFolder[MAX_PATH]{};

                    if (::GetSystemDirectoryW(systemFolder, ARRAYSIZE(systemFolder)) > 0)
                    {
                        resolved = std::wstring{ systemFolder } + L"\\" + resolved;
                    }
                }

                auto const attributes = ::GetFileAttributesW(resolved.c_str());

                info.ModuleFileFound =
                    attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;

                if (info.ModuleFileFound)
                {
                    info.ModuleVersion = GetFileVersionString(resolved);
                }
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to resolve a registered COM server.")
        }

        void ScanDrivers32(
            _In_ wchar_t const* const keyPath,
            _In_ bool const is64Bit,
            _Inout_ std::vector<RegistryEntryInfo>& entries,
            _Inout_ std::vector<std::wstring>& valuesToDelete,
            _Out_ bool& keyOpened,
            _Out_ bool& midiValueNeedsReplacing,
            _Out_ bool& midi1ValueNeedsReplacing,
            _Out_ bool& transferCompleteNeedsWriting) noexcept
        {
            keyOpened = false;
            midiValueNeedsReplacing = true;
            midi1ValueNeedsReplacing = true;
            transferCompleteNeedsWriting = is64Bit;

            try
            {
                wil::unique_hkey key{};

                if (FAILED(wil::reg::open_unique_key_nothrow(
                    HKEY_LOCAL_MACHINE, keyPath, key, wil::reg::key_access::read)))
                {
                    return;
                }

                keyOpened = true;

                for (auto const& valueData :
                    wil::make_range(wil::reg::value_iterator{ key.get() }, wil::reg::value_iterator{}))
                {
                    std::wstring const valueName{ valueData.name };
                    auto const loweredName = Lowered(valueName);

                    if (!loweredName.starts_with(L"midi"))
                    {
                        continue;
                    }

                    RegistryEntryInfo entry{};
                    entry.Name = valueName;

                    if (loweredName == Lowered(ValueMidisrvTransferComplete))
                    {
                        auto const dword = wil::reg::try_get_value_dword(key.get(), valueData.name.c_str());

                        if (dword.has_value())
                        {
                            entry.Value = std::to_wstring(dword.value());

                            if (dword.value() == 1)
                            {
                                entry.Severity = EntrySeverity::Ok;
                                entry.Comment = res::GetString(L"RegistryCommentTransferCompleteOk");
                                transferCompleteNeedsWriting = false;
                            }
                            else
                            {
                                entry.Severity = EntrySeverity::Error;
                                entry.Comment = res::GetString(L"RegistryCommentTransferCompleteWrong");
                            }
                        }
                        else
                        {
                            entry.Value = res::GetString(L"RegistryValueWrongType");
                            entry.Severity = EntrySeverity::Error;
                            entry.Comment = res::GetString(L"RegistryCommentTransferCompleteWrong");
                        }

                        entries.push_back(entry);
                        continue;
                    }

                    auto const stringValue = TryReadString(key.get(), valueData.name.c_str());

                    if (valueData.type != REG_SZ || !stringValue.has_value())
                    {
                        entry.Value = res::GetString(L"RegistryValueWrongType");
                        entry.Severity = EntrySeverity::Error;
                        entry.Comment = res::GetString(L"RegistryCommentWrongType");

                        if (IsNumericSuffix(loweredName))
                        {
                            valuesToDelete.push_back(valueName);
                        }

                        entries.push_back(entry);
                        continue;
                    }

                    entry.Value = stringValue.value();

                    auto const loweredDriver = Lowered(entry.Value);

                    if (loweredName == L"midi")
                    {
                        if (loweredDriver == RequiredMidiDriver)
                        {
                            entry.Severity = EntrySeverity::Ok;
                            entry.Comment = res::GetString(L"RegistryCommentInBoxSynth");
                            midiValueNeedsReplacing = false;
                        }
                        else
                        {
                            entry.Severity = EntrySeverity::Error;
                            entry.Comment = res::FormatString(L"RegistryCommentMustBeFormat", winrt::hstring{ RequiredMidiDriver });
                        }
                    }
                    else if (loweredName == L"midi1")
                    {
                        if (loweredDriver == RequiredMidi1Driver)
                        {
                            entry.Severity = EntrySeverity::Ok;
                            entry.Comment = res::GetString(L"RegistryCommentServiceInterface");
                            midi1ValueNeedsReplacing = false;
                        }
                        else
                        {
                            entry.Severity = EntrySeverity::Error;
                            entry.Comment = res::FormatString(L"RegistryCommentMustBeFormat", winrt::hstring{ RequiredMidi1Driver });
                        }
                    }
                    else if (loweredName == L"midi0")
                    {
                        entry.Severity = EntrySeverity::Error;
                        entry.Comment = res::GetString(L"RegistryCommentMidi0Invalid");
                        valuesToDelete.push_back(valueName);
                    }
                    else if (!IsNumericSuffix(loweredName))
                    {
                        // midimapper and anything else that is not one of the ten slots
                        entry.Severity = EntrySeverity::Ok;
                        entry.Comment = loweredName == L"midimapper" ?
                            res::GetString(L"RegistryCommentMidiMapper") :
                            res::GetString(L"RegistryCommentNotUsedByMidi");
                    }
                    else if (IsDeprecatedKorgUsbDriver(loweredDriver))
                    {
                        entry.Severity = EntrySeverity::Error;
                        entry.Comment = res::GetString(L"RegistryCommentDeprecatedKorgUsbDriver");
                        valuesToDelete.push_back(valueName);
                    }
                    else
                    {
                        std::wstring knownComment{};

                        if (IsKnownThirdPartyDriver(loweredDriver, knownComment))
                        {
                            entry.Severity = EntrySeverity::Ok;
                            entry.Comment = knownComment;
                        }
                        else
                        {
                            entry.Severity = EntrySeverity::Error;
                            entry.Comment = res::GetString(L"RegistryCommentUnrecognizedEntry");
                            valuesToDelete.push_back(valueName);
                        }
                    }

                    entries.push_back(entry);
                }
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to scan a Drivers32 key.")
        }

        void ScanTransports(_Inout_ std::vector<TransportRegistrationInfo>& transports) noexcept
        {
            try
            {
                for (auto const& pair : {
                        std::pair<std::wstring, std::wstring>{ res::GetString(L"TransportNameMidisrv"), MidisrvTransportClassId },
                        std::pair<std::wstring, std::wstring>{ res::GetString(L"TransportNameDiagnostics"), DiagnosticsTransportClassId } })
                {
                    TransportRegistrationInfo info{};

                    info.Name = pair.first;
                    info.ClassId = pair.second;
                    info.BuiltIn = true;
                    info.Enabled = true;

                    ResolveComServer(info);

                    transports.push_back(info);
                }

                wil::unique_hkey pluginsKey{};

                if (FAILED(wil::reg::open_unique_key_nothrow(
                    HKEY_LOCAL_MACHINE, TransportPluginsKey, pluginsKey, wil::reg::key_access::read)))
                {
                    return;
                }

                for (auto const& keyData :
                    wil::make_range(wil::reg::key_iterator{ pluginsKey.get() }, wil::reg::key_iterator{}))
                {
                    TransportRegistrationInfo info{};
                    info.Name = keyData.name;

                    wil::unique_hkey transportKey{};

                    auto const path = std::wstring{ TransportPluginsKey } + L"\\" + keyData.name;

                    if (SUCCEEDED(wil::reg::open_unique_key_nothrow(
                        HKEY_LOCAL_MACHINE, path.c_str(), transportKey, wil::reg::key_access::read)))
                    {
                        auto const classId = TryReadString(transportKey.get(), L"CLSID");

                        if (classId.has_value())
                        {
                            info.ClassId = classId.value();
                        }

                        auto const enabled = wil::reg::try_get_value_dword(transportKey.get(), L"Enabled");

                        info.EnabledValuePresent = enabled.has_value();

                        // absent means enabled, which is how the service reads it
                        info.Enabled = !enabled.has_value() || enabled.value() > 0;
                    }

                    ResolveComServer(info);

                    transports.push_back(info);
                }
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to scan the registered transports.")
        }

        void ScanServiceRoot(_Inout_ std::vector<RegistryEntryInfo>& entries) noexcept
        {
            try
            {
                wil::unique_hkey rootKey{};

                if (FAILED(wil::reg::open_unique_key_nothrow(
                    HKEY_LOCAL_MACHINE, MidiRootKey, rootKey, wil::reg::key_access::read)))
                {
                    RegistryEntryInfo missing{};

                    missing.Name = MidiRootKey;
                    missing.Value = res::GetString(L"RegistryValueMissing");
                    missing.Severity = EntrySeverity::Error;
                    missing.Comment = res::GetString(L"RegistryCommentServiceRootMissing");

                    entries.push_back(missing);
                    return;
                }

                RegistryEntryInfo configEntry{};
                configEntry.Name = L"CurrentConfig";

                auto const currentConfig = TryReadString(rootKey.get(), L"CurrentConfig");

                if (currentConfig.has_value())
                {
                    configEntry.Value = currentConfig.value();
                    configEntry.Comment = res::GetString(L"RegistryCommentCurrentConfig");
                }
                else
                {
                    configEntry.Value = res::GetString(L"RegistryValueMissing");
                    configEntry.Severity = EntrySeverity::Warning;
                    configEntry.Comment = res::GetString(L"RegistryCommentCurrentConfigMissing");
                }

                entries.push_back(configEntry);
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to scan the MIDI service registry root.")
        }
    }

    ApiMode GetCurrentApiMode() noexcept
    {
        try
        {
            auto const value = wil::reg::try_get_value_dword(HKEY_LOCAL_MACHINE, Drivers32Key, ValueUseLegacyMidi);

            if (value.has_value() && value.value() <= static_cast<uint32_t>(ApiMode::Hybrid))
            {
                return static_cast<ApiMode>(value.value());
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to read the API mode.")

        // not set, out of range, or unreadable: the default is in effect
        return ApiMode::WindowsMidiServices;
    }

    _Use_decl_annotations_
    bool TrySetApiMode(ApiMode mode) noexcept
    {
        try
        {
            return SUCCEEDED(wil::reg::set_value_dword_nothrow(
                HKEY_LOCAL_MACHINE, Drivers32Key, ValueUseLegacyMidi, static_cast<DWORD>(mode)));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to write the API mode.")

        return false;
    }

    std::vector<TransportRegistrationInfo> ScanRegisteredTransports() noexcept
    {
        std::vector<TransportRegistrationInfo> transports{};

        ScanTransports(transports);

        return transports;
    }

    RegistryScan ScanRegistry() noexcept
    {
        RegistryScan scan{};

        try
        {
            auto const apiModeValue = wil::reg::try_get_value_dword(HKEY_LOCAL_MACHINE, Drivers32Key, ValueUseLegacyMidi);

            scan.ApiModeValuePresent = apiModeValue.has_value();
            scan.Mode = GetCurrentApiMode();
            scan.LegacyMode = scan.Mode == ApiMode::Legacy;

            bool midiNeeds64{ false };
            bool midi1Needs64{ false };
            bool transferNeeds{ false };
            bool midiNeedsWow{ false };
            bool midi1NeedsWow{ false };
            bool transferNeedsWow{ false };

            ScanDrivers32(
                Drivers32Key, true,
                scan.Drivers32Entries, scan.Plan.DeleteValues64,
                scan.Drivers32KeyOpened, midiNeeds64, midi1Needs64, transferNeeds);

            ScanDrivers32(
                Drivers32WowKey, false,
                scan.Drivers32WowEntries, scan.Plan.DeleteValuesWow,
                scan.Drivers32WowKeyOpened, midiNeedsWow, midi1NeedsWow, transferNeedsWow);

            ScanServiceRoot(scan.ServiceRootEntries);
            ScanTransports(scan.Transports);

            // In legacy mode midisrv and wdmaud2.drv are deliberately out of the picture, so
            // rewriting these entries would break the customer's chosen configuration.
            if (scan.LegacyMode)
            {
                scan.Plan = RegistryRepairPlan{};
                return scan;
            }

            scan.Plan.SetMidiValue64 = midiNeeds64 && scan.Drivers32KeyOpened;
            scan.Plan.SetMidi1Value64 = midi1Needs64 && scan.Drivers32KeyOpened;
            scan.Plan.SetTransferComplete = transferNeeds && scan.Drivers32KeyOpened;

            scan.Plan.SetMidiValueWow = midiNeedsWow && scan.Drivers32WowKeyOpened;
            scan.Plan.SetMidi1ValueWow = midi1NeedsWow && scan.Drivers32WowKeyOpened;

            if (scan.Plan.SetMidiValue64)
            {
                scan.Plan.Descriptions.push_back(res::FormatString(
                    L"RepairSetValueFormat", winrt::hstring{ L"midi" }, winrt::hstring{ RequiredMidiDriver },
                    res::GetString(L"RegistryScope64Bit")));
            }

            if (scan.Plan.SetMidi1Value64)
            {
                scan.Plan.Descriptions.push_back(res::FormatString(
                    L"RepairSetValueFormat", winrt::hstring{ L"midi1" }, winrt::hstring{ RequiredMidi1Driver },
                    res::GetString(L"RegistryScope64Bit")));
            }

            for (auto const& name : scan.Plan.DeleteValues64)
            {
                scan.Plan.Descriptions.push_back(res::FormatString(
                    L"RepairDeleteValueFormat", winrt::hstring{ name }, res::GetString(L"RegistryScope64Bit")));
            }

            if (scan.Plan.SetTransferComplete)
            {
                scan.Plan.Descriptions.push_back(res::GetString(L"RepairSetTransferComplete"));
            }

            if (scan.Plan.SetMidiValueWow)
            {
                scan.Plan.Descriptions.push_back(res::FormatString(
                    L"RepairSetValueFormat", winrt::hstring{ L"midi" }, winrt::hstring{ RequiredMidiDriver },
                    res::GetString(L"RegistryScope32Bit")));
            }

            if (scan.Plan.SetMidi1ValueWow)
            {
                scan.Plan.Descriptions.push_back(res::FormatString(
                    L"RepairSetValueFormat", winrt::hstring{ L"midi1" }, winrt::hstring{ RequiredMidi1Driver },
                    res::GetString(L"RegistryScope32Bit")));
            }

            for (auto const& name : scan.Plan.DeleteValuesWow)
            {
                scan.Plan.Descriptions.push_back(res::FormatString(
                    L"RepairDeleteValueFormat", winrt::hstring{ name }, res::GetString(L"RegistryScope32Bit")));
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to scan the registry.")

        return scan;
    }

    namespace
    {
        bool ApplyToKey(
            _In_ wchar_t const* const keyPath,
            _In_ std::vector<std::wstring> const& valuesToDelete,
            _In_ bool const setMidi,
            _In_ bool const setMidi1,
            _In_ bool const setTransferComplete,
            _Inout_ std::vector<std::wstring>& messages) noexcept
        {
            bool allSucceeded{ true };

            try
            {
                wil::unique_hkey key{};

                if (FAILED(wil::reg::open_unique_key_nothrow(
                    HKEY_LOCAL_MACHINE, keyPath, key, wil::reg::key_access::readwrite)))
                {
                    messages.push_back(res::FormatString(L"RepairCannotOpenKeyFormat", winrt::hstring{ keyPath }));
                    return false;
                }

                for (auto const& valueName : valuesToDelete)
                {
                    if (::RegDeleteValueW(key.get(), valueName.c_str()) == ERROR_SUCCESS)
                    {
                        messages.push_back(res::FormatString(L"RepairDeletedValueFormat", winrt::hstring{ valueName }));
                    }
                    else
                    {
                        messages.push_back(res::FormatString(L"RepairDeleteFailedFormat", winrt::hstring{ valueName }));
                        allSucceeded = false;
                    }
                }

                if (setMidi)
                {
                    if (SUCCEEDED(wil::reg::set_value_string_nothrow(key.get(), L"midi", RequiredMidiDriver)))
                    {
                        messages.push_back(res::FormatString(
                            L"RepairWroteValueFormat", winrt::hstring{ L"midi" }, winrt::hstring{ RequiredMidiDriver }));
                    }
                    else
                    {
                        messages.push_back(res::FormatString(L"RepairWriteFailedFormat", winrt::hstring{ L"midi" }));
                        allSucceeded = false;
                    }
                }

                if (setMidi1)
                {
                    if (SUCCEEDED(wil::reg::set_value_string_nothrow(key.get(), L"midi1", RequiredMidi1Driver)))
                    {
                        messages.push_back(res::FormatString(
                            L"RepairWroteValueFormat", winrt::hstring{ L"midi1" }, winrt::hstring{ RequiredMidi1Driver }));
                    }
                    else
                    {
                        messages.push_back(res::FormatString(L"RepairWriteFailedFormat", winrt::hstring{ L"midi1" }));
                        allSucceeded = false;
                    }
                }

                if (setTransferComplete)
                {
                    if (SUCCEEDED(wil::reg::set_value_dword_nothrow(key.get(), ValueMidisrvTransferComplete, 1)))
                    {
                        messages.push_back(res::FormatString(
                            L"RepairWroteValueFormat", winrt::hstring{ ValueMidisrvTransferComplete }, winrt::hstring{ L"1" }));
                    }
                    else
                    {
                        messages.push_back(res::FormatString(
                            L"RepairWriteFailedFormat", winrt::hstring{ ValueMidisrvTransferComplete }));
                        allSucceeded = false;
                    }
                }
            }
            catch (...)
            {
                MIDI_TSHOOT_LOG_GENERAL_EXCEPTION(L"Unable to apply registry repairs.");
                allSucceeded = false;
            }

            return allSucceeded;
        }
    }

    _Use_decl_annotations_
    RepairResult ApplyRegistryRepair(RegistryRepairPlan const& plan) noexcept
    {
        RepairResult result{};

        if (!plan.AnyChanges())
        {
            result.Succeeded = true;
            return result;
        }

        auto const wroteMachine = ApplyToKey(
            Drivers32Key,
            plan.DeleteValues64,
            plan.SetMidiValue64,
            plan.SetMidi1Value64,
            plan.SetTransferComplete,
            result.Messages);

        auto const wroteWow = ApplyToKey(
            Drivers32WowKey,
            plan.DeleteValuesWow,
            plan.SetMidiValueWow,
            plan.SetMidi1ValueWow,
            false,
            result.Messages);

        result.Succeeded = wroteMachine && wroteWow;

        return result;
    }

    bool TryRestartComputer() noexcept
    {
        try
        {
            wil::unique_handle token;

            if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, token.put()))
            {
                return false;
            }

            TOKEN_PRIVILEGES privileges{};
            privileges.PrivilegeCount = 1;
            privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

            if (!::LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME, &privileges.Privileges[0].Luid))
            {
                return false;
            }

            if (!::AdjustTokenPrivileges(token.get(), FALSE, &privileges, 0, nullptr, nullptr))
            {
                return false;
            }

            // AdjustTokenPrivileges reports success even when it could not assign the privilege
            if (::GetLastError() != ERROR_SUCCESS)
            {
                return false;
            }

            return ::ExitWindowsEx(
                EWX_REBOOT | EWX_RESTARTAPPS,
                SHTDN_REASON_MAJOR_OPERATINGSYSTEM | SHTDN_REASON_MINOR_RECONFIG | SHTDN_REASON_FLAG_PLANNED) != FALSE;
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to restart the computer.")

        return false;
    }
}
