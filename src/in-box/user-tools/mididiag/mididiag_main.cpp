// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include "pch.h"

#include "console_tools_shared.h"
#include "mididiag_output.h"
#include "mididiag_sections.h"

#include <aclapi.h>
#include <mmddk.h>

// Every line of the report goes through mididiag_output.cpp, which lays it out, cleans what
// devices supply and keeps user names out. These keep the names the sections below were
// written with.

void OutputSectionHeader(_In_ std::wstring const& headerText)
{
    mididiag::WriteSection(headerText);
}

// a blank line between the records in a section
void OutputItemSeparator()
{
    mididiag::WriteRecordBreak();
}

void OutputHeader(_In_ std::wstring const& headerText)
{
    mididiag::WriteLine(headerText);
}

void OutputEntityNameField(_In_ std::wstring const& fieldName, _In_ winrt::hstring const& value)
{
    mididiag::WriteStyledField(fieldName, value, entityNameFieldValueTextStyle);
}

void OutputEntityIdentifierField(_In_ std::wstring const& fieldName, _In_ winrt::hstring const& value)
{
    mididiag::WriteStyledField(fieldName, value, entityIdentifierFieldValueTextStyle);
}

void OutputStringField(_In_ std::wstring const& fieldName, _In_ winrt::hstring const& value)
{
    mididiag::WriteField(fieldName, value);
}

void OutputStringField(_In_ std::wstring const& fieldName, _In_ std::wstring const& value)
{
    mididiag::WriteField(fieldName, value);
}

// most names and descriptions are empty, and a line with nothing on it only makes the report longer
void OutputStringFieldIfNotEmpty(_In_ std::wstring const& fieldName, _In_ std::wstring_view const value)
{
    if (!value.empty())
    {
        mididiag::WriteField(fieldName, value);
    }
}

void OutputBooleanField(_In_ std::wstring const& fieldName, _In_ bool const& value)
{
    mididiag::WriteBoolField(fieldName, value);
}

void OutputGuidField(_In_ std::wstring const& fieldName, _In_ winrt::guid const& value)
{
    mididiag::WriteStyledField(fieldName, internal::GuidToString(value), entityIdentifierFieldValueTextStyle);
}

void OutputTimestampField(_In_ std::wstring const& fieldName, _In_ uint64_t const value)
{
    mididiag::WriteNumberField(fieldName, value);
}

void OutputDateTimeField(_In_ std::wstring const& fieldName, _In_ foundation::DateTime const& value)
{
    auto const formatted = mididiag::FormatLocalTime(value);

    mididiag::WriteField(fieldName, formatted.empty() ? std::wstring{ L"Not reported" } : formatted);
}

void OutputCurrentTime()
{
    OutputDateTimeField(MIDIDIAG_FIELD_LABEL_CURRENT_TIME, winrt::clock::now());
}

void OutputNumericField(_In_ std::wstring const& fieldName, _In_ uint32_t const value)
{
    mididiag::WriteNumberField(fieldName, value);
}

void OutputDecimalMillisecondsField(_In_ std::wstring const& fieldName, _In_ double const value, _In_ uint32_t precision)
{
    mididiag::WriteField(fieldName, std::format(L"{:.{}f} ms", value, precision));
}

void OutputHexNumericField(_In_ std::wstring const& fieldName, _In_ uint32_t const value)
{
    mididiag::WriteField(fieldName, std::format(L"0x{:x}", value));
}


void OutputError(_In_ winrt::hresult_error const& error)
{
    mididiag::WriteError(mididiag::FormatHResult(error.code()) + L" : " + std::wstring{ error.message() });
}

void OutputError(_In_ std::wstring const& errorMessage)
{
    mididiag::WriteError(errorMessage);
}

void OutputRegStringValue(std::wstring label, HKEY const key, std::wstring value)
{
    auto keyValue = wil::reg::try_get_value_string(key, value.c_str());
    if (keyValue.has_value())
    {
        OutputStringField(label, keyValue.value());
    }
    else
    {
        OutputStringField(label, std::wstring{ L"Not present" });
    }
}

// dword value > 0 == true
void OutputRegDWordBooleanValue(std::wstring label, HKEY const key, std::wstring value)
{
    auto keyValue = wil::reg::try_get_value_dword(key, value.c_str());
    if (keyValue.has_value())
    {
        OutputBooleanField(label, keyValue.value() > 0);
    }
    else
    {
        OutputStringField(label, std::wstring{ L"Not present" });
    }
}

void OutputRegDWordNumericValue(std::wstring label, HKEY const key, std::wstring value)
{
    auto keyValue = wil::reg::try_get_value_dword(key, value.c_str());
    if (keyValue.has_value())
    {
        OutputNumericField(label, keyValue.value());
    }
    else
    {
        OutputStringField(label, std::wstring{ L"Not present" });
    }
}


namespace
{
    // What the registry section found, so the transport section can name a transport that is
    // registered and enabled but that the service did not report
    struct RegisteredTransport
    {
        std::wstring KeyName{};
        std::wstring ClassId{};
        bool Expected{ true };
    };

    std::vector<RegisteredTransport> g_registeredTransports{};

    // "{0F273B18-...}" and "0f273b18-..." are the same id
    std::wstring NormalizedGuidText(_In_ std::wstring_view const text)
    {
        std::wstring normalized{};

        for (auto const ch : text)
        {
            if (ch != L'{' && ch != L'}' && !::iswspace(ch))
            {
                normalized += static_cast<wchar_t>(::towlower(ch));
            }
        }

        return normalized;
    }

    std::wstring ExpandedPath(_In_ std::wstring const& path)
    {
        if (path.find(L'%') == std::wstring::npos)
        {
            return path;
        }

        auto const required = ::ExpandEnvironmentStringsW(path.c_str(), nullptr, 0);

        if (required == 0)
        {
            return path;
        }

        std::wstring expanded(required, L'\0');

        if (::ExpandEnvironmentStringsW(path.c_str(), expanded.data(), required) == 0)
        {
            return path;
        }

        expanded.resize(required - 1);

        return expanded;
    }

    // the DLL a class id loads, or empty when it is not registered
    std::wstring GetInprocServerPath(_In_ std::wstring const& classId)
    {
        auto const location = std::wstring{ L"CLSID\\" } + classId + L"\\InprocServer32";

        wil::unique_hkey key{};

        if (FAILED(wil::reg::open_unique_key_nothrow(HKEY_CLASSES_ROOT, location.c_str(), key, wil::reg::key_access::read)))
        {
            return {};
        }

        // the path is the "(default)" value, which an installer can write either way
        try
        {
            if (auto const path = wil::reg::try_get_value_string(key.get(), nullptr); path.has_value())
            {
                return ExpandedPath(path.value());
            }
        }
        catch (...)
        {
        }

        try
        {
            return wil::reg::try_get_value_expanded_string(key.get(), nullptr).value_or(std::wstring{});
        }
        catch (...)
        {
            return {};
        }
    }

    // adds version= and dll= for a class id, or dll="" when nothing is registered for it
    bool AddComponentFile(_Inout_ mididiag::KeyValueText& values, _In_ std::wstring const& classId)
    {
        auto const path = GetInprocServerPath(classId);

        if (path.empty())
        {
            values.Add(L"dll", L"");
            return false;
        }

        auto const version = FileVersionString(path);

        values.Add(L"version", version.empty() ? std::wstring{ L"unknown" } : version)
            .Add(L"dll", path);

        return true;
    }

    // The service runs as Local Service. A transport key that account cannot read keeps the
    // transport from loading, and nothing else says why. This checks the key's own permissions
    // for the groups the service is always in. When it cannot tell, it says yes.
    bool LocalServiceCanRead(_In_ HKEY const key)
    {
        PACL dacl{ nullptr };
        PSECURITY_DESCRIPTOR descriptor{ nullptr };

        if (::GetSecurityInfo(key, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION, nullptr, nullptr, &dacl, nullptr, &descriptor) != ERROR_SUCCESS)
        {
            return true;
        }

        wil::unique_hlocal_security_descriptor const freeDescriptor{ descriptor };

        // no DACL at all lets everyone in
        if (dacl == nullptr)
        {
            return true;
        }

        std::vector<std::vector<BYTE>> serviceSids{};

        for (auto const type : { WinWorldSid, WinAuthenticatedUserSid, WinBuiltinUsersSid, WinLocalServiceSid, WinServiceSid, WinLocalSid })
        {
            std::vector<BYTE> sid(SECURITY_MAX_SID_SIZE);
            DWORD size{ static_cast<DWORD>(sid.size()) };

            if (::CreateWellKnownSid(type, nullptr, sid.data(), &size))
            {
                serviceSids.push_back(std::move(sid));
            }
        }

        // the service's own SID, NT SERVICE\midisrv
        {
            std::vector<BYTE> sid(SECURITY_MAX_SID_SIZE);
            DWORD sidSize{ static_cast<DWORD>(sid.size()) };
            wchar_t domain[256]{};
            DWORD domainSize{ ARRAYSIZE(domain) };
            SID_NAME_USE use{};

            if (::LookupAccountNameW(nullptr, L"NT SERVICE\\midisrv", sid.data(), &sidSize, domain, &domainSize, &use))
            {
                serviceSids.push_back(std::move(sid));
            }
        }

        GENERIC_MAPPING mapping{ KEY_READ, KEY_WRITE, KEY_EXECUTE, KEY_ALL_ACCESS };
        ACCESS_MASK allowed{ 0 };
        ACCESS_MASK denied{ 0 };

        for (DWORD i = 0; i < dacl->AceCount; i++)
        {
            ACE_HEADER* header{ nullptr };

            // an inherit-only entry is for subkeys, not this key
            if (!::GetAce(dacl, i, reinterpret_cast<LPVOID*>(&header)) || header == nullptr ||
                (header->AceFlags & INHERIT_ONLY_ACE) != 0)
            {
                continue;
            }

            PSID aceSid{ nullptr };
            ACCESS_MASK mask{ 0 };

            if (header->AceType == ACCESS_ALLOWED_ACE_TYPE)
            {
                auto const ace = reinterpret_cast<ACCESS_ALLOWED_ACE*>(header);
                aceSid = &ace->SidStart;
                mask = ace->Mask;
            }
            else if (header->AceType == ACCESS_DENIED_ACE_TYPE)
            {
                auto const ace = reinterpret_cast<ACCESS_DENIED_ACE*>(header);
                aceSid = &ace->SidStart;
                mask = ace->Mask;
            }
            else
            {
                continue;
            }

            bool const applies = std::any_of(serviceSids.begin(), serviceSids.end(), [aceSid](std::vector<BYTE> const& sid)
                {
                    return ::EqualSid(aceSid, const_cast<BYTE*>(sid.data())) != FALSE;
                });

            if (!applies)
            {
                continue;
            }

            ::MapGenericMask(&mask, &mapping);

            if (header->AceType == ACCESS_ALLOWED_ACE_TYPE)
            {
                allowed |= mask;
            }
            else
            {
                denied |= mask;
            }
        }

        return ((allowed & ~denied) & KEY_QUERY_VALUE) == KEY_QUERY_VALUE;
    }
}

