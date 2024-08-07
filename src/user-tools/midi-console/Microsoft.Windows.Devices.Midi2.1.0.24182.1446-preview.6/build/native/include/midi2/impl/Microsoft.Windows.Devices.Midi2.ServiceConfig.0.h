// WARNING: Please don't edit this file. It was generated by C++/WinRT v2.0.240405.15

#pragma once
#ifndef WINRT_Microsoft_Windows_Devices_Midi2_ServiceConfig_0_H
#define WINRT_Microsoft_Windows_Devices_Midi2_ServiceConfig_0_H
WINRT_EXPORT namespace winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig
{
    enum class MidiServiceConfigResponseStatus : int32_t
    {
        Success = 0,
        ErrorTargetNotFound = 404,
        ErrorConfigJsonNullOrEmpty = 600,
        ErrorProcessingConfigJson = 601,
        ErrorProcessingResponseJson = 605,
        ErrorNotImplemented = 2600,
    };
    struct IMidiServiceConfig;
    struct IMidiServiceConfigStatics;
    struct IMidiServiceMessageProcessingPluginConfig;
    struct IMidiServiceTransportPluginConfig;
    struct MidiServiceConfig;
    struct MidiServiceConfigResponse;
}
namespace winrt::impl
{
    template <> struct category<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::IMidiServiceConfig>{ using type = interface_category; };
    template <> struct category<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::IMidiServiceConfigStatics>{ using type = interface_category; };
    template <> struct category<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::IMidiServiceMessageProcessingPluginConfig>{ using type = interface_category; };
    template <> struct category<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::IMidiServiceTransportPluginConfig>{ using type = interface_category; };
    template <> struct category<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::MidiServiceConfig>{ using type = class_category; };
    template <> struct category<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::MidiServiceConfigResponseStatus>{ using type = enum_category; };
    template <> struct category<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::MidiServiceConfigResponse>{ using type = struct_category<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::MidiServiceConfigResponseStatus, hstring>; };
    template <> inline constexpr auto& name_v<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::MidiServiceConfig> = L"Microsoft.Windows.Devices.Midi2.ServiceConfig.MidiServiceConfig";
    template <> inline constexpr auto& name_v<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::MidiServiceConfigResponseStatus> = L"Microsoft.Windows.Devices.Midi2.ServiceConfig.MidiServiceConfigResponseStatus";
    template <> inline constexpr auto& name_v<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::MidiServiceConfigResponse> = L"Microsoft.Windows.Devices.Midi2.ServiceConfig.MidiServiceConfigResponse";
    template <> inline constexpr auto& name_v<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::IMidiServiceConfig> = L"Microsoft.Windows.Devices.Midi2.ServiceConfig.IMidiServiceConfig";
    template <> inline constexpr auto& name_v<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::IMidiServiceConfigStatics> = L"Microsoft.Windows.Devices.Midi2.ServiceConfig.IMidiServiceConfigStatics";
    template <> inline constexpr auto& name_v<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::IMidiServiceMessageProcessingPluginConfig> = L"Microsoft.Windows.Devices.Midi2.ServiceConfig.IMidiServiceMessageProcessingPluginConfig";
    template <> inline constexpr auto& name_v<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::IMidiServiceTransportPluginConfig> = L"Microsoft.Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig";
    template <> inline constexpr guid guid_v<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::IMidiServiceConfig>{ 0x0F1E4863,0xC76E,0x501F,{ 0x97,0x6A,0xDB,0x48,0xFC,0x0C,0x5B,0xB7 } }; // 0F1E4863-C76E-501F-976A-DB48FC0C5BB7
    template <> inline constexpr guid guid_v<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::IMidiServiceConfigStatics>{ 0x20C5F99A,0x741B,0x513B,{ 0x86,0x55,0xAC,0x13,0x2F,0x05,0x16,0x6B } }; // 20C5F99A-741B-513B-8655-AC132F05166B
    template <> inline constexpr guid guid_v<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::IMidiServiceMessageProcessingPluginConfig>{ 0x2EBCFA13,0x585A,0x4376,{ 0x8F,0xE1,0x63,0x57,0x84,0xFA,0x7F,0xD4 } }; // 2EBCFA13-585A-4376-8FE1-635784FA7FD4
    template <> inline constexpr guid guid_v<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::IMidiServiceTransportPluginConfig>{ 0xB2417DDE,0xEF35,0x499B,{ 0xA8,0x9B,0x0A,0x4C,0x32,0xCC,0x69,0x9A } }; // B2417DDE-EF35-499B-A89B-0A4C32CC699A
    template <> struct default_interface<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::MidiServiceConfig>{ using type = winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::IMidiServiceConfig; };
    template <> struct abi<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::IMidiServiceConfig>
    {
        struct WINRT_IMPL_NOVTABLE type : inspectable_abi
        {
        };
    };
    template <> struct abi<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::IMidiServiceConfigStatics>
    {
        struct WINRT_IMPL_NOVTABLE type : inspectable_abi
        {
            virtual int32_t __stdcall UpdateTransportPluginConfig(void*, struct struct_Microsoft_Windows_Devices_Midi2_ServiceConfig_MidiServiceConfigResponse*) noexcept = 0;
            virtual int32_t __stdcall UpdateProcessingPluginConfig(void*, struct struct_Microsoft_Windows_Devices_Midi2_ServiceConfig_MidiServiceConfigResponse*) noexcept = 0;
        };
    };
    template <> struct abi<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::IMidiServiceMessageProcessingPluginConfig>
    {
        struct WINRT_IMPL_NOVTABLE type : inspectable_abi
        {
            virtual int32_t __stdcall get_EndpointDeviceId(void**) noexcept = 0;
            virtual int32_t __stdcall get_MessageProcessingPluginId(winrt::guid*) noexcept = 0;
            virtual int32_t __stdcall get_PluginInstanceId(winrt::guid*) noexcept = 0;
            virtual int32_t __stdcall get_IsFromCurrentConfigFile(bool*) noexcept = 0;
            virtual int32_t __stdcall GetConfigJson(void**) noexcept = 0;
        };
    };
    template <> struct abi<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::IMidiServiceTransportPluginConfig>
    {
        struct WINRT_IMPL_NOVTABLE type : inspectable_abi
        {
            virtual int32_t __stdcall get_TransportId(winrt::guid*) noexcept = 0;
            virtual int32_t __stdcall get_IsFromCurrentConfigFile(bool*) noexcept = 0;
            virtual int32_t __stdcall GetConfigJson(void**) noexcept = 0;
        };
    };
    template <typename D>
    struct consume_Microsoft_Windows_Devices_Midi2_ServiceConfig_IMidiServiceConfig
    {
    };
    template <> struct consume<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::IMidiServiceConfig>
    {
        template <typename D> using type = consume_Microsoft_Windows_Devices_Midi2_ServiceConfig_IMidiServiceConfig<D>;
    };
    template <typename D>
    struct consume_Microsoft_Windows_Devices_Midi2_ServiceConfig_IMidiServiceConfigStatics
    {
        auto UpdateTransportPluginConfig(winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::IMidiServiceTransportPluginConfig const& configUpdate) const;
        auto UpdateProcessingPluginConfig(winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::IMidiServiceMessageProcessingPluginConfig const& configUpdate) const;
    };
    template <> struct consume<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::IMidiServiceConfigStatics>
    {
        template <typename D> using type = consume_Microsoft_Windows_Devices_Midi2_ServiceConfig_IMidiServiceConfigStatics<D>;
    };
    template <typename D>
    struct consume_Microsoft_Windows_Devices_Midi2_ServiceConfig_IMidiServiceMessageProcessingPluginConfig
    {
        [[nodiscard]] auto EndpointDeviceId() const;
        [[nodiscard]] auto MessageProcessingPluginId() const;
        [[nodiscard]] auto PluginInstanceId() const;
        [[nodiscard]] auto IsFromCurrentConfigFile() const;
        auto GetConfigJson() const;
    };
    template <> struct consume<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::IMidiServiceMessageProcessingPluginConfig>
    {
        template <typename D> using type = consume_Microsoft_Windows_Devices_Midi2_ServiceConfig_IMidiServiceMessageProcessingPluginConfig<D>;
    };
    template <typename D>
    struct consume_Microsoft_Windows_Devices_Midi2_ServiceConfig_IMidiServiceTransportPluginConfig
    {
        [[nodiscard]] auto TransportId() const;
        [[nodiscard]] auto IsFromCurrentConfigFile() const;
        auto GetConfigJson() const;
    };
    template <> struct consume<winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::IMidiServiceTransportPluginConfig>
    {
        template <typename D> using type = consume_Microsoft_Windows_Devices_Midi2_ServiceConfig_IMidiServiceTransportPluginConfig<D>;
    };
    struct struct_Microsoft_Windows_Devices_Midi2_ServiceConfig_MidiServiceConfigResponse
    {
        int32_t Status;
        void* ResponseJson;
    };
    template <> struct abi<Microsoft::Windows::Devices::Midi2::ServiceConfig::MidiServiceConfigResponse>
    {
        using type = struct_Microsoft_Windows_Devices_Midi2_ServiceConfig_MidiServiceConfigResponse;
    };
}
#endif
