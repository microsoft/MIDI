// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// All field names here in case you want to parse the file. Just include or
// copy these definitions into your code

#define MIDIDIAG_PRODUCT_NAME                                            L"Microsoft Windows MIDI Services - Diagnostics Report"

#define MIDIDIAG_MAX_FIELD_LABEL_WIDTH                                   36


#define MIDIDIAG_FIELD_LABEL_FILE_VERSION                                L"file_version"


#define MIDIDIAG_HEADER_FIELD_LABEL_VERSION_BUILD_SOURCE                 L"ver_build_source"
#define MIDIDIAG_HEADER_FIELD_LABEL_VERSION_NAME                         L"ver_build_name"
#define MIDIDIAG_HEADER_FIELD_LABEL_VERSION_FULL                         L"ver_build_full"

#define MIDIDIAG_FIELD_SEPARATOR                                         L" : "
#define MIDIDIAG_SECTION_HEADER_SEPARATOR_CHAR                           L'='
#define MIDIDIAG_ITEM_SEPARATOR_CHAR                                     L'-'
#define MIDIDIAG_SEPARATOR_REPEATING_CHAR_COUNT_PER_LINE                 120

#define MIDIDIAG_FIELD_LABEL_ERROR                                       L"ERROR"

#define MIDIDIAG_SECTION_LABEL_FEATURE_ENABLEMENT                        L"feature_enablement"
#define MIDIDIAG_FIELD_LABEL_ENABLED_FEATURE                             L"enabled_feature"
#define MIDIDIAG_FIELD_LABEL_DISABLED_FEATURE                            L"disabled_feature"


#define MIDIDIAG_SECTION_LABEL_ENUM_REGISTRY                             L"enum_registry"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_VALUE                              L"transport_id"

#define MIDIDIAG_FIELD_LABEL_REGISTRY_ROOT                               L"reg_root"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_ROOT_DISCOVERY_ENABLED             L"reg_discovery_enabled"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_ROOT_DISCOVERY_TIMEOUT             L"reg_discovery_timeout_ms"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_ROOT_USE_MMCSS                     L"reg_use_mmcss"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_ROOT_CURRENT_CONFIG                L"reg_current_config"

#define MIDIDIAG_SECTION_DEV_MODE                                        L"dev_mode"
#define MIDIDIAG_FIELD_LABEL_DEV_MODE_ENABLED                            L"dev_mode_enabled"

#define MIDIDIAG_FIELD_LABEL_REGISTRY_SDK_INSTALLED                      L"reg_sdk_installed"

#define MIDIDIAG_FIELD_LABEL_REGISTRY_MIDISRV_EXENAME                    L"reg_midisrv_exe"

#define MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORTS                         L"reg_transports"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT_NAME                     L"reg_transport_name"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT_CLSID                    L"reg_transport_clsid"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT_ENABLED                  L"reg_transport_enabled"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT_DLLNAME                  L"reg_transport_dll"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT_PROGID                   L"reg_transport_progid"

#define MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSFORMS                         L"reg_transforms"

#define MIDIDIAG_SECTION_LABEL_ENUM_REGISTRY_DRIVERS32                   L"reg_drivers32"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_DRIVERS32_ENTRY                    L"reg_drivers32_entry"

#define MIDIDIAG_SECTION_LABEL_ENUM_REGISTRY_DRIVERS32WOW                L"reg_drivers32wow"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_DRIVERS32WOW_ENTRY                 L"reg_drivers32wow_entry"

#define MIDIDIAG_SECTION_LABEL_ENUM_COM                                  L"enum_com"


#define MIDIDIAG_SECTION_LABEL_ENUM_TRANSPORTS                           L"enum_transports"
#define MIDIDIAG_FIELD_LABEL_TRANSPORT_ID                                L"transport_id"
#define MIDIDIAG_FIELD_LABEL_TRANSPORT_NAME                              L"name"
#define MIDIDIAG_FIELD_LABEL_TRANSPORT_CODE                              L"transport_code"
#define MIDIDIAG_FIELD_LABEL_TRANSPORT_VERSION                           L"version"
#define MIDIDIAG_FIELD_LABEL_TRANSPORT_AUTHOR                            L"author"
#define MIDIDIAG_FIELD_LABEL_TRANSPORT_DESCRIPTION                       L"description"

