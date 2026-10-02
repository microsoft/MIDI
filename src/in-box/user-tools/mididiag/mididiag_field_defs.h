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

// Report format 2
//
// A section starts with a line that holds MIDIDIAG_SECTION_HEADER_PREFIX and the section label.
// Every other line that is not blank is a field: the field label, padded with spaces to
// MIDIDIAG_MAX_FIELD_LABEL_WIDTH, then MIDIDIAG_FIELD_SEPARATOR, then the value. Labels never
// contain spaces, so the first separator on a line is the one that ends the label. A blank line
// separates the records inside a section, for example one endpoint from the next.
//
// A value that describes one thing in several parts is written as key=value pairs separated by
// single spaces. A value that is empty or contains a space, an equals sign or a double quote is
// put in double quotes, and a double quote inside it is written twice, as in a CSV file. A name
// is usually the last pair on its line.
//
// The format version goes up only when a change could break a parser that follows these rules,
// such as a label that is renamed or removed, or a value that is written in a different form.
// New sections, labels and keys do not change it, so a parser should skip what it does not know.
#define MIDIDIAG_REPORT_FORMAT_VERSION                                   2

#define MIDIDIAG_MAX_FIELD_LABEL_WIDTH                                   30

#define MIDIDIAG_SECTION_LABEL_SUCCESSFUL_RUN                           L"successful_run"     
#define MIDIDIAG_SECTION_LABEL_ABORTED_RUN                              L"aborted_run"     


#define MIDIDIAG_HEADER_FIELD_LABEL_VERSION_BUILD_SOURCE                 L"ver_build_source"
#define MIDIDIAG_HEADER_FIELD_LABEL_VERSION_NAME                         L"ver_build_name"
#define MIDIDIAG_HEADER_FIELD_LABEL_VERSION_FULL                         L"ver_build_full"

#define MIDIDIAG_SECTION_HEADER_PREFIX                                   L"== "
#define MIDIDIAG_FIELD_SEPARATOR                                         L" : "

#define MIDIDIAG_FIELD_LABEL_ERROR                                       L"ERROR"

#define MIDIDIAG_SECTION_LABEL_HEADER                                    L"header"
#define MIDIDIAG_FIELD_LABEL_REPORT_FORMAT_VERSION                       L"report_format_version"
#define MIDIDIAG_FIELD_LABEL_CURRENT_TIME                                L"current_time"
#define MIDIDIAG_FIELD_LABEL_RUNNING_ELEVATED                            L"running_elevated"
#define MIDIDIAG_FIELD_LABEL_OPTIONS                                     L"options"

#define MIDIDIAG_SECTION_LABEL_OS                                        L"os"
#define MIDIDIAG_FIELD_LABEL_OS_VERSION                                  L"os_version"
#define MIDIDIAG_FIELD_LABEL_OS_EDITION                                  L"os_edition"
#define MIDIDIAG_FIELD_LABEL_OS_DISPLAY_VERSION                          L"os_display_version"
#define MIDIDIAG_FIELD_LABEL_OS_BUILD_LAB                                L"os_build_lab"
#define MIDIDIAG_FIELD_LABEL_OS_UI_LANGUAGE                              L"os_ui_language"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_UPTIME                               L"system_uptime"
#define MIDIDIAG_FIELD_LABEL_LAST_BOOT_TIME                              L"last_boot_time"
#define MIDIDIAG_FIELD_LABEL_LAST_BOOT_TYPE                              L"last_boot_type"
#define MIDIDIAG_FIELD_LABEL_FAST_STARTUP_ENABLED                        L"fast_startup_enabled"

#define MIDIDIAG_SECTION_LABEL_PROCESSOR_ENV                             L"processor_env"
#define MIDIDIAG_SECTION_LABEL_NATIVE_SYSTEM_INFO                        L"native_system_info"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_ARCH                  L"processor_architecture"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_LEVEL                 L"processor_level"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_REVISION              L"processor_revision"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_EMULATION             L"running_emulated"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMECAPS_ERROR                  L"timecaps_error"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMECAPS_MIN_PERIOD             L"timecaps_min_period"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMECAPS_MAX_PERIOD             L"timecaps_max_period"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMER_RESOLUTION_MIN_MS         L"timer_resolution_min"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMER_RESOLUTION_MAX_MS         L"timer_resolution_max"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_TIMER_RESOLUTION_CURRENT_MS     L"timer_resolution_current"

