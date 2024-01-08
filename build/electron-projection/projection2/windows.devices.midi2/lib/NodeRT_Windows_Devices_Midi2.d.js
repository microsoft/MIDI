  
MidiMessageStruct = (function () {
  var cls = function MidiMessageStruct() {
      this.word0 = new Number();
      this.word1 = new Number();
      this.word2 = new Number();
      this.word3 = new Number();
    };
  return cls;
}) ();
exports.MidiMessageStruct = MidiMessageStruct;


_Midi1ChannelVoiceMessageStatus = function () {
  this.noteOff = 0;
  this.noteOn = 1;
  this.polyPressure = 2;
  this.controlChange = 3;
  this.programChange = 4;
  this.channelPressure = 5;
  this.pitchBend = 6;
}
exports.Midi1ChannelVoiceMessageStatus = new _Midi1ChannelVoiceMessageStatus();

_Midi2ChannelVoiceMessageStatus = function () {
  this.registeredPerNoteController = 0;
  this.assignablePerNoteController = 1;
  this.registeredController = 2;
  this.assignableController = 3;
  this.relativeRegisteredController = 4;
  this.relativeAssignableController = 5;
  this.perNotePitchBend = 6;
  this.noteOff = 7;
  this.noteOn = 8;
  this.polyPressure = 9;
  this.controlChange = 10;
  this.programChange = 11;
  this.channelPressure = 12;
  this.pitchBend = 13;
  this.perNoteManagement = 14;
}
exports.Midi2ChannelVoiceMessageStatus = new _Midi2ChannelVoiceMessageStatus();

_MidiEndpointConnectionSharing = function () {
  this.unknown = 0;
  this.multiClient = 1;
  this.exclusive = 2;
}
exports.MidiEndpointConnectionSharing = new _MidiEndpointConnectionSharing();

_MidiEndpointConnectionSharingPreference = function () {
  this.default = 0;
  this.preferMultiClient = 1;
  this.requireMultiClient = 2;
  this.preferExclusiveMode = 3;
  this.requireExclusiveMode = 4;
}
exports.MidiEndpointConnectionSharingPreference = new _MidiEndpointConnectionSharingPreference();

_MidiEndpointDeviceInformationFilter = function () {
  this.includeClientUmpNative = 0;
  this.includeClientByteStreamNative = 1;
  this.includeVirtualDeviceResponder = 2;
  this.includeDiagnosticLoopback = 3;
  this.includeDiagnosticPing = 4;
}
exports.MidiEndpointDeviceInformationFilter = new _MidiEndpointDeviceInformationFilter();

_MidiEndpointDeviceInformationSortOrder = function () {
  this.none = 0;
  this.name = 1;
  this.deviceInstanceId = 2;
  this.containerThenName = 3;
  this.containerThenDeviceInstanceId = 4;
}
exports.MidiEndpointDeviceInformationSortOrder = new _MidiEndpointDeviceInformationSortOrder();

_MidiEndpointDevicePurpose = function () {
  this.normalMessageEndpoint = 0;
  this.virtualDeviceResponder = 1;
  this.inBoxGeneralMidiSynth = 2;
  this.diagnosticLoopback = 3;
  this.diagnosticPing = 4;
}
exports.MidiEndpointDevicePurpose = new _MidiEndpointDevicePurpose();

_MidiEndpointDiscoveryFilterFlags = function () {
  this.none = 0;
  this.requestEndpointInformation = 1;
  this.requestDeviceIdentity = 2;
  this.requestEndpointName = 3;
  this.requestProductInstanceId = 4;
  this.requestStreamConfiguration = 5;
}
exports.MidiEndpointDiscoveryFilterFlags = new _MidiEndpointDiscoveryFilterFlags();

_MidiEndpointNativeDataFormat = function () {
  this.unknown = 0;
  this.byteStream = 1;
  this.universalMidiPacket = 2;
}
exports.MidiEndpointNativeDataFormat = new _MidiEndpointNativeDataFormat();

_MidiFunctionBlockDirection = function () {
  this.undefined = 0;
  this.blockInput = 1;
  this.blockOutput = 2;
  this.bidirectional = 3;
}
exports.MidiFunctionBlockDirection = new _MidiFunctionBlockDirection();

_MidiFunctionBlockDiscoveryFilterFlags = function () {
  this.none = 0;
  this.requestFunctionBlockInformation = 1;
  this.requestFunctionBlockName = 2;
}
exports.MidiFunctionBlockDiscoveryFilterFlags = new _MidiFunctionBlockDiscoveryFilterFlags();

_MidiFunctionBlockMidi10 = function () {
  this.not10 = 0;
  this.yesBandwidthUnrestricted = 1;
  this.yesBandwidthRestricted = 2;
  this.reserved = 3;
}
exports.MidiFunctionBlockMidi10 = new _MidiFunctionBlockMidi10();

_MidiFunctionBlockUIHint = function () {
  this.unknown = 0;
  this.receiver = 1;
  this.sender = 2;
  this.bidirectional = 3;
}
exports.MidiFunctionBlockUIHint = new _MidiFunctionBlockUIHint();

_MidiGroupTerminalBlockDirection = function () {
  this.bidirectional = 0;
  this.blockInput = 1;
  this.blockOutput = 2;
}
exports.MidiGroupTerminalBlockDirection = new _MidiGroupTerminalBlockDirection();

_MidiGroupTerminalBlockProtocol = function () {
  this.unknown = 0;
  this.midi1Message64 = 1;
  this.midi1Message64WithJitterReduction = 2;
  this.midi1Message128 = 3;
  this.midi1Message128WithJitterReduction = 4;
  this.midi2 = 5;
  this.midi2WithJitterReduction = 6;
}
exports.MidiGroupTerminalBlockProtocol = new _MidiGroupTerminalBlockProtocol();

_MidiMessageType = function () {
  this.utilityMessage32 = 0;
  this.systemCommon32 = 1;
  this.midi1ChannelVoice32 = 2;
  this.dataMessage64 = 3;
  this.midi2ChannelVoice64 = 4;
  this.dataMessage128 = 5;
  this.futureReserved632 = 6;
  this.futureReserved732 = 7;
  this.futureReserved864 = 8;
  this.futureReserved964 = 9;
  this.futureReservedA64 = 10;
  this.futureReservedB96 = 11;
  this.futureReservedC96 = 12;
  this.flexData128 = 13;
  this.futureReservedE128 = 14;
  this.stream128 = 15;
}
exports.MidiMessageType = new _MidiMessageType();

_MidiPacketType = function () {
  this.unknownOrInvalid = 0;
  this.universalMidiPacket32 = 1;
  this.universalMidiPacket64 = 2;
  this.universalMidiPacket96 = 3;
  this.universalMidiPacket128 = 4;
}
exports.MidiPacketType = new _MidiPacketType();

_MidiProtocol = function () {
  this.default = 0;
  this.midi1 = 1;
  this.midi2 = 2;
}
exports.MidiProtocol = new _MidiProtocol();

_MidiSendMessageResult = function () {
  this.succeeded = 0;
  this.failed = 1;
  this.bufferFull = 2;
  this.endpointConnectionClosedOrInvalid = 3;
  this.invalidMessageTypeForWordCount = 4;
  this.invalidMessageOther = 5;
  this.dataIndexOutOfRange = 6;
  this.timestampOutOfRange = 7;
  this.other = 8;
}
exports.MidiSendMessageResult = new _MidiSendMessageResult();

_MidiSystemExclusive8Status = function () {
  this.completeMessageInSingleUmp = 0;
  this.startUmp = 1;
  this.continueUmp = 2;
  this.endUmp = 3;
}
exports.MidiSystemExclusive8Status = new _MidiSystemExclusive8Status();

IMidiEndpointConnectionSource = (function () {
  var cls = function IMidiEndpointConnectionSource() {
  };
  

  return cls;
}) ();
exports.IMidiEndpointConnectionSource = IMidiEndpointConnectionSource;

IMidiEndpointDefinedConnectionSettings = (function () {
  var cls = function IMidiEndpointDefinedConnectionSettings() {
    this.isDirty = new Boolean();
    this.settingsJson = new String();
  };
  

  return cls;
}) ();
exports.IMidiEndpointDefinedConnectionSettings = IMidiEndpointDefinedConnectionSettings;

IMidiEndpointMessageProcessingPlugin = (function () {
  var cls = function IMidiEndpointMessageProcessingPlugin() {
    this.id = new String();
    this.isEnabled = new Boolean();
    this.name = new String();
    this.tag = new Object();
  };
  

  cls.prototype.initialize = function initialize(endpointConnection) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="endpointConnection" type="IMidiEndpointConnectionSource">A param.</param>
    /// </signature>
  }


  cls.prototype.onEndpointConnectionOpened = function onEndpointConnectionOpened() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// </signature>
  }


  cls.prototype.processIncomingMessage = function processIncomingMessage(args, skipFurtherListeners, skipMainMessageReceivedEvent) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="args" type="MidiMessageReceivedEventArgs">A param.</param>
    /// <param name="skipFurtherListeners" type="Boolean">A param.</param>
    /// <param name="skipMainMessageReceivedEvent" type="Boolean">A param.</param>
    /// </signature>
  }


  cls.prototype.cleanup = function cleanup() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// </signature>
  }


  return cls;
}) ();
exports.IMidiEndpointMessageProcessingPlugin = IMidiEndpointMessageProcessingPlugin;

