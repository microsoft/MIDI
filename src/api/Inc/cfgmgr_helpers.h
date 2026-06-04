// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <cfgmgr32.h>
#include <devpropdef.h>
#include <optional>
#include <vector>
#include <winrt/base.h>

namespace WindowsMidiServicesInternal
{
    // Reads a DEVPROP_TYPE_STRING property from a devnode and returns its
    // value as a winrt::hstring. On any failure (devnode gone, wrong type,
    // missing property, etc.) returns an empty hstring. Optionally returns
    // the underlying CONFIGRET via 'outConfigRet' so callers that want to
    // distinguish "missing" from "failed" can do so.
    inline winrt::hstring GetDevNodeStringProperty(
        _In_ DEVINST devInst,
        _In_ DEVPROPKEY const& propertyKey,
        _Out_opt_ CONFIGRET* outConfigRet = nullptr) noexcept
    {
        if (outConfigRet) *outConfigRet = CR_FAILURE;

        DEVPROPTYPE propType{};
        ULONG bytes = 0;

        CONFIGRET cr = CM_Get_DevNode_PropertyW(
            devInst,
            &propertyKey,
            &propType,
            nullptr,
            &bytes,
            0);

        if (cr != CR_BUFFER_SMALL || propType != DEVPROP_TYPE_STRING || bytes < sizeof(wchar_t))
        {
            if (outConfigRet) *outConfigRet = cr;
            return {};
        }

        std::vector<BYTE> buffer(bytes);

        cr = CM_Get_DevNode_PropertyW(
            devInst,
            &propertyKey,
            &propType,
            buffer.data(),
            &bytes,
            0);

        if (outConfigRet) *outConfigRet = cr;

        if (cr != CR_SUCCESS || propType != DEVPROP_TYPE_STRING)
        {
            return {};
        }

        auto raw = reinterpret_cast<const wchar_t*>(buffer.data());
        size_t chars = bytes / sizeof(wchar_t);

        // Strip a single trailing null so it doesn't end up inside the hstring.
        if (chars > 0 && raw[chars - 1] == L'\0')
        {
            --chars;
        }

        return winrt::hstring{ raw, static_cast<winrt::hstring::size_type>(chars) };
    }

    // Reads a DEVPROP_TYPE_GUID property from a devnode. Returns std::nullopt
    // if the property is missing, the wrong type, or the read fails. Use the
    // optional 'outConfigRet' to distinguish missing vs. failed.
    inline std::optional<winrt::guid> GetDevNodeGuidProperty(
        _In_ DEVINST devInst,
        _In_ DEVPROPKEY const& propertyKey,
        _Out_opt_ CONFIGRET* outConfigRet = nullptr) noexcept
    {
        DEVPROPTYPE propType{};
        GUID value{};
        ULONG bytes = sizeof(value);

        CONFIGRET cr = CM_Get_DevNode_PropertyW(
            devInst,
            &propertyKey,
            &propType,
            reinterpret_cast<PBYTE>(&value),
            &bytes,
            0);

        if (outConfigRet) *outConfigRet = cr;

        if (cr != CR_SUCCESS || propType != DEVPROP_TYPE_GUID || bytes != sizeof(value))
        {
            return std::nullopt;
        }

        return winrt::guid{ value };
    }

    // Reads a DEVPROP_TYPE_UINT32 property from a devnode. Returns std::nullopt
    // if the property is missing, the wrong type, or the read fails.
    inline std::optional<uint32_t> GetDevNodeUInt32Property(
        _In_ DEVINST devInst,
        _In_ DEVPROPKEY const& propertyKey,
        _Out_opt_ CONFIGRET* outConfigRet = nullptr) noexcept
    {
        DEVPROPTYPE propType{};
        uint32_t value{};
        ULONG bytes = sizeof(value);

        CONFIGRET cr = CM_Get_DevNode_PropertyW(
            devInst,
            &propertyKey,
            &propType,
            reinterpret_cast<PBYTE>(&value),
            &bytes,
            0);

        if (outConfigRet) *outConfigRet = cr;

        if (cr != CR_SUCCESS || propType != DEVPROP_TYPE_UINT32 || bytes != sizeof(value))
        {
            return std::nullopt;
        }

        return value;
    }
}