#define MIDIDIAG_SECTION_DEV_MODE                                        L"dev_mode"
#define MIDIDIAG_FIELD_LABEL_DEV_MODE_ENABLED                            L"dev_mode_enabled"

#define MIDIDIAG_SECTION_LABEL_API_MODE                                  L"api_mode"
#define MIDIDIAG_FIELD_LABEL_API_MODE                                    L"api_mode"
#define MIDIDIAG_FIELD_LABEL_API_MODE_REGISTRY_VALUE                     L"api_mode_registry_value"
#define MIDIDIAG_FIELD_LABEL_API_MODE_SDK_REPORTED                       L"api_mode_sdk_reported"

#define MIDIDIAG_SECTION_LABEL_COMPONENT_VERSIONS                        L"component_versions"
#define MIDIDIAG_FIELD_LABEL_COMPONENT                                   L"component"

#define MIDIDIAG_SECTION_LABEL_SERVICE_STATUS                            L"service_status"
#define MIDIDIAG_FIELD_LABEL_SERVICE_INSTALLED                           L"service_installed"
#define MIDIDIAG_FIELD_LABEL_SERVICE_STATE_BEFORE_REPORT                 L"service_state_before_report"
#define MIDIDIAG_FIELD_LABEL_SERVICE_START_TYPE                          L"service_start_type"
#define MIDIDIAG_FIELD_LABEL_SERVICE_TRIGGER_COUNT                       L"service_trigger_count"
#define MIDIDIAG_FIELD_LABEL_SERVICE_ACCOUNT                             L"service_account"
#define MIDIDIAG_FIELD_LABEL_SERVICE_PROCESS_ID                          L"service_process_id"
#define MIDIDIAG_FIELD_LABEL_SERVICE_PROCESS                             L"service_process"

#define MIDIDIAG_SECTION_LABEL_SERVICE_HISTORY                           L"service_history"
#define MIDIDIAG_FIELD_LABEL_HISTORY_DAYS                                L"history_days"
#define MIDIDIAG_FIELD_LABEL_SERVICE_EVENT_COUNT                         L"service_event_count"
#define MIDIDIAG_FIELD_LABEL_SERVICE_EVENT                               L"service_event"
#define MIDIDIAG_FIELD_LABEL_MIDI_APP_CRASH_COUNT                        L"midi_app_crash_count"
#define MIDIDIAG_FIELD_LABEL_MIDI_APP_CRASH                              L"midi_app_crash"

#define MIDIDIAG_SECTION_LABEL_FEATURE_ENABLEMENT                        L"feature_enablement"
#define MIDIDIAG_FIELD_LABEL_ENABLED_FEATURE                             L"enabled_feature"
#define MIDIDIAG_FIELD_LABEL_DISABLED_FEATURE                            L"disabled_feature"

#define MIDIDIAG_SECTION_LABEL_ENUM_REGISTRY_DRIVERS32                   L"reg_drivers32"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_DRIVERS32_ENTRY                    L"reg_drivers32_entry"

#define MIDIDIAG_SECTION_LABEL_ENUM_REGISTRY_DRIVERS32WOW                L"reg_drivers32wow"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_DRIVERS32WOW_ENTRY                 L"reg_drivers32wow_entry"

#define MIDIDIAG_SECTION_LABEL_ENUM_REGISTRY                             L"enum_registry"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_ROOT_CURRENT_CONFIG                L"reg_current_config"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_ROOT_DISCOVERY_ENABLED             L"reg_discovery_enabled"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_ROOT_DISCOVERY_TIMEOUT             L"reg_discovery_timeout_ms"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_ROOT_USE_MMCSS                     L"reg_use_mmcss"
#define MIDIDIAG_FIELD_LABEL_REG_DEFAULT_MIDI1_NAME_TABLE_SELECTION      L"reg_default_midi1_winmm_naming"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_MIDISRV_EXENAME                    L"reg_midisrv_exe"
#define MIDIDIAG_FIELD_LABEL_REGISTRY_TRANSPORT                          L"reg_transport"