IMidiMessageReceivedEventSource = (function () {
  var cls = function IMidiMessageReceivedEventSource() {
  };
  

    cls.prototype.addListener = function addListener(eventName, callback){}
    cls.prototype.removeListener = function removeListener(eventName, callback){}
    cls.prototype.on = function on(eventName, callback){}
    cls.prototype.off = function off(eventName, callback){}
  return cls;
}) ();
exports.IMidiMessageReceivedEventSource = IMidiMessageReceivedEventSource;

IMidiTransportSettingsData = (function () {
  var cls = function IMidiTransportSettingsData() {
    this.isDirty = new Boolean();
    this.settingsJson = new String();
  };
  

  return cls;
}) ();
exports.IMidiTransportSettingsData = IMidiTransportSettingsData;

IMidiUniversalPacket = (function () {
  var cls = function IMidiUniversalPacket() {
    this.messageType = new MidiMessageType();
    this.packetType = new MidiPacketType();
    this.timestamp = new Number();
  };
  

  cls.prototype.peekFirstWord = function peekFirstWord() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  return cls;
}) ();
exports.IMidiUniversalPacket = IMidiUniversalPacket;

MidiChannel = (function () {
  var cls = function MidiChannel() {
    this.index = new Number();
    this.numberForDisplay = new Number();
  };
  
var cls = function MidiChannel(index) {
      this.index = new Number();
      this.numberForDisplay = new Number();
};


  cls.isValidChannelIndex = function isValidChannelIndex(index) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="index" type="Number">A param.</param>
    /// <returns type="Boolean" />
    /// </signature>
    return new Boolean();
  }


  cls.labelFull = new String();
  cls.labelShort = new String();
  return cls;
}) ();
exports.MidiChannel = MidiChannel;

MidiChannelEndpointListener = (function () {
  var cls = function MidiChannelEndpointListener() {
    this.preventFiringMainMessageReceivedEvent = new Boolean();
    this.preventCallingFurtherListeners = new Boolean();
    this.includeGroup = new MidiGroup();
    this.includeChannels = new Object();
    this.tag = new Object();
    this.name = new String();
    this.isEnabled = new Boolean();
    this.id = new String();
  };
  

  cls.prototype.initialize = function initialize(endpointConnection) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="endpointConnection" type="IMidiEndpointConnectionSource">A param.</param>
    /// </signature>
  }


  cls.prototype.onEndpointConnectionOpened = function onEndpointConnectionOpened() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// </signature>
  }


  cls.prototype.processIncomingMessage = function processIncomingMessage(args, skipFurtherListeners, skipMainMessageReceivedEvent) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="args" type="MidiMessageReceivedEventArgs">A param.</param>
    /// <param name="skipFurtherListeners" type="Boolean">A param.</param>
    /// <param name="skipMainMessageReceivedEvent" type="Boolean">A param.</param>
    /// </signature>
  }


  cls.prototype.cleanup = function cleanup() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// </signature>
  }


    cls.prototype.addListener = function addListener(eventName, callback){}
    cls.prototype.removeListener = function removeListener(eventName, callback){}
    cls.prototype.on = function on(eventName, callback){}
    cls.prototype.off = function off(eventName, callback){}
  return cls;
}) ();
exports.MidiChannelEndpointListener = MidiChannelEndpointListener;

MidiClock = (function () {
  var cls = function MidiClock() {
  };
  

  cls.convertTimestampToMicroseconds = function convertTimestampToMicroseconds(timestampValue) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestampValue" type="Number">A param.</param>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  cls.convertTimestampToMilliseconds = function convertTimestampToMilliseconds(timestampValue) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestampValue" type="Number">A param.</param>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  cls.offsetTimestampByTicks = function offsetTimestampByTicks(timestampValue, offsetTicks) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestampValue" type="Number">A param.</param>
    /// <param name="offsetTicks" type="Number">A param.</param>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  cls.offsetTimestampByMicroseconds = function offsetTimestampByMicroseconds(timestampValue, offsetMicroseconds) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestampValue" type="Number">A param.</param>
    /// <param name="offsetMicroseconds" type="Number">A param.</param>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  cls.offsetTimestampByMilliseconds = function offsetTimestampByMilliseconds(timestampValue, offsetMilliseconds) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestampValue" type="Number">A param.</param>
    /// <param name="offsetMilliseconds" type="Number">A param.</param>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  cls.now = new Number();
  cls.timestampFrequency = new Number();
  return cls;
}) ();
exports.MidiClock = MidiClock;

MidiEndpointConnection = (function () {
  var cls = function MidiEndpointConnection() {
    this.tag = new Object();
    this.connectionId = new String();
    this.endpointDeviceId = new String();
    this.isOpen = new Boolean();
    this.messageProcessingPlugins = new Object();
    this.settings = new IMidiEndpointDefinedConnectionSettings();
  };
  

  cls.prototype.open = function open() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <returns type="Boolean" />
    /// </signature>
    return new Boolean();
  }


  cls.prototype.sendMessagePacket = function sendMessagePacket(message) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="message" type="IMidiUniversalPacket">A param.</param>
    /// <returns type="MidiSendMessageResult" />
    /// </signature>
    return new MidiSendMessageResult();
  }


  cls.prototype.sendMessageStruct = function sendMessageStruct(timestamp, message, wordCount) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="message" type="MidiMessageStruct">A param.</param>
    /// <param name="wordCount" type="Number">A param.</param>
    /// <returns type="MidiSendMessageResult" />
    /// </signature>
    return new MidiSendMessageResult();
  }


  cls.prototype.sendMessageWordArray = function sendMessageWordArray(timestamp, words, startIndex, wordCount) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="words" type="Array<Number>">A param.</param>
    /// <param name="startIndex" type="Number">A param.</param>
    /// <param name="wordCount" type="Number">A param.</param>
    /// <returns type="MidiSendMessageResult" />
    /// </signature>
    return new MidiSendMessageResult();
  }


  cls.prototype.sendMessageWords = function sendMessageWords(timestamp, word0) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="word0" type="Number">A param.</param>
    /// <returns type="MidiSendMessageResult" />
    /// </signature>
    return new MidiSendMessageResult();
  }

cls.prototype.sendMessageWords = function sendMessageWords(timestamp, word0, word1) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="word0" type="Number">A param.</param>
    /// <param name="word1" type="Number">A param.</param>
    /// <returns type="MidiSendMessageResult" />
    /// </signature>
    return new MidiSendMessageResult();
  }

cls.prototype.sendMessageWords = function sendMessageWords(timestamp, word0, word1, word2) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="word0" type="Number">A param.</param>
    /// <param name="word1" type="Number">A param.</param>
    /// <param name="word2" type="Number">A param.</param>
    /// <returns type="MidiSendMessageResult" />
    /// </signature>
    return new MidiSendMessageResult();
  }

cls.prototype.sendMessageWords = function sendMessageWords(timestamp, word0, word1, word2, word3) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="word0" type="Number">A param.</param>
    /// <param name="word1" type="Number">A param.</param>
    /// <param name="word2" type="Number">A param.</param>
    /// <param name="word3" type="Number">A param.</param>
    /// <returns type="MidiSendMessageResult" />
    /// </signature>
    return new MidiSendMessageResult();
  }


  cls.prototype.sendMessageBuffer = function sendMessageBuffer(timestamp, buffer, byteOffset, byteLength) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="buffer" type="Object">A param.</param>
    /// <param name="byteOffset" type="Number">A param.</param>
    /// <param name="byteLength" type="Number">A param.</param>
    /// <returns type="MidiSendMessageResult" />
    /// </signature>
    return new MidiSendMessageResult();
  }


  cls.prototype.close = function close() {
}


  cls.getDeviceSelector = function getDeviceSelector() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <returns type="String" />
    /// </signature>
    return new String();
  }


  cls.sendMessageSucceeded = function sendMessageSucceeded(sendResult) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="sendResult" type="MidiSendMessageResult">A param.</param>
    /// <returns type="Boolean" />
    /// </signature>
    return new Boolean();
  }


  cls.sendMessageFailed = function sendMessageFailed(sendResult) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="sendResult" type="MidiSendMessageResult">A param.</param>
    /// <returns type="Boolean" />
    /// </signature>
    return new Boolean();
  }


    cls.prototype.addListener = function addListener(eventName, callback){}
    cls.prototype.removeListener = function removeListener(eventName, callback){}
    cls.prototype.on = function on(eventName, callback){}
    cls.prototype.off = function off(eventName, callback){}
  return cls;
}) ();
exports.MidiEndpointConnection = MidiEndpointConnection;

MidiEndpointConnectionOptions = (function () {
  var cls = function MidiEndpointConnectionOptions() {
    this.requestedStreamConfiguration = new MidiStreamConfigurationRequestedSettings();
    this.connectionSharingPreference = new MidiEndpointConnectionSharingPreference();
  };
  

  return cls;
}) ();
exports.MidiEndpointConnectionOptions = MidiEndpointConnectionOptions;

