// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include <windows.h>
#include <cfgmgr32.h>
#include <devpkey.h>
#include <devpropdef.h>

#include <algorithm>
#include <cwchar>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <winrt/base.h>

#include "MidiPnpDeviceInfo.h"

namespace WindowsMidiServicesInternal
{
    // ============================================================
    // File-local helpers
    // ============================================================
    namespace
    {
        // Build a null-terminated copy of a string_view we can hand to cfgmgr.
        inline std::wstring NullTerminate(std::wstring_view sv)
        {
            return std::wstring{ sv };
        }

        // Strip a single trailing null from a wide char range read out of a
        // DEVPROP_TYPE_STRING property.
        inline winrt::hstring HStringFromRawBuffer(const wchar_t* data, size_t bytes) noexcept
        {
            if (data == nullptr || bytes < sizeof(wchar_t))
            {
                return {};
            }

            size_t chars = bytes / sizeof(wchar_t);
            if (chars > 0 && data[chars - 1] == L'\0')
            {
                --chars;
            }

            return winrt::hstring{ data, static_cast<winrt::hstring::size_type>(chars) };
        }

        // Split a DEVPROP_TYPE_STRING_LIST buffer (double-null-terminated
        // multi-string) into individual hstrings.
        inline std::vector<winrt::hstring> SplitMultiString(const wchar_t* data, size_t bytes) noexcept
        {
            std::vector<winrt::hstring> result;
            if (data == nullptr || bytes < sizeof(wchar_t))
            {
                return result;
            }

            size_t totalChars = bytes / sizeof(wchar_t);
            const wchar_t* end = data + totalChars;
            const wchar_t* p = data;

            while (p < end && *p != L'\0')
            {
                size_t remaining = static_cast<size_t>(end - p);
                size_t len = ::wcsnlen(p, remaining);
                if (len == 0)
                {
                    break;
                }

                result.emplace_back(p, static_cast<winrt::hstring::size_type>(len));

                if (len >= remaining)
                {
                    break;
                }
                p += len + 1;
            }

            return result;
        }

        // ---- Generic devnode property reader ----------------------------------
        template <typename Reader>
        bool ReadVariableSizeProperty(
            DEVPROPTYPE expectedType,
            Reader reader,
            std::vector<BYTE>& outBuffer,
            DEVPROPTYPE& outType) noexcept
        {
            outType = 0;
            outBuffer.clear();

            ULONG bytes = 0;
            CONFIGRET cr = reader(nullptr, &bytes);

            if (cr != CR_BUFFER_SMALL || outType != expectedType || bytes == 0)
            {
                return false;
            }

            outBuffer.resize(bytes);
            cr = reader(outBuffer.data(), &bytes);
            if (cr != CR_SUCCESS || outType != expectedType)
            {
                outBuffer.clear();
                return false;
            }

            outBuffer.resize(bytes);
            return true;
        }

        // ---- Devnode wrappers --------------------------------------------------
        winrt::hstring ReadDevNodeString(DEVINST devInst, DEVPROPKEY const& key) noexcept
        {
            DEVPROPTYPE propType{};
            std::vector<BYTE> buffer;

            bool ok = ReadVariableSizeProperty(
                DEVPROP_TYPE_STRING,
                [&](PBYTE buf, PULONG size) -> CONFIGRET
                {
                    return CM_Get_DevNode_PropertyW(devInst, &key, &propType, buf, size, 0);
                },
                buffer,
                propType);

            if (!ok) return {};

            return HStringFromRawBuffer(
                reinterpret_cast<const wchar_t*>(buffer.data()),
                buffer.size());
        }

        std::vector<winrt::hstring> ReadDevNodeStringList(DEVINST devInst, DEVPROPKEY const& key) noexcept
        {
            DEVPROPTYPE propType{};
            std::vector<BYTE> buffer;

            bool ok = ReadVariableSizeProperty(
                DEVPROP_TYPE_STRING_LIST,
                [&](PBYTE buf, PULONG size) -> CONFIGRET
                {
                    return CM_Get_DevNode_PropertyW(devInst, &key, &propType, buf, size, 0);
                },
                buffer,
                propType);

            if (!ok) return {};

            return SplitMultiString(
                reinterpret_cast<const wchar_t*>(buffer.data()),
                buffer.size());
        }

        template <typename T>
        std::optional<T> ReadDevNodeFixed(DEVINST devInst, DEVPROPKEY const& key, DEVPROPTYPE expectedType) noexcept
        {
            DEVPROPTYPE propType{};
            T value{};
            ULONG bytes = sizeof(value);

            CONFIGRET cr = CM_Get_DevNode_PropertyW(
                devInst, &key, &propType, reinterpret_cast<PBYTE>(&value), &bytes, 0);

            if (cr != CR_SUCCESS || propType != expectedType || bytes != sizeof(value))
            {
                return std::nullopt;
            }

            return value;
        }

