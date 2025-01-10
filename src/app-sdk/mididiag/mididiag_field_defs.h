// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// All field names here in case you want to parse the file. Just include or
// copy these definitions into your code

#define MIDIDIAG_PRODUCT_NAME                                            "Microsoft Windows MIDI Services - Diagnostics Report"

#define MIDIDIAG_MAX_FIELD_LABEL_WIDTH                                   25


#define MIDIDIAG_FIELD_LABEL_FILE_VERSION                                "file_version"


#define MIDIDIAG_HEADER_FIELD_LABEL_VERSION_BUILD_SOURCE                 "ver_build_source"
#define MIDIDIAG_HEADER_FIELD_LABEL_VERSION_NAME                         "ver_build_name"
#define MIDIDIAG_HEADER_FIELD_LABEL_VERSION_FULL                         "ver_build_full"

#define MIDIDIAG_FIELD_SEPARATOR                                         " : "
#define MIDIDIAG_SECTION_HEADER_SEPARATOR_CHAR                           '='
#define MIDIDIAG_ITEM_SEPARATOR_CHAR                                     '-'
#define MIDIDIAG_SEPARATOR_REPEATING_CHAR_COUNT_PER_LINE                 120

#define MIDIDIAG_FIELD_LABEL_ERROR                                       "ERROR"

#define MIDIDIAG_SECTION_LABEL_ENUM_REGISTRY                             "enum_registry"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_VALUE                              "transport_id"

#define MIDIDIAG_FIELD_LABEL_REGISTRY_ROOT                               "reg_root"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_ROOT_DISCOVERY_ENABLED             "reg_discovery_enabled"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_ROOT_USE_MMCSS                     "reg_use_mmcss"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_ROOT_CURRENT_CONFIG                "reg_current_config"


#define MIDIDIAG_FIELD_LABEL_REGISTRY_SDK_INSTALLED                      "reg_sdk_installed"

#define MIDIDIAG_FIELD_LABEL_REGISTRY_MIDISRV_EXENAME                    "reg_midisrv_exe"

#define MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORTS                         "reg_transports"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT_NAME                     "reg_transport_name"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT_CLSID                    "reg_transport_clsid"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT_ENABLED                  "reg_transport_enabled"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT_DLLNAME                  "reg_transport_dll"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT_PROGID                   "reg_transport_progid"

#define MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSFORMS                         "reg_transforms"

#define MIDIDIAG_SECTION_LABEL_ENUM_REGISTRY_DRIVERS32                   "reg_drivers32"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_DRIVERS32_ENTRY                    "reg_drivers32_entry"

#define MIDIDIAG_SECTION_LABEL_ENUM_COM                                  "enum_com"


#define MIDIDIAG_SECTION_LABEL_ENUM_TRANSPORTS                           "enum_transports"
#define MIDIDIAG_FIELD_LABEL_TRANSPORT_ID                                "transport_id"
#define MIDIDIAG_FIELD_LABEL_TRANSPORT_NAME                              "name"
#define MIDIDIAG_FIELD_LABEL_TRANSPORT_CODE                              "transport_code"
#define MIDIDIAG_FIELD_LABEL_TRANSPORT_VERSION                           "version"
#define MIDIDIAG_FIELD_LABEL_TRANSPORT_AUTHOR                            "author"
#define MIDIDIAG_FIELD_LABEL_TRANSPORT_DESCRIPTION                       "description"

#define MIDIDIAG_SECTION_LABEL_MIDI2_API_ENDPOINTS                       "enum_ump_api_endpoints"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_ID                           "endpoint_device_id"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_NAME                         "name"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_TRANSPORT_CODE               "transport_code"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_USER_SUPPLIED_NAME           "name_user_supplied"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_ENDPOINT_SUPPLIED_NAME       "name_endpoint_supplied"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_TRANSPORT_SUPPLIED_NAME      "name_transport_supplied"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_TRANSPORT_SUPPLIED_DESC      "desc_transport_supplied"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_USER_SUPPLIED_DESC           "desc_user_supplied"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_ID                    "parent_id"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_NAME                  "parent_name"