MidiEndpointDeviceInformation = (function () {
  var cls = function MidiEndpointDeviceInformation() {
    this.configuredProtocol = new MidiProtocol();
    this.configuredToReceiveJRTimestamps = new Boolean();
    this.configuredToSendJRTimestamps = new Boolean();
    this.containerId = new String();
    this.description = new String();
    this.deviceIdentityDeviceFamilyLsb = new Number();
    this.deviceIdentityDeviceFamilyModelNumberLsb = new Number();
    this.deviceIdentityDeviceFamilyModelNumberMsb = new Number();
    this.deviceIdentityDeviceFamilyMsb = new Number();
    this.deviceIdentitySoftwareRevisionLevel = new Array<Number>();
    this.deviceIdentitySystemExclusiveId = new Array<Number>();
    this.deviceInstanceId = new String();
    this.endpointPurpose = new MidiEndpointDevicePurpose();
    this.endpointSuppliedName = new String();
    this.functionBlockCount = new Number();
    this.functionBlocks = new Object();
    this.groupTerminalBlocks = new Object();
    this.hasStaticFunctionBlocks = new Boolean();
    this.id = new String();
    this.largeImagePath = new String();
    this.manufacturerName = new String();
    this.name = new String();
    this.nativeDataFormat = new MidiEndpointNativeDataFormat();
    this.productInstanceId = new String();
    this.properties = new Object();
    this.recommendedCCAutomationIntervalMS = new Number();
    this.requiresNoteOffTranslation = new Boolean();
    this.smallImagePath = new String();
    this.specificationVersionMajor = new Number();
    this.specificationVersionMinor = new Number();
    this.supportsMidi10Protocol = new Boolean();
    this.supportsMidi20Protocol = new Boolean();
    this.supportsMidiPolyphonicExpression = new Boolean();
    this.supportsMultiClient = new Boolean();
    this.supportsReceivingJRTimestamps = new Boolean();
    this.supportsSendingJRTimestamps = new Boolean();
    this.transportId = new String();
    this.transportMnemonic = new String();
    this.transportSuppliedName = new String();
    this.transportSuppliedSerialNumber = new String();
    this.userSuppliedName = new String();
  };
  

  cls.prototype.getParentDeviceInformation = function getParentDeviceInformation() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <returns type="Object" />
    /// </signature>
    return new Object();
  }


  cls.prototype.getContainerInformation = function getContainerInformation() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <returns type="Object" />
    /// </signature>
    return new Object();
  }


  cls.prototype.updateFromDeviceInformation = function updateFromDeviceInformation(deviceInformation) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="deviceInformation" type="Object">A param.</param>
    /// <returns type="Boolean" />
    /// </signature>
    return new Boolean();
  }


  cls.prototype.updateFromDeviceInformationUpdate = function updateFromDeviceInformationUpdate(deviceInformationUpdate) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="deviceInformationUpdate" type="Object">A param.</param>
    /// <returns type="Boolean" />
    /// </signature>
    return new Boolean();
  }


  cls.getAdditionalPropertiesList = function getAdditionalPropertiesList() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <returns type="Object" />
    /// </signature>
    return new Object();
  }


  cls.deviceMatchesFilter = function deviceMatchesFilter(deviceInformation, endpointFilter) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="deviceInformation" type="MidiEndpointDeviceInformation">A param.</param>
    /// <param name="endpointFilter" type="MidiEndpointDeviceInformationFilter">A param.</param>
    /// <returns type="Boolean" />
    /// </signature>
    return new Boolean();
  }


  cls.createFromId = function createFromId(id) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="id" type="String">A param.</param>
    /// <returns type="MidiEndpointDeviceInformation" />
    /// </signature>
    return new MidiEndpointDeviceInformation();
  }


  cls.findAll = function findAll() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <returns type="Object" />
    /// </signature>
    return new Object();
  }

cls.findAll = function findAll(sortOrder) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="sortOrder" type="MidiEndpointDeviceInformationSortOrder">A param.</param>
    /// <returns type="Object" />
    /// </signature>
    return new Object();
  }

cls.findAll = function findAll(sortOrder, endpointFilter) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="sortOrder" type="MidiEndpointDeviceInformationSortOrder">A param.</param>
    /// <param name="endpointFilter" type="MidiEndpointDeviceInformationFilter">A param.</param>
    /// <returns type="Object" />
    /// </signature>
    return new Object();
  }


  cls.diagnosticsLoopbackAEndpointId = new String();
  cls.diagnosticsLoopbackBEndpointId = new String();
  cls.endpointInterfaceClass = new String();
  return cls;
}) ();
exports.MidiEndpointDeviceInformation = MidiEndpointDeviceInformation;

MidiEndpointDeviceInformationUpdateEventArgs = (function () {
  var cls = function MidiEndpointDeviceInformationUpdateEventArgs() {
    this.deviceInformationUpdate = new Object();
    this.id = new String();
    this.updatedAdditionalCapabilities = new Boolean();
    this.updatedDeviceIdentity = new Boolean();
    this.updatedEndpointInformation = new Boolean();
    this.updatedFunctionBlocks = new Boolean();
    this.updatedName = new Boolean();
    this.updatedStreamConfiguration = new Boolean();
    this.updatedUserMetadata = new Boolean();
  };
  

  return cls;
}) ();
exports.MidiEndpointDeviceInformationUpdateEventArgs = MidiEndpointDeviceInformationUpdateEventArgs;

MidiEndpointDeviceWatcher = (function () {
  var cls = function MidiEndpointDeviceWatcher() {
    this.enumeratedEndpointDevices = new Object();
    this.status = new Number();
  };
  

  cls.prototype.start = function start() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// </signature>
  }


  cls.prototype.stop = function stop() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// </signature>
  }


  cls.createWatcher = function createWatcher(endpointFilter) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="endpointFilter" type="MidiEndpointDeviceInformationFilter">A param.</param>
    /// <returns type="MidiEndpointDeviceWatcher" />
    /// </signature>
    return new MidiEndpointDeviceWatcher();
  }


    cls.prototype.addListener = function addListener(eventName, callback){}
    cls.prototype.removeListener = function removeListener(eventName, callback){}
    cls.prototype.on = function on(eventName, callback){}
    cls.prototype.off = function off(eventName, callback){}
  return cls;
}) ();
exports.MidiEndpointDeviceWatcher = MidiEndpointDeviceWatcher;

MidiFunctionBlock = (function () {
  var cls = function MidiFunctionBlock() {
    this.direction = new MidiFunctionBlockDirection();
    this.firstGroupIndex = new Number();
    this.groupCount = new Number();
    this.isActive = new Boolean();
    this.maxSystemExclusive8Streams = new Number();
    this.midi10Connection = new MidiFunctionBlockMidi10();
    this.midiCIMessageVersionFormat = new Number();
    this.name = new String();
    this.number = new Number();
    this.uIHint = new MidiFunctionBlockUIHint();
  };
  

  cls.prototype.includesGroup = function includesGroup(group) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="group" type="MidiGroup">A param.</param>
    /// <returns type="Boolean" />
    /// </signature>
    return new Boolean();
  }


  cls.prototype.updateFromJson = function updateFromJson(json) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="json" type="Object">A param.</param>
    /// <returns type="Boolean" />
    /// </signature>
    return new Boolean();
  }


  return cls;
}) ();
exports.MidiFunctionBlock = MidiFunctionBlock;

MidiGroup = (function () {
  var cls = function MidiGroup() {
    this.index = new Number();
    this.numberForDisplay = new Number();
  };
  
var cls = function MidiGroup(index) {
      this.index = new Number();
      this.numberForDisplay = new Number();
};


  cls.isValidGroupIndex = function isValidGroupIndex(index) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="index" type="Number">A param.</param>
    /// <returns type="Boolean" />
    /// </signature>
    return new Boolean();
  }


  cls.labelFull = new String();
  cls.labelShort = new String();
  return cls;
}) ();
exports.MidiGroup = MidiGroup;

MidiGroupEndpointListener = (function () {
  var cls = function MidiGroupEndpointListener() {
    this.tag = new Object();
    this.name = new String();
    this.isEnabled = new Boolean();
    this.id = new String();
    this.includeGroups = new Object();
  };
  

  cls.prototype.initialize = function initialize(endpointConnection) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="endpointConnection" type="IMidiEndpointConnectionSource">A param.</param>
    /// </signature>
  }


  cls.prototype.onEndpointConnectionOpened = function onEndpointConnectionOpened() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// </signature>
  }


  cls.prototype.processIncomingMessage = function processIncomingMessage(args, skipFurtherListeners, skipMainMessageReceivedEvent) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="args" type="MidiMessageReceivedEventArgs">A param.</param>
    /// <param name="skipFurtherListeners" type="Boolean">A param.</param>
    /// <param name="skipMainMessageReceivedEvent" type="Boolean">A param.</param>
    /// </signature>
  }


  cls.prototype.cleanup = function cleanup() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// </signature>
  }


    cls.prototype.addListener = function addListener(eventName, callback){}
    cls.prototype.removeListener = function removeListener(eventName, callback){}
    cls.prototype.on = function on(eventName, callback){}
    cls.prototype.off = function off(eventName, callback){}
  return cls;
}) ();
exports.MidiGroupEndpointListener = MidiGroupEndpointListener;

