// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Every user-facing string in this tool has an id here and a value in Resources.rc.
// Ids are grouped in blocks so a new string can be added to a section without renumbering.

// ---------------------------------------------------------------- application  50000
#define IDS_APP_TITLE                                   50000
#define IDS_APP_DESCRIPTION                             50001
#define IDS_APP_BANNER                                  50002
#define IDS_HELP_EXAMPLES_HEADING                       50003

// ---------------------------------------------------------------- shared errors 50050
#define IDS_ERROR_SERVICE_NOT_AVAILABLE                 50050
#define IDS_ERROR_CREATING_SESSION                      50051
#define IDS_ERROR_CREATING_CONNECTION                   50052
#define IDS_ERROR_OPENING_CONNECTION                    50053
#define IDS_ERROR_ENDPOINT_NOT_FOUND                    50054
#define IDS_ERROR_NO_ENDPOINTS_FOUND                    50055
#define IDS_ERROR_GENERAL_FAILURE                       50056
#define IDS_ERROR_NO_INTERACTIVE_CONSOLE                50057
#define IDS_ERROR_LEGACY_API_MODE                       50058
#define IDS_ERROR_FILE_NOT_FOUND                        50059
#define IDS_ERROR_TOO_FEW_WORDS                         50060
#define IDS_ERROR_TOO_MANY_WORDS                        50061
#define IDS_ERROR_INVALID_UMP                           50062
#define IDS_ERROR_INVALID_WORD_VALUE                    50063
#define IDS_ERROR_SEND_FAILED                           50064
#define IDS_ERROR_TIMESTAMP_AND_OFFSET                  50065
#define IDS_ERROR_COUNT_TOO_LOW                         50066
#define IDS_ERROR_INVALID_GROUP                         50067
#define IDS_ERROR_INVALID_CHANNEL                       50068
#define IDS_ERROR_INVALID_VELOCITY                      50069
#define IDS_ERROR_INVALID_NOTE                          50070
#define IDS_ERROR_NO_NOTES_SUPPLIED                     50071
#define IDS_ERROR_INVALID_ASSOCIATION_ID                50072
#define IDS_ERROR_INVALID_ENUM_VALUE                    50073
#define IDS_ERROR_UNHANDLED                             50074
#define IDS_ERROR_FUNCTION_BLOCK_NUMBER                 50075
#define IDS_ERROR_NO_REQUEST_FLAGS                      50076
#define IDS_ERROR_PING_TIMEOUT_TOO_LOW                  50077
#define IDS_ERROR_PING_COUNT_TOO_LOW                    50078
#define IDS_ERROR_PING_COUNT_TOO_HIGH                   50079
#define IDS_ERROR_NOT_UMP_ENDPOINT                      50080
#define IDS_ERROR_GROUP_REQUIRED                        50081

// ---------------------------------------------------------------- shared status 50100
#define IDS_STATUS_CANCELED                             50100
#define IDS_STATUS_DONE                                 50101
#define IDS_PROMPT_PRESS_ESCAPE_TO_STOP                 50102
#define IDS_PROMPT_PRESS_KEY_TO_CLOSE                   50103
#define IDS_PROMPT_SELECT_ENDPOINT                      50104
#define IDS_PROMPT_PICKER_KEYS                          50105
#define IDS_PROMPT_CANCEL_ENTRY                         50106
#define IDS_STATUS_ENUMERATING                          50107
#define IDS_STATUS_WAITING_FOR_MESSAGES                 50108
#define IDS_STATUS_CONNECTING                           50109
#define IDS_STATUS_SENDING                              50110
#define IDS_PROMPT_SELECT_FUNCTION_BLOCK                50111