        // ---- Interface wrappers -----------------------------------------------
        winrt::hstring ReadInterfaceString(PCWSTR ifaceId, DEVPROPKEY const& key) noexcept
        {
            if (ifaceId == nullptr) return {};

            DEVPROPTYPE propType{};
            std::vector<BYTE> buffer;

            bool ok = ReadVariableSizeProperty(
                DEVPROP_TYPE_STRING,
                [&](PBYTE buf, PULONG size) -> CONFIGRET
                {
                    return CM_Get_Device_Interface_PropertyW(ifaceId, &key, &propType, buf, size, 0);
                },
                buffer,
                propType);

            if (!ok) return {};

            return HStringFromRawBuffer(
                reinterpret_cast<const wchar_t*>(buffer.data()),
                buffer.size());
        }

        std::vector<winrt::hstring> ReadInterfaceStringList(PCWSTR ifaceId, DEVPROPKEY const& key) noexcept
        {
            if (ifaceId == nullptr) return {};

            DEVPROPTYPE propType{};
            std::vector<BYTE> buffer;

            bool ok = ReadVariableSizeProperty(
                DEVPROP_TYPE_STRING_LIST,
                [&](PBYTE buf, PULONG size) -> CONFIGRET
                {
                    return CM_Get_Device_Interface_PropertyW(ifaceId, &key, &propType, buf, size, 0);
                },
                buffer,
                propType);

            if (!ok) return {};

            return SplitMultiString(
                reinterpret_cast<const wchar_t*>(buffer.data()),
                buffer.size());
        }

        template <typename T>
        std::optional<T> ReadInterfaceFixed(PCWSTR ifaceId, DEVPROPKEY const& key, DEVPROPTYPE expectedType) noexcept
        {
            if (ifaceId == nullptr) return std::nullopt;

            DEVPROPTYPE propType{};
            T value{};
            ULONG bytes = sizeof(value);

            CONFIGRET cr = CM_Get_Device_Interface_PropertyW(
                ifaceId, &key, &propType, reinterpret_cast<PBYTE>(&value), &bytes, 0);

            if (cr != CR_SUCCESS || propType != expectedType || bytes != sizeof(value))
            {
                return std::nullopt;
            }

            return value;
        }

        // ---- Case-insensitive wide compare ------------------------------------
        bool EqualsCaseInsensitive(std::wstring_view a, std::wstring_view b) noexcept
        {
            if (a.size() != b.size()) return false;
            return ::CompareStringOrdinal(
                a.data(), static_cast<int>(a.size()),
                b.data(), static_cast<int>(b.size()),
                TRUE) == CSTR_EQUAL;
        }
    }

    // ============================================================
    // Factories
    // ============================================================

    _Use_decl_annotations_
    std::optional<MidiPnpDeviceInfo>
    MidiPnpDeviceInfo::CreateFromInterfaceId(std::wstring_view symbolicLink) noexcept
    {
        if (symbolicLink.empty()) return std::nullopt;

        std::wstring ifaceId = NullTerminate(symbolicLink);

        // Look up the owning device instance id via the interface property store.
        DEVPROPTYPE propType{};
        wchar_t instanceIdBuf[MAX_DEVICE_ID_LEN]{};
        ULONG bytes = sizeof(instanceIdBuf);

        CONFIGRET cr = CM_Get_Device_Interface_PropertyW(
            ifaceId.c_str(),
            &DEVPKEY_Device_InstanceId,
            &propType,
            reinterpret_cast<PBYTE>(instanceIdBuf),
            &bytes,
            0);

        if (cr != CR_SUCCESS || propType != DEVPROP_TYPE_STRING || bytes < sizeof(wchar_t))
        {
            return std::nullopt;
        }

        std::wstring instanceId{
            instanceIdBuf,
            (bytes / sizeof(wchar_t)) - 1   // strip trailing null
        };

        // Locate the devnode. Try present first, fall back to phantom.
        DEVINST devInst{};
        cr = CM_Locate_DevNodeW(
            &devInst,
            const_cast<DEVINSTID_W>(instanceId.c_str()),
            CM_LOCATE_DEVNODE_NORMAL);

        if (cr == CR_NO_SUCH_DEVNODE)
        {
            cr = CM_Locate_DevNodeW(
                &devInst,
                const_cast<DEVINSTID_W>(instanceId.c_str()),
                CM_LOCATE_DEVNODE_PHANTOM);
        }

        if (cr != CR_SUCCESS)
        {
            return std::nullopt;
        }

        MidiPnpDeviceInfo info;
        info.m_interfaceId = winrt::hstring{ ifaceId };
        info.m_instanceId  = winrt::hstring{ instanceId };
        info.m_devInst     = devInst;
        return info;
    }