MidiGroupTerminalBlock = (function () {
  var cls = function MidiGroupTerminalBlock() {
    this.calculatedMaxDeviceInputBandwidthBitsPerSecond = new Number();
    this.calculatedMaxDeviceOutputBandwidthBitsPerSecond = new Number();
    this.direction = new MidiGroupTerminalBlockDirection();
    this.firstGroupIndex = new Number();
    this.groupCount = new Number();
    this.maxDeviceInputBandwidthIn4KBitsPerSecondUnits = new Number();
    this.maxDeviceOutputBandwidthIn4KBitsPerSecondUnits = new Number();
    this.name = new String();
    this.number = new Number();
    this.protocol = new MidiGroupTerminalBlockProtocol();
  };
  

  cls.prototype.includesGroup = function includesGroup(group) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="group" type="MidiGroup">A param.</param>
    /// <returns type="Boolean" />
    /// </signature>
    return new Boolean();
  }


  return cls;
}) ();
exports.MidiGroupTerminalBlock = MidiGroupTerminalBlock;

MidiMessage128 = (function () {
  var cls = function MidiMessage128() {
    this.word3 = new Number();
    this.word2 = new Number();
    this.word1 = new Number();
    this.word0 = new Number();
    this.timestamp = new Number();
    this.messageType = new MidiMessageType();
    this.packetType = new MidiPacketType();
  };
  
var cls = function MidiMessage128(timestamp, word0, word1, word2, word3) {
      this.word3 = new Number();
      this.word2 = new Number();
      this.word1 = new Number();
      this.word0 = new Number();
      this.timestamp = new Number();
      this.messageType = new MidiMessageType();
      this.packetType = new MidiPacketType();
};

var cls = function MidiMessage128(timestamp, words) {
      this.word3 = new Number();
      this.word2 = new Number();
      this.word1 = new Number();
      this.word0 = new Number();
      this.timestamp = new Number();
      this.messageType = new MidiMessageType();
      this.packetType = new MidiPacketType();
};


  cls.prototype.peekFirstWord = function peekFirstWord() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  cls.prototype.toString = function toString() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <returns type="String" />
    /// </signature>
    return new String();
  }


  return cls;
}) ();
exports.MidiMessage128 = MidiMessage128;

MidiMessage32 = (function () {
  var cls = function MidiMessage32() {
    this.word0 = new Number();
    this.timestamp = new Number();
    this.messageType = new MidiMessageType();
    this.packetType = new MidiPacketType();
  };
  
var cls = function MidiMessage32(timestamp, word0) {
      this.word0 = new Number();
      this.timestamp = new Number();
      this.messageType = new MidiMessageType();
      this.packetType = new MidiPacketType();
};


  cls.prototype.peekFirstWord = function peekFirstWord() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  cls.prototype.toString = function toString() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <returns type="String" />
    /// </signature>
    return new String();
  }


  return cls;
}) ();
exports.MidiMessage32 = MidiMessage32;

MidiMessage64 = (function () {
  var cls = function MidiMessage64() {
    this.word1 = new Number();
    this.word0 = new Number();
    this.timestamp = new Number();
    this.messageType = new MidiMessageType();
    this.packetType = new MidiPacketType();
  };
  
var cls = function MidiMessage64(timestamp, word0, word1) {
      this.word1 = new Number();
      this.word0 = new Number();
      this.timestamp = new Number();
      this.messageType = new MidiMessageType();
      this.packetType = new MidiPacketType();
};

var cls = function MidiMessage64(timestamp, words) {
      this.word1 = new Number();
      this.word0 = new Number();
      this.timestamp = new Number();
      this.messageType = new MidiMessageType();
      this.packetType = new MidiPacketType();
};


  cls.prototype.peekFirstWord = function peekFirstWord() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  cls.prototype.toString = function toString() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <returns type="String" />
    /// </signature>
    return new String();
  }


  return cls;
}) ();
exports.MidiMessage64 = MidiMessage64;

MidiMessage96 = (function () {
  var cls = function MidiMessage96() {
    this.word2 = new Number();
    this.word1 = new Number();
    this.word0 = new Number();
    this.timestamp = new Number();
    this.messageType = new MidiMessageType();
    this.packetType = new MidiPacketType();
  };
  
var cls = function MidiMessage96(timestamp, word0, word1, word2) {
      this.word2 = new Number();
      this.word1 = new Number();
      this.word0 = new Number();
      this.timestamp = new Number();
      this.messageType = new MidiMessageType();
      this.packetType = new MidiPacketType();
};

var cls = function MidiMessage96(timestamp, words) {
      this.word2 = new Number();
      this.word1 = new Number();
      this.word0 = new Number();
      this.timestamp = new Number();
      this.messageType = new MidiMessageType();
      this.packetType = new MidiPacketType();
};


  cls.prototype.peekFirstWord = function peekFirstWord() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  cls.prototype.toString = function toString() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <returns type="String" />
    /// </signature>
    return new String();
  }


  return cls;
}) ();
exports.MidiMessage96 = MidiMessage96;

MidiMessageBuilder = (function () {
  var cls = function MidiMessageBuilder() {
  };
  

  cls.buildUtilityMessage = function buildUtilityMessage(timestamp, status, dataOrReserved) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="status" type="Number">A param.</param>
    /// <param name="dataOrReserved" type="Number">A param.</param>
    /// <returns type="MidiMessage32" />
    /// </signature>
    return new MidiMessage32();
  }


  cls.buildSystemMessage = function buildSystemMessage(timestamp, groupIndex, status, midi1Byte2, midi1Byte3) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="status" type="Number">A param.</param>
    /// <param name="midi1Byte2" type="Number">A param.</param>
    /// <param name="midi1Byte3" type="Number">A param.</param>
    /// <returns type="MidiMessage32" />
    /// </signature>
    return new MidiMessage32();
  }


  cls.buildMidi1ChannelVoiceMessage = function buildMidi1ChannelVoiceMessage(timestamp, groupIndex, status, channel, byte3, byte4) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="status" type="Midi1ChannelVoiceMessageStatus">A param.</param>
    /// <param name="channel" type="Number">A param.</param>
    /// <param name="byte3" type="Number">A param.</param>
    /// <param name="byte4" type="Number">A param.</param>
    /// <returns type="MidiMessage32" />
    /// </signature>
    return new MidiMessage32();
  }


  cls.buildSystemExclusive7Message = function buildSystemExclusive7Message(timestamp, groupIndex, status, numberOfBytes, dataByte0, dataByte1, dataByte2, dataByte3, dataByte4, dataByte5) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="status" type="Number">A param.</param>
    /// <param name="numberOfBytes" type="Number">A param.</param>
    /// <param name="dataByte0" type="Number">A param.</param>
    /// <param name="dataByte1" type="Number">A param.</param>
    /// <param name="dataByte2" type="Number">A param.</param>
    /// <param name="dataByte3" type="Number">A param.</param>
    /// <param name="dataByte4" type="Number">A param.</param>
    /// <param name="dataByte5" type="Number">A param.</param>
    /// <returns type="MidiMessage64" />
    /// </signature>
    return new MidiMessage64();
  }


  cls.buildMidi2ChannelVoiceMessage = function buildMidi2ChannelVoiceMessage(timestamp, groupIndex, status, channel, index, data) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="status" type="Midi2ChannelVoiceMessageStatus">A param.</param>
    /// <param name="channel" type="Number">A param.</param>
    /// <param name="index" type="Number">A param.</param>
    /// <param name="data" type="Number">A param.</param>
    /// <returns type="MidiMessage64" />
    /// </signature>
    return new MidiMessage64();
  }


  cls.buildSystemExclusive8Message = function buildSystemExclusive8Message(timestamp, groupIndex, status, numberOfValidDataBytesThisMessage, streamId, dataByte00, dataByte01, dataByte02, dataByte03, dataByte04, dataByte05, dataByte06, dataByte07, dataByte08, dataByte09, dataByte10, dataByte11, dataByte12) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="status" type="MidiSystemExclusive8Status">A param.</param>
    /// <param name="numberOfValidDataBytesThisMessage" type="Number">A param.</param>
    /// <param name="streamId" type="Number">A param.</param>
    /// <param name="dataByte00" type="Number">A param.</param>
    /// <param name="dataByte01" type="Number">A param.</param>
    /// <param name="dataByte02" type="Number">A param.</param>
    /// <param name="dataByte03" type="Number">A param.</param>
    /// <param name="dataByte04" type="Number">A param.</param>
    /// <param name="dataByte05" type="Number">A param.</param>
    /// <param name="dataByte06" type="Number">A param.</param>
    /// <param name="dataByte07" type="Number">A param.</param>
    /// <param name="dataByte08" type="Number">A param.</param>
    /// <param name="dataByte09" type="Number">A param.</param>
    /// <param name="dataByte10" type="Number">A param.</param>
    /// <param name="dataByte11" type="Number">A param.</param>
    /// <param name="dataByte12" type="Number">A param.</param>
    /// <returns type="MidiMessage128" />
    /// </signature>
    return new MidiMessage128();
  }


  cls.buildMixedDataSetChunkHeaderMessage = function buildMixedDataSetChunkHeaderMessage(timestamp, groupIndex, mdsId, numberValidDataBytesInThisChunk, numberChunksInMixedDataSet, numberOfThisChunk, manufacturerId, deviceId, subId1, subId2) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="mdsId" type="Number">A param.</param>
    /// <param name="numberValidDataBytesInThisChunk" type="Number">A param.</param>
    /// <param name="numberChunksInMixedDataSet" type="Number">A param.</param>
    /// <param name="numberOfThisChunk" type="Number">A param.</param>
    /// <param name="manufacturerId" type="Number">A param.</param>
    /// <param name="deviceId" type="Number">A param.</param>
    /// <param name="subId1" type="Number">A param.</param>
    /// <param name="subId2" type="Number">A param.</param>
    /// <returns type="MidiMessage128" />
    /// </signature>
    return new MidiMessage128();
  }


  cls.buildMixedDataSetChunkDataMessage = function buildMixedDataSetChunkDataMessage(timestamp, groupIndex, mdsId, dataByte00, dataByte01, dataByte02, dataByte03, dataByte04, dataByte05, dataByte06, dataByte07, dataByte08, dataByte09, dataByte10, dataByte11, dataByte12, dataByte13) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="mdsId" type="Number">A param.</param>
    /// <param name="dataByte00" type="Number">A param.</param>
    /// <param name="dataByte01" type="Number">A param.</param>
    /// <param name="dataByte02" type="Number">A param.</param>
    /// <param name="dataByte03" type="Number">A param.</param>
    /// <param name="dataByte04" type="Number">A param.</param>
    /// <param name="dataByte05" type="Number">A param.</param>
    /// <param name="dataByte06" type="Number">A param.</param>
    /// <param name="dataByte07" type="Number">A param.</param>
    /// <param name="dataByte08" type="Number">A param.</param>
    /// <param name="dataByte09" type="Number">A param.</param>
    /// <param name="dataByte10" type="Number">A param.</param>
    /// <param name="dataByte11" type="Number">A param.</param>
    /// <param name="dataByte12" type="Number">A param.</param>
    /// <param name="dataByte13" type="Number">A param.</param>
    /// <returns type="MidiMessage128" />
    /// </signature>
    return new MidiMessage128();
  }


  cls.buildFlexDataMessage = function buildFlexDataMessage(timestamp, groupIndex, form, address, channel, statusBank, status, word1Data, word2Data, word3Data) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="form" type="Number">A param.</param>
    /// <param name="address" type="Number">A param.</param>
    /// <param name="channel" type="Number">A param.</param>
    /// <param name="statusBank" type="Number">A param.</param>
    /// <param name="status" type="Number">A param.</param>
    /// <param name="word1Data" type="Number">A param.</param>
    /// <param name="word2Data" type="Number">A param.</param>
    /// <param name="word3Data" type="Number">A param.</param>
    /// <returns type="MidiMessage128" />
    /// </signature>
    return new MidiMessage128();
  }


  cls.buildStreamMessage = function buildStreamMessage(timestamp, form, status, word0RemainingData, word1Data, word2Data, word3Data) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="form" type="Number">A param.</param>
    /// <param name="status" type="Number">A param.</param>
    /// <param name="word0RemainingData" type="Number">A param.</param>
    /// <param name="word1Data" type="Number">A param.</param>
    /// <param name="word2Data" type="Number">A param.</param>
    /// <param name="word3Data" type="Number">A param.</param>
    /// <returns type="MidiMessage128" />
    /// </signature>
    return new MidiMessage128();
  }


  return cls;
}) ();
exports.MidiMessageBuilder = MidiMessageBuilder;