// ---------------------------------------------------------------- shared labels 50150
#define IDS_LABEL_NAME                                  50150
#define IDS_LABEL_ID                                    50151
#define IDS_LABEL_DESCRIPTION                           50152
#define IDS_LABEL_TRANSPORT                             50153
#define IDS_LABEL_MANUFACTURER                          50154
#define IDS_LABEL_MIDI2_PROTOCOL                        50155
#define IDS_LABEL_YES                                   50156
#define IDS_LABEL_NO                                    50157
#define IDS_LABEL_DIRECTION                             50158
#define IDS_LABEL_GROUPS                                50159
#define IDS_LABEL_ACTIVE                                50160
#define IDS_LABEL_NUMBER                                50161
#define IDS_LABEL_VERSION                               50162
#define IDS_LABEL_AUTHOR                                50163
#define IDS_LABEL_PURPOSE                               50164
#define IDS_LABEL_SERIAL_NUMBER                         50165
#define IDS_LABEL_VID_PID                               50166
#define IDS_LABEL_MULTICLIENT                           50167
#define IDS_LABEL_NATIVE_DATA_FORMAT                    50168
#define IDS_LABEL_PROTOCOL                              50169
#define IDS_LABEL_IMAGE_FILE_NAME                       50170
#define IDS_LABEL_PARENT_DEVICE                         50171
#define IDS_LABEL_CONTAINER                             50172
#define IDS_LABEL_KIND                                  50173
#define IDS_LABEL_VALUE                                 50174
#define IDS_LABEL_PROPERTY_KEY                          50175
#define IDS_LABEL_GROUP                                 50176
#define IDS_LABEL_CHANNEL                               50177
#define IDS_LABEL_PORT_NUMBER                           50178
#define IDS_LABEL_PORT_NAME                             50179
#define IDS_LABEL_MESSAGE_SOURCE                        50180
#define IDS_LABEL_MESSAGE_DESTINATION                   50181
#define IDS_LABEL_BIDIRECTIONAL                         50182
#define IDS_LABEL_UNKNOWN                               50183
#define IDS_LABEL_NONE                                  50184
#define IDS_LABEL_ENDPOINT                              50185
#define IDS_LABEL_ADDRESS                               50186
#define IDS_LABEL_STATUS                                50187
#define IDS_LABEL_UMP_VERSION                           50188
#define IDS_LABEL_FIRST_GROUP                           50189
#define IDS_LABEL_GROUP_COUNT                           50190
#define IDS_LABEL_UI_HINT                               50191
#define IDS_LABEL_INDEX                                 50192
#define IDS_LABEL_INACTIVE                              50193
#define IDS_LABEL_MIDI                                  50194
#define IDS_FB_MIDI10_NOT_MIDI10                        50195
#define IDS_FB_MIDI10_UNRESTRICTED                      50196
#define IDS_FB_MIDI10_RESTRICTED                        50197

// ---------------------------------------------------------------- command help  50300
#define IDS_CMD_ENUMERATE                               50300
#define IDS_CMD_ENUM_ENDPOINTS                          50301
#define IDS_CMD_ENUM_LEGACY                             50302
#define IDS_CMD_ENUM_SESSIONS                           50303
#define IDS_CMD_ENUM_TRANSPORTS                         50304
#define IDS_CMD_ENUM_PROPERTY_KEYS                      50305
#define IDS_CMD_ENDPOINT                                50306
#define IDS_CMD_EP_MONITOR                              50307
#define IDS_CMD_EP_SEND_MESSAGE                         50308
#define IDS_CMD_EP_SEND_MESSAGE_FILE                    50309
#define IDS_CMD_EP_SEND_SYSEX_FILE                      50310
#define IDS_CMD_EP_PLAY_NOTES                           50311
#define IDS_CMD_EP_PROPERTIES                           50312
#define IDS_CMD_EP_REQUEST                              50313
#define IDS_CMD_EP_REQUEST_FUNCTION_BLOCKS              50314
#define IDS_CMD_EP_REQUEST_ENDPOINT_INFO                50315
#define IDS_CMD_LOOPBACK                                50316
#define IDS_CMD_LOOPBACK_LIST                           50317
#define IDS_CMD_LOOPBACK_CREATE                         50318
#define IDS_CMD_LOOPBACK_REMOVE                         50319
#define IDS_CMD_BASIC_LOOPBACK                          50320
#define IDS_CMD_BASIC_LOOPBACK_LIST                     50321
#define IDS_CMD_BASIC_LOOPBACK_CREATE                   50322
#define IDS_CMD_BASIC_LOOPBACK_REMOVE                   50323
#define IDS_CMD_SERVICE                                 50324
#define IDS_CMD_SERVICE_STATUS                          50325
#define IDS_CMD_SERVICE_PING                            50326
#define IDS_CMD_TIME                                    50327
#define IDS_CMD_WATCH_ENDPOINTS                         50328
#define IDS_CMD_WATCH_PORTS                             50329
#define IDS_CMD_BLUETOOTH                               50330
#define IDS_CMD_BT_LIST                                 50331
#define IDS_CMD_BT_CONNECT                              50332
#define IDS_CMD_BT_DISCONNECT                           50333
#define IDS_CMD_BT_CUSTOMIZE                            50334
#define IDS_CMD_BT_PERIPHERAL                           50335
#define IDS_CMD_BT_PERIPHERAL_START                     50336
#define IDS_CMD_BT_PERIPHERAL_STOP                      50337
#define IDS_CMD_BT_PERIPHERAL_STATUS                    50338
#define IDS_CMD_BT_PERIPHERAL_CUSTOMIZE                 50339
#define IDS_CMD_BT_PERIPHERAL_APPROVE                   50340
#define IDS_CMD_BT_PERIPHERAL_DENY                      50341
#define IDS_CMD_BT_PERIPHERAL_FORGET                    50342

