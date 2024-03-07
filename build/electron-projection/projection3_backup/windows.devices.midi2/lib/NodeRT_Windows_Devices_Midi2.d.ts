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

  export enum MidiEndpointDeviceInformationFilters {
    includeClientUmpNative,
    includeClientByteStreamNative,
    includeVirtualDeviceResponder,
    includeDiagnosticLoopback,
    includeDiagnosticPing,
    allTypicalEndpoints,
  }

  export enum MidiEndpointDeviceInformationSortOrder {
    none,
    name,
    endpointDeviceId,
    deviceInstanceId,
    containerThenName,
    containerThenEndpointDeviceId,
    containerThenDeviceInstanceId,
    transportMnemonicThenName,
    transportMnemonicThenEndpointDeviceId,
    transportMnemonicThenDeviceInstanceId,
  }

  export enum MidiEndpointDevicePurpose {
    normalMessageEndpoint,
    virtualDeviceResponder,
    inBoxGeneralMidiSynth,
    diagnosticLoopback,
    diagnosticPing,
  }

  export enum MidiEndpointDiscoveryRequests {
    none,
    requestEndpointInfo,
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

  export enum MidiFunctionBlockDiscoveryRequests {
    none,
    requestFunctionBlockInfo,
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

  export enum MidiSendMessageResults {
    succeeded,
    failed,
    bufferFull,
    endpointConnectionClosedOrInvalid,
    invalidMessageTypeForWordCount,
    invalidMessageOther,
    dataIndexOutOfRange,
    timestampOutOfRange,
    messageListPartiallyProcessed,
    other,
  }

  export enum MidiServiceConfigurationResponseStatus {
    success,
    errorTargetNotFound,
    errorJsonNullOrEmpty,
    errorProcessingJson,
    errorNotImplemented,
    errorOther,
  }

  export enum MidiSystemExclusive8Status {
    completeMessageInSingleMessagePacket,
    startMessagePacket,
    continueMessagePacket,
    endMessagePacket,
  }

  export class IMidiEndpointConnectionSettings {
    settingsJson: String;
    constructor();

  }

  export class IMidiEndpointConnectionSource {
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

  export class IMidiServiceMessageProcessingPluginConfiguration {
    endpointDeviceId: String;
    isFromConfigurationFile: Boolean;
    messageProcessingPluginId: String;
    pluginInstanceId: String;
    settingsJson: Object;
    constructor();

  }

  export class IMidiServiceTransportPluginConfiguration {
    isFromConfigurationFile: Boolean;
    settingsJson: Object;
    transportId: String;
    constructor();

  }

  export class IMidiUniversalPacket {
    messageType: MidiMessageType;
    packetType: MidiPacketType;
    timestamp: Number;
    constructor();

    peekFirstWord(): Number;

    getAllWords(): Object;

    appendAllMessageWordsToList(targetList: Object): Number;

    fillBuffer(byteOffset: Number, buffer: Object): Number;

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
    static timestampConstantSendImmediately: Number;
    static timestampFrequency: Number;
    constructor();

    static convertTimestampToMicroseconds(timestampValue: Number): Number;


    static convertTimestampToMilliseconds(timestampValue: Number): Number;


    static convertTimestampToSeconds(timestampValue: Number): Number;


    static offsetTimestampByTicks(timestampValue: Number, offsetTicks: Number): Number;


    static offsetTimestampByMicroseconds(timestampValue: Number, offsetMicroseconds: Number): Number;


    static offsetTimestampByMilliseconds(timestampValue: Number, offsetMilliseconds: Number): Number;


    static offsetTimestampBySeconds(timestampValue: Number, offsetSeconds: Number): Number;


  }

  export class MidiEndpointConnection {
    tag: Object;
    connectionId: String;
    endpointDeviceId: String;
    isOpen: Boolean;
    messageProcessingPlugins: Object;
    settings: IMidiEndpointConnectionSettings;
    constructor();

    static getDeviceSelector(): String;


    static sendMessageSucceeded(sendResult: MidiSendMessageResults): Boolean;


    static sendMessageFailed(sendResult: MidiSendMessageResults): Boolean;


    open(): Boolean;

    addMessageProcessingPlugin(plugin: IMidiEndpointMessageProcessingPlugin): void;

    removeMessageProcessingPlugin(id: String): void;

    sendSingleMessagePacket(message: IMidiUniversalPacket): MidiSendMessageResults;

    sendSingleMessageStruct(timestamp: Number, wordCount: Number, message: MidiMessageStruct): MidiSendMessageResults;

    sendSingleMessageWordArray(timestamp: Number, startIndex: Number, wordCount: Number, words: Array<Number>): MidiSendMessageResults;

    sendSingleMessageWords(timestamp: Number, word0: Number): MidiSendMessageResults;
    sendSingleMessageWords(timestamp: Number, word0: Number, word1: Number): MidiSendMessageResults;
    sendSingleMessageWords(timestamp: Number, word0: Number, word1: Number, word2: Number): MidiSendMessageResults;
    sendSingleMessageWords(timestamp: Number, word0: Number, word1: Number, word2: Number, word3: Number): MidiSendMessageResults;

    sendSingleMessageBuffer(timestamp: Number, byteOffset: Number, byteCount: Number, buffer: Object): MidiSendMessageResults;

    sendMultipleMessagesWordList(timestamp: Number, words: Object): MidiSendMessageResults;

    sendMultipleMessagesWordArray(timestamp: Number, startIndex: Number, wordCount: Number, words: Array<Number>): MidiSendMessageResults;

    sendMultipleMessagesPacketList(messages: Object): MidiSendMessageResults;

    sendMultipleMessagesStructList(timestamp: Number, messages: Object): MidiSendMessageResults;

    sendMultipleMessagesStructArray(timestamp: Number, startIndex: Number, messageCount: Number, messages: Array<MidiMessageStruct>): MidiSendMessageResults;

    sendMultipleMessagesBuffer(timestamp: Number, byteOffset: Number, byteCount: Number, buffer: Object): MidiSendMessageResults;

    addListener(type: "MessageReceived", listener: (ev: Event) => void): void ;
    removeListener(type: "MessageReceived", listener: (ev: Event) => void): void ;
    on(type: "MessageReceived", listener: (ev: Event) => void): void ;
    off(type: "MessageReceived", listener: (ev: Event) => void): void ;
    
    addListener(type: string, listener: (ev: Event) => void): void ;
    removeListener(type: string, listener: (ev: Event) => void): void ;
    on(type: string, listener: (ev: Event) => void): void ;
    off(type: string, listener: (ev: Event) => void): void ;
    

  }

  export class MidiEndpointDeviceInformation {
    static diagnosticsLoopbackAEndpointId: String;
    static diagnosticsLoopbackBEndpointId: String;
    static endpointInterfaceClass: String;
    configuredProtocol: MidiProtocol;
    configuredToReceiveJRTimestamps: Boolean;
    configuredToSendJRTimestamps: Boolean;
    containerId: String;
    deviceIdentityDeviceFamilyLsb: Number;
    deviceIdentityDeviceFamilyModelNumberLsb: Number;
    deviceIdentityDeviceFamilyModelNumberMsb: Number;
    deviceIdentityDeviceFamilyMsb: Number;
    deviceIdentityLastUpdateTime: Date;
    deviceIdentitySoftwareRevisionLevel: Array<Number>;
    deviceIdentitySystemExclusiveId: Array<Number>;
    deviceInstanceId: String;
    endpointConfigurationLastUpdateTime: Date;
    endpointInformationLastUpdateTime: Date;
    endpointPurpose: MidiEndpointDevicePurpose;
    endpointSuppliedName: String;
    endpointSuppliedNameLastUpdateTime: Date;
    functionBlockCount: Number;
    functionBlocks: Object;
    functionBlocksLastUpdateTime: Date;
    groupTerminalBlocks: Object;
    hasStaticFunctionBlocks: Boolean;
    id: String;
    manufacturerName: String;
    name: String;
    nativeDataFormat: MidiEndpointNativeDataFormat;
    productInstanceId: String;
    productInstanceIdLastUpdateTime: Date;
    properties: Object;
    recommendedCCAutomationIntervalMS: Number;
    requiresNoteOffTranslation: Boolean;
    specificationVersionMajor: Number;
    specificationVersionMinor: Number;
    supportsMidi10Protocol: Boolean;
    supportsMidi20Protocol: Boolean;
    supportsMultiClient: Boolean;
    supportsReceivingJRTimestamps: Boolean;
    supportsSendingJRTimestamps: Boolean;
    transportId: String;
    transportMnemonic: String;
    transportSuppliedDescription: String;
    transportSuppliedName: String;
    transportSuppliedProductId: Number;
    transportSuppliedSerialNumber: String;
    transportSuppliedVendorId: Number;
    userSuppliedDescription: String;
    userSuppliedLargeImagePath: String;
    userSuppliedName: String;
    userSuppliedSmallImagePath: String;
    constructor();

    static createFromId(id: String): MidiEndpointDeviceInformation;


    static findAll(): Object;
    static findAll(sortOrder: MidiEndpointDeviceInformationSortOrder): Object;
    static findAll(sortOrder: MidiEndpointDeviceInformationSortOrder, endpointFilters: MidiEndpointDeviceInformationFilters): Object;


    static getAdditionalPropertiesList(): Object;


    static deviceMatchesFilter(deviceInformation: MidiEndpointDeviceInformation, endpointFilters: MidiEndpointDeviceInformationFilters): Boolean;


    updateFromDeviceInformation(deviceInformation: Object): Boolean;

    updateFromDeviceInformationUpdate(deviceInformationUpdate: Object): Boolean;

    getParentDeviceInformation(): Object;

    getContainerInformation(): Object;

  }

  export class MidiEndpointDeviceInformationAddedEventArgs {
    addedDevice: MidiEndpointDeviceInformation;
    constructor();

  }

  export class MidiEndpointDeviceInformationRemovedEventArgs {
    deviceInformationUpdate: Object;
    id: String;
    constructor();

  }

  export class MidiEndpointDeviceInformationUpdatedEventArgs {
    deviceInformationUpdate: Object;
    id: String;
    updatedAdditionalCapabilities: Boolean;
    updatedDeviceIdentity: Boolean;
    updatedEndpointInformation: Boolean;
    updatedFunctionBlocks: Boolean;
    updatedName: Boolean;
    updatedStreamConfiguration: Boolean;
    updatedUserMetadata: Boolean;
    constructor();

  }

  export class MidiEndpointDeviceWatcher {
    enumeratedEndpointDevices: Object;
    status: Number;
    constructor();

    static createWatcher(endpointFilters: MidiEndpointDeviceInformationFilters): MidiEndpointDeviceWatcher;


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
    uIHint: MidiFunctionBlockUIHint;
    number: Number;
    name: String;
    midiCIMessageVersionFormat: Number;
    midi10Connection: MidiFunctionBlockMidi10;
    maxSystemExclusive8Streams: Number;
    isActive: Boolean;
    groupCount: Number;
    firstGroupIndex: Number;
    direction: MidiFunctionBlockDirection;
    isReadOnly: Boolean;
    constructor();

    includesGroup(group: MidiGroup): Boolean;

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
    preventFiringMainMessageReceivedEvent: Boolean;
    preventCallingFurtherListeners: Boolean;
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

    asEquivalentFunctionBlock(): MidiFunctionBlock;

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

    getAllWords(): Object;

    appendAllMessageWordsToList(targetList: Object): Number;

    fillBuffer(byteOffset: Number, buffer: Object): Number;

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

    getAllWords(): Object;

    appendAllMessageWordsToList(targetList: Object): Number;

    fillBuffer(byteOffset: Number, buffer: Object): Number;

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

    getAllWords(): Object;

    appendAllMessageWordsToList(targetList: Object): Number;

    fillBuffer(byteOffset: Number, buffer: Object): Number;

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

    getAllWords(): Object;

    appendAllMessageWordsToList(targetList: Object): Number;

    fillBuffer(byteOffset: Number, buffer: Object): Number;

    toString(): String;

  }

  export class MidiMessageBuilder {
    constructor();

    static buildUtilityMessage(timestamp: Number, status: Number, dataOrReserved: Number): MidiMessage32;


    static buildSystemMessage(timestamp: Number, group: MidiGroup, status: Number, midi1Byte2: Number, midi1Byte3: Number): MidiMessage32;


    static buildMidi1ChannelVoiceMessage(timestamp: Number, group: MidiGroup, status: Midi1ChannelVoiceMessageStatus, channel: MidiChannel, byte3: Number, byte4: Number): MidiMessage32;


    static buildSystemExclusive7Message(timestamp: Number, group: MidiGroup, status: Number, numberOfBytes: Number, dataByte0: Number, dataByte1: Number, dataByte2: Number, dataByte3: Number, dataByte4: Number, dataByte5: Number): MidiMessage64;


    static buildMidi2ChannelVoiceMessage(timestamp: Number, group: MidiGroup, status: Midi2ChannelVoiceMessageStatus, channel: MidiChannel, index: Number, data: Number): MidiMessage64;


    static buildSystemExclusive8Message(timestamp: Number, group: MidiGroup, status: MidiSystemExclusive8Status, numberOfValidDataBytesThisMessage: Number, streamId: Number, dataByte00: Number, dataByte01: Number, dataByte02: Number, dataByte03: Number, dataByte04: Number, dataByte05: Number, dataByte06: Number, dataByte07: Number, dataByte08: Number, dataByte09: Number, dataByte10: Number, dataByte11: Number, dataByte12: Number): MidiMessage128;


    static buildMixedDataSetChunkHeaderMessage(timestamp: Number, group: MidiGroup, mdsId: Number, numberValidDataBytesInThisChunk: Number, numberChunksInMixedDataSet: Number, numberOfThisChunk: Number, manufacturerId: Number, deviceId: Number, subId1: Number, subId2: Number): MidiMessage128;


    static buildMixedDataSetChunkDataMessage(timestamp: Number, group: MidiGroup, mdsId: Number, dataByte00: Number, dataByte01: Number, dataByte02: Number, dataByte03: Number, dataByte04: Number, dataByte05: Number, dataByte06: Number, dataByte07: Number, dataByte08: Number, dataByte09: Number, dataByte10: Number, dataByte11: Number, dataByte12: Number, dataByte13: Number): MidiMessage128;


    static buildFlexDataMessage(timestamp: Number, group: MidiGroup, form: Number, address: Number, channel: MidiChannel, statusBank: Number, status: Number, word1Data: Number, word2Data: Number, word3Data: Number): MidiMessage128;


    static buildStreamMessage(timestamp: Number, form: Number, status: Number, word0RemainingData: Number, word1Data: Number, word2Data: Number, word3Data: Number): MidiMessage128;


  }

  export class MidiMessageConverter {
    constructor();

    static convertMidi1Message(timestamp: Number, group: MidiGroup, statusByte: Number): MidiMessage32;
    static convertMidi1Message(timestamp: Number, group: MidiGroup, statusByte: Number, dataByte1: Number): MidiMessage32;
    static convertMidi1Message(timestamp: Number, group: MidiGroup, statusByte: Number, dataByte1: Number, dataByte2: Number): MidiMessage32;


    static convertMidi1ChannelPressureMessage(timestamp: Number, group: MidiGroup, originalMessage: Object): MidiMessage32;


    static convertMidi1NoteOffMessage(timestamp: Number, group: MidiGroup, originalMessage: Object): MidiMessage32;


    static convertMidi1NoteOnMessage(timestamp: Number, group: MidiGroup, originalMessage: Object): MidiMessage32;


    static convertMidi1PitchBendChangeMessage(timestamp: Number, group: MidiGroup, originalMessage: Object): MidiMessage32;


    static convertMidi1PolyphonicKeyPressureMessage(timestamp: Number, group: MidiGroup, originalMessage: Object): MidiMessage32;


    static convertMidi1ProgramChangeMessage(timestamp: Number, group: MidiGroup, originalMessage: Object): MidiMessage32;


    static convertMidi1TimeCodeMessage(timestamp: Number, group: MidiGroup, originalMessage: Object): MidiMessage32;


    static convertMidi1SongPositionPointerMessage(timestamp: Number, group: MidiGroup, originalMessage: Object): MidiMessage32;


    static convertMidi1SongSelectMessage(timestamp: Number, group: MidiGroup, originalMessage: Object): MidiMessage32;


    static convertMidi1TuneRequestMessage(timestamp: Number, group: MidiGroup, originalMessage: Object): MidiMessage32;


    static convertMidi1TimingClockMessage(timestamp: Number, group: MidiGroup, originalMessage: Object): MidiMessage32;


    static convertMidi1StartMessage(timestamp: Number, group: MidiGroup, originalMessage: Object): MidiMessage32;


    static convertMidi1ContinueMessage(timestamp: Number, group: MidiGroup, originalMessage: Object): MidiMessage32;


    static convertMidi1StopMessage(timestamp: Number, group: MidiGroup, originalMessage: Object): MidiMessage32;


    static convertMidi1ActiveSensingMessage(timestamp: Number, group: MidiGroup, originalMessage: Object): MidiMessage32;


    static convertMidi1SystemResetMessage(timestamp: Number, group: MidiGroup, originalMessage: Object): MidiMessage32;


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
    fillBuffer(byteOffset: Number, buffer: Object): Number;

    appendWordsToList(wordList: Object): Number;

  }

  export class MidiMessageTypeEndpointListener {
    tag: Object;
    name: String;
    isEnabled: Boolean;
    id: String;
    preventFiringMainMessageReceivedEvent: Boolean;
    preventCallingFurtherListeners: Boolean;
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


    static getPacketListFromWordList(timestamp: Number, words: Object): Object;


    static getWordListFromPacketList(words: Object): Object;


  }

  export class MidiService {
    constructor();

    static pingService(pingCount: Number): MidiServicePingResponseSummary;
    static pingService(pingCount: Number, timeoutMilliseconds: Number): MidiServicePingResponseSummary;


    static getInstalledTransportPlugins(): Object;


    static getInstalledMessageProcessingPlugins(): Object;


    static getActiveSessions(): Object;


    static createTemporaryLoopbackEndpoints(associationId: String, endpointDefinitionA: MidiServiceLoopbackEndpointDefinition, endpointDefinitionB: MidiServiceLoopbackEndpointDefinition): MidiServiceLoopbackEndpointCreationResult;


    static removeTemporaryLoopbackEndpoints(associationId: String): Boolean;


    static updateTransportPluginConfiguration(configurationUpdate: IMidiServiceTransportPluginConfiguration): MidiServiceConfigurationResponse;


    static updateProcessingPluginConfiguration(configurationUpdate: IMidiServiceMessageProcessingPluginConfiguration): MidiServiceConfigurationResponse;


  }

  export class MidiServiceConfigurationResponse {
    responseJson: String;
    status: MidiServiceConfigurationResponseStatus;
    constructor();

  }

  export class MidiServiceLoopbackEndpointCreationResult {
    associationId: String;
    endpointDeviceIdA: String;
    endpointDeviceIdB: String;
    success: Boolean;
    constructor();

  }

  export class MidiServiceLoopbackEndpointDefinition {
    uniqueId: String;
    name: String;
    description: String;
    constructor();

  }

  export class MidiServiceMessageProcessingPluginInfo {
    author: String;
    clientConfigurationAssemblyName: String;
    description: String;
    id: String;
    isClientConfigurable: Boolean;
    isSystemManaged: Boolean;
    name: String;
    smallImagePath: String;
    supportsMultipleInstancesPerDevice: Boolean;
    version: String;
    constructor();

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

  export class MidiServiceSessionConnectionInfo {
    earliestConnectionTime: Date;
    endpointDeviceId: String;
    instanceCount: Number;
    constructor();

  }

  export class MidiServiceSessionInfo {
    connections: Object;
    processId: Number;
    processName: String;
    sessionId: String;
    sessionName: String;
    startTime: Date;
    constructor();

  }

  export class MidiServiceTransportPluginInfo {
    author: String;
    clientConfigurationAssemblyName: String;
    description: String;
    id: String;
    isClientConfigurable: Boolean;
    isRuntimeCreatableByApps: Boolean;
    isRuntimeCreatableBySettings: Boolean;
    isSystemManaged: Boolean;
    mnemonic: String;
    name: String;
    smallImagePath: String;
    version: String;
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
    createEndpointConnection(endpointDeviceId: String, settings: IMidiEndpointConnectionSettings): MidiEndpointConnection;

    createVirtualDeviceAndConnection(deviceDefinition: MidiVirtualEndpointDeviceDefinition): MidiEndpointConnection;

    disconnectEndpointConnection(endpointConnectionId: String): void;

    updateName(newName: String): Boolean;

    close(): void;
  }

  export class MidiSessionSettings {
    useMmcssThreads: Boolean;
    constructor();

  }

  export class MidiStreamConfigurationRequestReceivedEventArgs {
    preferredMidiProtocol: MidiProtocol;
    requestEndpointReceiveJitterReductionTimestamps: Boolean;
    requestEndpointTransmitJitterReductionTimestamps: Boolean;
    timestamp: Number;
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

    static buildEndpointDiscoveryMessage(timestamp: Number, umpVersionMajor: Number, umpVersionMinor: Number, requestFlags: MidiEndpointDiscoveryRequests): IMidiUniversalPacket;


    static buildEndpointInfoNotificationMessage(timestamp: Number, umpVersionMajor: Number, umpVersionMinor: Number, hasStaticFunctionBlocks: Boolean, numberOfFunctionBlocks: Number, supportsMidi20Protocol: Boolean, supportsMidi10Protocol: Boolean, supportsReceivingJitterReductionTimestamps: Boolean, supportsSendingJitterReductionTimestamps: Boolean): IMidiUniversalPacket;


    static buildDeviceIdentityNotificationMessage(timestamp: Number, deviceManufacturerSysExIdByte1: Number, deviceManufacturerSysExIdByte2: Number, deviceManufacturerSysExIdByte3: Number, deviceFamilyLsb: Number, deviceFamilyMsb: Number, deviceFamilyModelNumberLsb: Number, deviceFamilyModelNumberMsb: Number, softwareRevisionLevelByte1: Number, softwareRevisionLevelByte2: Number, softwareRevisionLevelByte3: Number, softwareRevisionLevelByte4: Number): IMidiUniversalPacket;


    static buildEndpointNameNotificationMessages(timestamp: Number, name: String): Object;


    static buildProductInstanceIdNotificationMessages(timestamp: Number, productInstanceId: String): Object;


    static parseEndpointNameNotificationMessages(messages: Object): String;


    static parseProductInstanceIdNotificationMessages(messages: Object): String;


    static buildStreamConfigurationRequestMessage(timestamp: Number, protocol: Number, expectToReceiveJRTimestamps: Boolean, requestToSendJRTimestamps: Boolean): IMidiUniversalPacket;


    static buildStreamConfigurationNotificationMessage(timestamp: Number, protocol: Number, confirmationWillReceiveJRTimestamps: Boolean, confirmationSendJRTimestamps: Boolean): IMidiUniversalPacket;


    static buildFunctionBlockDiscoveryMessage(timestamp: Number, functionBlockNumber: Number, requestFlags: MidiFunctionBlockDiscoveryRequests): IMidiUniversalPacket;


    static buildFunctionBlockInfoNotificationMessage(timestamp: Number, active: Boolean, functionBlockNumber: Number, uiHint: MidiFunctionBlockUIHint, midi10: MidiFunctionBlockMidi10, direction: MidiFunctionBlockDirection, firstGroup: Number, numberOfGroups: Number, midiCIVersionFormat: Number, maxNumberSysEx8Streams: Number): IMidiUniversalPacket;


    static buildFunctionBlockNameNotificationMessages(timestamp: Number, functionBlockNumber: Number, name: String): Object;


    static parseFunctionBlockNameNotificationMessages(messages: Object): String;


  }

  export class MidiUniqueId {
    static labelFull: String;
    static labelShort: String;
    byte4: Number;
    byte3: Number;
    byte2: Number;
    byte1: Number;
    asCombined28BitValue: Number;
    isBroadcast: Boolean;
    isReserved: Boolean;
    constructor();
    constructor(combined28BitValue: Number);
    constructor(sevenBitByte1: Number, sevenBitByte2: Number, sevenBitByte3: Number, sevenBitByte4: Number);

    static createBroadcast(): MidiUniqueId;


    static createRandom(): MidiUniqueId;


  }

  export class MidiVirtualEndpointDevice {
    tag: Object;
    name: String;
    isEnabled: Boolean;
    id: String;
    suppressHandledMessages: Boolean;
    deviceDefinition: MidiVirtualEndpointDeviceDefinition;
    functionBlocks: Object;
    constructor();

    updateFunctionBlock(block: MidiFunctionBlock): void;

    updateEndpointName(name: String): void;

    initialize(endpointConnection: IMidiEndpointConnectionSource): void;

    onEndpointConnectionOpened(): void;

    processIncomingMessage(args: MidiMessageReceivedEventArgs, skipFurtherListeners: Boolean, skipMainMessageReceivedEvent: Boolean): void;

    cleanup(): void;

    addListener(type: "StreamConfigurationRequestReceived", listener: (ev: Event) => void): void ;
    removeListener(type: "StreamConfigurationRequestReceived", listener: (ev: Event) => void): void ;
    on(type: "StreamConfigurationRequestReceived", listener: (ev: Event) => void): void ;
    off(type: "StreamConfigurationRequestReceived", listener: (ev: Event) => void): void ;
    
    addListener(type: string, listener: (ev: Event) => void): void ;
    removeListener(type: string, listener: (ev: Event) => void): void ;
    on(type: string, listener: (ev: Event) => void): void ;
    off(type: string, listener: (ev: Event) => void): void ;
    

  }

  export class MidiVirtualEndpointDeviceDefinition {
    transportSuppliedDescription: String;
    supportsSendingJRTimestamps: Boolean;
    supportsReceivingJRTimestamps: Boolean;
    supportsMidi2ProtocolMessages: Boolean;
    supportsMidi1ProtocolMessages: Boolean;
    endpointProductInstanceId: String;
    endpointName: String;
    deviceFamilyMsb: Number;
    deviceFamilyModelMsb: Number;
    deviceFamilyModelLsb: Number;
    deviceFamilyLsb: Number;
    areFunctionBlocksStatic: Boolean;
    deviceManufacturerSystemExclusiveId: Object;
    functionBlocks: Object;
    softwareRevisionLevel: Object;
    constructor();

  }

}