MidiMessageConverter = (function () {
  var cls = function MidiMessageConverter() {
  };
  

  cls.convertMidi1Message = function convertMidi1Message(timestamp, groupIndex, statusByte) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="statusByte" type="Number">A param.</param>
    /// <returns type="MidiMessage32" />
    /// </signature>
    return new MidiMessage32();
  }

cls.convertMidi1Message = function convertMidi1Message(timestamp, groupIndex, statusByte, dataByte1) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="statusByte" type="Number">A param.</param>
    /// <param name="dataByte1" type="Number">A param.</param>
    /// <returns type="MidiMessage32" />
    /// </signature>
    return new MidiMessage32();
  }

cls.convertMidi1Message = function convertMidi1Message(timestamp, groupIndex, statusByte, dataByte1, dataByte2) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="statusByte" type="Number">A param.</param>
    /// <param name="dataByte1" type="Number">A param.</param>
    /// <param name="dataByte2" type="Number">A param.</param>
    /// <returns type="MidiMessage32" />
    /// </signature>
    return new MidiMessage32();
  }


  cls.convertMidi1ChannelPressureMessage = function convertMidi1ChannelPressureMessage(timestamp, groupIndex, originalMessage) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="originalMessage" type="Object">A param.</param>
    /// <returns type="MidiMessage32" />
    /// </signature>
    return new MidiMessage32();
  }


  cls.convertMidi1ContinueMessage = function convertMidi1ContinueMessage(timestamp, groupIndex, originalMessage) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="originalMessage" type="Object">A param.</param>
    /// <returns type="MidiMessage32" />
    /// </signature>
    return new MidiMessage32();
  }


  cls.convertMidi1NoteOffMessage = function convertMidi1NoteOffMessage(timestamp, groupIndex, originalMessage) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="originalMessage" type="Object">A param.</param>
    /// <returns type="MidiMessage32" />
    /// </signature>
    return new MidiMessage32();
  }


  cls.convertMidi1NoteOnMessage = function convertMidi1NoteOnMessage(timestamp, groupIndex, originalMessage) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="originalMessage" type="Object">A param.</param>
    /// <returns type="MidiMessage32" />
    /// </signature>
    return new MidiMessage32();
  }


  cls.convertMidi1PitchBendChangeMessage = function convertMidi1PitchBendChangeMessage(timestamp, groupIndex, originalMessage) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="originalMessage" type="Object">A param.</param>
    /// <returns type="MidiMessage32" />
    /// </signature>
    return new MidiMessage32();
  }


  cls.convertMidi1PolyphonicKeyPressureMessage = function convertMidi1PolyphonicKeyPressureMessage(timestamp, groupIndex, originalMessage) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="originalMessage" type="Object">A param.</param>
    /// <returns type="MidiMessage32" />
    /// </signature>
    return new MidiMessage32();
  }


  cls.convertMidi1ProgramChangeMessage = function convertMidi1ProgramChangeMessage(timestamp, groupIndex, originalMessage) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="originalMessage" type="Object">A param.</param>
    /// <returns type="MidiMessage32" />
    /// </signature>
    return new MidiMessage32();
  }


  cls.convertMidi1SongPositionPointerMessage = function convertMidi1SongPositionPointerMessage(timestamp, groupIndex, originalMessage) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="originalMessage" type="Object">A param.</param>
    /// <returns type="MidiMessage32" />
    /// </signature>
    return new MidiMessage32();
  }


  cls.convertMidi1SongSelectMessage = function convertMidi1SongSelectMessage(timestamp, groupIndex, originalMessage) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="originalMessage" type="Object">A param.</param>
    /// <returns type="MidiMessage32" />
    /// </signature>
    return new MidiMessage32();
  }


  cls.convertMidi1StartMessage = function convertMidi1StartMessage(timestamp, groupIndex, originalMessage) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="originalMessage" type="Object">A param.</param>
    /// <returns type="MidiMessage32" />
    /// </signature>
    return new MidiMessage32();
  }


  cls.convertMidi1StopMessage = function convertMidi1StopMessage(timestamp, groupIndex, originalMessage) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="originalMessage" type="Object">A param.</param>
    /// <returns type="MidiMessage32" />
    /// </signature>
    return new MidiMessage32();
  }


  cls.convertMidi1SystemResetMessage = function convertMidi1SystemResetMessage(timestamp, groupIndex, originalMessage) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="originalMessage" type="Object">A param.</param>
    /// <returns type="MidiMessage32" />
    /// </signature>
    return new MidiMessage32();
  }


  cls.convertMidi1TimeCodeMessage = function convertMidi1TimeCodeMessage(timestamp, groupIndex, originalMessage) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="originalMessage" type="Object">A param.</param>
    /// <returns type="MidiMessage32" />
    /// </signature>
    return new MidiMessage32();
  }


  cls.convertMidi1TimingClockMessage = function convertMidi1TimingClockMessage(timestamp, groupIndex, originalMessage) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="originalMessage" type="Object">A param.</param>
    /// <returns type="MidiMessage32" />
    /// </signature>
    return new MidiMessage32();
  }


  cls.convertMidi1TuneRequestMessage = function convertMidi1TuneRequestMessage(timestamp, groupIndex, originalMessage) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="originalMessage" type="Object">A param.</param>
    /// <returns type="MidiMessage32" />
    /// </signature>
    return new MidiMessage32();
  }


  return cls;
}) ();
exports.MidiMessageConverter = MidiMessageConverter;

