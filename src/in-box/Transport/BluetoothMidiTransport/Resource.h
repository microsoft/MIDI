// Copyright (c) Microsoft Corporation. All rights reserved.

#define IDS_PROJNAME                    100
#define IDR_MIDI2BLE2MIDITRANSPORT		101

#define IDS_PLUGIN_METADATA_VERSION     500
#define IDS_PLUGIN_METADATA_NAME        501
#define IDS_PLUGIN_METADATA_DESCRIPTION 502
#define IDS_PLUGIN_METADATA_AUTHOR      503

// Names and descriptions shown for the transport's device nodes and endpoints
#define IDS_TRANSPORT_PARENT_DEVICE_NAME                        1000
#define IDS_MIDI1_ENDPOINT_DESCRIPTION                          1001
#define IDS_MIDI2_ENDPOINT_DESCRIPTION                          1002
#define IDS_PERIPHERAL_UNKNOWN_CLIENT_NAME                      1003
#define IDS_PERIPHERAL_MIDI1_ENDPOINT_DESCRIPTION               1004
#define IDS_PERIPHERAL_MIDI2_ENDPOINT_DESCRIPTION               1005
#define IDS_PERIPHERAL_UNKNOWN_CLIENT_NAME_WITH_ADDRESS         1006
#define IDS_UNNAMED_DEVICE_ENDPOINT_NAME                        1007

// Returned when a configuration command is rejected
#define IDS_ERROR_INVALID_JSON                                  2000
#define IDS_ERROR_CONNECT_MISSING_DEVICE_ID                     2001
#define IDS_ERROR_DISCONNECT_MISSING_DEVICE_ID                  2002
#define IDS_ERROR_INVALID_DEVICE_ID                             2003
#define IDS_ERROR_CONNECT_FAILED                                2004
#define IDS_ERROR_DISCONNECT_FAILED                             2005
#define IDS_ERROR_ENDPOINT_MANAGER_NOT_AVAILABLE                2006
#define IDS_ERROR_START_PERIPHERAL_FAILED                       2007
#define IDS_ERROR_STOP_PERIPHERAL_FAILED                        2008
#define IDS_ERROR_DECISION_MISSING_ADDRESS                      2009
#define IDS_ERROR_INVALID_APPROVAL_SCOPE                        2010
#define IDS_ERROR_ADDRESS_NOT_REMEMBERABLE                      2011
#define IDS_ERROR_DIFFERENT_CLIENT_WAITING                      2012
#define IDS_ERROR_NO_CLIENT_WAITING                             2013
#define IDS_ERROR_FORGET_MISSING_ADDRESS                        2014
#define IDS_ERROR_CLIENT_NOT_REMEMBERED                         2015
#define IDS_ERROR_INVALID_OFFLINE_RETENTION                     2016
#define IDS_ERROR_UNUSABLE_DEVICE_ID                            2017
#define IDS_ERROR_INVALID_CONNECTION_PARAMETERS                 2018
#define IDS_ERROR_UNSUPPORTED_COMMAND                           2019

// Why a connection attempt failed, reported as the device's last connect error. Strings with
// {0} are formatted with std::format, so the placeholder and its format spec must stay intact.
#define IDS_CONNECT_DEVICE_NOT_DISCOVERED                       2100
#define IDS_CONNECT_DEVICE_NOT_AVAILABLE                        2101
#define IDS_CONNECT_SHUTTING_DOWN                               2102
#define IDS_CONNECT_PAIRING_REQUIRED                            2103
#define IDS_CONNECT_SERVICE_READ_FAILED                         2104
#define IDS_CONNECT_SERVICE_READ_FAILED_PAIRING_MAY_HELP        2105
#define IDS_CONNECT_STOPPED_ANSWERING_PAIRING_MAY_HELP          2106
#define IDS_CONNECT_STOPPED_ANSWERING                           2107
#define IDS_CONNECT_UNREACHABLE_BUT_HEARD                       2108
#define IDS_CONNECT_UNREACHABLE_NEVER_HEARD                     2109
#define IDS_CONNECT_UNREACHABLE_NOT_HEARD_RECENTLY              2110
#define IDS_CONNECT_SERVICE_ACCESS_DENIED                       2111
#define IDS_CONNECT_PROTOCOL_ERROR                              2112
#define IDS_CONNECT_PROTOCOL_ERROR_WITH_CODE                    2113
#define IDS_CONNECT_MIDI_SERVICE_NOT_FOUND                      2114
#define IDS_CONNECT_OPEN_ACCESS_DENIED                          2115
#define IDS_CONNECT_DEVICE_IN_USE                               2116
#define IDS_CONNECT_OPEN_FAILED                                 2117
#define IDS_CONNECT_CHARACTERISTIC_NOT_FOUND                    2118
#define IDS_CONNECT_CHARACTERISTIC_READ_FAILED                  2119
#define IDS_CONNECT_CHARACTERISTIC_STOPPED_ANSWERING            2120
#define IDS_CONNECT_CHARACTERISTIC_UNREACHABLE                  2121
#define IDS_CONNECT_CHARACTERISTIC_ACCESS_DENIED                2122
#define IDS_CONNECT_SESSION_FAILED                              2123
#define IDS_CONNECT_UNEXPECTED_ERROR                            2124
#define IDS_CONNECT_SUBSCRIBE_FAILED                            2125
#define IDS_CONNECT_ENDPOINT_CREATION_FAILED                    2126



#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE        201
#define _APS_NEXT_COMMAND_VALUE         32768
#define _APS_NEXT_CONTROL_VALUE         201
#define _APS_NEXT_SYMED_VALUE           106
#endif
#endif