#define MIDIDIAG_SECTION_LABEL_MIDI1_API_INPUT_ENDPOINTS                 "enum_winrt_midi1_api_input_ports"
#define MIDIDIAG_SECTION_LABEL_MIDI1_API_OUTPUT_ENDPOINTS                "enum_winrt_midi1_api_output_ports"
#define MIDIDIAG_FIELD_LABEL_MIDI1_ENDPOINT_ID                           "endpoint_device_id"
#define MIDIDIAG_FIELD_LABEL_MIDI1_ENDPOINT_NAME                         "name"

#define MIDIDIAG_SECTION_LABEL_WINMM_API_INPUT_ENDPOINTS                 "enum_winmm_midi1_api_input_ports"
#define MIDIDIAG_SECTION_LABEL_WINMM_API_OUTPUT_ENDPOINTS                "enum_winmm_midi1_api_output_ports"
#define MIDIDIAG_FIELD_LABEL_WINMM_ENDPOINT_ID                           "endpoint_index"
#define MIDIDIAG_FIELD_LABEL_WINMM_ENDPOINT_NAME                         "name"
#define MIDIDIAG_FIELD_LABEL_WINMM_ENDPOINT_COUNT                        "endpoint_count"
#define MIDIDIAG_FIELD_LABEL_WINMM_ERROR_COUNT                           "dev_caps_error_count"


#define MIDIDIAG_SECTION_LABEL_PING_TEST                                 "ping_test"
#define MIDIDIAG_FIELD_LABEL_PING_ATTEMPT_COUNT                          "ping_attempt_count"
#define MIDIDIAG_FIELD_LABEL_PING_RETURN_COUNT                           "ping_return_count"
#define MIDIDIAG_FIELD_LABEL_PING_ROUND_TRIP_TOTAL_TICKS                 "round_trip_total_ticks"
#define MIDIDIAG_FIELD_LABEL_PING_ROUND_TRIP_AVERAGE_TICKS               "round_trip_average_ticks"
#define MIDIDIAG_FIELD_LABEL_PING_FAILURE_REASON                         "ping_failure_reason"

#define MIDIDIAG_SECTION_LABEL_MIDI_CLOCK                                "midi_clock"
#define MIDIDIAG_FIELD_LABEL_CLOCK_FREQUENCY                             "clock_frequency"
#define MIDIDIAG_FIELD_LABEL_CLOCK_NOW                                   "clock_now"

#define MIDIDIAG_SECTION_LABEL_SERVICE_STATUS                            "service_status"
#define MIDIDIAG_FIELD_LABEL_SERVICE_AVAILABLE                           "available"

#define MIDIDIAG_SECTION_LABEL_SDK_STATUS                                "sdk_runtime_status"
#define MIDIDIAG_FIELD_LABEL_SDK_INITIALIZED                             "sdk_runtime_initialized"

#define MIDIDIAG_SECTION_LABEL_OS                                        "os"
#define MIDIDIAG_FIELD_LABEL_OS_VERSION                                  "os_version"

#define MIDIDIAG_SECTION_LABEL_PROCESSOR_ENV                             "processor_env"
#define MIDIDIAG_SECTION_LABEL_NATIVE_SYSTEM_INFO                        "native_system_info"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_ARCH                  "processor_architecture"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_LEVEL                 "processor_level"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_REVISION              "processor_revision"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_EMULATION             "running_emulated"


#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMECAPS_ERROR                 "timecaps_error"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMECAPS_MIN_PERIOD            "timecaps_min_period"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMECAPS_MAX_PERIOD            "timecaps_max_period"

#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMER_RESOLUTION_MIN_MS        "timer_resolution_min"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMER_RESOLUTION_MAX_MS        "timer_resolution_max"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMER_RESOLUTION_CURRENT_MS    "timer_resolution_current"


#define MIDIDIAG_SECTION_LABEL_HEADER                                   "header"
#define MIDIDIAG_FIELD_LABEL_CURRENT_TIME                               "current_time"

#define MIDIDIAG_SECTION_LABEL_END_OF_FILE                              "end_of_file"