MidiMessageProcessingPluginInformation = (function () {
  var cls = function MidiMessageProcessingPluginInformation() {
    this.author = new String();
    this.classId = new String();
    this.clientConfigurationAssemblyName = new String();
    this.description = new String();
    this.iconPath = new String();
    this.isClientConfigurable = new Boolean();
    this.isEnabled = new Boolean();
    this.isSystemManaged = new Boolean();
    this.name = new String();
    this.registryKey = new String();
    this.servicePluginFileName = new String();
    this.shortName = new String();
  };
  

  return cls;
}) ();
exports.MidiMessageProcessingPluginInformation = MidiMessageProcessingPluginInformation;

MidiMessageReceivedEventArgs = (function () {
  var cls = function MidiMessageReceivedEventArgs() {
    this.messageType = new MidiMessageType();
    this.packetType = new MidiPacketType();
    this.timestamp = new Number();
  };
  

  cls.prototype.peekFirstWord = function peekFirstWord() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  cls.prototype.getMessagePacket = function getMessagePacket() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <returns type="IMidiUniversalPacket" />
    /// </signature>
    return new IMidiUniversalPacket();
  }


  cls.prototype.fillWords = function fillWords(word0, word1, word2, word3) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="word0" type="Number">A param.</param>
    /// <param name="word1" type="Number">A param.</param>
    /// <param name="word2" type="Number">A param.</param>
    /// <param name="word3" type="Number">A param.</param>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  cls.prototype.fillMessageStruct = function fillMessageStruct(message) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="message" type="MidiMessageStruct">A param.</param>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  cls.prototype.fillMessage32 = function fillMessage32(message) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="message" type="MidiMessage32">A param.</param>
    /// <returns type="Boolean" />
    /// </signature>
    return new Boolean();
  }


  cls.prototype.fillMessage64 = function fillMessage64(message) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="message" type="MidiMessage64">A param.</param>
    /// <returns type="Boolean" />
    /// </signature>
    return new Boolean();
  }


  cls.prototype.fillMessage96 = function fillMessage96(message) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="message" type="MidiMessage96">A param.</param>
    /// <returns type="Boolean" />
    /// </signature>
    return new Boolean();
  }


  cls.prototype.fillMessage128 = function fillMessage128(message) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="message" type="MidiMessage128">A param.</param>
    /// <returns type="Boolean" />
    /// </signature>
    return new Boolean();
  }


  cls.prototype.fillWordArray = function () {
}

  cls.prototype.fillByteArray = function () {
}

  cls.prototype.fillBuffer = function fillBuffer(buffer, byteOffset) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="buffer" type="Object">A param.</param>
    /// <param name="byteOffset" type="Number">A param.</param>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  return cls;
}) ();
exports.MidiMessageReceivedEventArgs = MidiMessageReceivedEventArgs;

MidiMessageTranslator = (function () {
  var cls = function MidiMessageTranslator() {
  };
  

  cls.upscaleMidi1ChannelVoiceMessageToMidi2 = function upscaleMidi1ChannelVoiceMessageToMidi2(originalMessage) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="originalMessage" type="MidiMessage32">A param.</param>
    /// <returns type="MidiMessage64" />
    /// </signature>
    return new MidiMessage64();
  }

cls.upscaleMidi1ChannelVoiceMessageToMidi2 = function upscaleMidi1ChannelVoiceMessageToMidi2(timestamp, groupIndex, statusByte) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="statusByte" type="Number">A param.</param>
    /// <returns type="MidiMessage64" />
    /// </signature>
    return new MidiMessage64();
  }

cls.upscaleMidi1ChannelVoiceMessageToMidi2 = function upscaleMidi1ChannelVoiceMessageToMidi2(timestamp, groupIndex, statusByte, dataByte1) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="statusByte" type="Number">A param.</param>
    /// <param name="dataByte1" type="Number">A param.</param>
    /// <returns type="MidiMessage64" />
    /// </signature>
    return new MidiMessage64();
  }

cls.upscaleMidi1ChannelVoiceMessageToMidi2 = function upscaleMidi1ChannelVoiceMessageToMidi2(timestamp, groupIndex, statusByte, dataByte1, dataByte2) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="groupIndex" type="Number">A param.</param>
    /// <param name="statusByte" type="Number">A param.</param>
    /// <param name="dataByte1" type="Number">A param.</param>
    /// <param name="dataByte2" type="Number">A param.</param>
    /// <returns type="MidiMessage64" />
    /// </signature>
    return new MidiMessage64();
  }


  cls.downscaleMidi2ChannelVoiceMessageToMidi1 = function downscaleMidi2ChannelVoiceMessageToMidi1(originalMessage) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="originalMessage" type="MidiMessage64">A param.</param>
    /// <returns type="MidiMessage32" />
    /// </signature>
    return new MidiMessage32();
  }


  return cls;
}) ();
exports.MidiMessageTranslator = MidiMessageTranslator;

MidiMessageTypeEndpointListener = (function () {
  var cls = function MidiMessageTypeEndpointListener() {
    this.tag = new Object();
    this.name = new String();
    this.isEnabled = new Boolean();
    this.id = new String();
    this.includeMessageTypes = new Object();
  };
  

  cls.prototype.initialize = function initialize(endpointConnection) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="endpointConnection" type="IMidiEndpointConnectionSource">A param.</param>
    /// </signature>
  }


  cls.prototype.onEndpointConnectionOpened = function onEndpointConnectionOpened() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// </signature>
  }


  cls.prototype.processIncomingMessage = function processIncomingMessage(args, skipFurtherListeners, skipMainMessageReceivedEvent) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="args" type="MidiMessageReceivedEventArgs">A param.</param>
    /// <param name="skipFurtherListeners" type="Boolean">A param.</param>
    /// <param name="skipMainMessageReceivedEvent" type="Boolean">A param.</param>
    /// </signature>
  }


  cls.prototype.cleanup = function cleanup() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// </signature>
  }


    cls.prototype.addListener = function addListener(eventName, callback){}
    cls.prototype.removeListener = function removeListener(eventName, callback){}
    cls.prototype.on = function on(eventName, callback){}
    cls.prototype.off = function off(eventName, callback){}
  return cls;
}) ();
exports.MidiMessageTypeEndpointListener = MidiMessageTypeEndpointListener;

MidiMessageUtility = (function () {
  var cls = function MidiMessageUtility() {
  };
  

  cls.validateMessage32MessageType = function validateMessage32MessageType(word0) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="word0" type="Number">A param.</param>
    /// <returns type="Boolean" />
    /// </signature>
    return new Boolean();
  }


  cls.validateMessage64MessageType = function validateMessage64MessageType(word0) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="word0" type="Number">A param.</param>
    /// <returns type="Boolean" />
    /// </signature>
    return new Boolean();
  }


  cls.validateMessage96MessageType = function validateMessage96MessageType(word0) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="word0" type="Number">A param.</param>
    /// <returns type="Boolean" />
    /// </signature>
    return new Boolean();
  }


  cls.validateMessage128MessageType = function validateMessage128MessageType(word0) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="word0" type="Number">A param.</param>
    /// <returns type="Boolean" />
    /// </signature>
    return new Boolean();
  }


  cls.getMessageTypeFromMessageFirstWord = function getMessageTypeFromMessageFirstWord(word0) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="word0" type="Number">A param.</param>
    /// <returns type="MidiMessageType" />
    /// </signature>
    return new MidiMessageType();
  }


  cls.getPacketTypeFromMessageFirstWord = function getPacketTypeFromMessageFirstWord(word0) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="word0" type="Number">A param.</param>
    /// <returns type="MidiPacketType" />
    /// </signature>
    return new MidiPacketType();
  }


  cls.messageTypeHasGroupField = function messageTypeHasGroupField(messageType) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="messageType" type="MidiMessageType">A param.</param>
    /// <returns type="Boolean" />
    /// </signature>
    return new Boolean();
  }


  cls.replaceGroupInMessageFirstWord = function replaceGroupInMessageFirstWord(word0, newGroup) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="word0" type="Number">A param.</param>
    /// <param name="newGroup" type="MidiGroup">A param.</param>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  cls.getGroupFromMessageFirstWord = function getGroupFromMessageFirstWord(word0) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="word0" type="Number">A param.</param>
    /// <returns type="MidiGroup" />
    /// </signature>
    return new MidiGroup();
  }


  cls.getStatusFromUtilityMessage = function getStatusFromUtilityMessage(word0) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="word0" type="Number">A param.</param>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  cls.getStatusFromMidi1ChannelVoiceMessage = function getStatusFromMidi1ChannelVoiceMessage(word0) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="word0" type="Number">A param.</param>
    /// <returns type="Midi1ChannelVoiceMessageStatus" />
    /// </signature>
    return new Midi1ChannelVoiceMessageStatus();
  }


  cls.getStatusFromMidi2ChannelVoiceMessageFirstWord = function getStatusFromMidi2ChannelVoiceMessageFirstWord(word0) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="word0" type="Number">A param.</param>
    /// <returns type="Midi2ChannelVoiceMessageStatus" />
    /// </signature>
    return new Midi2ChannelVoiceMessageStatus();
  }


  cls.getStatusBankFromFlexDataMessageFirstWord = function getStatusBankFromFlexDataMessageFirstWord(word0) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="word0" type="Number">A param.</param>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  cls.getStatusFromFlexDataMessageFirstWord = function getStatusFromFlexDataMessageFirstWord(word0) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="word0" type="Number">A param.</param>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  cls.getStatusFromSystemCommonMessage = function getStatusFromSystemCommonMessage(word0) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="word0" type="Number">A param.</param>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  cls.getStatusFromDataMessage64FirstWord = function getStatusFromDataMessage64FirstWord(word0) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="word0" type="Number">A param.</param>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  cls.getNumberOfBytesFromDataMessage64FirstWord = function getNumberOfBytesFromDataMessage64FirstWord(word0) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="word0" type="Number">A param.</param>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  cls.getStatusFromDataMessage128FirstWord = function getStatusFromDataMessage128FirstWord(word0) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="word0" type="Number">A param.</param>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  cls.getNumberOfBytesFromDataMessage128FirstWord = function getNumberOfBytesFromDataMessage128FirstWord(word0) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="word0" type="Number">A param.</param>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  cls.messageTypeHasChannelField = function messageTypeHasChannelField(messageType) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="messageType" type="MidiMessageType">A param.</param>
    /// <returns type="Boolean" />
    /// </signature>
    return new Boolean();
  }


  cls.replaceChannelInMessageFirstWord = function replaceChannelInMessageFirstWord(word0, newChannel) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="word0" type="Number">A param.</param>
    /// <param name="newChannel" type="MidiChannel">A param.</param>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  cls.getChannelFromMessageFirstWord = function getChannelFromMessageFirstWord(word0) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="word0" type="Number">A param.</param>
    /// <returns type="MidiChannel" />
    /// </signature>
    return new MidiChannel();
  }


  cls.getFormFromStreamMessageFirstWord = function getFormFromStreamMessageFirstWord(word0) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="word0" type="Number">A param.</param>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  cls.getStatusFromStreamMessageFirstWord = function getStatusFromStreamMessageFirstWord(word0) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="word0" type="Number">A param.</param>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  cls.getMessageFriendlyNameFromFirstWord = function getMessageFriendlyNameFromFirstWord(word0) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="word0" type="Number">A param.</param>
    /// <returns type="String" />
    /// </signature>
    return new String();
  }


  return cls;
}) ();
exports.MidiMessageUtility = MidiMessageUtility;

