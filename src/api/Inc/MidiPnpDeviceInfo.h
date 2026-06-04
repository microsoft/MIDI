// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <cfgmgr32.h>
#include <devpropdef.h>
#include <optional>
#include <string_view>
#include <vector>
#include <cstdint>
#include <winrt/base.h>

namespace WindowsMidiServicesInternal
{
    // A synchronous, apartment-agnostic alternative to
    // Windows.Devices.Enumeration.DeviceInformation for the slice of behavior
    // the Windows MIDI Services code actually needs:
    //
    //   * Look up a device by interface symbolic link or device instance id
    //   * Enumerate device interfaces by class GUID
    //   * Read typed properties from either the interface store or the
    //     devnode (Device) store
    //
    // Backed entirely by cfgmgr32, so it cannot deadlock an STA caller and
    // does not round-trip to the PnP service per call. Instances are
    // immutable and trivially copyable after construction.
    class MidiPnpDeviceInfo
    {
    public:
        MidiPnpDeviceInfo() = default;

        // ---- Factories ----

        // Construct from a device interface symbolic link (e.g. "\\?\SWD#...").
        // Resolves the owning devnode via DEVPKEY_Device_InstanceId. Returns
        // std::nullopt if the interface isn't registered or the devnode is gone.
        static std::optional<MidiPnpDeviceInfo>
            CreateFromInterfaceId(_In_ std::wstring_view symbolicLink) noexcept;

        // Construct from a device instance id (e.g. "USB\VID_...&PID_...\6&...").
        // Has no interface id and no interface-store properties.
        static std::optional<MidiPnpDeviceInfo>
            CreateFromInstanceId(_In_ std::wstring_view instanceId) noexcept;

        // Enumerate all device interfaces of the given class. If 'presentOnly'
        // is true (default) only currently-attached interfaces are returned;
        // otherwise both present and phantom interfaces are returned.
        static std::vector<MidiPnpDeviceInfo>
            FindAllByInterfaceClass(
                _In_ GUID const& interfaceClass,
                _In_ bool presentOnly = true) noexcept;

        // Same as FindAllByInterfaceClass, filtered to entries whose devnode
        // has DEVPKEY_Device_ContainerId equal to 'containerId'.
        static std::vector<MidiPnpDeviceInfo>
            FindAllByInterfaceClassAndContainerId(
                _In_ GUID const& interfaceClass,
                _In_ GUID const& containerId,
                _In_ bool presentOnly = true) noexcept;

        // Same as FindAllByInterfaceClass, filtered to entries whose devnode
        // has DEVPKEY_Device_Parent equal to 'parentInstanceId'
        // (case-insensitive compare).
        static std::vector<MidiPnpDeviceInfo>
            FindAllByInterfaceClassAndParent(
                _In_ GUID const& interfaceClass,
                _In_ std::wstring_view parentInstanceId,
                _In_ bool presentOnly = true) noexcept;

        // ---- Identity ----

        winrt::hstring const& InterfaceId() const noexcept { return m_interfaceId; }
        winrt::hstring const& InstanceId()  const noexcept { return m_instanceId;  }
        DEVINST               DevNode()     const noexcept { return m_devInst;     }
        bool                  HasInterface() const noexcept { return !m_interfaceId.empty(); }

        // ---- Device (devnode) property accessors ----
        // String accessors return an empty hstring on missing/wrong-type.
        // Fixed-size accessors return std::nullopt on missing/wrong-type.

        winrt::hstring              GetDeviceString    (_In_ DEVPROPKEY const& key) const noexcept;
        std::vector<winrt::hstring> GetDeviceStringList(_In_ DEVPROPKEY const& key) const noexcept;
        std::optional<winrt::guid>  GetDeviceGuid      (_In_ DEVPROPKEY const& key) const noexcept;
        std::optional<uint32_t>     GetDeviceUInt32    (_In_ DEVPROPKEY const& key) const noexcept;
        std::optional<bool>         GetDeviceBoolean   (_In_ DEVPROPKEY const& key) const noexcept;

        // ---- Interface property accessors ----

        winrt::hstring              GetInterfaceString    (_In_ DEVPROPKEY const& key) const noexcept;
        std::vector<winrt::hstring> GetInterfaceStringList(_In_ DEVPROPKEY const& key) const noexcept;
        std::optional<winrt::guid>  GetInterfaceGuid      (_In_ DEVPROPKEY const& key) const noexcept;
        std::optional<uint32_t>     GetInterfaceUInt32    (_In_ DEVPROPKEY const& key) const noexcept;
        std::optional<bool>         GetInterfaceBoolean   (_In_ DEVPROPKEY const& key) const noexcept;

        // ---- Convenience accessors over common DEVPKEYs ----

        // FriendlyName -> NAME -> DeviceDesc, first non-empty wins.
        winrt::hstring              Name() const noexcept;

        // DEVPKEY_Device_Parent on the devnode.
        winrt::hstring              ParentInstanceId() const noexcept;

        // DEVPKEY_Device_ContainerId on the devnode.
        std::optional<winrt::guid>  ContainerId() const noexcept;

        // DEVPKEY_DeviceInterface_ClassGuid on the interface (requires HasInterface()).
        std::optional<winrt::guid>  InterfaceClassGuid() const noexcept;

        // DEVPKEY_DeviceInterface_Enabled on the interface. Defaults to false
        // for instance-only instances or when the property is missing.
        bool                        IsInterfaceEnabled() const noexcept;

    private:
        winrt::hstring m_interfaceId;   // SymbolicLink, empty if constructed from instance id
        winrt::hstring m_instanceId;
        DEVINST        m_devInst{ 0 };
    };
}