bool DoSectionDrivers32WOWRegistryEntries(_In_ bool const verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_ENUM_REGISTRY_DRIVERS32WOW);

    // list all MIDI values under Drivers32

    try
    {
        std::wstring drivers32KeyLocation = std::wstring{ L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows NT\\CurrentVersion\\Drivers32" };
        wil::unique_hkey drivers32Key{ };

        if (SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, drivers32KeyLocation.c_str(), drivers32Key, wil::reg::key_access::read)))
        {
            bool wdmaud2drvFound{ false };

            for (const auto& valueData : wil::make_range(wil::reg::value_iterator{ drivers32Key.get() }, wil::reg::value_iterator{}))
            {
                //valueData.name;
                //valueData.type;

                if (valueData.name.starts_with(L"midi") && valueData.name != L"midimapper")
                {
                    auto val = wil::reg::try_get_value_string(drivers32Key.get(), valueData.name.c_str());

                    if (val.has_value())
                    {
                        OutputStringField(MIDIDIAG_FIELD_LABEL_REGISTRY_DRIVERS32WOW_ENTRY, valueData.name + L" = " + val.value());

                        // this is added by something in the korg uninstall process. Possibly third-party, possibly korg.
                        // it's an invalid value that is not picked up by WinMM
                        if (valueData.name == L"midi0")
                        {
                            OutputError(internal::ResourceGetWString(IDS_ERROR_MIDI0_ENTRY_INVALID));

                            mididiag::AddFinding(L"drivers32_midi0",
                                mididiag::FormatResourceString(IDS_FINDING_DRIVERS32_MIDI0, L"HKLM\\" + drivers32KeyLocation));
                        }
                        else if (internal::ToLowerTrimmedWStringCopy(val.value()) == L"wdmaud2.drv")
                        {
                            wdmaud2drvFound = true;
                        }
                    }

                }
                else if (valueData.name == L"MidisrvTransferComplete")
                {
                    auto val = wil::reg::try_get_value_dword(drivers32Key.get(), valueData.name.c_str());

                    if (val.has_value())
                    {
                        OutputStringField(MIDIDIAG_FIELD_LABEL_REGISTRY_DRIVERS32WOW_ENTRY, valueData.name + L" = " + std::to_wstring(val.value()));
                    }
                }
            }

            if (!wdmaud2drvFound)
            {
                // Legacy API mode does not use wdmaud2.drv, so its absence changes nothing there
                if (!mididiag::Context().LegacyApiMode)
                {
                    mididiag::AddFinding(L"drivers32_no_wdmaud2",
                        mididiag::FormatResourceString(IDS_FINDING_NO_WDMAUD2, L"HKLM\\" + drivers32KeyLocation));
                }

                OutputError(internal::ResourceGetWString(IDS_ERROR_NO_WDMAUD2_ENTRY) + drivers32KeyLocation + L".");
                OutputError(internal::ResourceGetWString(IDS_ERROR_NO_WDMAUD2_ENTRY_TYPICAL));
                OutputError(internal::ResourceGetWString(IDS_ERROR_NO_WDMAUD2_ENTRY_REMEDY));
            }
        }
        else
        {
            OutputStringField(L"ERROR", drivers32KeyLocation);
            OutputError(internal::ResourceGetWString(IDS_ERROR_COULD_NOT_OPEN_DRIVERS32));
        }
    }
    catch (...)
    {
        OutputError(internal::ResourceGetWString(IDS_ERROR_EXCEPTION_ENUMERATING_REGISTRY));

        return false;
    }


    return true;

}



bool DoSectionDrivers32RegistryEntries(_In_ bool const verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_ENUM_REGISTRY_DRIVERS32);

    // list all MIDI values under Drivers32

    try
    {
        std::wstring drivers32KeyLocation = std::wstring{ L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Drivers32" };
        wil::unique_hkey drivers32Key{ };

        if (SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, drivers32KeyLocation.c_str(), drivers32Key, wil::reg::key_access::read)))
        {
            bool wdmaud2drvFound{ false };

            for (const auto& valueData : wil::make_range(wil::reg::value_iterator{ drivers32Key.get() }, wil::reg::value_iterator{}))
            {
                //valueData.name;
                //valueData.type;

                if (valueData.name.starts_with(L"midi") && valueData.name != L"midimapper")
                {
                    auto val = wil::reg::try_get_value_string(drivers32Key.get(), valueData.name.c_str());

                    if (val.has_value())
                    {
                        OutputStringField(MIDIDIAG_FIELD_LABEL_REGISTRY_DRIVERS32_ENTRY, valueData.name + L" = " + val.value());

                        // this is added by something in the korg uninstall process. Possibly third-party, possibly korg.
                        // it's an invalid value that is not picked up by WinMM
                        if (valueData.name == L"midi0")
                        {
                            OutputError(internal::ResourceGetWString(IDS_ERROR_MIDI0_ENTRY_INVALID));

                            mididiag::AddFinding(L"drivers32_midi0",
                                mididiag::FormatResourceString(IDS_FINDING_DRIVERS32_MIDI0, L"HKLM\\" + drivers32KeyLocation));
                        }
                        else if (internal::ToLowerTrimmedWStringCopy(val.value()) == L"wdmaud2.drv")
                        {
                            wdmaud2drvFound = true;
                        }
                    }

                }
                else if (valueData.name == L"MidisrvTransferComplete")
                {
                    auto val = wil::reg::try_get_value_dword(drivers32Key.get(), valueData.name.c_str());

                    if (val.has_value())
                    {
                        OutputStringField(MIDIDIAG_FIELD_LABEL_REGISTRY_DRIVERS32_ENTRY, valueData.name + L" = " + std::to_wstring(val.value()));
                    }
                }
                else if (valueData.name == L"UseLegacyMidi")
                {
                    auto val = wil::reg::try_get_value_dword(drivers32Key.get(), valueData.name.c_str());

                    if (val.has_value())
                    {
                        OutputStringField(MIDIDIAG_FIELD_LABEL_REGISTRY_DRIVERS32_ENTRY, valueData.name + L" = " + std::to_wstring(val.value()));
                    }
                }
            }

            if (!wdmaud2drvFound)
            {
                // Legacy API mode does not use wdmaud2.drv, so its absence changes nothing there
                if (!mididiag::Context().LegacyApiMode)
                {
                    mididiag::AddFinding(L"drivers32_no_wdmaud2",
                        mididiag::FormatResourceString(IDS_FINDING_NO_WDMAUD2, L"HKLM\\" + drivers32KeyLocation));
                }

                OutputError(internal::ResourceGetWString(IDS_ERROR_NO_WDMAUD2_ENTRY) + drivers32KeyLocation + L".");
                OutputError(internal::ResourceGetWString(IDS_ERROR_NO_WDMAUD2_ENTRY_TYPICAL));
                OutputError(internal::ResourceGetWString(IDS_ERROR_NO_WDMAUD2_ENTRY_REMEDY));
            }
        }
        else
        {
            OutputStringField(L"ERROR", drivers32KeyLocation);
            OutputError(internal::ResourceGetWString(IDS_ERROR_COULD_NOT_OPEN_DRIVERS32));
        }
    }
    catch (...)
    {
        OutputError(internal::ResourceGetWString(IDS_ERROR_EXCEPTION_ENUMERATING_REGISTRY));

        return false;
    }


    return true;
}



std::wstring GetDisplayValueFromNamingSelection(midi2enum::Midi1PortNamingApproach namingSelection)
{
    std::wstring namingSelectionDisplayString{};

    switch (namingSelection)
    {
    case midi2enum::Midi1PortNamingApproach::Default:
        namingSelectionDisplayString = L"Use global default from registry";
        break;
    case midi2enum::Midi1PortNamingApproach::UseClassicCompatible:
        namingSelectionDisplayString = L"Use legacy WinMM-compatible names";
        break;
    case midi2enum::Midi1PortNamingApproach::UseNewStyle:
        namingSelectionDisplayString = L"Use new-style names";
        break;
    case midi2enum::Midi1PortNamingApproach::UseAutomatic:
        namingSelectionDisplayString = L"Let Windows choose per device";
        break;
    default:
        namingSelectionDisplayString = L"INVALID VALUE";
        break;
    }

    return namingSelectionDisplayString;
}




bool DoSectionMidi2RegistryEntries(_In_ bool const verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_ENUM_REGISTRY);

    try
    {
        // check to see if the root is there

        wil::unique_hkey rootKey{};
        if (SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, MIDI_ROOT_REG_KEY, rootKey, wil::reg::key_access::read)))
        {
            // list all values in the root

            OutputRegStringValue(MIDIDIAG_FIELD_LABEL_REGISTRY_ROOT_CURRENT_CONFIG, rootKey.get(), MIDI_CONFIG_FILE_REG_VALUE);
            OutputRegDWordBooleanValue(MIDIDIAG_FIELD_LABEL_REGISTRY_ROOT_DISCOVERY_ENABLED, rootKey.get(), MIDI_DISCOVERY_ENABLED_REG_VALUE);
            OutputRegDWordNumericValue(MIDIDIAG_FIELD_LABEL_REGISTRY_ROOT_DISCOVERY_TIMEOUT, rootKey.get(), MIDI_DISCOVERY_TIMEOUT_REG_VALUE);
            OutputRegDWordBooleanValue(MIDIDIAG_FIELD_LABEL_REGISTRY_ROOT_USE_MMCSS, rootKey.get(), MIDI_USE_MMCSS_REG_VALUE);

            // the MIDI 1.0 port naming every endpoint uses unless it has its own setting
            try
            {
                if (auto const naming = wil::reg::try_get_value_dword(rootKey.get(), L"DefaultMidi1PortNaming"); naming.has_value())
                {
                    mididiag::WriteField(MIDIDIAG_FIELD_LABEL_REG_DEFAULT_MIDI1_NAME_TABLE_SELECTION, mididiag::KeyValueText{}
                        .AddNumber(L"value", naming.value())
                        .Add(L"meaning", naming.value() == 0 ?
                            std::wstring{ L"Use the built-in default" } :
                            GetDisplayValueFromNamingSelection(static_cast<midi2enum::Midi1PortNamingApproach>(naming.value()))));
                }
                else
                {
                    OutputStringField(MIDIDIAG_FIELD_LABEL_REG_DEFAULT_MIDI1_NAME_TABLE_SELECTION, std::wstring{ L"Not present" });
                }
            }
            catch (...)
            {
                OutputStringField(MIDIDIAG_FIELD_LABEL_REG_DEFAULT_MIDI1_NAME_TABLE_SELECTION, std::wstring{ L"INVALID VALUE" });
            }

            // Without it nothing can be saved, and until recently only MIDI Settings created it
            std::optional<std::wstring> configFile{};

            try
            {
                configFile = wil::reg::try_get_value_string(rootKey.get(), MIDI_CONFIG_FILE_REG_VALUE);
            }
            catch (...)
            {
            }

            if (!mididiag::Context().LegacyApiMode &&
                (!configFile.has_value() || internal::TrimmedWStringCopy(configFile.value()).empty()))
            {
                mididiag::AddFinding(L"config_not_registered", internal::ResourceGetWString(IDS_FINDING_CONFIG_NOT_REGISTERED));
            }

            OutputItemSeparator();
        }
        else
        {
            OutputError(internal::ResourceGetWString(IDS_ERROR_NO_ROOT_REGISTRY_KEY));
        }

        // List midisrv info

        wil::unique_hkey midisrvkey{ };
        if (SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\Midisrv", midisrvkey, wil::reg::key_access::read)))
        {
            if (midisrvkey.is_valid())
            {
                auto midisrvImagePath = wil::reg::try_get_value_string(midisrvkey.get(), L"ImagePath");

                if (midisrvImagePath.has_value())
                {
                    OutputStringField(MIDIDIAG_FIELD_LABEL_REGISTRY_MIDISRV_EXENAME, midisrvImagePath.value());
                }
                else
                {
                    OutputError(internal::ResourceGetWString(IDS_ERROR_MIDISRV_NO_IMAGE_PATH));
                }

            }
            else
            {
                OutputError(internal::ResourceGetWString(IDS_ERROR_MIDISRV_KEY_INVALID));
            }
        }
        else
        {
            OutputError(internal::ResourceGetWString(IDS_ERROR_NO_MIDISRV_SERVICES_KEY));
        }

        OutputItemSeparator();

        // Transports. The Midisrv transport loads in the app, not the service, so it is not
        // under Transport Plugins, and the diagnostics transport is built in.
        for (auto const& [name, classId] : std::initializer_list<std::pair<PCWSTR, PCWSTR>>{
            { L"(Midisrv Transport)", L"{2BA15E4E-5417-4A66-85B8-2B2260EFBC84}" },
            { L"(Diagnostics Transport)", L"{ac9b5417-3fe0-4e62-960f-034ee4235a1a}" } })
        {
            mididiag::KeyValueText values{};
            values.Add(L"key", name).Add(L"clsid", classId);

            bool const registered = AddComponentFile(values, classId);

            mididiag::WriteField(MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT, values);

            if (!registered)
            {
                OutputError(internal::ResourceGetWString(IDS_ERROR_NO_INPROC_SERVER));
            }
        }

        wil::unique_hkey transportPluginsKey{ };
        if (SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, MIDI_ROOT_TRANSPORT_PLUGINS_REG_KEY, transportPluginsKey)))
        {
            for (const auto& keyData : wil::make_range(wil::reg::key_iterator{ transportPluginsKey.get() }, wil::reg::key_iterator{}))
            {
                mididiag::KeyValueText values{};
                values.Add(L"key", keyData.name);

                wil::unique_hkey key{ };
                auto const openResult = wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE,
                    std::wstring(std::wstring(MIDI_ROOT_TRANSPORT_PLUGINS_REG_KEY) + L"\\" + keyData.name).c_str(), key, wil::reg::key_access::read);

                if (FAILED(openResult))
                {
                    values.AddBool(L"readable", false).Add(L"error", mididiag::FormatHResult(openResult));
                    mididiag::WriteField(MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT, values);

                    mididiag::AddFinding(L"transport_key_unreadable",
                        mididiag::FormatResourceString(IDS_FINDING_TRANSPORT_KEY_UNREADABLE, keyData.name));

                    continue;
                }

                // missing or of the wrong type reads as enabled, the way the service reads it
                bool enabled{ true };

                try
                {
                    if (auto const value = wil::reg::try_get_value_dword(key.get(), MIDI_PLUGIN_ENABLED_REG_VALUE); value.has_value())
                    {
                        enabled = value.value() != 0;
                    }
                }
                catch (...)
                {
                }

                std::wstring classId{};

                try
                {
                    classId = wil::reg::try_get_value_string(key.get(), MIDI_PLUGIN_CLSID_REG_VALUE).value_or(std::wstring{});
                }
                catch (...)
                {
                }

                values.AddBool(L"enabled", enabled).Add(L"clsid", classId);

                if (!classId.empty())
                {
                    AddComponentFile(values, classId);
                }

                bool const serviceCanRead = LocalServiceCanRead(key.get());

                if (!serviceCanRead)
                {
                    values.AddBool(L"service_can_read", false);

                    mididiag::AddFinding(L"transport_key_service_cant_read",
                        mididiag::FormatResourceString(IDS_FINDING_TRANSPORT_KEY_SERVICE_CANT_READ, keyData.name));
                }

                mididiag::WriteField(MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT, values);

                if (classId.empty())
                {
                    OutputError(internal::ResourceGetWString(IDS_ERROR_NO_TRANSPORT_CLSID));
                }

                // one the service cannot read already has its own finding
                g_registeredTransports.push_back({ keyData.name, classId, enabled && serviceCanRead });
            }
        }
        else
        {
            OutputError(internal::ResourceGetWString(IDS_ERROR_CANNOT_ENUMERATE_TRANSPORTS));
        }

    }
    catch (...)
    {
        OutputError(internal::ResourceGetWString(IDS_ERROR_EXCEPTION_ENUMERATING_REGISTRY));

        return false;
    }

    return true;
}