#define MIDIDIAG_SECTION_LABEL_DEVICE_NODES                              L"device_nodes"
#define MIDIDIAG_FIELD_LABEL_PROBLEM_DEVICE_COUNT                        L"problem_device_count"
#define MIDIDIAG_FIELD_LABEL_PROBLEM_DEVICE                              L"problem_device"
#define MIDIDIAG_FIELD_LABEL_ENDPOINT_NODES                              L"endpoint_nodes"
#define MIDIDIAG_FIELD_LABEL_ENDPOINT_NODES_TOTAL                        L"endpoint_nodes_total"
#define MIDIDIAG_FIELD_LABEL_MIDI1_PORT_NODES_TOTAL                      L"midi1_port_nodes_total"

#define MIDIDIAG_SECTION_LABEL_NETWORK                                   L"network"
#define MIDIDIAG_FIELD_LABEL_CONNECTED_NETWORK                           L"connected_network"
#define MIDIDIAG_FIELD_LABEL_FIREWALL_PROFILE                            L"firewall_profile"
#define MIDIDIAG_FIELD_LABEL_FIREWALL_RULE                               L"midisrv_firewall_rule"

#define MIDIDIAG_SECTION_LABEL_MIDI1_API_INPUT_ENDPOINTS                 L"enum_winrt_midi1_api_input_ports"
#define MIDIDIAG_SECTION_LABEL_MIDI1_API_OUTPUT_ENDPOINTS                L"enum_winrt_midi1_api_output_ports"
#define MIDIDIAG_FIELD_LABEL_WINRT_MIDI1_PORT                            L"winrt_midi1_port"

#define MIDIDIAG_SECTION_LABEL_SERVICE_RESPONSE                          L"service_response"
#define MIDIDIAG_FIELD_LABEL_SERVICE_RESPONSE_MS                         L"service_response_ms"
#define MIDIDIAG_FIELD_LABEL_SERVICE_RUNNING                             L"service_running"
#define MIDIDIAG_FIELD_LABEL_SERVICE_STARTED_BY_REPORT                   L"service_started_by_report"
#define MIDIDIAG_FIELD_LABEL_SERVICE_SECTIONS_SKIPPED                    L"service_sections_skipped"

#define MIDIDIAG_SECTION_LABEL_MIDI_CLOCK                                L"midi_clock"
#define MIDIDIAG_FIELD_LABEL_CLOCK_FREQUENCY                             L"clock_frequency"
#define MIDIDIAG_FIELD_LABEL_CLOCK_NOW                                   L"clock_now"

#define MIDIDIAG_SECTION_LABEL_ENUM_TRANSPORTS                           L"enum_transports"
#define MIDIDIAG_FIELD_LABEL_TRANSPORT                                   L"transport"
#define MIDIDIAG_FIELD_LABEL_TRANSPORT_CAPABILITIES                      L"transport_capabilities"
#define MIDIDIAG_FIELD_LABEL_TRANSPORT_NOT_REPORTED                      L"transport_not_reported"

