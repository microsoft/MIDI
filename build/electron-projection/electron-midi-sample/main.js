// The WinRT projection was built using a customized version of NodeRT
// generic NodeRT guidance here: https://github.com/NodeRT/NodeRT

const { app, BrowserWindow } = require('electron');
const path = require('node:path');
const midi2init = require('microsoft.windows.devices.midi2.initialization');
const midi2 = require('microsoft.windows.devices.midi2');

// these other namespaces cover other capabilities
//const ci = require('microsoft.windows.devices.midi2.capabilityinquiry');
//const plug = require('microsoft.windows.devices.midi2.clientplugins');
//const diag = require('microsoft.windows.devices.midi2.diagnostics');
//const loop = require('microsoft.windows.devices.midi2.endpoints.loopback');
//const virt = require('microsoft.windows.devices.midi2.endpoints.virtual');
//const msgs = require('microsoft.windows.devices.midi2.messages');
//const rept = require('microsoft.windows.devices.midi2.reporting');
//const svc = require('microsoft.windows.devices.midi2.serviceconfig');
//const sysex = require('microsoft.windows.devices.midi2.utilities.sysextransfer');
//const vpb = require('microsoft.windows.devices.midi2.virtualpatchbay');


// initialize Windows MIDI Services

var initSuccess = midi2init.MidiServicesInitializer.ensureServiceAvailable();

// create a new session
var session = midi2.MidiSession.createSession("Electron Test Session");


function createWindow () {
    const mainWindow = new BrowserWindow({
      width: 800,
      height: 600,
      webPreferences: {
        preload: path.join(__dirname, 'preload.js')
      }      
    })
  
    mainWindow.loadFile('index.html')

//    console.log(midi2.MidiClock.now);

    // Enumerate endpoints
    console.log("Enumerating endpoints");

    const endpoints = midi2.MidiEndpointDeviceInformation.findAll(
        midi2.MidiEndpointDeviceInformationSortOrder.name, 
        midi2.MidiEndpointDeviceInformationFilter.includeDiagnosticLoopback |
            midi2.MidiEndpointDeviceInformationFilter.includeClientUmpNative |
            midi2.MidiEndpointDeviceInformationFilter.includeClientByteStreamNative);

    console.log(endpoints);

    for (var i = 0; i < endpoints.size; i++)
    {
        var endpoint = endpoints.getAt(i);

        console.log(endpoint.id);
        console.log(endpoint.name);
        console.log("------------------------------------------------");
        console.log("");
    }

 
    // these are the hard-wired IDs that are always present if Windows MIDI Services is present
    const loopbackAId = midi2.MidiEndpointDeviceInformation.diagnosticsLoopbackAEndpointId;
    const loopbackBId = midi2.MidiEndpointDeviceInformation.diagnosticsLoopbackBEndpointId;

    console.log("Creating a new session");

    console.log("Creating send and receive connections");

    // connect to loopbacks A and B
    var sendConnection = session.createEndpointConnection(loopbackAId);
    var receiveConnection = session.createEndpointConnection(loopbackBId);

    // wire up the event handler for incoming messages.
    // loopback A and B are hard-wired together in the service, so any message
    // send out on A will arrive in on B and vice versa

    console.log("Wiring up event handler");

    const messageReceiveHandler = (sender, args) => {
        // there are several ways to read the incoming message. 
        // One is to use strongly-typed message types. Another is to read an array, and still another is to 
        // get back an interface you will cast to an appropriate type. Lots of options there, and different 
        // ones work better in different client languages, and they have different amounts of overhead.

        var packet = args.getMessagePacket();

        if (packet.packetType == midi2.MidiPacketType.universalMidiPacket32) {
            const ump = midi2.MidiMessage32.castFrom(packet);

            // output in hex
            console.log(args.timestamp.toString(10) + ", 0x" + ump.word0.toString(16));
        }
        else if (packet.packetType == midi2.MidiPacketType.universalMidiPacket64) {
            const ump = midi2.MidiMessage64.castFrom(packet);

            // output in hex
            console.log(args.timestamp.toString(10) + ", 0x" + ump.word0.toString(16) + ", 0x" + ump.word1.toString(16));
        }
        else if (packet.packetType == midi2.MidiPacketType.universalMidiPacket96) {
            const ump = midi2.MidiMessage96.castFrom(packet);

            // output in hex
            console.log(args.timestamp.toString(10) + ", 0x" + ump.word0.toString(16) + ", 0x" + ump.word1.toString(16) + ", 0x" + ump.word2.toString(16));
        }
        else if (packet.packetType == midi2.MidiPacketType.universalMidiPacket128) {
            const ump = midi2.MidiMessage128.castFrom(packet);

            // output in hex
            console.log(args.timestamp.toString(10) + ", 0x" + ump.word0.toString(16) + ", 0x" + ump.word1.toString(16) + ", 0x" + ump.word2.toString(16) + ", 0x" + ump.word3.toString(16));
        }
        
    }

    // this connects the event
    receiveConnection.on("MessageReceived", messageReceiveHandler);

    console.log("Opening the connections");

    // connection needs to be opened before it is used. Open the connection
    // after you've wired up your event handlers
    sendConnection.open();
    receiveConnection.open();


    console.log("Sending messages");

    // send messages out to that endpoint
    for (var j = 0; j < 1000; j++)
    {
        sendConnection.sendMessageWords(midi2.MidiClock.now, 0x48675309, 0xDEADBEEF);
    }

  }


  app.whenReady().then(() => {
    createWindow();
  })

  app.on('window-all-closed', () => {

    console.log("Closing session");

    session.close();


    if (process.platform !== 'darwin') app.quit();
  })


  const fs = require('fs');
  
  const npmScope = '';
  
  // This little trick makes Node.js Tools for VS load IntelliSense for the module
 // if (fs.existsSync(path.join(__dirname, 'NodeRT_Windows_Devices_Midi2.d.js)'))) {
 //   module.exports = require('./NodeRT_Windows_Devices_Midi2.d.js');
 // } else {
 //   module.exports = require('../build/Release/binding.node');
 // }
  
var externalReferencedNamespaces = 
    ['Windows.Foundation', 'Windows.Devices.Enumeration', 'Windows.Data.Json', 'Windows.Devices.Midi'];

if (externalReferencedNamespaces.length > 0) {
  var namespaceRegistry = global.__winRtNamespaces__;

  if (!namespaceRegistry) {
    namespaceRegistry = {};

    Object.defineProperty(global, '__winRtNamespaces__', {
      configurable: true,
      writable: false,
      enumerable: false,
      value: namespaceRegistry
    });
  }

  function requireNamespace(namespace) {
    var moduleName = namespace.toLowerCase();

    if (npmScope) {
      moduleName = '@' + npmScope + '/' + moduleName;
    }

    var m = require(moduleName);
    delete namespaceRegistry[namespace];
    namespaceRegistry[namespace] = m;
    return m;
  }

  for (var i in externalReferencedNamespaces) {
    var ns = externalReferencedNamespaces[i];

    if (!namespaceRegistry.hasOwnProperty(ns)) {
      Object.defineProperty(namespaceRegistry, ns, {
        configurable: true,
        enumerable: true,
        get: requireNamespace.bind(null, ns)
      });
    }
  }
}