bool DoSectionTransports(_In_ bool const verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    try
    {
        OutputSectionHeader(MIDIDIAG_SECTION_LABEL_ENUM_TRANSPORTS);

        auto transports = rept::MidiReporting::GetInstalledTransportPlugins();

        if (transports == nullptr || transports.Size() == 0)
        {
            OutputError(internal::ResourceGetWString(IDS_ERROR_NO_TRANSPORTS_FOUND));
            return false;
        }

        std::vector<std::wstring> reportedIds{};

        for (auto const& transport : transports)
        {
            mididiag::WriteField(MIDIDIAG_FIELD_LABEL_TRANSPORT, mididiag::KeyValueText{}
                .Add(L"code", transport.TransportCode())
                .Add(L"id", internal::GuidToString(transport.TransportId()))
                .Add(L"version", transport.Version())
                .Add(L"author", transport.Author())
                .Add(L"name", transport.Name()));

            OutputTransportCapabilities(transport.TransportId(), transport.TransportCode());

            reportedIds.push_back(NormalizedGuidText(internal::GuidToString(transport.TransportId())));
        }

        // The service builds this list from the registry each time it is asked, so a transport
        // that is registered and enabled but missing here failed to load or to describe itself.
        for (auto const& registered : g_registeredTransports)
        {
            if (!registered.Expected || registered.ClassId.empty() ||
                std::find(reportedIds.begin(), reportedIds.end(), NormalizedGuidText(registered.ClassId)) != reportedIds.end())
            {
                continue;
            }

            mididiag::WriteField(MIDIDIAG_FIELD_LABEL_TRANSPORT_NOT_REPORTED, mididiag::KeyValueText{}
                .Add(L"key", registered.KeyName)
                .Add(L"clsid", registered.ClassId));

            mididiag::AddFinding(L"transport_not_reported",
                mididiag::FormatResourceString(IDS_FINDING_TRANSPORT_NOT_REPORTED, registered.KeyName));
        }
    }
    catch (...)
    {
        OutputError(internal::ResourceGetWString(IDS_ERROR_EXCEPTION_ENUMERATING_TRANSPORTS));
        return false;
    }

    return true;
}


namespace
{
    std::wstring LowerId(_In_ std::wstring_view const id)
    {
        std::wstring lower{ id };

        for (auto& ch : lower)
        {
            ch = static_cast<wchar_t>(::towlower(ch));
        }

        return lower;
    }

    uint8_t GroupIndex(_In_ midi2::MidiGroup const& group)
    {
        return group == nullptr ? 0 : group.Index();
    }

    uint8_t GroupNumber(_In_ midi2::MidiGroup const& group)
    {
        return group == nullptr ? 0 : group.DisplayValue();
    }

    PCWSTR DirectionName(_In_ midi2enum::MidiGroupTerminalBlockDirection const direction)
    {
        switch (direction)
        {
        case midi2enum::MidiGroupTerminalBlockDirection::BlockInput:    return L"destination";
        case midi2enum::MidiGroupTerminalBlockDirection::BlockOutput:   return L"source";
        default:                                                        return L"bidirectional";
        }
    }

    PCWSTR DirectionName(_In_ midi2enum::MidiFunctionBlockDirection const direction)
    {
        switch (direction)
        {
        case midi2enum::MidiFunctionBlockDirection::BlockInput:     return L"destination";
        case midi2enum::MidiFunctionBlockDirection::BlockOutput:    return L"source";
        case midi2enum::MidiFunctionBlockDirection::Bidirectional:  return L"bidirectional";
        default:                                                    return L"undefined";
        }
    }

    PCWSTR Midi10ConnectionName(_In_ midi2enum::MidiFunctionBlockRepresentsMidi10Connection const connection)
    {
        switch (connection)
        {
        case midi2enum::MidiFunctionBlockRepresentsMidi10Connection::Not10:                     return L"no";
        case midi2enum::MidiFunctionBlockRepresentsMidi10Connection::YesBandwidthUnrestricted:  return L"unrestricted";
        case midi2enum::MidiFunctionBlockRepresentsMidi10Connection::YesBandwidthRestricted:    return L"restricted";
        default:                                                                                return L"reserved";
        }
    }

    PCWSTR Midi1PortFlowName(_In_ midi2enum::Midi1PortFlow const flow)
    {
        return flow == midi2enum::Midi1PortFlow::MidiMessageSource ? L"in" : L"out";
    }

    PCWSTR ProtocolName(_In_ midi2enum::MidiProtocol const protocol)
    {
        switch (protocol)
        {
        case midi2enum::MidiProtocol::Midi1:    return L"midi1";
        case midi2enum::MidiProtocol::Midi2:    return L"midi2";
        default:                                return L"default";
        }
    }

    PCWSTR PurposeName(_In_ midi2enum::MidiEndpointDevicePurpose const purpose)
    {
        switch (purpose)
        {
        case midi2enum::MidiEndpointDevicePurpose::VirtualDeviceResponder:  return L"virtual_device_responder";
        case midi2enum::MidiEndpointDevicePurpose::InBoxGeneralMidiSynth:   return L"general_midi_synth";
        case midi2enum::MidiEndpointDevicePurpose::DiagnosticLoopback:      return L"diagnostic_loopback";
        case midi2enum::MidiEndpointDevicePurpose::DiagnosticPing:          return L"diagnostic_ping";
        default:                                                            return L"normal";
        }
    }

    // 00-21-09
    std::wstring HexBytes(_In_ winrt::com_array<uint8_t> const& bytes)
    {
        std::wstring text{};

        for (auto const byte : bytes)
        {
            text += text.empty() ? L"" : L"-";
            text += std::format(L"{:02X}", byte);
        }

        return text;
    }
}

