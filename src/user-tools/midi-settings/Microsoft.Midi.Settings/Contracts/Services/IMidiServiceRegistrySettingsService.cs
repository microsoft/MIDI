// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Contracts.Services
{
    // this must be kept in sync with the one in MidiDefs.h
    public enum Midi1PortNameSelectionProperty
    {
        PortName_UseGlobalDefault = 0,                  // global default tells us which to defer to
        PortName_UseLegacyWinMM = 10,                  // compatible with pre-Windows MIDI Services WinMM port names

        PortName_UsePinName = 50,                      // names including the iJack name that customers have asked us for
        PortName_UseInterfacePlusPinName = 51,         // names including the iJack name that customers have asked us for

        PortName_UseGroupTerminalBlocksExactly = 500,   // for devices named by MIDI 2 driver
    };

    public interface IMidiServiceRegistrySettingsService
    {
        bool GetDefaultUseNewStyleMidi1PortNaming();
        bool SetDefaultUseNewStyleMidi1PortNaming(Midi1PortNameSelectionProperty newValue);


        bool GetUseMMCSS();
        bool SetUseMMCSS(bool newValue);


        bool GetMidi2DiscoveryEnabled();
        bool SetMidi2DiscoveryEnabled(bool newValue);


        UInt32 GetMidi2DiscoveryTimeoutMS();
        bool SetMidi2DiscoveryTimeoutMS(UInt32 newValue);


        bool IsConfigFileSpecified();

        string GetCurrentConfigFileName();

        bool UpdateRegistryCurrentConfigFileName(string configFileName);

    }
}