// ---------------------------------------------------------------- option help   50400
#define IDS_OPT_ENDPOINT_DEVICE_ID                      50400
#define IDS_OPT_VERBOSE                                 50401
#define IDS_OPT_SHOW_ENDPOINT_ID                        50402
#define IDS_OPT_INCLUDE_DIAGNOSTIC_LOOPBACK             50403
#define IDS_OPT_INCLUDE_ALL_ENDPOINTS                   50404
#define IDS_OPT_LEGACY_DIRECTION                        50405
#define IDS_OPT_INCLUDE_PORT_ID                         50406
#define IDS_OPT_SESSIONS_ALL                            50407
#define IDS_OPT_SINGLE_MESSAGE                          50408
#define IDS_OPT_CAPTURE_TO_FILE                         50409
#define IDS_OPT_ANNOTATE_CAPTURE                        50410
#define IDS_OPT_CAPTURE_FIELD_DELIMITER                 50411
#define IDS_OPT_WARN_SKIPPED_INCREMENT                  50412
#define IDS_OPT_AUTO_RECONNECT                          50413
#define IDS_OPT_JITTER_STATISTICS                       50414
#define IDS_OPT_INCLUDE_TIMESTAMP                       50415
#define IDS_OPT_DECODE_MESSAGES                         50416
#define IDS_OPT_INCLUDE_REAL_TIME                       50417
#define IDS_OPT_INCLUDE_UTILITY                         50418
#define IDS_OPT_PAUSE                                   50419
#define IDS_OPT_WORD_FORMAT                             50420
#define IDS_OPT_NO_WAIT                                 50421
#define IDS_OPT_COUNT                                   50422
#define IDS_OPT_OFFSET_MICROSECONDS                     50423
#define IDS_OPT_TIMESTAMP                               50424
#define IDS_OPT_DEBUG_AUTO_INCREMENT                    50425
#define IDS_OPT_MIDI_WORDS_ARGUMENT                     50426
#define IDS_OPT_INPUT_FILE_ARGUMENT                     50427
#define IDS_OPT_FILE_DELIMITER                          50428
#define IDS_OPT_NEW_GROUP_INDEX                         50429
#define IDS_OPT_SYSEX_GROUP_INDEX                       50430
#define IDS_OPT_MESSAGE_TRANSFER_COUNT                  50431
#define IDS_OPT_NOTE_INDEXES_ARGUMENT                   50432
#define IDS_OPT_NOTE_LENGTH                             50433
#define IDS_OPT_NOTE_REST                               50434
#define IDS_OPT_GROUP_NUMBER                            50435
#define IDS_OPT_CHANNEL_NUMBER                          50436
#define IDS_OPT_VELOCITY                                50437
#define IDS_OPT_FOREVER                                 50438
#define IDS_OPT_MIDI2                                   50439
#define IDS_OPT_INCLUDE_RAW_PROPERTIES                  50440
#define IDS_OPT_INCLUDE_NAME_TABLE                      50441
#define IDS_OPT_REQUEST_ALL                             50442
#define IDS_OPT_FUNCTION_BLOCK_NUMBER                   50443
#define IDS_OPT_REQUEST_INFO                            50444
#define IDS_OPT_REQUEST_NAME_NOTIFICATION               50445
#define IDS_OPT_REQUEST_ENDPOINT_INFO                   50446
#define IDS_OPT_REQUEST_DEVICE_IDENTITY                 50447
#define IDS_OPT_REQUEST_ENDPOINT_NAME                   50448
#define IDS_OPT_REQUEST_PRODUCT_INSTANCE_ID             50449
#define IDS_OPT_REQUEST_STREAM_CONFIGURATION            50450
#define IDS_OPT_UMP_VERSION_MAJOR                       50451
#define IDS_OPT_UMP_VERSION_MINOR                       50452
#define IDS_OPT_LOOPBACK_NAME_A                         50453
#define IDS_OPT_LOOPBACK_NAME_B                         50454
#define IDS_OPT_LOOPBACK_ROOT_NAME                      50455
#define IDS_OPT_UNIQUE_IDENTIFIER                       50456
#define IDS_OPT_SAVE_TO_CONFIG                          50457
#define IDS_OPT_ASSOCIATION_ID                          50458
#define IDS_OPT_BASIC_LOOPBACK_NAME                     50459
#define IDS_OPT_PING_COUNT                              50460
#define IDS_OPT_PING_TIMEOUT                            50461
#define IDS_OPT_WATCH_INCLUDE_LOOPBACK                  50462
#define IDS_OPT_BT_DEVICE_ID_ARGUMENT                   50463
#define IDS_OPT_BT_TEMPORARY                            50464
#define IDS_OPT_BT_FORGET                               50465
#define IDS_OPT_BT_NAME                                 50466
#define IDS_OPT_BT_DESCRIPTION                          50467
#define IDS_OPT_BT_IMAGE                                50468
#define IDS_OPT_BT_CLEAR                                50469
#define IDS_OPT_BT_KEEP_WHEN_OFFLINE                    50470
#define IDS_OPT_BT_PROTOCOL                             50471
#define IDS_OPT_BT_ADDRESS_ARGUMENT                     50472
#define IDS_OPT_BT_SCOPE                                50473