bool DoSectionMidi2ApiEndpoints(_In_ bool const verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_MIDI2_API_ENDPOINTS);

    // list devices

    collections::IVectorView<midi2enum::MidiEndpointDeviceInformation> devices{ nullptr };

    try
    {
        // list all devices
        devices = midi2enum::MidiEndpointDeviceInformation::FindAll(
            midi2enum::MidiEndpointDeviceInformationSortOrder::Name,
            midi2enum::MidiEndpointDeviceInformationFilters::StandardNativeMidi1ByteFormat |
            midi2enum::MidiEndpointDeviceInformationFilters::StandardNativeUniversalMidiPacketFormat |
            midi2enum::MidiEndpointDeviceInformationFilters::DiagnosticLoopback |
            midi2enum::MidiEndpointDeviceInformationFilters::DiagnosticPing |
            midi2enum::MidiEndpointDeviceInformationFilters::VirtualDeviceResponder
        );
    }
    catch (...)
    {
        OutputError(internal::ResourceGetWString(IDS_ERROR_CANNOT_ENUMERATE_DEVICES));
        return false;
    }

    // every MIDI 1.0 port at once, grouped by the endpoint it belongs to
    std::map<std::wstring, std::vector<legacy::MidiLegacyPortDeviceInformation>> portsByEndpoint{};

    try
    {
        for (auto const& port : legacy::MidiLegacyPortDeviceInformation::FindAll())
        {
            portsByEndpoint[LowerId(port.AssociatedEndpointDeviceId())].push_back(port);
        }
    }
    catch (...)
    {
        // the endpoints are still worth listing without their ports
    }

    auto& context = mididiag::Context();

    if (devices != nullptr && devices.Size() > 0)
    {
        for (uint32_t i = 0; i < devices.Size(); i++)
        {
            // Separator goes first so the diagnostic endpoints skipped below still get one
            if (i > 0)
            {
                OutputItemSeparator();
            }

            auto device = devices.GetAt(i);

            auto transportInfo = device.GetTransportSuppliedInfo();
            auto userInfo = device.GetUserSuppliedInfo();
            auto endpointInfo = device.GetDeclaredEndpointInfo();
            auto const endpointKey = LowerId(device.EndpointDeviceId());

            // the sessions section uses these to name what each app has open
            context.EndpointNames[endpointKey] = std::wstring{ device.Name() };

            // These names should not be localized because customers may parse these output fields

            OutputEntityIdentifierField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_ID, device.EndpointDeviceId());
            OutputEntityNameField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_NAME, device.Name());
            OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_TRANSPORT_CODE, transportInfo.TransportCode());

            if (device.EndpointPurpose() != midi2enum::MidiEndpointDevicePurpose::NormalMessageEndpoint)
            {
                OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PURPOSE, std::wstring{ PurposeName(device.EndpointPurpose()) });
            }

            OutputStringFieldIfNotEmpty(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_USER_SUPPLIED_NAME, userInfo.Name());
            OutputStringFieldIfNotEmpty(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_ENDPOINT_SUPPLIED_NAME, endpointInfo.Name());
            OutputStringFieldIfNotEmpty(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_TRANSPORT_SUPPLIED_NAME, transportInfo.Name());
            OutputStringFieldIfNotEmpty(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_USER_SUPPLIED_DESC, userInfo.Description());
            OutputStringFieldIfNotEmpty(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_TRANSPORT_SUPPLIED_DESC, transportInfo.Description());
            OutputStringFieldIfNotEmpty(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_MANUFACTURER, transportInfo.ManufacturerName());

            auto const nativeFormat = transportInfo.NativeDataFormat();
            bool const isUmpNative = nativeFormat == midi2enum::MidiEndpointNativeDataFormat::UniversalMidiPacketFormat;

            OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_NATIVE_DATA_FORMAT, std::wstring{ isUmpNative ? L"ump" :
                nativeFormat == midi2enum::MidiEndpointNativeDataFormat::Midi1ByteFormat ? L"midi1_bytestream" : L"unknown" });

            OutputBooleanField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_MULTI_CLIENT, transportInfo.SupportsMultiClient());
            OutputBooleanField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_MUTED, device.IsMuted());
            OutputBooleanField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_DISCOVERY_COMPLETE, device.IsEndpointDiscoveryComplete());

            if (device.ContainerId() != winrt::guid{})
            {
                OutputGuidField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_CONTAINER_ID, device.ContainerId());
            }

            OutputStringFieldIfNotEmpty(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_DRIVER_DEVICE_INTERFACE, transportInfo.DriverDeviceInterfaceId());

            if (device.EndpointPurpose() == midi2enum::MidiEndpointDevicePurpose::DiagnosticLoopback ||
                device.EndpointPurpose() == midi2enum::MidiEndpointDevicePurpose::DiagnosticPing)
            {
                // skip diagnostic endpoints
                continue;
            }

            // what a UMP endpoint said about itself when it was discovered
            if (isUmpNative)
            {
                mididiag::WriteField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_DECLARED_ENDPOINT, mididiag::KeyValueText{}
                    .Add(L"ump_version", std::format(L"{}.{}", endpointInfo.SpecificationVersionMajor(), endpointInfo.SpecificationVersionMinor()))
                    .AddBool(L"midi1", endpointInfo.SupportsMidi10Protocol())
                    .AddBool(L"midi2", endpointInfo.SupportsMidi20Protocol())
                    .AddBool(L"jr_receive", endpointInfo.SupportsReceivingJitterReductionTimestamps())
                    .AddBool(L"jr_send", endpointInfo.SupportsSendingJitterReductionTimestamps())
                    .AddBool(L"static_function_blocks", endpointInfo.HasStaticFunctionBlocks())
                    .AddNumber(L"function_blocks", endpointInfo.DeclaredFunctionBlockCount())
                    .AddIfNotEmpty(L"product_instance_id", endpointInfo.ProductInstanceId()));

                if (auto const identity = device.GetDeclaredDeviceIdentity(); identity != nullptr)
                {
                    auto const sysExId = identity.SystemExclusiveId();
                    auto const revision = identity.SoftwareRevisionLevel();

                    bool const anyIdentity =
                        std::any_of(sysExId.begin(), sysExId.end(), [](uint8_t const b) { return b != 0; }) ||
                        identity.DeviceFamilyLsb() != 0 || identity.DeviceFamilyMsb() != 0 ||
                        identity.DeviceFamilyModelNumberLsb() != 0 || identity.DeviceFamilyModelNumberMsb() != 0;

                    // bytes in the order the device sends them, least significant first
                    if (anyIdentity)
                    {
                        mididiag::WriteField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_DECLARED_DEVICE_IDENTITY, mididiag::KeyValueText{}
                            .Add(L"sysex_id", HexBytes(sysExId))
                            .Add(L"family", std::format(L"{:02X}-{:02X}", identity.DeviceFamilyLsb(), identity.DeviceFamilyMsb()))
                            .Add(L"model", std::format(L"{:02X}-{:02X}", identity.DeviceFamilyModelNumberLsb(), identity.DeviceFamilyModelNumberMsb()))
                            .Add(L"revision", HexBytes(revision)));
                    }
                }

                if (auto const stream = device.GetDeclaredStreamConfiguration(); stream != nullptr)
                {
                    mididiag::WriteField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_STREAM_CONFIGURATION, mididiag::KeyValueText{}
                        .Add(L"protocol", ProtocolName(stream.Protocol()))
                        .AddBool(L"jr_receive", stream.ReceiveJitterReductionTimestamps())
                        .AddBool(L"jr_send", stream.SendJitterReductionTimestamps()));
                }
            }

            // the customer's own settings that change what is sent to the device
            if (userInfo.RequiresNoteOffTranslation())
            {
                OutputBooleanField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_NOTE_OFF_TRANSLATION, true);
            }

            if (userInfo.SupportsMidiPolyphonicExpression())
            {
                OutputBooleanField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_SUPPORTS_MPE, true);
            }

            if (userInfo.RecommendedControlChangeAutomationIntervalMilliseconds() != 0)
            {
                OutputNumericField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_RECOMMENDED_CC_INTERVAL, userInfo.RecommendedControlChangeAutomationIntervalMilliseconds());
            }

            if (userInfo.UseCustomMidiOutgoingLatencyTicksForScheduling() ||
                userInfo.CustomMidiOutgoingLatencyTicks() != 0 ||
                userInfo.CalculatedMidiOutgoingLatencyTicks() != 0)
            {
                mididiag::WriteField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_OUTGOING_LATENCY, mididiag::KeyValueText{}
                    .AddBool(L"use_custom", userInfo.UseCustomMidiOutgoingLatencyTicksForScheduling())
                    .AddSignedNumber(L"custom_ticks", userInfo.CustomMidiOutgoingLatencyTicks())
                    .AddSignedNumber(L"calculated_ticks", userInfo.CalculatedMidiOutgoingLatencyTicks()));
            }

            // blocks decide which MIDI 1.0 ports exist and what they are called

            for (auto const& gtb : device.GetGroupTerminalBlocks())
            {
                mididiag::WriteField(MIDIDIAG_FIELD_LABEL_GTB, mididiag::KeyValueText{}
                    .AddNumber(L"number", gtb.Number())
                    .Add(L"direction", DirectionName(gtb.Direction()))
                    .AddNumber(L"first_group", GroupNumber(gtb.FirstGroup()))
                    .AddNumber(L"groups", gtb.GroupCount())
                    .Add(L"name", gtb.Name()));
            }

            for (auto const& fb : device.GetDeclaredFunctionBlocks())
            {
                mididiag::WriteField(MIDIDIAG_FIELD_LABEL_FUNCTION_BLOCK, mididiag::KeyValueText{}
                    .AddNumber(L"number", fb.Number())
                    .AddBool(L"active", fb.IsActive())
                    .Add(L"direction", DirectionName(fb.Direction()))
                    .AddNumber(L"first_group", GroupNumber(fb.FirstGroup()))
                    .AddNumber(L"groups", fb.GroupCount())
                    .Add(L"midi1", Midi10ConnectionName(fb.RepresentsMidi10Connection()))
                    .Add(L"name", fb.Name()));
            }

            // MIDI 1.0 ports, outputs first, with the other names each one could have had
            OutputStringField(MIDIDIAG_FIELD_LABEL_NAME_TABLE_SELECTION, GetDisplayValueFromNamingSelection(device.Midi1PortNamingApproach()));

            auto const nameEntries = device.GetNameTable();
            uint32_t const nameEntryCount = nameEntries == nullptr ? 0 : nameEntries.Size();
            std::vector<bool> nameEntryShown(nameEntryCount, false);

            std::vector<legacy::MidiLegacyPortDeviceInformation> ports{};

            if (auto const found = portsByEndpoint.find(endpointKey); found != portsByEndpoint.end())
            {
                ports = found->second;
            }

            std::sort(ports.begin(), ports.end(), [](legacy::MidiLegacyPortDeviceInformation const& a, legacy::MidiLegacyPortDeviceInformation const& b)
                {
                    bool const aIsInput = a.Flow() == midi2enum::Midi1PortFlow::MidiMessageSource;
                    bool const bIsInput = b.Flow() == midi2enum::Midi1PortFlow::MidiMessageSource;

                    if (aIsInput != bIsInput)
                    {
                        return !aIsInput;
                    }

                    return GroupIndex(a.Group()) < GroupIndex(b.Group());
                });

            for (auto const& port : ports)
            {
                auto const flow = Midi1PortFlowName(port.Flow());
                auto const portName = std::wstring{ port.Name() };

                mididiag::KeyValueText values{};
                values.Add(L"flow", flow)
                    .AddNumber(L"number", port.Number())
                    .AddNumber(L"group", GroupNumber(port.Group()));

                for (uint32_t n = 0; n < nameEntryCount; n++)
                {
                    auto const entry = nameEntries.GetAt(n);

                    if (entry.Flow() != port.Flow() || GroupIndex(entry.Group()) != GroupIndex(port.Group()))
                    {
                        continue;
                    }

                    nameEntryShown[n] = true;

                    // only the names that differ from the one in use
                    auto const legacyName = std::wstring{ entry.LegacyCompatibleName() };
                    auto const newStyleName = std::wstring{ entry.NewStyleName() };

                    values.AddIfNotEmpty(L"custom_name", entry.CustomName());

                    if (legacyName != portName)
                    {
                        values.Add(L"legacy_name", legacyName);
                    }

                    if (newStyleName != portName)
                    {
                        values.Add(L"new_style_name", newStyleName);
                    }

                    break;
                }

                values.Add(L"name", portName);

                mididiag::WriteField(MIDIDIAG_FIELD_LABEL_MIDI1_PORT, values);

                context.Midi1Ports[LowerId(port.PortDeviceId())] = mididiag::Midi1PortSummary{ flow, port.Number(), portName };
            }

            if (nameEntries != nullptr && nameEntryCount == 0 && !ports.empty())
            {
                OutputError(internal::ResourceGetWString(IDS_ERROR_NO_NAMING_TABLE));
            }

            // rows with no port behind them still show what Windows would call one
            for (uint32_t n = 0; n < nameEntryCount; n++)
            {
                if (nameEntryShown[n])
                {
                    continue;
                }

                auto const entry = nameEntries.GetAt(n);

                mididiag::WriteField(MIDIDIAG_FIELD_LABEL_NAME_TABLE_ENTRY, mididiag::KeyValueText{}
                    .Add(L"flow", Midi1PortFlowName(entry.Flow()))
                    .AddNumber(L"group", GroupNumber(entry.Group()))
                    .AddIfNotEmpty(L"custom_name", entry.CustomName())
                    .Add(L"legacy_name", entry.LegacyCompatibleName())
                    .Add(L"new_style_name", entry.NewStyleName()));
            }

            // Parent device

            auto parent = device.GetParentDeviceInformation();

            if (parent != nullptr)
            {
                OutputEntityIdentifierField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_ID, parent.Id());
                OutputEntityNameField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_NAME, parent.Name());

                // The SDK leaves these at zero when the parent is not a USB device
                if (parent.UsbVendorId() != 0 || parent.UsbProductId() != 0)
                {
                    OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_USB_VID, std::format(L"0x{:04X}", parent.UsbVendorId()));
                    OutputStringField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_USB_PID, std::format(L"0x{:04X}", parent.UsbProductId()));
                    OutputStringFieldIfNotEmpty(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_USB_SERIAL, parent.UsbSerialNumber());
                }

                // Driver properties come from the &MI_xx media interface when there is one, not from the composite parent
                auto driverDeviceId = parent.RelatedParentMediaDriverDeviceInstanceId();

                if (driverDeviceId.empty())
                {
                    driverDeviceId = parent.Id();
                }

                auto const details = GetParentDeviceDetails(std::wstring{ parent.Id() }, std::wstring{ driverDeviceId });

                // the hubs between the device and the PC, where a lot of connection trouble starts
                if (!details.UsbLocationPath.empty())
                {
                    mididiag::WriteField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_USB_LOCATION, mididiag::KeyValueText{}
                        .AddNumber(L"hubs", details.UsbHubCount)
                        .Add(L"path", details.UsbLocationPath));
                }

                OutputStringFieldIfNotEmpty(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_LAST_ARRIVAL, details.LastArrival);
                OutputStringFieldIfNotEmpty(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_LAST_REMOVAL, details.LastRemoval);

                if (details.ProblemCode != 0)
                {
                    OutputNumericField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_PROBLEM, details.ProblemCode);
                }

                OutputEntityIdentifierField(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_DRIVER_DEVICE_ID, driverDeviceId);
                OutputStringFieldIfNotEmpty(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_ENUMERATOR_NAME, parent.EnumeratorName());
                OutputStringFieldIfNotEmpty(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_SERVICE_NAME, parent.ServiceName());
                OutputStringFieldIfNotEmpty(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_DRIVER_INF_PATH, parent.DriverInfPath());
                OutputStringFieldIfNotEmpty(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_DRIVER_PROVIDER, parent.DriverProvider());
                OutputStringFieldIfNotEmpty(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_DRIVER_VERSION, parent.DriverVersion());
                OutputStringFieldIfNotEmpty(MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_DRIVER_DATE, details.DriverDate);
            }
            else
            {
                OutputError(internal::ResourceGetWString(IDS_ERROR_NO_ENDPOINT_PARENT));
            }
        }
    }
    else
    {
        OutputError(internal::ResourceGetWString(IDS_ERROR_NO_DEVICES_FOUND_1));
        OutputError(internal::ResourceGetWString(IDS_ERROR_NO_DEVICES_FOUND_2));
        OutputError(internal::ResourceGetWString(IDS_ERROR_NO_DEVICES_FOUND_3));
        return false;
    }

    return true;
}