#define MIDIDIAG_SECTION_LABEL_MIDI2_API_ENDPOINTS                       L"enum_ump_api_endpoints"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_ID                           L"endpoint_device_id"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_NAME                         L"name"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_TRANSPORT_CODE               L"transport_code"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_USER_SUPPLIED_NAME           L"name_user_supplied"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_ENDPOINT_SUPPLIED_NAME       L"name_endpoint_supplied"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_TRANSPORT_SUPPLIED_NAME      L"name_transport_supplied"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_TRANSPORT_SUPPLIED_DESC      L"desc_transport_supplied"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_USER_SUPPLIED_DESC           L"desc_user_supplied"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_ID                    L"parent_id"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_NAME                  L"parent_name"

#define MIDIDIAG_FIELD_LABEL_GTB_NUMBER                                  L"gtb_number"
#define MIDIDIAG_FIELD_LABEL_GTB_NAME                                    L"gtb_name"
#define MIDIDIAG_FIELD_LABEL_GTB_FIRST_GROUP                             L"gtb_first_group_number"
#define MIDIDIAG_FIELD_LABEL_GTB_GROUP_COUNT                             L"gtb_group_count"
#define MIDIDIAG_FIELD_LABEL_GTB_DIRECTION                               L"gtb_direction"

#define MIDIDIAG_FIELD_LABEL_REG_DEFAULT_MIDI1_NAME_TABLE_SELECTION      L"reg_default_midi1_winmm_naming"
#define MIDIDIAG_FIELD_LABEL_REG_DEFAULT_MIDI2_UMP_NAME_TABLE_SELECTION  L"reg_default_midi2_ump_winmm_naming"
#define MIDIDIAG_FIELD_LABEL_REG_DEFAULT_MIDI1_UMP_NAME_TABLE_SELECTION  L"reg_default_midi1_ump_winmm_naming"

#define MIDIDIAG_FIELD_LABEL_NAME_TABLE_SELECTION                        L"name_table_selection"

#define MIDIDIAG_FIELD_LABEL_NAME_TABLE_GROUP_NUMBER                     L"name_table_group_number"
#define MIDIDIAG_FIELD_LABEL_NAME_TABLE_DATA_FLOW                        L"name_table_data_flow"
#define MIDIDIAG_FIELD_LABEL_NAME_TABLE_CUSTOM_NAME                      L"name_table_custom_name"
#define MIDIDIAG_FIELD_LABEL_NAME_TABLE_LEGACY_WINMM_NAME                L"name_table_legacy_winmm_name"
#define MIDIDIAG_FIELD_LABEL_NAME_TABLE_GTB_NAME                         L"name_table_gtb_name"
#define MIDIDIAG_FIELD_LABEL_NAME_TABLE_FILTER_PLUS_GTB_NAME             L"name_table_filter_plus_gtb_name"
#define MIDIDIAG_FIELD_LABEL_NAME_TABLE_PIN_NAME                         L"name_table_pin_name"
#define MIDIDIAG_FIELD_LABEL_NAME_TABLE_FILTER_PLUS_PIN_NAME             L"name_table_filter_plus_pin_name"

#define MIDIDIAG_FIELD_LABEL_MIDI1_PORT_IN                               L"midi_in_port"
#define MIDIDIAG_FIELD_LABEL_MIDI1_PORT_OUT                              L"midi_out_port"