// ---------------------------------------------------------------- enumerate     50700
#define IDS_ENUM_ENDPOINTS_TABLE_TITLE                  50700
#define IDS_ENUM_ENDPOINTS_COUNT                        50701
#define IDS_ENUM_LEGACY_TABLE_TITLE                     50702
#define IDS_ENUM_SESSIONS_TABLE_TITLE                   50703
#define IDS_ENUM_SESSIONS_NONE                          50704
#define IDS_ENUM_SESSIONS_CONNECTION_COUNT              50705
#define IDS_ENUM_SESSIONS_STARTED                       50706
#define IDS_ENUM_SESSIONS_PROCESS                       50707
#define IDS_ENUM_TRANSPORTS_TABLE_TITLE                 50708
#define IDS_ENUM_TRANSPORT_IS_API_CREATABLE             50709
#define IDS_ENUM_TRANSPORT_IS_CONFIG_CREATABLE          50710
#define IDS_ENUM_TRANSPORT_IS_SYSTEM_MANAGED            50711
#define IDS_ENUM_PROPERTY_KEYS_TABLE_TITLE              50712
#define IDS_ENUM_FUNCTION_BLOCKS_TITLE                  50713
#define IDS_ENUM_GROUP_TERMINAL_BLOCKS_TITLE            50714
#define IDS_ENUM_LEGACY_FOOTNOTE                        50715

// ---------------------------------------------------------------- endpoint      50800
#define IDS_EP_SECTION_IDENTIFICATION                   50800
#define IDS_EP_SECTION_ENDPOINT_METADATA                50801
#define IDS_EP_SECTION_USER_DATA                        50802
#define IDS_EP_SECTION_ACTIVE_CONFIGURATION             50803
#define IDS_EP_SECTION_DECLARED_CAPABILITIES            50804
#define IDS_EP_SECTION_FUNCTION_BLOCKS                  50805
#define IDS_EP_SECTION_GROUP_TERMINAL_BLOCKS            50806
#define IDS_EP_SECTION_MIDI1_PORTS                      50807
#define IDS_EP_SECTION_NAME_TABLE                       50808
#define IDS_EP_SECTION_RAW_PROPERTIES                   50809
#define IDS_EP_SECTION_CONTAINER                        50810
#define IDS_EP_SECTION_PARENT                           50811
#define IDS_EP_PROP_PRODUCT_INSTANCE_ID                 50812
#define IDS_EP_PROP_ENDPOINT_SUPPLIED_NAME              50813
#define IDS_EP_PROP_SYSTEM_EXCLUSIVE_ID                 50814
#define IDS_EP_PROP_DEVICE_FAMILY                       50815
#define IDS_EP_PROP_SOFTWARE_REVISION                   50816
#define IDS_EP_PROP_SEND_JR_TIMESTAMPS                  50817
#define IDS_EP_PROP_RECEIVE_JR_TIMESTAMPS               50818
#define IDS_EP_PROP_SUPPORTS_MIDI1_PROTOCOL             50819
#define IDS_EP_PROP_SUPPORTS_MIDI2_PROTOCOL             50820
#define IDS_EP_PROP_SUPPORTS_SENDING_JR                 50821
#define IDS_EP_PROP_SUPPORTS_RECEIVING_JR               50822
#define IDS_EP_PROP_HAS_STATIC_FUNCTION_BLOCKS          50823
#define IDS_EP_PROP_FUNCTION_BLOCK_COUNT                50824
#define IDS_EP_PROP_PORT_NAMING_APPROACH                50825
#define IDS_EP_PROP_DRIVER_PROVIDER                     50826
#define IDS_EP_PROP_DRIVER_VERSION                      50827
#define IDS_EP_PROP_DRIVER_INF_PATH                     50828
#define IDS_EP_PROP_USER_SUPPLIED_NAME                  50829
#define IDS_EP_PROP_TRANSPORT_DESCRIPTION               50830
#define IDS_EP_PROP_USER_DESCRIPTION                    50831
#define IDS_EP_NAME_TABLE_CUSTOM                        50832
#define IDS_EP_NAME_TABLE_LEGACY_COMPATIBLE             50833
#define IDS_EP_NAME_TABLE_NEW_STYLE                     50834
#define IDS_EP_MONITOR_HEADER_INDEX                     50835
#define IDS_EP_MONITOR_HEADER_MESSAGE_TIMESTAMP         50836
#define IDS_EP_MONITOR_HEADER_FROM_LAST                 50837
#define IDS_EP_MONITOR_HEADER_RECEIVED_TIMESTAMP        50838
#define IDS_EP_MONITOR_HEADER_RECEIVE_DELTA             50839
#define IDS_EP_MONITOR_HEADER_DATA                      50840
#define IDS_EP_MONITOR_HEADER_GROUP_SHORT               50841
#define IDS_EP_MONITOR_HEADER_CHANNEL_SHORT             50842
#define IDS_EP_MONITOR_HEADER_MESSAGE_TYPE              50843
#define IDS_EP_MONITOR_HEADER_DECODED_DATA              50844
#define IDS_EP_MONITOR_MONITORING                       50845
#define IDS_EP_MONITOR_KEYS                             50846
#define IDS_EP_MONITOR_COMMENT_TITLE                    50847
#define IDS_EP_MONITOR_COMMENT_HINT                     50848
#define IDS_EP_MONITOR_COMMENT_PLACEHOLDER              50849
#define IDS_EP_MONITOR_MESSAGES_RECEIVED                50850
#define IDS_EP_MONITOR_CONSOLE_TITLE                    50851
#define IDS_EP_MONITOR_POSSIBLE_ERROR                   50852
#define IDS_EP_DECODE_NOTE                              50853
#define IDS_EP_DECODE_VELOCITY                          50854
#define IDS_EP_DECODE_CONTROLLER                        50855
#define IDS_EP_DECODE_VALUE                             50856
#define IDS_EP_DECODE_FINE                              50857
#define IDS_EP_DECODE_COARSE                            50858
#define IDS_EP_DECODE_KEY                               50859
#define IDS_EP_SEND_SUMMARY                             50860
#define IDS_EP_SEND_MESSAGES_SENT                       50861
#define IDS_EP_SEND_MESSAGES_FAILED                     50862
#define IDS_EP_SEND_LINES_SKIPPED                       50863
#define IDS_EP_SEND_BYTES_READ                          50864
#define IDS_EP_PLAY_PLAYING                             50865
#define IDS_EP_REQUEST_SENT                             50866
#define IDS_EP_SENDING_TO                               50867

