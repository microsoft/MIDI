// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Windows.Devices.Midi2.Utilities.RuntimeInformation;


namespace Microsoft.Midi.Settings.Contracts.Services;

public interface IMidiServiceRegistrySettingsService
{
    bool GetDefaultUseNewStyleMidi1PortNaming();
    bool SetDefaultUseNewStyleMidi1PortNaming(Midi1PortNamingApproach newValue);


    bool GetUseMMCSS();
    bool SetUseMMCSS(bool newValue);


    bool GetMidi2DiscoveryEnabled();
    bool SetMidi2DiscoveryEnabled(bool newValue);


    UInt32 GetMidi2DiscoveryTimeoutMS();
    bool SetMidi2DiscoveryTimeoutMS(UInt32 newValue);


    MidiRuntimeReleaseTypes GetPreferredSdkRuntimeReleaseType(MidiRuntimeReleaseTypes defaultIfMissing);
    bool SetPreferredSdkRuntimeReleaseType(MidiRuntimeReleaseTypes releaseType);


    bool GetAutoCheckForUpdatesEnabled();
    bool SetAutoCheckForUpdatesEnabled(bool newValue);
    



    bool IsConfigFileSpecified();

    string GetCurrentConfigFileName();

    bool UpdateRegistryCurrentConfigFileName(string configFileName);

}
