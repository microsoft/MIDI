// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#ifndef JSON_DEFS_H
#define JSON_DEFS_H

#pragma once

//#define MIDI_CONFIG_JSON_ENDPOINT_USER_SUPPLIED_NAME_PROPERTY_KEY               L"userSuppliedName"
//#define MIDI_CONFIG_JSON_ENDPOINT_USER_SUPPLIED_DESCRIPTION_PROPERTY_KEY        L"userSuppliedDescription"
//#define MIDI_CONFIG_JSON_ENDPOINT_FORCE_SINGLE_CLIENT_PROPERTY_KEY              L"forceSingleClientOnly"

//#define MIDI_CONFIG_JSON_ENDPOINT_IDENTIFIER_SWD                                L"SWD: "


// structural

#define MIDI_CONFIG_JSON_HEADER_OBJECT                                                  L"header"
#define MIDI_CONFIG_JSON_HEADER_PRODUCT_KEY                                             L"product"
#define MIDI_CONFIG_JSON_HEADER_FILE_VERSION_KEY                                        L"fileVersion"



#define MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT                               L"endpointTransportPluginSettings"
#define MIDI_CONFIG_JSON_ENDPOINT_PROCESSING_PLUGIN_SETTINGS_OBJECT                     L"endpointProcessingPluginSettings"

// common properties

#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY                                  L"name"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_DESCRIPTION_PROPERTY                           L"description"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_UNIQUE_ID_PROPERTY                             L"uniqueIdentifier"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_MANUFACTURER_PROPERTY                          L"manufacturer"


#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_NAME_PROPERTY                    L"userSuppliedName"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_DESCRIPTION_PROPERTY             L"userSuppliedDescription"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_SMALL_IMAGE_PROPERTY             L"userSuppliedSmallImage"
#define MIDI_CONFIG_JSON_ENDPOINT_COMMON_USER_SUPPLIED_LARGE_IMAGE_PROPERTY             L"userSuppliedLargeImage"

#define MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_SUCCESS_PROPERTY_KEY                    L"success"
#define MIDI_CONFIG_JSON_CONFIGURATION_RESPONSE_MESSAGE_PROPERTY_KEY                    L"message"



// Virtual MIDI (here because also needed by the client API)

#define MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICES_CREATE_ARRAY_KEY                      L"createVirtualDevices"
#define MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICES_UPDATE_ARRAY_KEY                      L"updateVirtualDevices"
#define MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICES_REMOVE_ARRAY_KEY                      L"removeVirtualDevices"

#define MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_ASSOCIATION_ID_PROPERTY_KEY            L"associationIdentifier"
#define MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_UNIQUE_ID_MAX_LEN  32


#define MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_CREATED_DEVICES_ARRAY_KEY     L"createdDevices"
#define MIDI_CONFIG_JSON_ENDPOINT_VIRTUAL_DEVICE_RESPONSE_CREATED_ID_PROPERTY_KEY       L"id"



// loopback MIDI (here because these can also be created via the client API)

#define MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICES_CREATE_KEY                           L"create"
#define MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICES_UPDATE_KEY                           L"update"
#define MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICES_REMOVE_KEY                           L"remove"

#define MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_ENDPOINT_A_KEY                        L"endpointA"
#define MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_ENDPOINT_B_KEY                        L"endpointB"


#define MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_RESPONSE_CREATED_ENDPOINT_A_ID_KEY    L"endpointA"
#define MIDI_CONFIG_JSON_ENDPOINT_LOOPBACK_DEVICE_RESPONSE_CREATED_ENDPOINT_B_ID_KEY    L"endpointB"




// Session tracker. These are used in the service and in the client API

#define MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_ARRAY_PROPERTY_KEY             L"sessions"

#define MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_ID_PROPERTY_KEY                L"id"
#define MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_NAME_PROPERTY_KEY              L"name"
#define MIDI_SESSION_TRACKER_JSON_RESULT_PROCESS_ID_PROPERTY_KEY                L"clientProcessId"
#define MIDI_SESSION_TRACKER_JSON_RESULT_PROCESS_NAME_PROPERTY_KEY              L"processName"
#define MIDI_SESSION_TRACKER_JSON_RESULT_SESSION_TIME_PROPERTY_KEY              L"startTime"


#define MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_ARRAY_PROPERTY_KEY          L"connections"

#define MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_TIME_PROPERTY_KEY           L"earliestStartTime"
#define MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_COUNT_PROPERTY_KEY          L"instanceCount"
#define MIDI_SESSION_TRACKER_JSON_RESULT_CONNECTION_ENDPOINT_ID_PROPERTY_KEY    L"endpointId"


#define MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_NAME_PROPERTY_KEY                         L"name"
#define MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_MNEMONIC_PROPERTY_KEY                     L"mnemonic"
#define MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_DESCRIPTION_PROPERTY_KEY                  L"description"
#define MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_AUTHOR_PROPERTY_KEY                       L"author"
#define MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_SMALL_IMAGE_PATH_PROPERTY_KEY             L"smallImagePath"
#define MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_VERSION_PROPERTY_KEY                      L"version"
#define MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_IS_RT_CREATABLE_APPS_PROPERTY_KEY         L"isRuntimeCreatableByApps"
#define MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_IS_RT_CREATABLE_SETTINGS_PROPERTY_KEY     L"isRuntimeCreatableBySettings"
#define MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_IS_SYSTEM_MANAGED_PROPERTY_KEY            L"isSystemManaged"
#define MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_IS_CLIENT_CONFIGURABLE_PROPERTY_KEY       L"isClientConfigurable"
#define MIDI_SERVICE_JSON_ABSTRACTION_PLUGIN_INFO_CLIENT_CONFIG_ASSEMBLY_PROPERTY_KEY       L"clientConfigAssembly"


#define MIDI_SERVICE_JSON_TRANSFORM_PLUGIN_INFO_NAME_PROPERTY_KEY                           L"name"
#define MIDI_SERVICE_JSON_TRANSFORM_PLUGIN_INFO_DESCRIPTION_PROPERTY_KEY                    L"description"
#define MIDI_SERVICE_JSON_TRANSFORM_PLUGIN_INFO_AUTHOR_PROPERTY_KEY                         L"author"
#define MIDI_SERVICE_JSON_TRANSFORM_PLUGIN_INFO_SMALL_IMAGE_PATH_PROPERTY_KEY               L"smallImagePath"
#define MIDI_SERVICE_JSON_TRANSFORM_PLUGIN_INFO_VERSION_PROPERTY_KEY                        L"version"
#define MIDI_SERVICE_JSON_TRANSFORM_PLUGIN_INFO_IS_SYSTEM_MANAGED_PROPERTY_KEY              L"isSystemManaged"
#define MIDI_SERVICE_JSON_TRANSFORM_PLUGIN_INFO_IS_CLIENT_CONFIGURABLE_PROPERTY_KEY         L"isClientConfigurable"
#define MIDI_SERVICE_JSON_TRANSFORM_PLUGIN_INFO_CLIENT_CONFIG_ASSEMBLY_PROPERTY_KEY         L"clientConfigAssembly"


#endif