// ---------------------------------------------------------------- loopback      50900
#define IDS_LOOPBACK_TABLE_TITLE                        50900
#define IDS_LOOPBACK_BASIC_TABLE_TITLE                  50901
#define IDS_LOOPBACK_NONE                               50902
#define IDS_LOOPBACK_CREATED                            50903
#define IDS_LOOPBACK_REMOVED                            50904
#define IDS_LOOPBACK_CREATE_FAILED                      50905
#define IDS_LOOPBACK_REMOVE_FAILED                      50906
#define IDS_LOOPBACK_LABEL_ASSOCIATION_ID               50907
#define IDS_LOOPBACK_LABEL_SAVED_TO_CONFIG              50908
#define IDS_LOOPBACK_LABEL_ENDPOINT_A                   50909
#define IDS_LOOPBACK_LABEL_ENDPOINT_B                   50910

// ---------------------------------------------------------------- service       51000
#define IDS_SERVICE_STATUS_TABLE_TITLE                  51000
#define IDS_SERVICE_LABEL_STATE                         51001
#define IDS_SERVICE_LABEL_START_TYPE                    51002
#define IDS_SERVICE_LABEL_DISPLAY_NAME                  51003
#define IDS_SERVICE_LABEL_SERVICE_NAME                  51004
#define IDS_SERVICE_LABEL_BINARY_PATH                   51005
#define IDS_SERVICE_NOT_INSTALLED                       51006
#define IDS_SERVICE_PING_TABLE_TITLE                    51007
#define IDS_SERVICE_PING_FAILED                         51008
#define IDS_SERVICE_PING_SUMMARY                        51009
#define IDS_SERVICE_PING_LABEL_INDEX                    51010
#define IDS_SERVICE_PING_LABEL_ROUND_TRIP               51011
#define IDS_SERVICE_PING_AVERAGE                        51012
#define IDS_SERVICE_PING_MINIMUM                        51013
#define IDS_SERVICE_PING_MAXIMUM                        51014
#define IDS_SERVICE_PING_TOTAL                          51015
#define IDS_SERVICE_PING_SENT                           51016

// ---------------------------------------------------------------- watch         51100
#define IDS_WATCH_ENDPOINTS_STARTING                    51100
#define IDS_WATCH_PORTS_STARTING                        51101
#define IDS_WATCH_ADDED                                 51102
#define IDS_WATCH_REMOVED                               51103
#define IDS_WATCH_UPDATED                               51104
#define IDS_WATCH_ENUMERATION_COMPLETED                 51105
#define IDS_WATCH_STOPPED                               51106
#define IDS_WATCH_UPDATE_NAME                           51107
#define IDS_WATCH_UPDATE_ENDPOINT_INFORMATION           51108
#define IDS_WATCH_UPDATE_STREAM_CONFIGURATION           51109
#define IDS_WATCH_UPDATE_FUNCTION_BLOCKS                51110
#define IDS_WATCH_UPDATE_DEVICE_IDENTITY                51111
#define IDS_WATCH_UPDATE_USER_METADATA                  51112
#define IDS_WATCH_UPDATE_ADDITIONAL_CAPABILITIES        51113
#define IDS_WATCH_UPDATE_UNIQUE_IDS                     51114
#define IDS_WATCH_UPDATE_PORT_NUMBER                    51115