bool DoSectionSessions(_In_ bool const verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_SESSIONS);

    try
    {
        auto const& context = mididiag::Context();
        auto const servicePid = static_cast<uint64_t>(MidiServiceProcessId());
        auto const thisPid = static_cast<uint64_t>(::GetCurrentProcessId());

        auto sessions = rept::MidiReporting::GetActiveSessions();
        uint32_t const sessionCount = sessions == nullptr ? 0 : sessions.Size();

        OutputNumericField(MIDIDIAG_FIELD_LABEL_SESSION_COUNT, sessionCount);

        for (uint32_t i = 0; i < sessionCount; i++)
        {
            auto session = sessions.GetAt(i);

            OutputItemSeparator();

            OutputEntityNameField(MIDIDIAG_FIELD_LABEL_SESSION_NAME, session.SessionName());
            OutputStringField(MIDIDIAG_FIELD_LABEL_SESSION_PROCESS_NAME, session.ProcessName());
            OutputStringField(MIDIDIAG_FIELD_LABEL_SESSION_PROCESS_ID, std::to_wstring(session.ProcessId()));

            // the service's own sessions and this report's are not apps the customer is running
            if (servicePid != 0 && session.ProcessId() == servicePid)
            {
                OutputStringField(MIDIDIAG_FIELD_LABEL_SESSION_OWNER, std::wstring{ L"service" });
            }
            else if (session.ProcessId() == thisPid)
            {
                OutputStringField(MIDIDIAG_FIELD_LABEL_SESSION_OWNER, std::wstring{ L"this_report" });
            }

            OutputDateTimeField(MIDIDIAG_FIELD_LABEL_SESSION_START_TIME, session.StartTime());

            auto connections = session.Connections();
            uint32_t const connectionCount = connections == nullptr ? 0 : connections.Size();

            OutputNumericField(MIDIDIAG_FIELD_LABEL_SESSION_CONNECTION_COUNT, connectionCount);

            for (uint32_t j = 0; j < connectionCount; j++)
            {
                auto connection = connections.GetAt(j);
                auto const deviceId = std::wstring{ connection.EndpointOrPortDeviceId() };
                auto const key = LowerId(deviceId);

                mididiag::KeyValueText values{};
                values.AddNumber(L"instances", connection.InstanceCount())
                    .AddIfNotEmpty(L"since", mididiag::FormatLocalTime(connection.EarliestConnectionTime()));

                // what the id is, from the endpoint section
                if (auto const endpoint = context.EndpointNames.find(key); endpoint != context.EndpointNames.end())
                {
                    values.Add(L"endpoint", endpoint->second);
                }
                else if (auto const port = context.Midi1Ports.find(key); port != context.Midi1Ports.end())
                {
                    values.Add(L"port_flow", port->second.Flow)
                        .AddNumber(L"port_number", port->second.Number)
                        .Add(L"port_name", port->second.Name);
                }

                values.Add(L"id", deviceId);

                mididiag::WriteField(MIDIDIAG_FIELD_LABEL_SESSION_CONNECTION, values);
            }
        }
    }
    catch (...)
    {
        OutputError(internal::ResourceGetWString(IDS_ERROR_EXCEPTION_ENUMERATING_SESSIONS));

        return false;
    }

    return true;
}

bool DoSectionWinRTMidi1ApiEndpoints(_In_ bool const verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    bool succeeded{ true };

    for (bool const inputs : { true, false })
    {
        OutputSectionHeader(inputs ? MIDIDIAG_SECTION_LABEL_MIDI1_API_INPUT_ENDPOINTS : MIDIDIAG_SECTION_LABEL_MIDI1_API_OUTPUT_ENDPOINTS);

        try
        {
            auto const selector = inputs ?
                winrt::Windows::Devices::Midi::MidiInPort::GetDeviceSelector() :
                winrt::Windows::Devices::Midi::MidiOutPort::GetDeviceSelector();

            for (auto const& device : winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(selector).get())
            {
                mididiag::WriteField(MIDIDIAG_FIELD_LABEL_WINRT_MIDI1_PORT, mididiag::KeyValueText{}
                    .Add(L"name", device.Name())
                    .Add(L"id", device.Id()));
            }
        }
        catch (...)
        {
            OutputError(internal::ResourceGetWString(IDS_ERROR_EXCEPTION_ENUMERATING_WINRT_MIDI1));
            succeeded = false;
        }
    }

    return succeeded;
}

void DisplayWinMMGetDevCapsErrorResult(MMRESULT result)
{
    switch (result)
    {
    case MMSYSERR_NOERROR:
        // don't display anything
        break;

    case MMSYSERR_NODRIVER:
        OutputError(internal::ResourceGetWString(IDS_ERROR_PORT_NODRIVER));
        break;

    case MMSYSERR_INVALPARAM:
        OutputError(internal::ResourceGetWString(IDS_ERROR_PORT_INVALPARAM));
        break;

    case MMSYSERR_BADDEVICEID:
        OutputError(internal::ResourceGetWString(IDS_ERROR_PORT_BADDEVICEID));
        break;

    case MMSYSERR_NOMEM:
        OutputError(internal::ResourceGetWString(IDS_ERROR_PORT_NOMEM));
        break;

    }
}

namespace
{
    // What Windows MIDI Services says a MIDI 1.0 port is, to compare with what WinMM says
    struct ExpectedWinMMPort
    {
        std::wstring NameLower{};
        std::wstring InterfaceLower{};
        uint32_t Number{ 0 };
    };

    constexpr ULONG MaxDeviceInterfaceBytes{ 4096 };

    std::vector<ExpectedWinMMPort> GetExpectedWinMMPorts(_In_ midi2enum::Midi1PortFlow const flow)
    {
        std::vector<ExpectedWinMMPort> ports{};

        // the service is off in Legacy API mode, and WinMM uses other drivers
        if (mididiag::Context().LegacyApiMode)
        {
            return ports;
        }

        try
        {
            for (auto const& port : legacy::MidiLegacyPortDeviceInformation::FindAll(flow))
            {
                // WinMM keeps only the first MAXPNAMELEN - 1 characters of a name
                auto const name = std::wstring{ port.Name() }.substr(0, MAXPNAMELEN - 1);

                ports.push_back({ LowerId(name), LowerId(port.DriverDeviceInterfaceId()), port.Number() });
            }
        }
        catch (...)
        {
        }

        return ports;
    }

    // DRV_QUERYDEVICEINTERFACE: the device interface the driver says is behind a WinMM port
    std::wstring InputDeviceInterface(_In_ uint32_t const index)
    {
        auto const handle = reinterpret_cast<HMIDIIN>(static_cast<UINT_PTR>(index));
        ULONG size{ 0 };

        if (::midiInMessage(handle, DRV_QUERYDEVICEINTERFACESIZE, reinterpret_cast<DWORD_PTR>(&size), 0) != MMSYSERR_NOERROR ||
            size < sizeof(wchar_t) || size > MaxDeviceInterfaceBytes)
        {
            return {};
        }

        std::vector<wchar_t> buffer(size / sizeof(wchar_t) + 1, L'\0');

        if (::midiInMessage(handle, DRV_QUERYDEVICEINTERFACE, reinterpret_cast<DWORD_PTR>(buffer.data()), size) != MMSYSERR_NOERROR)
        {
            return {};
        }

        return std::wstring{ buffer.data() };
    }

    std::wstring OutputDeviceInterface(_In_ uint32_t const index)
    {
        auto const handle = reinterpret_cast<HMIDIOUT>(static_cast<UINT_PTR>(index));
        ULONG size{ 0 };

        if (::midiOutMessage(handle, DRV_QUERYDEVICEINTERFACESIZE, reinterpret_cast<DWORD_PTR>(&size), 0) != MMSYSERR_NOERROR ||
            size < sizeof(wchar_t) || size > MaxDeviceInterfaceBytes)
        {
            return {};
        }

        std::vector<wchar_t> buffer(size / sizeof(wchar_t) + 1, L'\0');

        if (::midiOutMessage(handle, DRV_QUERYDEVICEINTERFACE, reinterpret_cast<DWORD_PTR>(buffer.data()), size) != MMSYSERR_NOERROR)
        {
            return {};
        }

        return std::wstring{ buffer.data() };
    }

    PCWSTR OutputTechnologyName(_In_ WORD const technology)
    {
        switch (technology)
        {
        case MOD_MIDIPORT:  return L"port";
        case MOD_SYNTH:     return L"synth";
        case MOD_SQSYNTH:   return L"square_wave_synth";
        case MOD_FMSYNTH:   return L"fm_synth";
        case MOD_MAPPER:    return L"mapper";
        case MOD_WAVETABLE: return L"wavetable";
        case MOD_SWSYNTH:   return L"software_synth";
        default:            return L"unknown";
        }
    }

    // major.minor, from the low word
    std::wstring DriverVersionText(_In_ MMVERSION const version)
    {
        return std::format(L"{}.{}", HIBYTE(LOWORD(version)), LOBYTE(LOWORD(version)));
    }
}

