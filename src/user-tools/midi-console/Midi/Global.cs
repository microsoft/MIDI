// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

global using System;
global using System.Collections;
global using System.Collections.Generic;
global using System.ComponentModel;

global using Spectre.Console;
global using Spectre.Console.Cli;

//global using Microsoft.Windows.Devices.Midi2.Initialization;
global using Windows.Devices.Midi2;
global using Windows.Devices.Midi2.Diagnostics;
global using Windows.Devices.Midi2.Enumeration;
global using Windows.Devices.Midi2.Enumeration.Legacy;
global using Windows.Devices.Midi2.Utilities.Messages;
global using Windows.Devices.Midi2.Reporting;
global using Windows.Devices.Midi2.Transports.BasicLoopback;
global using Windows.Devices.Midi2.Transports.Loopback;
global using Windows.Devices.Midi2.Transports.Virtual;

//global using Microsoft.Windows.Devices.Midi2.Utilities.SysExTransfer;
//global using Microsoft.Windows.Devices.Midi2.Utilities.Sequencing;

global using Microsoft.Midi.ConsoleApp;
global using Microsoft.Midi.ConsoleApp.Resources;