MidiService = (function () {
  var cls = function MidiService() {
  };
  

  cls.pingService = function pingService(pingCount) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="pingCount" type="Number">A param.</param>
    /// <returns type="MidiServicePingResponseSummary" />
    /// </signature>
    return new MidiServicePingResponseSummary();
  }

cls.pingService = function pingService(pingCount, timeoutMilliseconds) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="pingCount" type="Number">A param.</param>
    /// <param name="timeoutMilliseconds" type="Number">A param.</param>
    /// <returns type="MidiServicePingResponseSummary" />
    /// </signature>
    return new MidiServicePingResponseSummary();
  }


  cls.getInstalledTransportPlugins = function getInstalledTransportPlugins() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <returns type="Object" />
    /// </signature>
    return new Object();
  }


  cls.getInstalledMessageProcessingPlugins = function getInstalledMessageProcessingPlugins() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <returns type="Object" />
    /// </signature>
    return new Object();
  }


  cls.getOutgoingMessageQueueMaxMessageCapacity = function getOutgoingMessageQueueMaxMessageCapacity() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <returns type="Number" />
    /// </signature>
    return new Number();
  }


  return cls;
}) ();
exports.MidiService = MidiService;

MidiServicePingResponse = (function () {
  var cls = function MidiServicePingResponse() {
    this.clientDeltaTimestamp = new Number();
    this.clientReceiveMidiTimestamp = new Number();
    this.clientSendMidiTimestamp = new Number();
    this.index = new Number();
    this.serviceReportedMidiTimestamp = new Number();
    this.sourceId = new Number();
  };
  

  return cls;
}) ();
exports.MidiServicePingResponse = MidiServicePingResponse;

MidiServicePingResponseSummary = (function () {
  var cls = function MidiServicePingResponseSummary() {
    this.averagePingRoundTripMidiClock = new Number();
    this.failureReason = new String();
    this.responses = new Object();
    this.success = new Boolean();
    this.totalPingRoundTripMidiClock = new Number();
  };
  

  return cls;
}) ();
exports.MidiServicePingResponseSummary = MidiServicePingResponseSummary;

MidiSession = (function () {
  var cls = function MidiSession() {
    this.connections = new Object();
    this.id = new String();
    this.isOpen = new Boolean();
    this.name = new String();
    this.settings = new MidiSessionSettings();
  };
  

  cls.prototype.createEndpointConnection = function createEndpointConnection(endpointDeviceId) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="endpointDeviceId" type="String">A param.</param>
    /// <returns type="MidiEndpointConnection" />
    /// </signature>
    return new MidiEndpointConnection();
  }

cls.prototype.createEndpointConnection = function createEndpointConnection(endpointDeviceId, options) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="endpointDeviceId" type="String">A param.</param>
    /// <param name="options" type="MidiEndpointConnectionOptions">A param.</param>
    /// <returns type="MidiEndpointConnection" />
    /// </signature>
    return new MidiEndpointConnection();
  }

cls.prototype.createEndpointConnection = function createEndpointConnection(endpointDeviceId, options, settings) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="endpointDeviceId" type="String">A param.</param>
    /// <param name="options" type="MidiEndpointConnectionOptions">A param.</param>
    /// <param name="settings" type="IMidiEndpointDefinedConnectionSettings">A param.</param>
    /// <returns type="MidiEndpointConnection" />
    /// </signature>
    return new MidiEndpointConnection();
  }


  cls.prototype.createVirtualDeviceAndConnection = function createVirtualDeviceAndConnection(endpointName, endpointDeviceInstanceId) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="endpointName" type="String">A param.</param>
    /// <param name="endpointDeviceInstanceId" type="String">A param.</param>
    /// <returns type="MidiEndpointConnection" />
    /// </signature>
    return new MidiEndpointConnection();
  }


  cls.prototype.disconnectEndpointConnection = function disconnectEndpointConnection(endpointConnectionId) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="endpointConnectionId" type="String">A param.</param>
    /// </signature>
  }


  cls.prototype.close = function close() {
}


  cls.createSession = function createSession(sessionName) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="sessionName" type="String">A param.</param>
    /// <returns type="MidiSession" />
    /// </signature>
    return new MidiSession();
  }

cls.createSession = function createSession(sessionName, settings) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="sessionName" type="String">A param.</param>
    /// <param name="settings" type="MidiSessionSettings">A param.</param>
    /// <returns type="MidiSession" />
    /// </signature>
    return new MidiSession();
  }


  return cls;
}) ();
exports.MidiSession = MidiSession;

MidiSessionSettings = (function () {
  var cls = function MidiSessionSettings() {
    this.useMmcssThreads = new Boolean();
  };
  

  return cls;
}) ();
exports.MidiSessionSettings = MidiSessionSettings;

MidiStreamConfigurationRequestedSettings = (function () {
  var cls = function MidiStreamConfigurationRequestedSettings() {
    this.specificationVersionMinor = new Number();
    this.specificationVersionMajor = new Number();
    this.requestEndpointTransmitJitterReductionTimestamps = new Boolean();
    this.requestEndpointReceiveJitterReductionTimestamps = new Boolean();
    this.preferredMidiProtocol = new MidiProtocol();
  };
  

  return cls;
}) ();
exports.MidiStreamConfigurationRequestedSettings = MidiStreamConfigurationRequestedSettings;