// ---------------------------------------------------------------- bluetooth     51200
#define IDS_BT_TABLE_TITLE                              51200
#define IDS_BT_NO_DEVICES                               51201
#define IDS_BT_LABEL_DEVICE_ID                          51202
#define IDS_BT_LABEL_CONNECTED                          51203
#define IDS_BT_LABEL_IN_COUNTS                          51204
#define IDS_BT_LABEL_OUT_COUNTS                         51205
#define IDS_BT_CONNECTING                               51206
#define IDS_BT_CONNECTED                                51207
#define IDS_BT_CONNECT_FAILED                           51208
#define IDS_BT_DISCONNECTING                            51209
#define IDS_BT_DISCONNECTED                             51210
#define IDS_BT_DISCONNECT_FAILED                        51211
#define IDS_BT_CUSTOMIZE_SUCCEEDED                      51212
#define IDS_BT_CUSTOMIZE_FAILED                         51213
#define IDS_BT_PERIPHERAL_STARTED                       51214
#define IDS_BT_PERIPHERAL_START_FAILED                  51215
#define IDS_BT_PERIPHERAL_STOPPED                       51216
#define IDS_BT_PERIPHERAL_STOP_FAILED                   51217
#define IDS_BT_PERIPHERAL_STATUS_TITLE                  51218
#define IDS_BT_PERIPHERAL_PUBLISHED                     51219
#define IDS_BT_PERIPHERAL_NOT_PUBLISHED                 51220
#define IDS_BT_PERIPHERAL_APPROVED                      51221
#define IDS_BT_PERIPHERAL_DENIED                        51222
#define IDS_BT_PERIPHERAL_FORGOTTEN                     51223
#define IDS_BT_PERIPHERAL_COMMAND_FAILED                51224
#define IDS_BT_LABEL_KEEP_WHEN_OFFLINE                  51225
#define IDS_BT_LABEL_PROTOCOL                           51226
#define IDS_BT_LABEL_CLIENT_POLICY                      51227
#define IDS_BT_NOT_AVAILABLE                            51228

// ---------------------------------------------------------------- time          51400
#define IDS_TIME_TABLE_TITLE                            51400
#define IDS_TIME_LABEL_FREQUENCY                        51401
#define IDS_TIME_LABEL_NOW                              51402
#define IDS_TIME_LABEL_TICKS_PER_MICROSECOND            51403
#define IDS_TIME_LABEL_TICKS_PER_MILLISECOND            51404
#define IDS_TIME_LABEL_TICKS_PER_SECOND                 51405
#define IDS_TIME_LABEL_TIME_UNTIL_WRAP                  51406
#define IDS_TIME_LABEL_SYSTEM_TIMER_MINIMUM             51407
#define IDS_TIME_LABEL_SYSTEM_TIMER_MAXIMUM             51408
#define IDS_TIME_LABEL_SYSTEM_TIMER_CURRENT             51409

// ---------------------------------------------------------------- system exclusive 51500
#define IDS_CMD_SYSEX                                   51500
#define IDS_CMD_SYSEX_SEND_FILE                         51501
#define IDS_CMD_SYSEX_RECEIVE_FILE                      51502
#define IDS_OPT_SYSEX_OUTPUT_FILE_ARGUMENT              51503
#define IDS_OPT_SYSEX_GROUP_NUMBER                      51504
#define IDS_OPT_SYSEX_OVERWRITE                         51505
#define IDS_SYSEX_SENDING                               51506
#define IDS_SYSEX_SEND_PROGRESS                         51507
#define IDS_SYSEX_SEND_SUCCEEDED                        51508
#define IDS_SYSEX_SEND_FAILED                           51509
#define IDS_SYSEX_RECEIVING                             51510
#define IDS_SYSEX_RECEIVE_PROGRESS                      51511
#define IDS_SYSEX_RECEIVE_SAVED                         51512
#define IDS_SYSEX_RECEIVE_NOTHING                       51513
#define IDS_SYSEX_FILE_EXISTS                           51514
#define IDS_SYSEX_MESSAGES                              51515
#define IDS_SYSEX_RECEIVING_FROM                        51516
#define IDS_OPT_SYSEX_TIMEOUT                           51517
#define IDS_SYSEX_RECEIVE_TIMED_OUT                     51518

