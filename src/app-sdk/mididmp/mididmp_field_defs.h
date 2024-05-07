// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// All field names here in case you want to parse the file. Just include or
// copy these definitions into your code

#define MIDIDMP_PRODUCT_NAME                                            L"Microsoft Windows MIDI Services"

#define MIDIDMP_MAX_FIELD_LABEL_WIDTH                                   25

#define MIDIDMP_FIELD_SEPARATOR                                         L" : "
#define MIDIDMP_SECTION_HEADER_SEPARATOR_CHAR                           '='
#define MIDIDMP_ITEM_SEPARATOR_CHAR                                     '-'
#define MIDIDMP_SEPARATOR_REPEATING_CHAR_COUNT_PER_LINE                 79

#define MIDIDMP_FIELD_LABEL_ERROR                                       L"ERROR"

#define MIDIDMP_SECTION_LABEL_ENUM_TRANSPORTS                           L"enum_transports"
#define MIDIDMP_FIELD_LABEL_TRANSPORT_ID                                L"transport_id"
#define MIDIDMP_FIELD_LABEL_TRANSPORT_NAME                              L"name"
#define MIDIDMP_FIELD_LABEL_TRANSPORT_MNEMONIC                          L"mnemonic"
#define MIDIDMP_FIELD_LABEL_TRANSPORT_VERSION                           L"version"
#define MIDIDMP_FIELD_LABEL_TRANSPORT_AUTHOR                            L"author"
#define MIDIDMP_FIELD_LABEL_TRANSPORT_DESCRIPTION                       L"description"

#define MIDIDMP_SECTION_LABEL_MIDI2_API_ENDPOINTS                       L"enum_ump_api_endpoints"
#define MIDIDMP_FIELD_LABEL_MIDI2_ENDPOINT_ID                           L"endpoint_device_id"
#define MIDIDMP_FIELD_LABEL_MIDI2_ENDPOINT_NAME                         L"name"
#define MIDIDMP_FIELD_LABEL_MIDI2_ENDPOINT_TRANSPORT_MNEMONIC           L"transport_mnemonic"
#define MIDIDMP_FIELD_LABEL_MIDI2_ENDPOINT_USER_SUPPLIED_NAME           L"name_user_supplied"
#define MIDIDMP_FIELD_LABEL_MIDI2_ENDPOINT_ENDPOINT_SUPPLIED_NAME       L"name_endpoint_supplied"
#define MIDIDMP_FIELD_LABEL_MIDI2_ENDPOINT_TRANSPORT_SUPPLIED_NAME      L"name_transport_supplied"
#define MIDIDMP_FIELD_LABEL_MIDI2_ENDPOINT_TRANSPORT_SUPPLIED_DESC      L"desc_transport_supplied"
#define MIDIDMP_FIELD_LABEL_MIDI2_ENDPOINT_USER_SUPPLIED_DESC           L"desc_user_supplied"
#define MIDIDMP_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_ID                    L"parent_id"
#define MIDIDMP_FIELD_LABEL_MIDI2_ENDPOINT_PARENT_NAME                  L"parent_name"

#define MIDIDMP_SECTION_LABEL_MIDI1_API_INPUT_ENDPOINTS                 L"enum_midi1_api_inputs_endpoints"
#define MIDIDMP_SECTION_LABEL_MIDI1_API_OUTPUT_ENDPOINTS                L"enum_midi1_api_output_endpoints"
#define MIDIDMP_FIELD_LABEL_MIDI1_ENDPOINT_ID                           L"endpoint_device_id"
#define MIDIDMP_FIELD_LABEL_MIDI1_ENDPOINT_NAME                         L"name"

#define MIDIDMP_SECTION_LABEL_PING_TEST                                 L"ping_test"
#define MIDIDMP_FIELD_LABEL_PING_ATTEMPT_COUNT                          L"ping_attempt_count"
#define MIDIDMP_FIELD_LABEL_PING_RETURN_COUNT                           L"ping_return_count"
#define MIDIDMP_FIELD_LABEL_PING_ROUND_TRIP_TOTAL_TICKS                 L"round_trip_total_ticks"
#define MIDIDMP_FIELD_LABEL_PING_ROUND_TRIP_AVERAGE_TICKS               L"round_trip_average_ticks"
#define MIDIDMP_FIELD_LABEL_PING_FAILURE_REASON                         L"ping_failure_reason"

#define MIDIDMP_SECTION_LABEL_MIDI_CLOCK                                L"midi_clock"
#define MIDIDMP_FIELD_LABEL_CLOCK_FREQUENCY                             L"clock_frequency"
#define MIDIDMP_FIELD_LABEL_CLOCK_NOW                                   L"clock_now"

#define MIDIDMP_SECTION_LABEL_SERVICE_STATUS                            L"service_status"
#define MIDIDMP_FIELD_LABEL_SERVICE_AVAILABLE                           L"available"

#define MIDIDMP_SECTION_LABEL_OS                                        L"os"
#define MIDIDMP_FIELD_LABEL_OS_VERSION                                  L"os_version"

#define MIDIDMP_SECTION_LABEL_APPARENT_SYSTEM_INFO                      L"apparent_system_info"
#define MIDIDMP_SECTION_LABEL_NATIVE_SYSTEM_INFO                        L"native_system_info"
#define MIDIDMP_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_ARCH                  L"processor_architecture"
#define MIDIDMP_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_LEVEL                 L"processor_level"
#define MIDIDMP_FIELD_LABEL_SYSTEM_INFO_PROCESSOR_REVISION              L"processor_revision"

#define MIDIDMP_SECTION_LABEL_HEADER                                    L"header"
#define MIDIDMP_FIELD_LABEL_CURRENT_TIME                                L"current_time"

#define MIDIDMP_SECTION_LABEL_END_OF_FILE                               L"end_of_file"

