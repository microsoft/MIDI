declare module "windows.devices.midi2" {
  export class MidiMessageStruct {
    word0: Number;
    word1: Number;
    word2: Number;
    word3: Number;
    constructor();
  }

  export enum Midi1ChannelVoiceMessageStatus {
    noteOff,
    noteOn,
    polyPressure,
    controlChange,
    programChange,
    channelPressure,
    pitchBend,
  }

  export enum Midi2ChannelVoiceMessageStatus {
    registeredPerNoteController,
    assignablePerNoteController,
    registeredController,
    assignableController,
    relativeRegisteredController,
    relativeAssignableController,
    perNotePitchBend,
    noteOff,
    noteOn,
    polyPressure,
    controlChange,
    programChange,
    channelPressure,
    pitchBend,
    perNoteManagement,
  }

  export enum MidiEndpointConnectionSharing {
    unknown,
    multiClient,
    exclusive,
  }

  export enum MidiEndpointConnectionSharingPreference {
    default,
    preferMultiClient,
    requireMultiClient,
    preferExclusiveMode,
    requireExclusiveMode,
  }

  export enum MidiEndpointDeviceInformationFilter {
    includeClientUmpNative,
    includeClientByteStreamNative,
    includeVirtualDeviceResponder,
    includeDiagnosticLoopback,
    includeDiagnosticPing,
  }

  export enum MidiEndpointDeviceInformationSortOrder {
    none,
    name,
    deviceInstanceId,
    containerThenName,
    containerThenDeviceInstanceId,
  }

  export enum MidiEndpointDevicePurpose {
    normalMessageEndpoint,
    virtualDeviceResponder,
    inBoxGeneralMidiSynth,
    diagnosticLoopback,
    diagnosticPing,
  }

  export enum MidiEndpointDiscoveryFilterFlags {
    none,
    requestEndpointInformation,
    requestDeviceIdentity,
    requestEndpointName,
    requestProductInstanceId,
    requestStreamConfiguration,
  }

  export enum MidiEndpointNativeDataFormat {
    unknown,
    byteStream,
    universalMidiPacket,
  }

  export enum MidiFunctionBlockDirection {
    undefined,
    blockInput,
    blockOutput,
    bidirectional,
  }

  export enum MidiFunctionBlockDiscoveryFilterFlags {
    none,
    requestFunctionBlockInformation,
    requestFunctionBlockName,
  }

  export enum MidiFunctionBlockMidi10 {
    not10,
    yesBandwidthUnrestricted,
    yesBandwidthRestricted,
    reserved,
  }

  export enum MidiFunctionBlockUIHint {
    unknown,
    receiver,
    sender,
    bidirectional,
  }

  export enum MidiGroupTerminalBlockDirection {
    bidirectional,
    blockInput,
    blockOutput,
  }

  export enum MidiGroupTerminalBlockProtocol {
    unknown,
    midi1Message64,
    midi1Message64WithJitterReduction,
    midi1Message128,
    midi1Message128WithJitterReduction,
    midi2,
    midi2WithJitterReduction,
  }

  export enum MidiMessageType {
    utilityMessage32,
    systemCommon32,
    midi1ChannelVoice32,
    dataMessage64,
    midi2ChannelVoice64,
    dataMessage128,
    futureReserved632,
    futureReserved732,
    futureReserved864,
    futureReserved964,
    futureReservedA64,
    futureReservedB96,
    futureReservedC96,
    flexData128,
    futureReservedE128,
    stream128,
  }

  export enum MidiPacketType {
    unknownOrInvalid,
    universalMidiPacket32,
    universalMidiPacket64,
    universalMidiPacket96,
    universalMidiPacket128,
  }

  export enum MidiProtocol {
    default,
    midi1,
    midi2,
  }

  export enum MidiSendMessageResult {
    success,
    errorBufferFull,
    errorInvalidMessageTypeForWordCount,
    errorInvalidMessageOther,
    errorSendDataIndexOutOfRange,
    errorEndpointConnectionClosedOrInvalid,
    errorUnexpected,
  }

  export enum MidiSystemExclusive8Status {
    completeMessageInSingleUmp,
    startUmp,
    continueUmp,
    endUmp,
  }

  export class IMidiEndpointConnectionSource {
    constructor();

  }

  export class IMidiEndpointDefinedConnectionSettings {
    isDirty: Boolean;
    settingsJson: String;
    constructor();

  }

  export class IMidiEndpointMessageProcessingPlugin {
    id: String;
    isEnabled: Boolean;
    name: String;
    tag: Object;
    constructor();

    initialize(endpointConnection: IMidiEndpointConnectionSource): void;

    onEndpointConnectionOpened(): void;

    processIncomingMessage(args: MidiMessageReceivedEventArgs, skipFurtherListeners: Boolean, skipMainMessageReceivedEvent: Boolean): void;

    cleanup(): void;

  }

  export class IMidiMessageReceivedEventSource {
    constructor();

    addListener(type: "MessageReceived", listener: (ev: Event) => void): void ;
    removeListener(type: "MessageReceived", listener: (ev: Event) => void): void ;
    on(type: "MessageReceived", listener: (ev: Event) => void): void ;
    off(type: "MessageReceived", listener: (ev: Event) => void): void ;
    
    addListener(type: string, listener: (ev: Event) => void): void ;
    removeListener(type: string, listener: (ev: Event) => void): void ;
    on(type: string, listener: (ev: Event) => void): void ;
    off(type: string, listener: (ev: Event) => void): void ;
    

  }

  export class IMidiTransportSettingsData {
    isDirty: Boolean;
    settingsJson: String;
    constructor();

  }

  export class IMidiUniversalPacket {
    messageType: MidiMessageType;
    packetType: MidiPacketType;
    timestamp: Number;
    constructor();

    peekFirstWord(): Number;

  }

  export class MidiChannel {
    static labelFull: String;
    static labelShort: String;
    index: Number;
    numberForDisplay: Number;
    constructor();
    constructor(index: Number);

    static isValidChannelIndex(index: Number): Boolean;


  }

  export class MidiChannelEndpointListener {
    preventFiringMainMessageReceivedEvent: Boolean;
    preventCallingFurtherListeners: Boolean;
    includeGroup: MidiGroup;
    includeChannels: Object;
    tag: Object;
    name: String;
    isEnabled: Boolean;
    id: String;
    constructor();

    initialize(endpointConnection: IMidiEndpointConnectionSource): void;

    onEndpointConnectionOpened(): void;

    processIncomingMessage(args: MidiMessageReceivedEventArgs, skipFurtherListeners: Boolean, skipMainMessageReceivedEvent: Boolean): void;

    cleanup(): void;

    addListener(type: "MessageReceived", listener: (ev: Event) => void): void ;
    removeListener(type: "MessageReceived", listener: (ev: Event) => void): void ;
    on(type: "MessageReceived", listener: (ev: Event) => void): void ;
    off(type: "MessageReceived", listener: (ev: Event) => void): void ;
    
    addListener(type: string, listener: (ev: Event) => void): void ;
    removeListener(type: string, listener: (ev: Event) => void): void ;
    on(type: string, listener: (ev: Event) => void): void ;
    off(type: string, listener: (ev: Event) => void): void ;
    

  }

  export class MidiClock {
    static now: Number;
    static timestampFrequency: Number;
    constructor();

    static convertTimestampToMicroseconds(timestampValue: Number): Number;


    static convertTimestampToMilliseconds(timestampValue: Number): Number;


    static offsetTimestampByTicks(timestampValue: Number, offsetTicks: Number): Number;


    static offsetTimestampByMicroseconds(timestampValue: Number, offsetMicroseconds: Number): Number;


    static offsetTimestampByMilliseconds(timestampValue: Number, offsetMilliseconds: Number): Number;


  }

  export class MidiEndpointConnection {
    tag: Object;
    connectionId: String;
    endpointDeviceId: String;
    isOpen: Boolean;
    messageProcessingPlugins: Object;
    settings: IMidiEndpointDefinedConnectionSettings;
    constructor();

    static getDeviceSelector(): String;


    open(): Boolean;

    sendMessagePacket(message: IMidiUniversalPacket): MidiSendMessageResult;

    sendMessageStruct(timestamp: Number, message: MidiMessageStruct, wordCount: Number): MidiSendMessageResult;

    sendMessageWordArray(timestamp: Number, words: Array<Number>, startIndex: Number, wordCount: Number): MidiSendMessageResult;

    sendMessageWords(timestamp: Number, word0: Number): MidiSendMessageResult;
    sendMessageWords(timestamp: Number, word0: Number, word1: Number): MidiSendMessageResult;
    sendMessageWords(timestamp: Number, word0: Number, word1: Number, word2: Number): MidiSendMessageResult;
    sendMessageWords(timestamp: Number, word0: Number, word1: Number, word2: Number, word3: Number): MidiSendMessageResult;

    sendMessageBuffer(timestamp: Number, buffer: Object, byteOffset: Number, byteLength: Number): MidiSendMessageResult;

    close(): void;
    addListener(type: "MessageReceived", listener: (ev: Event) => void): void ;
    removeListener(type: "MessageReceived", listener: (ev: Event) => void): void ;
    on(type: "MessageReceived", listener: (ev: Event) => void): void ;
    off(type: "MessageReceived", listener: (ev: Event) => void): void ;
    
    addListener(type: string, listener: (ev: Event) => void): void ;
    removeListener(type: string, listener: (ev: Event) => void): void ;
    on(type: string, listener: (ev: Event) => void): void ;
    off(type: string, listener: (ev: Event) => void): void ;
    

  }

  export class MidiEndpointConnectionOptions {
    requestedStreamConfiguration: MidiStreamConfigurationRequestedSettings;
    connectionSharingPreference: MidiEndpointConnectionSharingPreference;
    constructor();

  }

  export class MidiEndpointDeviceInformation {
    static diagnosticsLoopbackAEndpointId: String;
    static diagnosticsLoopbackBEndpointId: String;
    static endpointInterfaceClass: String;
    configuredProtocol: MidiProtocol;
    containerId: String;
    description: String;
    deviceInstanceId: String;
    endpointPurpose: MidiEndpointDevicePurpose;
    endpointSuppliedName: String;
    functionBlocks: Object;
    groupTerminalBlocks: Object;
    hasStaticFunctionBlocks: Boolean;
    id: String;
    largeImagePath: String;
    name: String;
    nativeDataFormat: MidiEndpointNativeDataFormat;
    productInstanceId: String;
    properties: Object;
    smallImagePath: String;
    specificationVersionMajor: Number;
    specificationVersionMinor: Number;
    supportsMidi10Protocol: Boolean;
    supportsMidi20Protocol: Boolean;
    supportsMultiClient: Boolean;
    supportsReceivingJRTimestamps: Boolean;
    supportsSendingJRTimestamps: Boolean;
    transportId: String;
    transportMnemonic: String;
    transportSuppliedName: String;
    uniqueIdentifier: String;
    userSuppliedName: String;
    constructor();

    static createFromId(id: String): MidiEndpointDeviceInformation;


    static findAll(): Object;
    static findAll(sortOrder: MidiEndpointDeviceInformationSortOrder): Object;
    static findAll(sortOrder: MidiEndpointDeviceInformationSortOrder, endpointFilter: MidiEndpointDeviceInformationFilter): Object;


    static getAdditionalPropertiesList(): Object;


    static deviceMatchesFilter(deviceInformation: MidiEndpointDeviceInformation, endpointFilter: MidiEndpointDeviceInformationFilter): Boolean;


    getParentDeviceInformation(): Object;

    getContainerInformation(): Object;

    updateFromDeviceInformation(deviceInformation: Object): Boolean;

    updateFromDeviceInformationUpdate(deviceInformationUpdate: Object): Boolean;

  }

  export class MidiEndpointDeviceWatcher {
    enumeratedEndpointDevices: Object;
    status: Number;
    constructor();

    static createWatcher(endpointFilter: MidiEndpointDeviceInformationFilter): MidiEndpointDeviceWatcher;


    start(): void;

    stop(): void;

    addListener(type: "Added", listener: (ev: Event) => void): void ;
    removeListener(type: "Added", listener: (ev: Event) => void): void ;
    on(type: "Added", listener: (ev: Event) => void): void ;
    off(type: "Added", listener: (ev: Event) => void): void ;
    
    addListener(type: "EnumerationCompleted", listener: (ev: Event) => void): void ;
    removeListener(type: "EnumerationCompleted", listener: (ev: Event) => void): void ;
    on(type: "EnumerationCompleted", listener: (ev: Event) => void): void ;
    off(type: "EnumerationCompleted", listener: (ev: Event) => void): void ;
    
    addListener(type: "Removed", listener: (ev: Event) => void): void ;
    removeListener(type: "Removed", listener: (ev: Event) => void): void ;
    on(type: "Removed", listener: (ev: Event) => void): void ;
    off(type: "Removed", listener: (ev: Event) => void): void ;
    
    addListener(type: "Stopped", listener: (ev: Event) => void): void ;
    removeListener(type: "Stopped", listener: (ev: Event) => void): void ;
    on(type: "Stopped", listener: (ev: Event) => void): void ;
    off(type: "Stopped", listener: (ev: Event) => void): void ;
    
    addListener(type: "Updated", listener: (ev: Event) => void): void ;
    removeListener(type: "Updated", listener: (ev: Event) => void): void ;
    on(type: "Updated", listener: (ev: Event) => void): void ;
    off(type: "Updated", listener: (ev: Event) => void): void ;
    
    addListener(type: string, listener: (ev: Event) => void): void ;
    removeListener(type: string, listener: (ev: Event) => void): void ;
    on(type: string, listener: (ev: Event) => void): void ;
    off(type: string, listener: (ev: Event) => void): void ;
    

  }

  export class MidiFunctionBlock {
    direction: MidiFunctionBlockDirection;
    firstGroupIndex: Number;
    groupCount: Number;
    isActive: Boolean;
    maxSystemExclusive8Streams: Number;
    midi10Connection: MidiFunctionBlockMidi10;
    midiCIMessageVersionFormat: Number;
    name: String;
    number: Number;
    uIHint: MidiFunctionBlockUIHint;
    constructor();

    includesGroup(group: MidiGroup): Boolean;

    updateFromJson(json: Object): Boolean;

  }

  export class MidiGroup {
    static labelFull: String;
    static labelShort: String;
    index: Number;
    numberForDisplay: Number;
    constructor();
    constructor(index: Number);

    static isValidGroupIndex(index: Number): Boolean;


  }

  export class MidiGroupEndpointListener {
    tag: Object;
    name: String;
    isEnabled: Boolean;
    id: String;
    includeGroups: Object;
    constructor();

    initialize(endpointConnection: IMidiEndpointConnectionSource): void;

    onEndpointConnectionOpened(): void;

    processIncomingMessage(args: MidiMessageReceivedEventArgs, skipFurtherListeners: Boolean, skipMainMessageReceivedEvent: Boolean): void;

    cleanup(): void;

    addListener(type: "MessageReceived", listener: (ev: Event) => void): void ;
    removeListener(type: "MessageReceived", listener: (ev: Event) => void): void ;
    on(type: "MessageReceived", listener: (ev: Event) => void): void ;
    off(type: "MessageReceived", listener: (ev: Event) => void): void ;
    
    addListener(type: string, listener: (ev: Event) => void): void ;
    removeListener(type: string, listener: (ev: Event) => void): void ;
    on(type: string, listener: (ev: Event) => void): void ;
    off(type: string, listener: (ev: Event) => void): void ;
    

  }

  export class MidiGroupTerminalBlock {
    calculatedMaxDeviceInputBandwidthBitsPerSecond: Number;
    calculatedMaxDeviceOutputBandwidthBitsPerSecond: Number;
    direction: MidiGroupTerminalBlockDirection;
    firstGroupIndex: Number;
    groupCount: Number;
    maxDeviceInputBandwidthIn4KBitsPerSecondUnits: Number;
    maxDeviceOutputBandwidthIn4KBitsPerSecondUnits: Number;
    name: String;
    number: Number;
    protocol: MidiGroupTerminalBlockProtocol;
    constructor();

    includesGroup(group: MidiGroup): Boolean;

  }

  export class MidiMessage128 {
    word3: Number;
    word2: Number;
    word1: Number;
    word0: Number;
    timestamp: Number;
    messageType: MidiMessageType;
    packetType: MidiPacketType;
    constructor();
    constructor(timestamp: Number, word0: Number, word1: Number, word2: Number, word3: Number);
    constructor(timestamp: Number, words: Array<Number>);

    peekFirstWord(): Number;

    toString(): String;

  }

  export class MidiMessage32 {
    word0: Number;
    timestamp: Number;
    messageType: MidiMessageType;
    packetType: MidiPacketType;
    constructor();
    constructor(timestamp: Number, word0: Number);

    peekFirstWord(): Number;

    toString(): String;

  }

  export class MidiMessage64 {
    word1: Number;
    word0: Number;
    timestamp: Number;
    messageType: MidiMessageType;
    packetType: MidiPacketType;
    constructor();
    constructor(timestamp: Number, word0: Number, word1: Number);
    constructor(timestamp: Number, words: Array<Number>);

    peekFirstWord(): Number;

    toString(): String;

  }

  export class MidiMessage96 {
    word2: Number;
    word1: Number;
    word0: Number;
    timestamp: Number;
    messageType: MidiMessageType;
    packetType: MidiPacketType;
    constructor();
    constructor(timestamp: Number, word0: Number, word1: Number, word2: Number);
    constructor(timestamp: Number, words: Array<Number>);

    peekFirstWord(): Number;

    toString(): String;

  }

  export class MidiMessageBuilder {
    constructor();

    static buildUtilityMessage(timestamp: Number, status: Number, dataOrReserved: Number): MidiMessage32;


    static buildSystemMessage(timestamp: Number, groupIndex: Number, status: Number, midi1Byte2: Number, midi1Byte3: Number): MidiMessage32;


    static buildMidi1ChannelVoiceMessage(timestamp: Number, groupIndex: Number, status: Midi1ChannelVoiceMessageStatus, channel: Number, byte3: Number, byte4: Number): MidiMessage32;


    static buildSystemExclusive7Message(timestamp: Number, groupIndex: Number, status: Number, numberOfBytes: Number, dataByte0: Number, dataByte1: Number, dataByte2: Number, dataByte3: Number, dataByte4: Number, dataByte5: Number): MidiMessage64;


    static buildMidi2ChannelVoiceMessage(timestamp: Number, groupIndex: Number, status: Midi2ChannelVoiceMessageStatus, channel: Number, index: Number, data: Number): MidiMessage64;


    static buildSystemExclusive8Message(timestamp: Number, groupIndex: Number, status: MidiSystemExclusive8Status, numberOfValidDataBytesThisMessage: Number, streamId: Number, dataByte00: Number, dataByte01: Number, dataByte02: Number, dataByte03: Number, dataByte04: Number, dataByte05: Number, dataByte06: Number, dataByte07: Number, dataByte08: Number, dataByte09: Number, dataByte10: Number, dataByte11: Number, dataByte12: Number): MidiMessage128;


    static buildMixedDataSetChunkHeaderMessage(timestamp: Number, groupIndex: Number, mdsId: Number, numberValidDataBytesInThisChunk: Number, numberChunksInMixedDataSet: Number, numberOfThisChunk: Number, manufacturerId: Number, deviceId: Number, subId1: Number, subId2: Number): MidiMessage128;


    static buildMixedDataSetChunkDataMessage(timestamp: Number, groupIndex: Number, mdsId: Number, dataByte00: Number, dataByte01: Number, dataByte02: Number, dataByte03: Number, dataByte04: Number, dataByte05: Number, dataByte06: Number, dataByte07: Number, dataByte08: Number, dataByte09: Number, dataByte10: Number, dataByte11: Number, dataByte12: Number, dataByte13: Number): MidiMessage128;


    static buildFlexDataMessage(timestamp: Number, groupIndex: Number, form: Number, address: Number, channel: Number, statusBank: Number, status: Number, word1Data: Number, word2Data: Number, word3Data: Number): MidiMessage128;


    static buildStreamMessage(timestamp: Number, form: Number, status: Number, word0RemainingData: Number, word1Data: Number, word2Data: Number, word3Data: Number): MidiMessage128;


  }

  export class MidiMessageConverter {
    constructor();

    static convertMidi1Message(timestamp: Number, groupIndex: Number, statusByte: Number): MidiMessage32;
    static convertMidi1Message(timestamp: Number, groupIndex: Number, statusByte: Number, dataByte1: Number): MidiMessage32;
    static convertMidi1Message(timestamp: Number, groupIndex: Number, statusByte: Number, dataByte1: Number, dataByte2: Number): MidiMessage32;


    static convertMidi1ChannelPressureMessage(timestamp: Number, groupIndex: Number, originalMessage: Object): MidiMessage32;


    static convertMidi1ContinueMessage(timestamp: Number, groupIndex: Number, originalMessage: Object): MidiMessage32;


    static convertMidi1NoteOffMessage(timestamp: Number, groupIndex: Number, originalMessage: Object): MidiMessage32;


    static convertMidi1NoteOnMessage(timestamp: Number, groupIndex: Number, originalMessage: Object): MidiMessage32;


    static convertMidi1PitchBendChangeMessage(timestamp: Number, groupIndex: Number, originalMessage: Object): MidiMessage32;


    static convertMidi1PolyphonicKeyPressureMessage(timestamp: Number, groupIndex: Number, originalMessage: Object): MidiMessage32;


    static convertMidi1ProgramChangeMessage(timestamp: Number, groupIndex: Number, originalMessage: Object): MidiMessage32;


    static convertMidi1SongPositionPointerMessage(timestamp: Number, groupIndex: Number, originalMessage: Object): MidiMessage32;


    static convertMidi1SongSelectMessage(timestamp: Number, groupIndex: Number, originalMessage: Object): MidiMessage32;


    static convertMidi1StartMessage(timestamp: Number, groupIndex: Number, originalMessage: Object): MidiMessage32;


    static convertMidi1StopMessage(timestamp: Number, groupIndex: Number, originalMessage: Object): MidiMessage32;


    static convertMidi1SystemResetMessage(timestamp: Number, groupIndex: Number, originalMessage: Object): MidiMessage32;


    static convertMidi1TimeCodeMessage(timestamp: Number, groupIndex: Number, originalMessage: Object): MidiMessage32;


    static convertMidi1TimingClockMessage(timestamp: Number, groupIndex: Number, originalMessage: Object): MidiMessage32;


    static convertMidi1TuneRequestMessage(timestamp: Number, groupIndex: Number, originalMessage: Object): MidiMessage32;


  }

  export class MidiMessageProcessingPluginInformation {
    author: String;
    classId: String;
    clientConfigurationAssemblyName: String;
    description: String;
    iconPath: String;
    isClientConfigurable: Boolean;
    isEnabled: Boolean;
    isSystemManaged: Boolean;
    name: String;
    registryKey: String;
    servicePluginFileName: String;
    shortName: String;
    constructor();

  }

  export class MidiMessageReceivedEventArgs {
    messageType: MidiMessageType;
    packetType: MidiPacketType;
    timestamp: Number;
    constructor();

    peekFirstWord(): Number;

    getMessagePacket(): IMidiUniversalPacket;

    fillWords(word0: Number, word1: Number, word2: Number, word3: Number): Number;

    fillMessageStruct(message: MidiMessageStruct): Number;

    fillMessage32(message: MidiMessage32): Boolean;

    fillMessage64(message: MidiMessage64): Boolean;

    fillMessage96(message: MidiMessage96): Boolean;

    fillMessage128(message: MidiMessage128): Boolean;

    fillWordArray();
    fillByteArray();
    fillBuffer(buffer: Object, byteOffset: Number): Number;

  }

  export class MidiMessageTranslator {
    constructor();

    static upscaleMidi1ChannelVoiceMessageToMidi2(originalMessage: MidiMessage32): MidiMessage64;
    static upscaleMidi1ChannelVoiceMessageToMidi2(timestamp: Number, groupIndex: Number, statusByte: Number): MidiMessage64;
    static upscaleMidi1ChannelVoiceMessageToMidi2(timestamp: Number, groupIndex: Number, statusByte: Number, dataByte1: Number): MidiMessage64;
    static upscaleMidi1ChannelVoiceMessageToMidi2(timestamp: Number, groupIndex: Number, statusByte: Number, dataByte1: Number, dataByte2: Number): MidiMessage64;


    static downscaleMidi2ChannelVoiceMessageToMidi1(originalMessage: MidiMessage64): MidiMessage32;


  }

  export class MidiMessageTypeEndpointListener {
    tag: Object;
    name: String;
    isEnabled: Boolean;
    id: String;
    includeMessageTypes: Object;
    constructor();

    initialize(endpointConnection: IMidiEndpointConnectionSource): void;

    onEndpointConnectionOpened(): void;

    processIncomingMessage(args: MidiMessageReceivedEventArgs, skipFurtherListeners: Boolean, skipMainMessageReceivedEvent: Boolean): void;

    cleanup(): void;

    addListener(type: "MessageReceived", listener: (ev: Event) => void): void ;
    removeListener(type: "MessageReceived", listener: (ev: Event) => void): void ;
    on(type: "MessageReceived", listener: (ev: Event) => void): void ;
    off(type: "MessageReceived", listener: (ev: Event) => void): void ;
    
    addListener(type: string, listener: (ev: Event) => void): void ;
    removeListener(type: string, listener: (ev: Event) => void): void ;
    on(type: string, listener: (ev: Event) => void): void ;
    off(type: string, listener: (ev: Event) => void): void ;
    

  }

  export class MidiMessageUtility {
    constructor();

    static validateMessage32MessageType(word0: Number): Boolean;


    static validateMessage64MessageType(word0: Number): Boolean;


    static validateMessage96MessageType(word0: Number): Boolean;


    static validateMessage128MessageType(word0: Number): Boolean;


    static getMessageTypeFromMessageFirstWord(word0: Number): MidiMessageType;


    static getPacketTypeFromMessageFirstWord(word0: Number): MidiPacketType;


    static messageTypeHasGroupField(messageType: MidiMessageType): Boolean;


    static replaceGroupInMessageFirstWord(word0: Number, newGroup: MidiGroup): Number;


    static getGroupFromMessageFirstWord(word0: Number): MidiGroup;


    static getStatusFromUtilityMessage(word0: Number): Number;


    static getStatusFromMidi1ChannelVoiceMessage(word0: Number): Midi1ChannelVoiceMessageStatus;


    static getStatusFromMidi2ChannelVoiceMessageFirstWord(word0: Number): Midi2ChannelVoiceMessageStatus;


    static getStatusBankFromFlexDataMessageFirstWord(word0: Number): Number;


    static getStatusFromFlexDataMessageFirstWord(word0: Number): Number;


    static getStatusFromSystemCommonMessage(word0: Number): Number;


    static getStatusFromDataMessage64FirstWord(word0: Number): Number;


    static getNumberOfBytesFromDataMessage64FirstWord(word0: Number): Number;


    static getStatusFromDataMessage128FirstWord(word0: Number): Number;


    static getNumberOfBytesFromDataMessage128FirstWord(word0: Number): Number;


    static messageTypeHasChannelField(messageType: MidiMessageType): Boolean;


    static replaceChannelInMessageFirstWord(word0: Number, newChannel: MidiChannel): Number;


    static getChannelFromMessageFirstWord(word0: Number): MidiChannel;


    static getFormFromStreamMessageFirstWord(word0: Number): Number;


    static getStatusFromStreamMessageFirstWord(word0: Number): Number;


    static getMessageFriendlyNameFromFirstWord(word0: Number): String;


  }

  export class MidiService {
    constructor();

    static pingService(pingCount: Number): MidiServicePingResponseSummary;
    static pingService(pingCount: Number, timeoutMilliseconds: Number): MidiServicePingResponseSummary;


    static getInstalledTransportPlugins(): Object;


    static getInstalledMessageProcessingPlugins(): Object;


    static getOutgoingMessageQueueMaxMessageCapacity(): Number;


  }

  export class MidiServicePingResponse {
    clientDeltaTimestamp: Number;
    clientReceiveMidiTimestamp: Number;
    clientSendMidiTimestamp: Number;
    index: Number;
    serviceReportedMidiTimestamp: Number;
    sourceId: Number;
    constructor();

  }

  export class MidiServicePingResponseSummary {
    averagePingRoundTripMidiClock: Number;
    failureReason: String;
    responses: Object;
    success: Boolean;
    totalPingRoundTripMidiClock: Number;
    constructor();

  }

  export class MidiSession {
    connections: Object;
    id: String;
    isOpen: Boolean;
    name: String;
    settings: MidiSessionSettings;
    constructor();

    static createSession(sessionName: String): MidiSession;
    static createSession(sessionName: String, settings: MidiSessionSettings): MidiSession;


    createEndpointConnection(endpointDeviceId: String): MidiEndpointConnection;
    createEndpointConnection(endpointDeviceId: String, options: MidiEndpointConnectionOptions): MidiEndpointConnection;
    createEndpointConnection(endpointDeviceId: String, options: MidiEndpointConnectionOptions, settings: IMidiEndpointDefinedConnectionSettings): MidiEndpointConnection;

    createVirtualDeviceAndConnection(endpointName: String, endpointDeviceInstanceId: String): MidiEndpointConnection;

    disconnectEndpointConnection(endpointConnectionId: String): void;

    close(): void;
  }

  export class MidiSessionSettings {
    useMmcssThreads: Boolean;
    constructor();

  }

  export class MidiStreamConfigurationRequestedSettings {
    specificationVersionMinor: Number;
    specificationVersionMajor: Number;
    requestEndpointTransmitJitterReductionTimestamps: Boolean;
    requestEndpointReceiveJitterReductionTimestamps: Boolean;
    preferredMidiProtocol: MidiProtocol;
    constructor();

  }

  export class MidiStreamMessageBuilder {
    constructor();

    static buildEndpointDiscoveryMessage(timestamp: Number, umpVersionMajor: Number, umpVersionMinor: Number, requestFlags: MidiEndpointDiscoveryFilterFlags): MidiMessage128;


    static buildEndpointInformationNotificationMessage(timestamp: Number, umpVersionMajor: Number, umpVersionMinor: Number, hasStaticFunctionBlocks: Boolean, numberOfFunctionBlocks: Number, supportsMidi20Protocol: Boolean, supportsMidi10Protocol: Boolean, supportsReceivingJitterReductionTimestamps: Boolean, supportsSendingJitterReductionTimestamps: Boolean): MidiMessage128;


    static buildDeviceIdentityNotificationMessage(timestamp: Number, deviceManufacturerSysExIdByte1: Number, deviceManufacturerSysExIdByte2: Number, deviceManufacturerSysExIdByte3: Number, deviceFamilyLsb: Number, deviceFamilyMsb: Number, deviceFamilyModelNumberLsb: Number, deviceFamilyModelNumberMsb: Number, softwareRevisionLevelByte1: Number, softwareRevisionLevelByte2: Number, softwareRevisionLevelByte3: Number, softwareRevisionLevelByte4: Number): MidiMessage128;


    static buildEndpointNameNotificationMessages(timestamp: Number, name: String): Object;


    static buildProductInstanceIdNotificationMessages(timestamp: Number, productInstanceId: String): Object;


    static parseEndpointNameNotificationMessages(messages: Object): String;


    static parseProductInstanceIdNotificationMessages(messages: Object): String;


    static buildStreamConfigurationRequestMessage(timestamp: Number, protocol: Number, expectToReceiveJRTimestamps: Boolean, requestToSendJRTimestamps: Boolean): MidiMessage128;


    static buildStreamConfigurationNotificationMessage(timestamp: Number, protocol: Number, confirmationWillReceiveJRTimestamps: Boolean, confirmationSendJRTimestamps: Boolean): MidiMessage128;


    static buildFunctionBlockDiscoveryMessage(timestamp: Number, functionBlockNumber: Number, requestFlags: MidiFunctionBlockDiscoveryFilterFlags): MidiMessage128;


    static buildFunctionBlockInfoNotificationMessage(timestamp: Number, active: Boolean, functionBlockNumber: Number, uiHint: MidiFunctionBlockUIHint, midi10: MidiFunctionBlockMidi10, direction: MidiFunctionBlockDirection, firstGroup: Number, numberOfGroups: Number, midiCIVersionFormat: Number, maxNumberSysEx8Streams: Number): MidiMessage128;


    static buildFunctionBlockNameNotificationMessages(timestamp: Number, functionBlockNumber: Number, name: String): Object;


    static parseFunctionBlockNameNotificationMessages(messages: Object): String;


  }

  export class MidiTransportPluginInformation {
    author: String;
    classId: String;
    clientConfigurationAssemblyName: String;
    description: String;
    iconPath: String;
    isClientConfigurable: Boolean;
    isEnabled: Boolean;
    isRuntimeCreatable: Boolean;
    isSystemManaged: Boolean;
    mnemonic: String;
    name: String;
    registryKey: String;
    shortName: String;
    constructor();

  }

  export class MidiUniqueId {
    static labelFull: String;
    static labelShort: String;
    byte4: Number;
    byte3: Number;
    byte2: Number;
    byte1: Number;
    constructor();
    constructor(sevenBitByte1: Number, sevenBitByte2: Number, sevenBitByte3: Number, sevenBitByte4: Number);

  }

  export class MidiVirtualDevice {
    tag: Object;
    name: String;
    isEnabled: Boolean;
    id: String;
    suppressHandledMessages: Boolean;
    endpointProductInstanceId: String;
    endpointName: String;
    areFunctionBlocksStatic: Boolean;
    functionBlocks: Object;
    constructor();

    addFunctionBlock(block: MidiFunctionBlock): Boolean;

    updateFunctionBlock(block: MidiFunctionBlock): void;

    removeFunctionBlock(functionBlockNumber: Number): void;

    initialize(endpointConnection: IMidiEndpointConnectionSource): void;

    onEndpointConnectionOpened(): void;

    processIncomingMessage(args: MidiMessageReceivedEventArgs, skipFurtherListeners: Boolean, skipMainMessageReceivedEvent: Boolean): void;

    cleanup(): void;

  }

}