// ---------------------------------------------------------------- raw properties  51600
#define IDS_EP_RAW_NULL                                 51600
#define IDS_EP_RAW_BYTE_COUNT                           51601

// ---------------------------------------------------------------- api mode        51700
#define IDS_CMD_API_MODE                                51700
#define IDS_CMD_API_MODE_GET                            51701
#define IDS_CMD_API_MODE_SET                            51702
#define IDS_OPT_API_MODE_VALUE_ARGUMENT                 51703
#define IDS_API_MODE_TABLE_TITLE                        51704
#define IDS_API_MODE_LABEL_EFFECTIVE                    51705
#define IDS_API_MODE_LABEL_SDK_REPORTED                 51706
#define IDS_API_MODE_FULL                               51707
#define IDS_API_MODE_LEGACY                             51708
#define IDS_API_MODE_HYBRID                             51709
#define IDS_API_MODE_MISMATCH                           51710
#define IDS_API_MODE_NEEDS_ELEVATION                    51711
#define IDS_API_MODE_SET_SUCCEEDED                      51712
#define IDS_API_MODE_SET_FAILED                         51713
#define IDS_API_MODE_RESTART_REQUIRED                   51714
#define IDS_API_MODE_UNCHANGED                          51715

// ---------------------------------------------------------------- endpoint customize 51730
#define IDS_CMD_EP_CUSTOMIZE                            51730
#define IDS_OPT_CUSTOMIZE_PORT_NAMING                   51731
#define IDS_OPT_CUSTOMIZE_NOTE_OFF_TRANSLATION          51732
#define IDS_OPT_CUSTOMIZE_MPE                           51733
#define IDS_OPT_CUSTOMIZE_CC_INTERVAL                   51734
#define IDS_OPT_CUSTOMIZE_OUTPUT_LATENCY                51735
#define IDS_CUSTOMIZE_NOTHING_TO_DO                     51736
#define IDS_CUSTOMIZE_APPLIED                           51737
#define IDS_CUSTOMIZE_SAVED                             51738
#define IDS_CUSTOMIZE_FAILED                            51739
#define IDS_CUSTOMIZE_TEMPORARY_NOTE                    51740
#define IDS_CUSTOMIZE_RECONNECT_NOTE                    51741
#define IDS_CUSTOMIZE_BACKUP_WRITTEN                    51742
#define IDS_CUSTOMIZE_SAVE_FAILED                       51743
#define IDS_ERROR_INVALID_CC_INTERVAL                   51744
#define IDS_CUSTOMIZE_NOT_SUPPORTED                     51745

// ---------------------------------------------------------------- endpoint ids     51760
#define IDS_CMD_EP_SHORT_ID                             51760
#define IDS_CMD_EP_FULL_ID                              51761
#define IDS_OPT_ID_VALUE_ARGUMENT                       51762
#define IDS_ERROR_NOT_AN_ENDPOINT_ID                    51763
#define IDS_EP_SECTION_IN_USE_BY                        51764
#define IDS_EP_IN_USE_BY_NOBODY                         51765

// ---------------------------------------------------------------- beat clock       51780
#define IDS_CMD_EP_SEND_CLOCK                           51780
#define IDS_OPT_CLOCK_TEMPO                             51781
#define IDS_OPT_CLOCK_PPQN                              51782
#define IDS_OPT_CLOCK_GROUPS                            51783
#define IDS_OPT_CLOCK_SEND_START                        51784
#define IDS_OPT_CLOCK_SEND_STOP                         51785
#define IDS_ERROR_CLOCK_NO_GROUPS                       51786
#define IDS_ERROR_CLOCK_TEMPO_RANGE                     51787
#define IDS_ERROR_CLOCK_PPQN_RANGE                      51788
#define IDS_CLOCK_LABEL_TEMPO                           51789
#define IDS_CLOCK_LABEL_PPQN                            51790
#define IDS_CLOCK_LABEL_INTERVAL                        51791
#define IDS_CLOCK_PRESS_ESCAPE                          51792
#define IDS_CLOCK_STOPPED                               51793
#define IDS_CLOCK_LABEL_PULSES_SENT                     51794
#define IDS_CLOCK_DRAINING                              51795

// ---------------------------------------------------------------- monitor capture  51810
#define IDS_CAPTURE_TO                                  51813
#define IDS_CAPTURE_FAILED                              51814
#define IDS_CAPTURE_CLOSED                              51815

// ---------------------------------------------------------------- loopback mute    51830
#define IDS_CMD_LOOPBACK_MUTE                           51830
#define IDS_CMD_LOOPBACK_UNMUTE                         51831
#define IDS_LOOPBACK_MUTED                              51832
#define IDS_LOOPBACK_UNMUTED                            51833
#define IDS_LOOPBACK_MUTE_FAILED                        51834
#define IDS_LABEL_MUTED                                 51835