#define MIDIDIAG_SECTION_LABEL_MIDI1_API_INPUT_ENDPOINTS                 L"enum_winrt_midi1_api_input_ports"
#define MIDIDIAG_SECTION_LABEL_MIDI1_API_OUTPUT_ENDPOINTS                L"enum_winrt_midi1_api_output_ports"
#define MIDIDIAG_FIELD_LABEL_MIDI1_ENDPOINT_ID                           L"endpoint_device_id"
#define MIDIDIAG_FIELD_LABEL_MIDI1_ENDPOINT_NAME                         L"name"

#define MIDIDIAG_SECTION_LABEL_WINMM_API_INPUT_ENDPOINTS                 L"enum_winmm_midi1_api_input_ports"
#define MIDIDIAG_SECTION_LABEL_WINMM_API_OUTPUT_ENDPOINTS                L"enum_winmm_midi1_api_output_ports"
#define MIDIDIAG_FIELD_LABEL_WINMM_ENDPOINT_ID                           L"endpoint_index"
#define MIDIDIAG_FIELD_LABEL_WINMM_ENDPOINT_NAME                         L"name"
#define MIDIDIAG_FIELD_LABEL_WINMM_ENDPOINT_COUNT                        L"endpoint_count"
#define MIDIDIAG_FIELD_LABEL_WINMM_ERROR_COUNT                           L"dev_caps_error_count"


#define MIDIDIAG_SECTION_LABEL_PING_TEST                                 L"ping_test"
#define MIDIDIAG_FIELD_LABEL_PING_ATTEMPT_COUNT                          L"ping_attempt_count"
#define MIDIDIAG_FIELD_LABEL_PING_RETURN_COUNT                           L"ping_return_count"
#define MIDIDIAG_FIELD_LABEL_PING_ROUND_TRIP_TOTAL_TICKS                 L"round_trip_total_ticks"
#define MIDIDIAG_FIELD_LABEL_PING_ROUND_TRIP_AVERAGE_TICKS               L"round_trip_average_ticks"
#define MIDIDIAG_FIELD_LABEL_PING_FAILURE_REASON                         L"ping_failure_reason"

#define MIDIDIAG_SECTION_LABEL_MIDI_CLOCK                                L"midi_clock"
#define MIDIDIAG_FIELD_LABEL_CLOCK_FREQUENCY                             L"clock_frequency"
#define MIDIDIAG_FIELD_LABEL_CLOCK_NOW                                   L"clock_now"

#define MIDIDIAG_SECTION_LABEL_SERVICE_STATUS                            L"service_status"
#define MIDIDIAG_FIELD_LABEL_SERVICE_AVAILABLE                           L"available"

#define MIDIDIAG_SECTION_LABEL_SDK_STATUS                                L"sdk_runtime_status"
#define MIDIDIAG_FIELD_LABEL_SDK_INITIALIZED                             L"sdk_runtime_initialized"

#define MIDIDIAG_SECTION_LABEL_OS                                        L"os"
#define MIDIDIAG_FIELD_LABEL_OS_VERSION                                  L"os_version"

#define MIDIDIAG_SECTION_LABEL_PROCESSOR_ENV                             L"processor_env"
#define MIDIDIAG_SECTION_LABEL_NATIVE_SYSTEM_INFO                        L"native_system_info"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_ARCH                  L"processor_architecture"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_LEVEL                 L"processor_level"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_REVISION              L"processor_revision"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_EMULATION             L"running_emulated"


#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMECAPS_ERROR                 L"timecaps_error"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMECAPS_MIN_PERIOD            L"timecaps_min_period"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMECAPS_MAX_PERIOD            L"timecaps_max_period"

#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMER_RESOLUTION_MIN_MS        L"timer_resolution_min"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMER_RESOLUTION_MAX_MS        L"timer_resolution_max"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMER_RESOLUTION_CURRENT_MS    L"timer_resolution_current"


#define MIDIDIAG_SECTION_LABEL_HEADER                                   L"header"
#define MIDIDIAG_FIELD_LABEL_CURRENT_TIME                               L"current_time"

#define MIDIDIAG_SECTION_LABEL_END_OF_FILE                              L"end_of_file"

