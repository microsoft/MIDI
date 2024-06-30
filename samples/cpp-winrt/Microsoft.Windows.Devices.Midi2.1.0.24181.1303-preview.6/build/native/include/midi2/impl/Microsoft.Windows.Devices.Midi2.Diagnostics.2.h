// WARNING: Please don't edit this file. It was generated by C++/WinRT v2.0.240405.15

#pragma once
#ifndef WINRT_Microsoft_Windows_Devices_Midi2_Diagnostics_2_H
#define WINRT_Microsoft_Windows_Devices_Midi2_Diagnostics_2_H
#include "winrt/impl/Windows.Foundation.2.h"
#include "winrt/impl/Microsoft.Windows.Devices.Midi2.Diagnostics.1.h"
WINRT_EXPORT namespace winrt::Microsoft::Windows::Devices::Midi2::Diagnostics
{
    struct MidiServiceMessageProcessingPluginInfo
    {
        winrt::guid Id;
        hstring Name;
        hstring Description;
        hstring Author;
        hstring SmallImagePath;
        hstring Version;
        bool SupportsMultipleInstancesPerDevice;
        bool IsSystemManaged;
        bool IsClientConfigurable;
    };
    inline bool operator==(MidiServiceMessageProcessingPluginInfo const& left, MidiServiceMessageProcessingPluginInfo const& right) noexcept
    {
        return left.Id == right.Id && left.Name == right.Name && left.Description == right.Description && left.Author == right.Author && left.SmallImagePath == right.SmallImagePath && left.Version == right.Version && left.SupportsMultipleInstancesPerDevice == right.SupportsMultipleInstancesPerDevice && left.IsSystemManaged == right.IsSystemManaged && left.IsClientConfigurable == right.IsClientConfigurable;
    }
    inline bool operator!=(MidiServiceMessageProcessingPluginInfo const& left, MidiServiceMessageProcessingPluginInfo const& right) noexcept
    {
        return !(left == right);
    }
    struct MidiServicePingResponse
    {
        uint32_t SourceId;
        uint32_t Index;
        uint64_t ClientSendMidiTimestamp;
        uint64_t ServiceReportedMidiTimestamp;
        uint64_t ClientReceiveMidiTimestamp;
        uint64_t ClientDeltaTimestamp;
    };
    inline bool operator==(MidiServicePingResponse const& left, MidiServicePingResponse const& right) noexcept
    {
        return left.SourceId == right.SourceId && left.Index == right.Index && left.ClientSendMidiTimestamp == right.ClientSendMidiTimestamp && left.ServiceReportedMidiTimestamp == right.ServiceReportedMidiTimestamp && left.ClientReceiveMidiTimestamp == right.ClientReceiveMidiTimestamp && left.ClientDeltaTimestamp == right.ClientDeltaTimestamp;
    }
    inline bool operator!=(MidiServicePingResponse const& left, MidiServicePingResponse const& right) noexcept
    {
        return !(left == right);
    }
    struct MidiServiceSessionConnectionInfo
    {
        hstring EndpointDeviceId;
        uint16_t InstanceCount;
        winrt::Windows::Foundation::DateTime EarliestConnectionTime;
    };
    inline bool operator==(MidiServiceSessionConnectionInfo const& left, MidiServiceSessionConnectionInfo const& right) noexcept
    {
        return left.EndpointDeviceId == right.EndpointDeviceId && left.InstanceCount == right.InstanceCount && left.EarliestConnectionTime == right.EarliestConnectionTime;
    }
    inline bool operator!=(MidiServiceSessionConnectionInfo const& left, MidiServiceSessionConnectionInfo const& right) noexcept
    {
        return !(left == right);
    }
    struct MidiServiceTransportPluginInfo
    {
        winrt::guid Id;
        hstring Name;
        hstring Abbreviation;
        hstring Description;
        hstring SmallImagePath;
        hstring Author;
        hstring Version;
        bool IsRuntimeCreatableByApps;
        bool IsRuntimeCreatableBySettings;
        bool IsSystemManaged;
        bool CanConfigure;
    };
    inline bool operator==(MidiServiceTransportPluginInfo const& left, MidiServiceTransportPluginInfo const& right) noexcept
    {
        return left.Id == right.Id && left.Name == right.Name && left.Abbreviation == right.Abbreviation && left.Description == right.Description && left.SmallImagePath == right.SmallImagePath && left.Author == right.Author && left.Version == right.Version && left.IsRuntimeCreatableByApps == right.IsRuntimeCreatableByApps && left.IsRuntimeCreatableBySettings == right.IsRuntimeCreatableBySettings && left.IsSystemManaged == right.IsSystemManaged && left.CanConfigure == right.CanConfigure;
    }
    inline bool operator!=(MidiServiceTransportPluginInfo const& left, MidiServiceTransportPluginInfo const& right) noexcept
    {
        return !(left == right);
    }
    struct MidiDiagnostics
    {
        MidiDiagnostics() = delete;
        [[nodiscard]] static auto DiagnosticsLoopbackAEndpointDeviceId();
        [[nodiscard]] static auto DiagnosticsLoopbackBEndpointDeviceId();
        static auto PingService(uint8_t pingCount);
        static auto PingService(uint8_t pingCount, uint32_t timeoutMilliseconds);
    };
    struct MidiReporting
    {
        MidiReporting() = delete;
        static auto GetInstalledTransportPlugins();
        static auto GetInstalledMessageProcessingPlugins();
        static auto GetActiveSessions();
    };
    struct WINRT_IMPL_EMPTY_BASES MidiServicePingResponseSummary : winrt::Microsoft::Windows::Devices::Midi2::Diagnostics::IMidiServicePingResponseSummary
    {
        MidiServicePingResponseSummary(std::nullptr_t) noexcept {}
        MidiServicePingResponseSummary(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Microsoft::Windows::Devices::Midi2::Diagnostics::IMidiServicePingResponseSummary(ptr, take_ownership_from_abi) {}
    };
    struct WINRT_IMPL_EMPTY_BASES MidiServiceSessionInfo : winrt::Microsoft::Windows::Devices::Midi2::Diagnostics::IMidiServiceSessionInfo
    {
        MidiServiceSessionInfo(std::nullptr_t) noexcept {}
        MidiServiceSessionInfo(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Microsoft::Windows::Devices::Midi2::Diagnostics::IMidiServiceSessionInfo(ptr, take_ownership_from_abi) {}
    };
}
#endif