    _Use_decl_annotations_
    std::optional<MidiPnpDeviceInfo>
    MidiPnpDeviceInfo::CreateFromInstanceId(std::wstring_view instanceId) noexcept
    {
        if (instanceId.empty()) return std::nullopt;

        std::wstring nullTerminated = NullTerminate(instanceId);

        DEVINST devInst{};
        CONFIGRET cr = CM_Locate_DevNodeW(
            &devInst,
            const_cast<DEVINSTID_W>(nullTerminated.c_str()),
            CM_LOCATE_DEVNODE_NORMAL);

        if (cr == CR_NO_SUCH_DEVNODE)
        {
            cr = CM_Locate_DevNodeW(
                &devInst,
                const_cast<DEVINSTID_W>(nullTerminated.c_str()),
                CM_LOCATE_DEVNODE_PHANTOM);
        }

        if (cr != CR_SUCCESS)
        {
            return std::nullopt;
        }

        MidiPnpDeviceInfo info;
        info.m_instanceId = winrt::hstring{ nullTerminated };
        info.m_devInst    = devInst;
        return info;
    }

    _Use_decl_annotations_
    std::vector<MidiPnpDeviceInfo>
    MidiPnpDeviceInfo::FindAllByInterfaceClass(GUID const& interfaceClass, bool presentOnly) noexcept
    {
        std::vector<MidiPnpDeviceInfo> results;

        ULONG flags = presentOnly
            ? CM_GET_DEVICE_INTERFACE_LIST_PRESENT
            : CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES;

        GUID classGuid = interfaceClass;
        ULONG listChars = 0;

        CONFIGRET cr = CM_Get_Device_Interface_List_SizeW(
            &listChars, &classGuid, nullptr, flags);

        if (cr != CR_SUCCESS || listChars <= 1)
        {
            return results;
        }

        std::vector<wchar_t> buffer(listChars);
        cr = CM_Get_Device_Interface_ListW(
            &classGuid,
            nullptr,
            buffer.data(),
            static_cast<ULONG>(buffer.size()),
            flags);

        if (cr != CR_SUCCESS)
        {
            return results;
        }

        const wchar_t* end = buffer.data() + buffer.size();
        const wchar_t* p = buffer.data();
        while (p < end && *p != L'\0')
        {
            size_t remaining = static_cast<size_t>(end - p);
            size_t len = ::wcsnlen(p, remaining);
            if (len == 0)
            {
                break;
            }

            std::wstring_view link{ p, len };
            if (auto info = CreateFromInterfaceId(link))
            {
                results.push_back(std::move(*info));
            }

            if (len >= remaining)
            {
                break;
            }
            p += len + 1;
        }

        return results;
    }

    _Use_decl_annotations_
    std::vector<MidiPnpDeviceInfo>
    MidiPnpDeviceInfo::FindAllByInterfaceClassAndContainerId(
        GUID const& interfaceClass,
        GUID const& containerId,
        bool presentOnly) noexcept
    {
        auto all = FindAllByInterfaceClass(interfaceClass, presentOnly);

        winrt::guid wanted{ containerId };
        all.erase(
            std::remove_if(all.begin(), all.end(),
                [&](MidiPnpDeviceInfo const& info)
                {
                    auto c = info.ContainerId();
                    return !c.has_value() || c.value() != wanted;
                }),
            all.end());

        return all;
    }

    _Use_decl_annotations_
    std::vector<MidiPnpDeviceInfo>
    MidiPnpDeviceInfo::FindAllByInterfaceClassAndParent(
        GUID const& interfaceClass,
        std::wstring_view parentInstanceId,
        bool presentOnly) noexcept
    {
        auto all = FindAllByInterfaceClass(interfaceClass, presentOnly);

        all.erase(
            std::remove_if(all.begin(), all.end(),
                [&](MidiPnpDeviceInfo const& info)
                {
                    auto parent = info.ParentInstanceId();
                    return !EqualsCaseInsensitive(
                        std::wstring_view{ parent }, parentInstanceId);
                }),
            all.end());

        return all;
    }

    // ============================================================
    // Device (devnode) accessors
    // ============================================================

    _Use_decl_annotations_
    winrt::hstring MidiPnpDeviceInfo::GetDeviceString(DEVPROPKEY const& key) const noexcept
    {
        return ReadDevNodeString(m_devInst, key);
    }