// ---------------------------------------------------------------- network          51850
#define IDS_CMD_NETWORK                                 51850
#define IDS_CMD_NET_HOSTS                               51851
#define IDS_CMD_NET_CLIENTS                             51852
#define IDS_CMD_NET_BROWSE                              51853
#define IDS_CMD_NET_PENDING                             51854
#define IDS_CMD_NET_STATUS                              51855
#define IDS_NET_NOT_AVAILABLE                           51856
#define IDS_NET_NO_HOSTS                                51857
#define IDS_NET_NO_CLIENTS                              51858
#define IDS_NET_NO_ADVERTISED                           51859
#define IDS_NET_NO_PENDING                              51860
#define IDS_NET_HOSTS_TABLE_TITLE                       51861
#define IDS_NET_CLIENTS_TABLE_TITLE                     51862
#define IDS_NET_ADVERTISED_TABLE_TITLE                  51863
#define IDS_NET_PENDING_TABLE_TITLE                     51864
#define IDS_NET_CONNECTIONS_TABLE_TITLE                 51865
#define IDS_NET_LABEL_HOST_ID                           51866
#define IDS_NET_LABEL_CLIENT_ID                         51867
#define IDS_NET_LABEL_ENABLED                           51868
#define IDS_NET_LABEL_STARTED                           51869
#define IDS_NET_LABEL_PORT                              51870
#define IDS_NET_LABEL_CONFIGURED_PORT                   51871
#define IDS_NET_LABEL_SERVICE_INSTANCE                  51872
#define IDS_NET_LABEL_PRODUCT_INSTANCE_ID               51873
#define IDS_NET_LABEL_REMOTE_POLICY                     51874
#define IDS_NET_LABEL_CREATE_MIDI1_PORTS                51875
#define IDS_NET_LABEL_SESSION_ACTIVE                    51876
#define IDS_NET_LABEL_ENTRY_STATE                       51877
#define IDS_NET_LABEL_REMOTE                            51878
#define IDS_NET_LABEL_LOCAL                             51879
#define IDS_NET_LABEL_LATENCY                           51880
#define IDS_NET_LABEL_RETRANSMITS                       51881
#define IDS_NET_LABEL_PACKETS                           51882
#define IDS_NET_LABEL_MATCH_ID                          51883
#define IDS_NET_LABEL_DIRECT                            51884
#define IDS_NET_LABEL_PENDING_APPROVAL                  51885
#define IDS_NET_LABEL_DNS_SERVICE_TYPE                  51886
#define IDS_NET_LABEL_DNS_DOMAIN                        51887
#define IDS_NET_LABEL_FULL_SERVICE_NAME                 51888
#define IDS_NET_LABEL_USED_PORT_FALLBACK                51889
#define IDS_NET_SUMMARY                                 51890
#define IDS_NET_PORT_FALLBACK_NOTE                      51891
#define IDS_NET_SETTINGS_TITLE                          51892
#define IDS_NET_LABEL_MAX_FEC                           51893
#define IDS_NET_LABEL_MAX_RETRANSMIT_BUFFER             51894
#define IDS_NET_LABEL_PING_INTERVAL                     51895
#define IDS_NET_LABEL_INVITATION_TIMEOUT                51896
#define IDS_NET_LABEL_MAX_HOST_CONNECTIONS              51897
#define IDS_NET_LABEL_SCAN_INTERVAL                     51898
#define IDS_NET_STATUS_TITLE                            51899

// ---------------------------------------------------------------- bluetooth status 51920
#define IDS_CMD_BT_STATUS                               51920
#define IDS_BT_LABEL_RADIO_NAME                         51921
#define IDS_BT_LABEL_RADIO_PRESENT                      51922
#define IDS_BT_LABEL_RADIO_ENABLED                      51923
#define IDS_BT_LABEL_LOW_ENERGY_SUPPORTED               51924
#define IDS_BT_LABEL_PERIPHERAL_SUPPORTED               51925
#define IDS_BT_LABEL_DEFAULT_RETENTION                  51926
#define IDS_BT_RADIO_UNAVAILABLE                        51927
#define IDS_BT_RADIO_OFF                                51928
#define IDS_BT_SAVED_TO_CONFIG                          51929
#define IDS_BT_TEMPORARY_NOTE                           51930
#define IDS_BT_SAVE_FAILED                              51931
#define IDS_BT_PENDING_TABLE_TITLE                      51932
#define IDS_BT_NO_PENDING                               51933
#define IDS_BT_LABEL_PAIRED                             51934
#define IDS_BT_RETENTION_ALWAYS                         51935
#define IDS_BT_RETENTION_IMMEDIATE                      51936
#define IDS_BT_RETENTION_SECONDS                        51937
#define IDS_BT_STATUS_TITLE                             51938