bool DoSectionWinMMMidi1ApiEndpoints(_In_ bool const verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    bool succeeded{ true };

    for (bool const inputs : { true, false })
    {
        OutputSectionHeader(inputs ? MIDIDIAG_SECTION_LABEL_WINMM_API_INPUT_ENDPOINTS : MIDIDIAG_SECTION_LABEL_WINMM_API_OUTPUT_ENDPOINTS);

        try
        {
            auto const expected = GetExpectedWinMMPorts(inputs ?
                midi2enum::Midi1PortFlow::MidiMessageSource :
                midi2enum::Midi1PortFlow::MidiMessageDestination);

            uint32_t errorCount{ 0 };
            std::map<int64_t, uint32_t> offsetCounts{};
            std::map<std::wstring, uint32_t> nameCounts{};

            uint32_t const deviceCount = inputs ? ::midiInGetNumDevs() : ::midiOutGetNumDevs();

            OutputNumericField(MIDIDIAG_FIELD_LABEL_WINMM_ENDPOINT_COUNT, deviceCount);

            for (uint32_t i = 0; i < deviceCount; i++)
            {
                mididiag::KeyValueText values{};
                values.AddNumber(L"index", i);

                std::wstring name{};
                std::wstring deviceInterface{};
                MMRESULT result{ MMSYSERR_NOERROR };

                if (inputs)
                {
                    MIDIINCAPSW caps{};
                    result = ::midiInGetDevCapsW(i, &caps, sizeof(caps));

                    if (result == MMSYSERR_NOERROR)
                    {
                        name = caps.szPname;
                        values.AddNumber(L"mid", caps.wMid)
                            .AddNumber(L"pid", caps.wPid)
                            .Add(L"version", DriverVersionText(caps.vDriverVersion));
                    }

                    deviceInterface = InputDeviceInterface(i);
                }
                else
                {
                    MIDIOUTCAPSW caps{};
                    result = ::midiOutGetDevCapsW(i, &caps, sizeof(caps));

                    if (result == MMSYSERR_NOERROR)
                    {
                        name = caps.szPname;
                        values.Add(L"technology", OutputTechnologyName(caps.wTechnology))
                            .AddNumber(L"mid", caps.wMid)
                            .AddNumber(L"pid", caps.wPid)
                            .Add(L"version", DriverVersionText(caps.vDriverVersion));
                    }

                    deviceInterface = OutputDeviceInterface(i);
                }

                bool matched{ false };

                if (result != MMSYSERR_NOERROR)
                {
                    values.AddNumber(L"error", result);
                    errorCount++;
                }
                else
                {
                    auto const nameLower = LowerId(name);
                    nameCounts[nameLower]++;

                    // the port Windows MIDI Services expects here: same name, and same device interface when names repeat
                    std::vector<ExpectedWinMMPort const*> candidates{};

                    for (auto const& port : expected)
                    {
                        if (port.NameLower == nameLower)
                        {
                            candidates.push_back(&port);
                        }
                    }

                    if (candidates.size() > 1 && !deviceInterface.empty())
                    {
                        auto const interfaceLower = LowerId(deviceInterface);

                        std::erase_if(candidates, [&interfaceLower](ExpectedWinMMPort const* port) { return port->InterfaceLower != interfaceLower; });
                    }

                    if (candidates.size() == 1)
                    {
                        matched = true;
                        values.AddNumber(L"sdk_number", candidates.front()->Number);
                        offsetCounts[static_cast<int64_t>(i) - static_cast<int64_t>(candidates.front()->Number)]++;
                    }
                }

                // only when it helps explain a port Windows MIDI Services could not match
                if (!matched)
                {
                    values.AddIfNotEmpty(L"interface", deviceInterface);
                }

                values.Add(L"name", name);

                mididiag::WriteField(MIDIDIAG_FIELD_LABEL_WINMM_PORT, values);

                DisplayWinMMGetDevCapsErrorResult(result);
            }

            OutputNumericField(MIDIDIAG_FIELD_LABEL_WINMM_ERROR_COUNT, errorCount);

            // the shift most ports share is the one that matters
            if (!offsetCounts.empty())
            {
                auto const common = std::max_element(offsetCounts.begin(), offsetCounts.end(),
                    [](auto const& a, auto const& b) { return a.second < b.second; });

                if (common->first != 0)
                {
                    mididiag::AddFinding(inputs ? L"winmm_input_offset" : L"winmm_output_offset",
                        mididiag::FormatResourceString(inputs ? IDS_FINDING_WINMM_INPUT_OFFSET : IDS_FINDING_WINMM_OUTPUT_OFFSET, common->first));
                }
            }

            uint32_t duplicateCount{ 0 };

            for (auto const& [nameLower, count] : nameCounts)
            {
                if (count > 1)
                {
                    duplicateCount += count;
                }
            }

            if (duplicateCount > 0)
            {
                mididiag::AddFinding(inputs ? L"winmm_input_duplicate_names" : L"winmm_output_duplicate_names",
                    mididiag::FormatResourceString(inputs ? IDS_FINDING_WINMM_INPUT_DUPLICATE_NAMES : IDS_FINDING_WINMM_OUTPUT_DUPLICATE_NAMES, duplicateCount));
            }
        }
        catch (...)
        {
            OutputError(internal::ResourceGetWString(inputs ?
                IDS_ERROR_EXCEPTION_ENUMERATING_WINMM_INPUTS :
                IDS_ERROR_EXCEPTION_ENUMERATING_WINMM_OUTPUTS));

            succeeded = false;
        }
    }

    return succeeded;
}


bool DoSectionPingTest(_In_ bool const verbose, _In_ uint8_t const pingCount)
{
    UNREFERENCED_PARAMETER(verbose);

    try
    {
        OutputSectionHeader(MIDIDIAG_SECTION_LABEL_PING_TEST);

        OutputNumericField(MIDIDIAG_FIELD_LABEL_PING_ATTEMPT_COUNT, (uint32_t)pingCount);

        auto pingResult = diag::MidiDiagnostics::PingService(pingCount);

        //std::cout << "DEBUG: PingService returned" << std::endl;

        if (pingResult != nullptr)
        {
            //std::cout << "DEBUG: pingresult != nullptr" << std::endl;

            OutputNumericField(MIDIDIAG_FIELD_LABEL_PING_RETURN_COUNT, pingResult.Responses().Size());

            if (pingResult.Success())
            {
                //std::cout << "DEBUG: pingresult.Success()" << std::endl;

                OutputTimestampField(MIDIDIAG_FIELD_LABEL_PING_ROUND_TRIP_TOTAL_TICKS, pingResult.TotalPingRoundTripMidiClock());
                OutputTimestampField(MIDIDIAG_FIELD_LABEL_PING_ROUND_TRIP_AVERAGE_TICKS, pingResult.AveragePingRoundTripMidiClock());

                return true;
            }
            else
            {
                OutputError(internal::ResourceGetWString(IDS_ERROR_PING_FAILED));
                OutputStringField(MIDIDIAG_FIELD_LABEL_PING_FAILURE_REASON, pingResult.FailureReason());
            }
        }
        else
        {
            OutputError(internal::ResourceGetWString(IDS_ERROR_PING_FAILED_NULL_RESULT));
        }
    }
    catch (...)
    {
        OutputError(internal::ResourceGetWString(IDS_ERROR_PING_FAILED_EXCEPTION));
    }

    mididiag::AddFinding(L"ping_failed", internal::ResourceGetWString(IDS_FINDING_PING_FAILED));

    return false;

}

bool DoSectionClock(_In_ bool const verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_MIDI_CLOCK);

    try
    {
        OutputTimestampField(MIDIDIAG_FIELD_LABEL_CLOCK_FREQUENCY, midi2::MidiClock::TimestampFrequency());
        OutputTimestampField(MIDIDIAG_FIELD_LABEL_CLOCK_NOW, midi2::MidiClock::Now());

        return true;
    }
    catch (winrt::hresult_error ex)
    {
        OutputError(ex.message().c_str());
        OutputError(internal::ResourceGetWString(IDS_ERROR_CLOCK_HRESULT));

        return false;
    }
}





std::wstring GetOSVersion()
{
    try
    {
        OSVERSIONINFOW versionInfo{};

        NTSTATUS (WINAPI *rtlGetVersion)(PRTL_OSVERSIONINFOW) = nullptr;

        HINSTANCE ntdll = LoadLibrary(L"ntdll.dll");

        if (ntdll != nullptr)
        {
            rtlGetVersion = (NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW)) GetProcAddress(ntdll, "RtlGetVersion");

            if (rtlGetVersion != nullptr)
            {
                rtlGetVersion((PRTL_OSVERSIONINFOW)&versionInfo);
            }

            // do this before anything else so we ensure if frees
            FreeLibrary(ntdll);

            if (rtlGetVersion != nullptr)
            {
                auto version = std::format(L"{}.{}.{}", versionInfo.dwMajorVersion, versionInfo.dwMinorVersion, versionInfo.dwBuildNumber);

                // the update revision, which tells one monthly update from the next
                try
                {
                    if (auto const revision = wil::reg::try_get_value_dword(HKEY_LOCAL_MACHINE,
                        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"UBR"); revision.has_value())
                    {
                        version += std::format(L".{}", revision.value());
                    }
                }
                catch (...)
                {
                }

                return version;
            }

        }

        return L"unknown";

    }
    catch (...)
    {
        return L"exception";
    }
}


std::wstring GetProcessorArchitectureString(WORD const arch)
{
    switch (arch)
    {
    case PROCESSOR_ARCHITECTURE_AMD64:
        return L"64-bit Intel/AMD";
    case PROCESSOR_ARCHITECTURE_ARM:
        return L"32-bit Arm";
    case PROCESSOR_ARCHITECTURE_ARM64:
        return L"Arm64";
    case PROCESSOR_ARCHITECTURE_IA64:
        return L"Itanium";
    case PROCESSOR_ARCHITECTURE_INTEL:
        return L"32-bit Intel x86";
    default:
        return L"Unknown";
    }

}

#define ENV_PROCESSOR_ARCHITECTURE_WIDE  L"PROCESSOR_ARCHITECTURE"
#define ENV_PROCESSOR_IDENTIFIER_WIDE    L"PROCESSOR_IDENTIFIER"
#define ENV_PROCESSOR_LEVEL_WIDE         L"PROCESSOR_LEVEL"
#define ENV_PROCESSOR_REVISION_WIDE      L"PROCESSOR_REVISION"


#pragma warning(push)
#pragma warning(disable: 4996)
void OutputProcessorEnvVariables()
{
    OutputStringField(ENV_PROCESSOR_ARCHITECTURE_WIDE, std::wstring{ _wgetenv(ENV_PROCESSOR_ARCHITECTURE_WIDE)});
    OutputStringField(ENV_PROCESSOR_IDENTIFIER_WIDE, std::wstring{ _wgetenv(ENV_PROCESSOR_IDENTIFIER_WIDE) });
    OutputStringField(ENV_PROCESSOR_LEVEL_WIDE, std::wstring{ _wgetenv(ENV_PROCESSOR_LEVEL_WIDE) });
    OutputStringField(ENV_PROCESSOR_REVISION_WIDE, std::wstring{ _wgetenv(ENV_PROCESSOR_REVISION_WIDE) });
}
#pragma warning(pop)

void OutputSystemInfo(_In_ SYSTEM_INFO const& sysinfo)
{
    // that sysinfo.dwNumberOfProcessors can return some strange results.
    
//    OutputNumericField(L"num_processors", sysinfo.dwNumberOfProcessors);
    std::wstring processorArchitecture = GetProcessorArchitectureString(sysinfo.wProcessorArchitecture);

    OutputStringField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_ARCH, processorArchitecture);
    OutputNumericField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_LEVEL, sysinfo.wProcessorLevel);
    OutputHexNumericField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_REVISION, sysinfo.wProcessorRevision);
}

