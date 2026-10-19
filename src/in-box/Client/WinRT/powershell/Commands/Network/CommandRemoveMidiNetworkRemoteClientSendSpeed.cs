// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management.Automation;

using Windows.Devices.Midi2.Transports.Network;

namespace WindowsMidiServices
{
    // The device goes back to the host's sending speed. A device already connected changes speed
    // straight away.
    [Cmdlet(VerbsCommon.Remove, "MidiNetworkRemoteClientSendSpeed", SupportsShouldProcess = true)]
    public class CommandRemoveMidiNetworkRemoteClientSendSpeed : MidiNetworkRemoteClientSpeedCmdletBase
    {
        protected override void ProcessRecord()
        {
            RequireMidiServices();
            RequireTransport(MidiNetworkTransportManager.IsTransportAvailable, Strings.TransportNameNetwork);

            var settingsList = GetOtherRemoteClientSettings(out var existing);

            if (settingsList is null)
            {
                return;
            }

            // An entry which is only there for the device's MIDI 1.0 ports has no speed to remove
            if (existing is null || existing.UseHostSendSpeed)
            {
                WriteNonTerminating(
                    new ItemNotFoundException(Format(Strings.RemoteClientSpeedNotFoundFormat, RemoteClientTarget)),
                    "MidiNetworkRemoteClientSpeedNotFound",
                    ErrorCategory.ObjectNotFound,
                    RemoteClientTarget);

                return;
            }

            if (!ShouldProcess(RemoteClientTarget, Strings.RemoteClientSpeedRemoveAction))
            {
                return;
            }

            // MIDI 1.0 ports set for this device stay, and it goes back to the host's speed
            if (existing.FallbackMidi1PortGroups.Count > 0)
            {
                var portsOnly = new MidiNetworkRemoteClientSettings(existing.RemoteClientName, existing.RemoteClientProductInstanceId)
                {
                    UseHostSendSpeed = true
                };

                foreach (var group in existing.FallbackMidi1PortGroups)
                {
                    portsOnly.FallbackMidi1PortGroups.Add(group);
                }

                settingsList.Add(portsOnly);
            }

            SendRemoteClientSettings(settingsList);
        }
    }

}