MidiStreamMessageBuilder = (function () {
  var cls = function MidiStreamMessageBuilder() {
  };
  

  cls.buildEndpointDiscoveryMessage = function buildEndpointDiscoveryMessage(timestamp, umpVersionMajor, umpVersionMinor, requestFlags) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="umpVersionMajor" type="Number">A param.</param>
    /// <param name="umpVersionMinor" type="Number">A param.</param>
    /// <param name="requestFlags" type="MidiEndpointDiscoveryFilterFlags">A param.</param>
    /// <returns type="MidiMessage128" />
    /// </signature>
    return new MidiMessage128();
  }


  cls.buildEndpointInformationNotificationMessage = function buildEndpointInformationNotificationMessage(timestamp, umpVersionMajor, umpVersionMinor, hasStaticFunctionBlocks, numberOfFunctionBlocks, supportsMidi20Protocol, supportsMidi10Protocol, supportsReceivingJitterReductionTimestamps, supportsSendingJitterReductionTimestamps) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="umpVersionMajor" type="Number">A param.</param>
    /// <param name="umpVersionMinor" type="Number">A param.</param>
    /// <param name="hasStaticFunctionBlocks" type="Boolean">A param.</param>
    /// <param name="numberOfFunctionBlocks" type="Number">A param.</param>
    /// <param name="supportsMidi20Protocol" type="Boolean">A param.</param>
    /// <param name="supportsMidi10Protocol" type="Boolean">A param.</param>
    /// <param name="supportsReceivingJitterReductionTimestamps" type="Boolean">A param.</param>
    /// <param name="supportsSendingJitterReductionTimestamps" type="Boolean">A param.</param>
    /// <returns type="MidiMessage128" />
    /// </signature>
    return new MidiMessage128();
  }


  cls.buildDeviceIdentityNotificationMessage = function buildDeviceIdentityNotificationMessage(timestamp, deviceManufacturerSysExIdByte1, deviceManufacturerSysExIdByte2, deviceManufacturerSysExIdByte3, deviceFamilyLsb, deviceFamilyMsb, deviceFamilyModelNumberLsb, deviceFamilyModelNumberMsb, softwareRevisionLevelByte1, softwareRevisionLevelByte2, softwareRevisionLevelByte3, softwareRevisionLevelByte4) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="deviceManufacturerSysExIdByte1" type="Number">A param.</param>
    /// <param name="deviceManufacturerSysExIdByte2" type="Number">A param.</param>
    /// <param name="deviceManufacturerSysExIdByte3" type="Number">A param.</param>
    /// <param name="deviceFamilyLsb" type="Number">A param.</param>
    /// <param name="deviceFamilyMsb" type="Number">A param.</param>
    /// <param name="deviceFamilyModelNumberLsb" type="Number">A param.</param>
    /// <param name="deviceFamilyModelNumberMsb" type="Number">A param.</param>
    /// <param name="softwareRevisionLevelByte1" type="Number">A param.</param>
    /// <param name="softwareRevisionLevelByte2" type="Number">A param.</param>
    /// <param name="softwareRevisionLevelByte3" type="Number">A param.</param>
    /// <param name="softwareRevisionLevelByte4" type="Number">A param.</param>
    /// <returns type="MidiMessage128" />
    /// </signature>
    return new MidiMessage128();
  }


  cls.buildEndpointNameNotificationMessages = function buildEndpointNameNotificationMessages(timestamp, name) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="name" type="String">A param.</param>
    /// <returns type="Object" />
    /// </signature>
    return new Object();
  }


  cls.buildProductInstanceIdNotificationMessages = function buildProductInstanceIdNotificationMessages(timestamp, productInstanceId) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="productInstanceId" type="String">A param.</param>
    /// <returns type="Object" />
    /// </signature>
    return new Object();
  }


  cls.parseEndpointNameNotificationMessages = function parseEndpointNameNotificationMessages(messages) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="messages" type="Object">A param.</param>
    /// <returns type="String" />
    /// </signature>
    return new String();
  }


  cls.parseProductInstanceIdNotificationMessages = function parseProductInstanceIdNotificationMessages(messages) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="messages" type="Object">A param.</param>
    /// <returns type="String" />
    /// </signature>
    return new String();
  }


  cls.buildStreamConfigurationRequestMessage = function buildStreamConfigurationRequestMessage(timestamp, protocol, expectToReceiveJRTimestamps, requestToSendJRTimestamps) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="protocol" type="Number">A param.</param>
    /// <param name="expectToReceiveJRTimestamps" type="Boolean">A param.</param>
    /// <param name="requestToSendJRTimestamps" type="Boolean">A param.</param>
    /// <returns type="MidiMessage128" />
    /// </signature>
    return new MidiMessage128();
  }


  cls.buildStreamConfigurationNotificationMessage = function buildStreamConfigurationNotificationMessage(timestamp, protocol, confirmationWillReceiveJRTimestamps, confirmationSendJRTimestamps) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="protocol" type="Number">A param.</param>
    /// <param name="confirmationWillReceiveJRTimestamps" type="Boolean">A param.</param>
    /// <param name="confirmationSendJRTimestamps" type="Boolean">A param.</param>
    /// <returns type="MidiMessage128" />
    /// </signature>
    return new MidiMessage128();
  }


  cls.buildFunctionBlockDiscoveryMessage = function buildFunctionBlockDiscoveryMessage(timestamp, functionBlockNumber, requestFlags) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="functionBlockNumber" type="Number">A param.</param>
    /// <param name="requestFlags" type="MidiFunctionBlockDiscoveryFilterFlags">A param.</param>
    /// <returns type="MidiMessage128" />
    /// </signature>
    return new MidiMessage128();
  }


  cls.buildFunctionBlockInfoNotificationMessage = function buildFunctionBlockInfoNotificationMessage(timestamp, active, functionBlockNumber, uiHint, midi10, direction, firstGroup, numberOfGroups, midiCIVersionFormat, maxNumberSysEx8Streams) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="active" type="Boolean">A param.</param>
    /// <param name="functionBlockNumber" type="Number">A param.</param>
    /// <param name="uiHint" type="MidiFunctionBlockUIHint">A param.</param>
    /// <param name="midi10" type="MidiFunctionBlockMidi10">A param.</param>
    /// <param name="direction" type="MidiFunctionBlockDirection">A param.</param>
    /// <param name="firstGroup" type="Number">A param.</param>
    /// <param name="numberOfGroups" type="Number">A param.</param>
    /// <param name="midiCIVersionFormat" type="Number">A param.</param>
    /// <param name="maxNumberSysEx8Streams" type="Number">A param.</param>
    /// <returns type="MidiMessage128" />
    /// </signature>
    return new MidiMessage128();
  }


  cls.buildFunctionBlockNameNotificationMessages = function buildFunctionBlockNameNotificationMessages(timestamp, functionBlockNumber, name) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="timestamp" type="Number">A param.</param>
    /// <param name="functionBlockNumber" type="Number">A param.</param>
    /// <param name="name" type="String">A param.</param>
    /// <returns type="Object" />
    /// </signature>
    return new Object();
  }


  cls.parseFunctionBlockNameNotificationMessages = function parseFunctionBlockNameNotificationMessages(messages) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="messages" type="Object">A param.</param>
    /// <returns type="String" />
    /// </signature>
    return new String();
  }


  return cls;
}) ();
exports.MidiStreamMessageBuilder = MidiStreamMessageBuilder;

MidiTransportPluginInformation = (function () {
  var cls = function MidiTransportPluginInformation() {
    this.author = new String();
    this.classId = new String();
    this.clientConfigurationAssemblyName = new String();
    this.description = new String();
    this.iconPath = new String();
    this.isClientConfigurable = new Boolean();
    this.isEnabled = new Boolean();
    this.isRuntimeCreatable = new Boolean();
    this.isSystemManaged = new Boolean();
    this.mnemonic = new String();
    this.name = new String();
    this.registryKey = new String();
    this.shortName = new String();
  };
  

  return cls;
}) ();
exports.MidiTransportPluginInformation = MidiTransportPluginInformation;

MidiUniqueId = (function () {
  var cls = function MidiUniqueId() {
    this.byte4 = new Number();
    this.byte3 = new Number();
    this.byte2 = new Number();
    this.byte1 = new Number();
  };
  
var cls = function MidiUniqueId(sevenBitByte1, sevenBitByte2, sevenBitByte3, sevenBitByte4) {
      this.byte4 = new Number();
      this.byte3 = new Number();
      this.byte2 = new Number();
      this.byte1 = new Number();
};


  cls.labelFull = new String();
  cls.labelShort = new String();
  return cls;
}) ();
exports.MidiUniqueId = MidiUniqueId;

MidiVirtualDevice = (function () {
  var cls = function MidiVirtualDevice() {
    this.tag = new Object();
    this.name = new String();
    this.isEnabled = new Boolean();
    this.id = new String();
    this.suppressHandledMessages = new Boolean();
    this.endpointProductInstanceId = new String();
    this.endpointName = new String();
    this.areFunctionBlocksStatic = new Boolean();
    this.functionBlocks = new Object();
  };
  

  cls.prototype.addFunctionBlock = function addFunctionBlock(block) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="block" type="MidiFunctionBlock">A param.</param>
    /// <returns type="Boolean" />
    /// </signature>
    return new Boolean();
  }


  cls.prototype.updateFunctionBlock = function updateFunctionBlock(block) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="block" type="MidiFunctionBlock">A param.</param>
    /// </signature>
  }


  cls.prototype.removeFunctionBlock = function removeFunctionBlock(functionBlockNumber) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="functionBlockNumber" type="Number">A param.</param>
    /// </signature>
  }


  cls.prototype.initialize = function initialize(endpointConnection) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="endpointConnection" type="IMidiEndpointConnectionSource">A param.</param>
    /// </signature>
  }


  cls.prototype.onEndpointConnectionOpened = function onEndpointConnectionOpened() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// </signature>
  }


  cls.prototype.processIncomingMessage = function processIncomingMessage(args, skipFurtherListeners, skipMainMessageReceivedEvent) {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// <param name="args" type="MidiMessageReceivedEventArgs">A param.</param>
    /// <param name="skipFurtherListeners" type="Boolean">A param.</param>
    /// <param name="skipMainMessageReceivedEvent" type="Boolean">A param.</param>
    /// </signature>
  }


  cls.prototype.cleanup = function cleanup() {
    /// <signature>
    /// <summary>Function summary.</summary>
    /// </signature>
  }


  return cls;
}) ();
exports.MidiVirtualDevice = MidiVirtualDevice;