#define MIDIDIAG_SECTION_LABEL_MIDI2_API_ENDPOINTS                       L"enum_ump_api_endpoints"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_ID                           L"endpoint_device_id"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_NAME                         L"name"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_TRANSPORT_CODE               L"transport_code"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PURPOSE                      L"purpose"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_USER_SUPPLIED_NAME           L"name_user_supplied"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_ENDPOINT_SUPPLIED_NAME       L"name_endpoint_supplied"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_TRANSPORT_SUPPLIED_NAME      L"name_transport_supplied"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_USER_SUPPLIED_DESC           L"desc_user_supplied"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_TRANSPORT_SUPPLIED_DESC      L"desc_transport_supplied"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_MANUFACTURER                 L"manufacturer"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_NATIVE_DATA_FORMAT           L"native_data_format"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_MULTI_CLIENT                 L"multi_client"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_MUTED                        L"muted"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_DISCOVERY_COMPLETE           L"endpoint_discovery_complete"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_CONTAINER_ID                 L"container_id"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_DRIVER_DEVICE_INTERFACE      L"driver_device_interface"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_DECLARED_ENDPOINT            L"declared_endpoint"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_DECLARED_DEVICE_IDENTITY     L"declared_device_identity"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_STREAM_CONFIGURATION         L"stream_configuration"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_OUTGOING_LATENCY             L"outgoing_latency"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_NOTE_OFF_TRANSLATION         L"requires_note_off_translation"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_SUPPORTS_MPE                 L"supports_mpe"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_RECOMMENDED_CC_INTERVAL      L"recommended_cc_interval_ms"
#define MIDIDIAG_FIELD_LABEL_GTB                                         L"gtb"
#define MIDIDIAG_FIELD_LABEL_FUNCTION_BLOCK                              L"function_block"
#define MIDIDIAG_FIELD_LABEL_NAME_TABLE_SELECTION                        L"name_table_selection"
#define MIDIDIAG_FIELD_LABEL_MIDI1_PORT                                  L"midi1_port"
#define MIDIDIAG_FIELD_LABEL_NAME_TABLE_ENTRY                            L"name_table_entry"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_ID                    L"parent_id"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_NAME                  L"parent_name"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_USB_VID               L"parent_usb_vid"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_USB_PID               L"parent_usb_pid"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_USB_SERIAL            L"parent_usb_serial"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_USB_LOCATION          L"parent_usb_location"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_LAST_ARRIVAL          L"parent_last_arrival"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_LAST_REMOVAL          L"parent_last_removal"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_PROBLEM               L"parent_problem"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_DRIVER_DEVICE_ID      L"parent_driver_device_id"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_ENUMERATOR_NAME       L"parent_enumerator_name"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_SERVICE_NAME          L"parent_service_name"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_DRIVER_INF_PATH       L"parent_driver_inf_path"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_DRIVER_PROVIDER       L"parent_driver_provider"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_DRIVER_VERSION        L"parent_driver_version"
#define MIDIDIAG_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_DRIVER_DATE           L"parent_driver_date"

#define MIDIDIAG_SECTION_LABEL_WINMM_API_INPUT_ENDPOINTS                 L"enum_winmm_midi1_api_input_ports"
#define MIDIDIAG_SECTION_LABEL_WINMM_API_OUTPUT_ENDPOINTS                L"enum_winmm_midi1_api_output_ports"
#define MIDIDIAG_FIELD_LABEL_WINMM_ENDPOINT_COUNT                        L"endpoint_count"
#define MIDIDIAG_FIELD_LABEL_WINMM_PORT                                  L"winmm_port"
#define MIDIDIAG_FIELD_LABEL_WINMM_ERROR_COUNT                           L"dev_caps_error_count"

#define MIDIDIAG_SECTION_LABEL_BLUETOOTH                                 L"transport_bluetooth"
#define MIDIDIAG_FIELD_LABEL_BLUETOOTH_RADIO                             L"bluetooth_radio"
#define MIDIDIAG_FIELD_LABEL_BLUETOOTH_DEFAULT_RETENTION                 L"bluetooth_default_retention"
#define MIDIDIAG_FIELD_LABEL_BLUETOOTH_DEVICE                            L"bluetooth_device"
#define MIDIDIAG_FIELD_LABEL_BLUETOOTH_DEVICE_ERROR                      L"bluetooth_device_error"
#define MIDIDIAG_FIELD_LABEL_BLUETOOTH_SAVED_DEVICE                      L"bluetooth_saved_device"
#define MIDIDIAG_FIELD_LABEL_BLUETOOTH_PERIPHERAL                        L"bluetooth_peripheral"
#define MIDIDIAG_FIELD_LABEL_PENDING_REMOTE_CLIENT                       L"pending_remote_client"

#define MIDIDIAG_SECTION_LABEL_NETWORK_MIDI2                             L"transport_network_midi2"
#define MIDIDIAG_SECTION_LABEL_RTP_MIDI                                  L"transport_rtp_midi"
#define MIDIDIAG_FIELD_LABEL_NETWORK_HOST                                L"host"
#define MIDIDIAG_FIELD_LABEL_NETWORK_HOST_CONNECTION                     L"host_connection"
#define MIDIDIAG_FIELD_LABEL_NETWORK_CLIENT                              L"client"
#define MIDIDIAG_FIELD_LABEL_NETWORK_CLIENT_CONNECTION                   L"client_connection"
#define MIDIDIAG_FIELD_LABEL_ADVERTISED_HOST                             L"advertised_host"
#define MIDIDIAG_FIELD_LABEL_SAVED_HOST                                  L"saved_host"
#define MIDIDIAG_FIELD_LABEL_SAVED_CLIENT                                L"saved_client"