    _Use_decl_annotations_
    std::vector<winrt::hstring> MidiPnpDeviceInfo::GetDeviceStringList(DEVPROPKEY const& key) const noexcept
    {
        return ReadDevNodeStringList(m_devInst, key);
    }

    _Use_decl_annotations_
    std::optional<winrt::guid> MidiPnpDeviceInfo::GetDeviceGuid(DEVPROPKEY const& key) const noexcept
    {
        auto g = ReadDevNodeFixed<GUID>(m_devInst, key, DEVPROP_TYPE_GUID);
        if (!g) return std::nullopt;
        return winrt::guid{ *g };
    }

    _Use_decl_annotations_
    std::optional<uint32_t> MidiPnpDeviceInfo::GetDeviceUInt32(DEVPROPKEY const& key) const noexcept
    {
        return ReadDevNodeFixed<uint32_t>(m_devInst, key, DEVPROP_TYPE_UINT32);
    }

    _Use_decl_annotations_
    std::optional<bool> MidiPnpDeviceInfo::GetDeviceBoolean(DEVPROPKEY const& key) const noexcept
    {
        auto b = ReadDevNodeFixed<DEVPROP_BOOLEAN>(m_devInst, key, DEVPROP_TYPE_BOOLEAN);
        if (!b) return std::nullopt;
        return (*b == DEVPROP_TRUE);
    }

    // ============================================================
    // Interface accessors
    // ============================================================

    _Use_decl_annotations_
    winrt::hstring MidiPnpDeviceInfo::GetInterfaceString(DEVPROPKEY const& key) const noexcept
    {
        if (!HasInterface()) return {};
        return ReadInterfaceString(m_interfaceId.c_str(), key);
    }

    _Use_decl_annotations_
    std::vector<winrt::hstring> MidiPnpDeviceInfo::GetInterfaceStringList(DEVPROPKEY const& key) const noexcept
    {
        if (!HasInterface()) return {};
        return ReadInterfaceStringList(m_interfaceId.c_str(), key);
    }

    _Use_decl_annotations_
    std::optional<winrt::guid> MidiPnpDeviceInfo::GetInterfaceGuid(DEVPROPKEY const& key) const noexcept
    {
        if (!HasInterface()) return std::nullopt;
        auto g = ReadInterfaceFixed<GUID>(m_interfaceId.c_str(), key, DEVPROP_TYPE_GUID);
        if (!g) return std::nullopt;
        return winrt::guid{ *g };
    }

    _Use_decl_annotations_
    std::optional<uint32_t> MidiPnpDeviceInfo::GetInterfaceUInt32(DEVPROPKEY const& key) const noexcept
    {
        if (!HasInterface()) return std::nullopt;
        return ReadInterfaceFixed<uint32_t>(m_interfaceId.c_str(), key, DEVPROP_TYPE_UINT32);
    }

    _Use_decl_annotations_
    std::optional<bool> MidiPnpDeviceInfo::GetInterfaceBoolean(DEVPROPKEY const& key) const noexcept
    {
        if (!HasInterface()) return std::nullopt;
        auto b = ReadInterfaceFixed<DEVPROP_BOOLEAN>(m_interfaceId.c_str(), key, DEVPROP_TYPE_BOOLEAN);
        if (!b) return std::nullopt;
        return (*b == DEVPROP_TRUE);
    }

    // ============================================================
    // Convenience accessors
    // ============================================================

    winrt::hstring MidiPnpDeviceInfo::Name() const noexcept
    {
        // Friendliest name first.
        auto friendly = GetDeviceString(DEVPKEY_Device_FriendlyName);
        if (!friendly.empty()) return friendly;

        auto name = GetDeviceString(DEVPKEY_NAME);
        if (!name.empty()) return name;

        return GetDeviceString(DEVPKEY_Device_DeviceDesc);
    }

    winrt::hstring MidiPnpDeviceInfo::ParentInstanceId() const noexcept
    {
        return GetDeviceString(DEVPKEY_Device_Parent);
    }

    std::optional<winrt::guid> MidiPnpDeviceInfo::ContainerId() const noexcept
    {
        return GetDeviceGuid(DEVPKEY_Device_ContainerId);
    }

    std::optional<winrt::guid> MidiPnpDeviceInfo::InterfaceClassGuid() const noexcept
    {
        return GetInterfaceGuid(DEVPKEY_DeviceInterface_ClassGuid);
    }

    bool MidiPnpDeviceInfo::IsInterfaceEnabled() const noexcept
    {
        auto enabled = GetInterfaceBoolean(DEVPKEY_DeviceInterface_Enabled);
        return enabled.value_or(false);
    }
}