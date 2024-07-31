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

#define MIDIDIAG_PRODUCT_NAME                                            L"Microsoft Windows MIDI Services"

#define MIDIDIAG_MAX_FIELD_LABEL_WIDTH                                   25

#define MIDIDIAG_FIELD_SEPARATOR                                         L" : "
#define MIDIDIAG_SECTION_HEADER_SEPARATOR_CHAR                           '='
#define MIDIDIAG_ITEM_SEPARATOR_CHAR                                     '-'
#define MIDIDIAG_SEPARATOR_REPEATING_CHAR_COUNT_PER_LINE                 79

#define MIDIDIAG_FIELD_LABEL_ERROR                                       L"ERROR"

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

#define MIDIDIAG_SECTION_LABEL_MIDI1_API_INPUT_ENDPOINTS                 L"enum_midi1_api_inputs_endpoints"
#define MIDIDIAG_SECTION_LABEL_MIDI1_API_OUTPUT_ENDPOINTS                L"enum_midi1_api_output_endpoints"
#define MIDIDIAG_FIELD_LABEL_MIDI1_ENDPOINT_ID                           L"endpoint_device_id"
#define MIDIDIAG_FIELD_LABEL_MIDI1_ENDPOINT_NAME                         L"name"

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

#define MIDIDIAG_SECTION_LABEL_OS                                        L"os"
#define MIDIDIAG_FIELD_LABEL_OS_VERSION                                  L"os_version"

#define MIDIDIAG_SECTION_LABEL_APPARENT_SYSTEM_INFO                      L"apparent_system_info"
#define MIDIDIAG_SECTION_LABEL_NATIVE_SYSTEM_INFO                        L"native_system_info"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_ARCH                  L"processor_architecture"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_LEVEL                 L"processor_level"
#define MIDIDIAG_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_REVISION              L"processor_revision"

#define MIDIDIAG_SECTION_LABEL_HEADER                                    L"header"
#define MIDIDIAG_FIELD_LABEL_CURRENT_TIME                                L"current_time"

#define MIDIDIAG_SECTION_LABEL_END_OF_FILE                               L"end_of_file"