#define MIDIDIAG_SECTION_LABEL_LOOPBACK                                  L"transport_loopback"
#define MIDIDIAG_SECTION_LABEL_BASIC_LOOPBACK                            L"transport_basic_loopback"
#define MIDIDIAG_FIELD_LABEL_FEEDBACK_PROTECTION_AVAILABLE               L"feedback_protection_available"
#define MIDIDIAG_FIELD_LABEL_LOOPBACK                                    L"loopback"
#define MIDIDIAG_FIELD_LABEL_SAVED_LOOPBACK                              L"saved_loopback"

#define MIDIDIAG_SECTION_LABEL_ENDPOINT_CUSTOMIZATIONS                   L"endpoint_customizations"
#define MIDIDIAG_FIELD_LABEL_CUSTOMIZATION_COUNT                         L"customization_count"
#define MIDIDIAG_FIELD_LABEL_ORPHANED_CUSTOMIZATION_COUNT                L"orphaned_customization_count"
#define MIDIDIAG_FIELD_LABEL_ORPHANED_CUSTOMIZATION                      L"orphaned_customization"

#define MIDIDIAG_SECTION_LABEL_SESSIONS                                  L"enum_sessions"
#define MIDIDIAG_FIELD_LABEL_SESSION_COUNT                               L"session_count"
#define MIDIDIAG_FIELD_LABEL_SESSION_NAME                                L"session_name"
#define MIDIDIAG_FIELD_LABEL_SESSION_PROCESS_NAME                        L"process_name"
#define MIDIDIAG_FIELD_LABEL_SESSION_PROCESS_ID                          L"process_id"
#define MIDIDIAG_FIELD_LABEL_SESSION_OWNER                               L"session_owner"
#define MIDIDIAG_FIELD_LABEL_SESSION_START_TIME                          L"session_start_time"
#define MIDIDIAG_FIELD_LABEL_SESSION_CONNECTION_COUNT                    L"connection_count"
#define MIDIDIAG_FIELD_LABEL_SESSION_CONNECTION                          L"connection"

#define MIDIDIAG_SECTION_LABEL_PING_TEST                                 L"ping_test"
#define MIDIDIAG_FIELD_LABEL_PING_ATTEMPT_COUNT                          L"ping_attempt_count"
#define MIDIDIAG_FIELD_LABEL_PING_RETURN_COUNT                           L"ping_return_count"
#define MIDIDIAG_FIELD_LABEL_PING_ROUND_TRIP_TOTAL_TICKS                 L"round_trip_total_ticks"
#define MIDIDIAG_FIELD_LABEL_PING_ROUND_TRIP_AVERAGE_TICKS               L"round_trip_average_ticks"
#define MIDIDIAG_FIELD_LABEL_PING_FAILURE_REASON                         L"ping_failure_reason"

#define MIDIDIAG_SECTION_LABEL_CONNECTION_TIMING                         L"connection_timing"
#define MIDIDIAG_FIELD_LABEL_SESSION_CREATE_MS                           L"session_create_ms"
#define MIDIDIAG_FIELD_LABEL_LOOPBACK_OPEN_MS                            L"diagnostics_loopback_open_ms"
#define MIDIDIAG_FIELD_LABEL_LOOPBACK_OPENED                             L"diagnostics_loopback_opened"

#define MIDIDIAG_SECTION_LABEL_FINDINGS                                  L"findings"
#define MIDIDIAG_FIELD_LABEL_FINDING_COUNT                               L"finding_count"
#define MIDIDIAG_FIELD_LABEL_FINDING                                     L"finding"

#define MIDIDIAG_SECTION_LABEL_SECTION_TIMING                            L"section_timing"
#define MIDIDIAG_FIELD_LABEL_SECTION_TIMING                              L"section"
#define MIDIDIAG_FIELD_LABEL_TOTAL_ELAPSED_MS                            L"total_elapsed_ms"
#define MIDIDIAG_FIELD_LABEL_SECTION_TIMED_OUT                           L"section_timed_out"

#define MIDIDIAG_SECTION_LABEL_END_OF_FILE                               L"end_of_file"