void OutputProcessAndNativeMachine()
{
    USHORT processMachine{ 0 };
    USHORT nativeMachine{ 0 };

    HANDLE hProcess = ::GetCurrentProcess();

    if (hProcess)
    {
        auto worked = ::IsWow64Process2(hProcess, &processMachine, &nativeMachine);

        if (worked)
        {
            // if not running emulated, IsWow64Process2 returns machine unknown for the process.
            if ((processMachine == IMAGE_FILE_MACHINE_UNKNOWN || processMachine == IMAGE_FILE_MACHINE_ARM64) && nativeMachine == IMAGE_FILE_MACHINE_ARM64)
            {
                OutputStringField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_EMULATION, std::wstring{ L"Native Arm64 process on Arm64 PC (Not Emulated)" });
            }
            else if ((processMachine == IMAGE_FILE_MACHINE_UNKNOWN || processMachine == IMAGE_FILE_MACHINE_AMD64) && nativeMachine == IMAGE_FILE_MACHINE_AMD64)
            {
                OutputStringField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_EMULATION, std::wstring{ L"Native Intel/AMD x64 on x64 PC (Not Emulated)" });
            }
            else if (processMachine == IMAGE_FILE_MACHINE_AMD64 && nativeMachine == IMAGE_FILE_MACHINE_ARM64)
            {
                OutputStringField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_EMULATION, std::wstring{ L"Intel/AMD x64 process on Arm64 PC (Emulated)" });
            }
            else if (processMachine == IMAGE_FILE_MACHINE_ARM64 && nativeMachine == IMAGE_FILE_MACHINE_AMD64)
            {
                // not supported today, but here in case it is some day
                OutputStringField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_EMULATION, std::wstring{ L"Arm64 process on Intel/AMD x64 PC (Emulated)" });
            }
            else
            {
                OutputError(internal::ResourceGetWString(IDS_ERROR_UNIDENTIFIED_ARCHITECTURE));
            }
        }
        else
        {
            OutputError(internal::ResourceGetWString(IDS_ERROR_CANNOT_QUERY_ARCHITECTURE));
        }

    }
}

bool DoSectionDevMode(_In_ bool verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    // dev mode check

    OutputSectionHeader(MIDIDIAG_SECTION_DEV_MODE);

    wil::unique_hkey devModeKey{ };
    if (SUCCEEDED(wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\AppModelUnlock", devModeKey)))
    {
        auto devModeEnabledValue = wil::reg::try_get_value_dword(devModeKey.get(), L"AllowDevelopmentWithoutDevLicense");

        if (devModeEnabledValue.has_value())
        {
            OutputBooleanField(MIDIDIAG_FIELD_LABEL_DEV_MODE_ENABLED, (bool)(devModeEnabledValue.value() > 0));
        }
        else
        {
            OutputStringField(MIDIDIAG_FIELD_LABEL_DEV_MODE_ENABLED, std::wstring{ L"Value Not Present" });
        }
    }
    else
    {
        OutputStringField(MIDIDIAG_FIELD_LABEL_DEV_MODE_ENABLED, std::wstring{ L"Key Not Present" });
    }

    return true;
}


bool DoSectionSystemInfo(_In_ bool verbose)
{
    UNREFERENCED_PARAMETER(verbose);

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_OS);
    OutputStringField(MIDIDIAG_FIELD_LABEL_OS_VERSION, GetOSVersion());
    OutputOperatingSystemFields();


    // if running under emulation on Arm64, this is going to return the emulated sys info
    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_PROCESSOR_ENV);
    OutputProcessorEnvVariables();

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_NATIVE_SYSTEM_INFO);

    SYSTEM_INFO sysinfoNative;
    ::GetNativeSystemInfo(&sysinfoNative);
    OutputSystemInfo(sysinfoNative);
    OutputProcessAndNativeMachine();

    TIMECAPS timecaps;
    auto tcresult = ::timeGetDevCaps(&timecaps, sizeof(timecaps));

    if (tcresult == MMSYSERR_NOERROR)
    {
        OutputNumericField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMECAPS_MIN_PERIOD, timecaps.wPeriodMin);
        OutputNumericField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMECAPS_MAX_PERIOD, timecaps.wPeriodMax);
    }
    else
    {
        OutputStringField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMECAPS_ERROR, std::wstring{ L"Could not get timecaps" });
    }

    ULONG minResolution;
    ULONG maxResolution;
    ULONG actualResolution;

    auto resresult = NtQueryTimerResolution(&maxResolution, &minResolution, &actualResolution);

    if (resresult == STATUS_SUCCESS)
    {
        double minResolutionMilliseconds = (double)minResolution / 10000.0;   // minResolution is in 100 nanosecond units
        double maxResolutionMilliseconds = (double)maxResolution / 10000.0;   // maxResolution is in 100 nanosecond units
        double actualResolutionMilliseconds = (double)actualResolution / 10000.0;   // actualResolution is in 100 nanosecond units

        // results here are in 100ns units
        OutputDecimalMillisecondsField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMER_RESOLUTION_MIN_MS, minResolutionMilliseconds, 3);
        OutputDecimalMillisecondsField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMER_RESOLUTION_MAX_MS, maxResolutionMilliseconds, 3);
        OutputDecimalMillisecondsField(MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMER_RESOLUTION_CURRENT_MS, actualResolutionMilliseconds, 3);
    }

    return true;
}


// =======================================================
// TODO: This must be updated for each KIR add or remove
// Also, these results are only valid for in-box builds
// =======================================================


// 11D 2026 (planned)
#include "Feature_Servicing_MIDIPortDisambiguators.h"
#include "Feature_Servicing_MIDI2LoopbackMuteAndList.h"
#include "Feature_Servicing_MIDI2UnicodeConversion.h"
#include "Feature_Servicing_MIDI2KSInputRemovalDeadlock.h"
#include "Feature_Servicing_MIDI2KSOutputWriteHang.h"
#include "Feature_Servicing_MIDI2KSInputReadCompletionTimestamp.h"
#include "Feature_Servicing_MIDI2KSAWatcherHardening.h"
#include "Feature_Servicing_MIDI2PortNumberCache.h"
#include "Feature_Servicing_MIDI2VirtualDeviceRemovalDeadlock.h"
#include "Feature_Servicing_MIDI2USBSystemRealTimeUmpSize.h"
#include "Feature_Servicing_MIDI2USBSystemRealTimeCin.h"
#include "Feature_Servicing_MIDI2USBCableMaskDirection.h"
#include "Feature_Servicing_MIDI2BsToUMPConvDisallowNOOPs.h"
#include "Feature_Servicing_MIDI2XProcSendWaitTimeouts.h"
#include "Feature_Servicing_MIDI2ConfigJsonSizeLimit.h"
#include "Feature_Servicing_MIDI2SessionNameLimit.h"
#include "Feature_Servicing_MIDI2ComponentSignatureCache.h"
#include "Feature_Servicing_MIDI2CustomOutgoingLatency.h"
#include "Feature_Servicing_MIDI2DuplicateDeviceNaming.h"
#include "Feature_Servicing_MIDI2EndpointCustomizationEnhancements.h"
#include "Feature_Servicing_MIDI2EndpointNameUtf8ByteLimit.h"
#include "Feature_Servicing_MIDI2EndpointUniqueIdValidation.h"
#include "Feature_Servicing_MIDI2LoopbackErrorStringResources.h"
#include "Feature_Servicing_MIDI2LoopbackFeedbackProtection.h"
#include "Feature_Servicing_MIDI2LoopbackUniqueEndpointNames.h"
#include "Feature_Servicing_MIDI2PortNamingRework.h"
#include "Feature_Servicing_MIDI2ProtocolNegotiationDeadlock.h"
#include "Feature_Servicing_MIDI2RecommendedCCIntervalProp.h"
#include "Feature_Servicing_MIDI2SchedulerV2.h"
#include "Feature_Servicing_MIDI2ServiceConfigJsonHardening.h"
#include "Feature_Servicing_MIDI2SessionTrackerConnectionTime.h"
#include "Feature_Servicing_MIDI2StreamTextUtf8.h"
#include "Feature_Servicing_MIDI2StringCharacterSets.h"
#include "Feature_Servicing_MIDI2SynchronizedStart.h"
#include "Feature_Servicing_MIDI2TransportAssociationIdGuidValidation.h"
#include "Feature_Servicing_MIDI2TransportCommandJsonHardening.h"
#include "Feature_Servicing_MIDI2TransportConfigRejectionReasons.h"
#include "Feature_Servicing_MIDI2VirtualDeviceClientEndpointInUse.h"
#include "Feature_Servicing_MIDI2VirtualDeviceClientReconnect.h"
#include "Feature_Servicing_MIDI2WinMMCleanupAfterDeviceRemoval.h"
#include "Feature_Servicing_MIDI2WinMMCompleteLongBufferOnFailure.h"
#include "Feature_Servicing_MIDI2WinMMInterfaceRemovalPerf.h"
#include "Feature_Servicing_MIDI2WinMMPortHandleSlotWidth.h"
#include "Feature_Servicing_MIDI2WinMMShortMessageNoSendWait.h"
#include "Feature_Servicing_MIDI2XProcBatchedReads.h"
#include "Feature_Servicing_MIDI2KSAShutdownCrash.h"
#include "Feature_Servicing_MIDI2DuplicateDeviceNaming.h"

void OutputSingleFeatureEnablement(_In_ bool enabled, _In_ std::wstring const& featureName)
{
    if (enabled)
    {
        OutputStringField(MIDIDIAG_FIELD_LABEL_ENABLED_FEATURE, featureName);
    }
    else
    {
        OutputStringField(MIDIDIAG_FIELD_LABEL_DISABLED_FEATURE, featureName);
    }
}

bool DoSectionFeatureEnablement(_In_ bool verbose)
{
    UNREFERENCED_PARAMETER(verbose);

#if false
    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_FEATURE_ENABLEMENT);

    // 11d 2026 (planned)

    OutputSingleFeatureEnablement(Feature_Servicing_MIDIPortDisambiguators::IsEnabled(),                    L"MIDIPortDisambiguators");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2LoopbackMuteAndList::IsEnabled(),                  L"MIDI2LoopbackMuteAndList");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2UnicodeConversion::IsEnabled(),                    L"MIDI2UnicodeConversion");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2KSInputRemovalDeadlock::IsEnabled(),               L"MIDI2KSInputRemovalDeadlock (workaround surprise removal for InMusic drivers)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2KSOutputWriteHang::IsEnabled(),                    L"MIDI2KSOutputWriteHang (fix for declared midi out when none present)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2KSInputReadCompletionTimestamp::IsEnabled(),       L"MIDI2KSInputReadCompletionTimestamp (timestamp incoming midi 1.0 driver data when the service reads it, so winmm apps stop dropping it)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2KSAWatcherHardening::IsEnabled(),                  L"MIDI2KSAWatcherHardening (fix for MONTAGE M / MODX usb port move)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2PortNumberCache::IsEnabled(),                      L"MIDI2PortNumberCache (greatly speeds up service startup)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2USBSystemRealTimeUmpSize::IsEnabled(),             L"MIDI2USBSystemRealTimeUmpSize (fix timing clock coming in as NOOP on MIDI2 driver)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2USBSystemRealTimeCin::IsEnabled(),                 L"MIDI2USBSystemRealTimeCin (fix timing clock, start and stop not recognized by some USB MIDI 1.0 devices on MIDI2 driver)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2USBCableMaskDirection::IsEnabled(),                L"MIDI2USBCableMaskDirection (fix missing ports on USB MIDI 1.0 devices with different numbers of inputs and outputs on MIDI2 driver)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2BsToUMPConvDisallowNOOPs::IsEnabled(),             L"MIDI2BsToUMPConvDisallowNOOPs (ensure over-stated MIDI 1 packet size doesn't result in trailing NOOPs)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2VirtualDeviceRemovalDeadlock::IsEnabled(),         L"MIDI2VirtualDeviceRemovalDeadlock (fix service hang when a virtual device is shut down)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2XProcSendWaitTimeouts::IsEnabled(),                L"MIDI2XProcSendWaitTimeouts (stop aborting sends to devices that are slow to accept data)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2ConfigJsonSizeLimit::IsEnabled(),                  L"MIDI2ConfigJsonSizeLimit (bound the configuration json a client may send over rpc)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2SessionNameLimit::IsEnabled(),                     L"MIDI2SessionNameLimit (bound the session name a client may register or update)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2ComponentSignatureCache::IsEnabled(),              L"MIDI2ComponentSignatureCache (cache transport and transform signature checks, which cost about a second per dll)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2CustomOutgoingLatency::IsEnabled(),                L"MIDI2CustomOutgoingLatency (user-supplied outgoing latency compensation for an endpoint)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2DuplicateDeviceNaming::IsEnabled(),                 L"MIDI2DuplicateDeviceNaming (service assigns the number for a second or later unit of a model, and remembers it)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2EndpointCustomizationEnhancements::IsEnabled(),    L"MIDI2EndpointCustomizationEnhancements (rework of how endpoint customizations are matched and applied)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2EndpointNameUtf8ByteLimit::IsEnabled(),            L"MIDI2EndpointNameUtf8ByteLimit (enforce the ump spec utf-8 byte limit on endpoint names)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2EndpointUniqueIdValidation::IsEnabled(),           L"MIDI2EndpointUniqueIdValidation (reject a unique id which is not usable in a device id)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2LoopbackErrorStringResources::IsEnabled(),         L"MIDI2LoopbackErrorStringResources (localizable error text for loopback configuration failures)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2LoopbackFeedbackProtection::IsEnabled(),           L"MIDI2LoopbackFeedbackProtection (mute a loopback when MIDI feeds back into it)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2LoopbackUniqueEndpointNames::IsEnabled(),          L"MIDI2LoopbackUniqueEndpointNames (reject duplicate names when creating loopback endpoints)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2PortNamingRework::IsEnabled(),                     L"MIDI2PortNamingRework (rework of how midi 1.0 port names are generated)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2ProtocolNegotiationDeadlock::IsEnabled(),          L"MIDI2ProtocolNegotiationDeadlock (fix service hang during endpoint protocol negotiation)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2RecommendedCCIntervalProp::IsEnabled(),            L"MIDI2RecommendedCCIntervalProp (recommended control change interval property for an endpoint)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2SchedulerV2::IsEnabled(),                          L"MIDI2SchedulerV2 (replacement message scheduler, removes the busy wait and applies latency compensation)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2ServiceConfigJsonHardening::IsEnabled(),           L"MIDI2ServiceConfigJsonHardening (harden parsing of the configuration json sent over rpc)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2SessionTrackerConnectionTime::IsEnabled(),         L"MIDI2SessionTrackerConnectionTime (report when each client connected to an endpoint)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2StreamTextUtf8::IsEnabled(),                       L"MIDI2StreamTextUtf8 (decode ump stream message text as utf-8)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2StringCharacterSets::IsEnabled(),                  L"MIDI2StringCharacterSets (remove characters which are not allowed in ids and names)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2SynchronizedStart::IsEnabled(),                    L"MIDI2SynchronizedStart (client waits for the service to finish enumerating devices)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2TransportAssociationIdGuidValidation::IsEnabled(), L"MIDI2TransportAssociationIdGuidValidation (reject a loopback association id which is not a valid guid)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2TransportCommandJsonHardening::IsEnabled(),        L"MIDI2TransportCommandJsonHardening (harden parsing of transport command json)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2TransportConfigRejectionReasons::IsEnabled(),      L"MIDI2TransportConfigRejectionReasons (return the reason a transport rejected a configuration)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2VirtualDeviceClientEndpointInUse::IsEnabled(),     L"MIDI2VirtualDeviceClientEndpointInUse (tell a virtual device when its client endpoint is in use)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2VirtualDeviceClientReconnect::IsEnabled(),         L"MIDI2VirtualDeviceClientReconnect (allow a client to reconnect to a virtual device endpoint)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2WinMMCleanupAfterDeviceRemoval::IsEnabled(),       L"MIDI2WinMMCleanupAfterDeviceRemoval (clean up winmm port state after a device is removed)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2WinMMCompleteLongBufferOnFailure::IsEnabled(),     L"MIDI2WinMMCompleteLongBufferOnFailure (return a winmm long buffer to the app when a send fails)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2WinMMInterfaceRemovalPerf::IsEnabled(),            L"MIDI2WinMMInterfaceRemovalPerf (speed up winmm handling of device interface removal)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2WinMMPortHandleSlotWidth::IsEnabled(),             L"MIDI2WinMMPortHandleSlotWidth (fix winmm port handle corruption in 32 bit clients)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2WinMMShortMessageNoSendWait::IsEnabled(),          L"MIDI2WinMMShortMessageNoSendWait (stop winmm waiting for send completion on short messages)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2XProcBatchedReads::IsEnabled(),                    L"MIDI2XProcBatchedReads (batch cross-process reads to reduce per-message overhead)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2KSAShutdownCrash::IsEnabled(),                     L"MIDI2KSAShutdownCrash (shutdown race)");
    OutputSingleFeatureEnablement(Feature_Servicing_MIDI2DuplicateDeviceNaming::IsEnabled(),                L"MIDI2DuplicateDeviceNaming (service assigns the number for a second or later unit of a model, and remembers it)");

    
#endif    
        

    return true;
}



namespace
{
    // These sections read Windows itself, but a busy PC with many devices can still be slow
    constexpr std::chrono::seconds LocalSectionTimeout{ 60 };

    // A section waiting on a stuck service never finishes. This is how long a customer waits
    // before the report says so.
    constexpr std::chrono::seconds ServiceSectionTimeout{ 60 };

    constexpr uint8_t PingCount{ 10 };

    void OutputUsage()
    {
        mididiag::WriteLine(internal::ResourceGetWString(IDS_USAGE_SYNTAX));
        mididiag::WriteLine(internal::ResourceGetWString(IDS_USAGE_OPTION_WINRT_MIDI1));
        mididiag::WriteLine(internal::ResourceGetWString(IDS_USAGE_OPTION_HELP));
    }
}

int __cdecl wmain(_In_ int argc, _In_reads_(argc) wchar_t* argv[])
{
    if (!TrySetConsoleTextMode())
    {
        return RETURN_ERROR_SETTING_CONSOLE_MODE;
    }

    auto& context = mididiag::Context();

    for (int i = 1; i < argc; i++)
    {
        std::wstring const argument{ argv[i] == nullptr ? L"" : argv[i] };

        if (_wcsicmp(argument.c_str(), L"--include-winrt-midi1") == 0)
        {
            context.IncludeWinRTMidi1 = true;
        }
        else if (argument == L"--help" || argument == L"-h" || argument == L"-?" || argument == L"/?")
        {
            OutputUsage();
            return RETURN_SUCCESS;
        }
        else
        {
            mididiag::WriteError(mididiag::FormatResourceString(IDS_ERROR_UNKNOWN_OPTION, argument));
            OutputUsage();
            return RETURN_INVALID_MODE;
        }
    }

    winrt::init_apartment();

    // before anything below can start the service by asking it something
    CaptureServiceStateBeforeReport();
    context.Elevated = CheckForAdminPermissions();

    mididiag::SetReportPhase(mididiag::ReportPhase::Local, LocalSectionTimeout);
    mididiag::StartWatchdog();

    bool const verbose = true;
    bool runSucceeded = true;

    // free text for people, before the first section
    OutputHeader(internal::ResourceGetWString(IDS_BANNER_TOOL_INFO));
    OutputHeader(internal::ResourceGetWString(IDS_BANNER_COPYRIGHT));
    OutputHeader(internal::ResourceGetWString(IDS_BANNER_INFO_URL));
    OutputHeader(MIDIDIAG_PRODUCT_NAME);

    OutputSectionHeader(MIDIDIAG_SECTION_LABEL_HEADER);
    OutputNumericField(MIDIDIAG_FIELD_LABEL_REPORT_FORMAT_VERSION, MIDIDIAG_REPORT_FORMAT_VERSION);
    OutputStringField(MIDIDIAG_HEADER_FIELD_LABEL_VERSION_BUILD_SOURCE, std::wstring{ WINDOWS_MIDI_SERVICES_NUGET_BUILD_SOURCE });
    OutputStringField(MIDIDIAG_HEADER_FIELD_LABEL_VERSION_NAME, std::wstring{ WINDOWS_MIDI_SERVICES_NUGET_BUILD_VERSION_NAME });
    OutputStringField(MIDIDIAG_HEADER_FIELD_LABEL_VERSION_FULL, std::wstring{ WINDOWS_MIDI_SERVICES_NUGET_BUILD_VERSION_FULL });
    OutputCurrentTime();
    OutputBooleanField(MIDIDIAG_FIELD_LABEL_RUNNING_ELEVATED, context.Elevated);

    if (context.IncludeWinRTMidi1)
    {
        OutputStringField(MIDIDIAG_FIELD_LABEL_OPTIONS, std::wstring{ L"--include-winrt-midi1" });
    }

    try
    {
        // Everything in this phase reads Windows itself and never calls the MIDI service, so it
        // is all in the report even when the service is stuck.
        DoSectionSystemInfo(verbose);
        DoSectionDevMode(verbose);
        DoSectionApiMode();
        DoSectionComponentVersions();
        DoSectionServiceStatus();
        DoSectionServiceHistory();
        DoSectionFeatureEnablement(verbose);
        DoSectionDrivers32RegistryEntries(verbose);
        DoSectionDrivers32WOWRegistryEntries(verbose);
        DoSectionMidi2RegistryEntries(verbose);
        DoSectionDeviceNodes();
        DoSectionNetwork();
        DoSectionMdns();
        DoSectionNetworkHistory();

        if (context.IncludeWinRTMidi1)
        {
            DoSectionWinRTMidi1ApiEndpoints(verbose);
        }

        // From here on a call can wait on the service. If it never answers, the watchdog ends the report.
        mididiag::SetReportPhase(mididiag::ReportPhase::Service, ServiceSectionTimeout);

        bool serviceAvailable{ false };

        if (context.LegacyApiMode || !context.ServiceInstalled || context.ServiceDisabled)
        {
            // asking the service anything would start it, which Legacy API mode must not do
            OutputSectionHeader(MIDIDIAG_SECTION_LABEL_SERVICE_RESPONSE);
            OutputStringField(MIDIDIAG_FIELD_LABEL_SERVICE_SECTIONS_SKIPPED, std::wstring{
                context.LegacyApiMode ? L"legacy_api_mode" :
                !context.ServiceInstalled ? L"service_not_installed" : L"service_disabled" });
        }
        else
        {
            serviceAvailable = DoSectionServiceResponse();
        }

        if (serviceAvailable)
        {
            if (!DoSectionClock(verbose) || !DoSectionTransports(verbose) || !DoSectionMidi2ApiEndpoints(verbose))
            {
                runSucceeded = false;
            }
        }

        // most apps use WinMM, so its ports are worth listing even when the service sections were skipped
        DoSectionWinMMMidi1ApiEndpoints(verbose);

        if (serviceAvailable)
        {
            DoSectionBluetooth();
            DoSectionNetworkMidi2();
            DoSectionRtpMidi();
            DoSectionLoopback();
            DoSectionBasicLoopback();
            DoSectionEndpointCustomizations();

            // before the ping test and the connection timing, so their own sessions are not listed
            DoSectionSessions(verbose);

            if (!DoSectionPingTest(verbose, PingCount))
            {
                runSucceeded = false;
            }

            DoSectionConnectionTiming();
        }
    }
    catch (...)
    {
        OutputError(internal::ResourceGetWString(IDS_ERROR_EXCEPTION_GATHERING_INFORMATION));
        runSucceeded = false;
    }

    try
    {
        mididiag::StopWatchdog();
    }
    catch (...)
    {
    }

    mididiag::WriteFindingsSection();
    mididiag::WriteSectionTimingSection();

    // don't localize
    if (runSucceeded)
    {
        mididiag::WriteClosingSection(MIDIDIAG_SECTION_LABEL_SUCCESSFUL_RUN);
    }
    else
    {
        mididiag::WriteClosingSection(MIDIDIAG_SECTION_LABEL_ABORTED_RUN);
        OutputError(internal::ResourceGetWString(IDS_ERROR_ABORTING_RUN));
    }

    mididiag::WriteClosingSection(MIDIDIAG_SECTION_LABEL_END_OF_FILE);
    fflush(stdout);

    return runSucceeded ? RETURN_SUCCESS : RETURN_GENERAL_FAILURE;
}
