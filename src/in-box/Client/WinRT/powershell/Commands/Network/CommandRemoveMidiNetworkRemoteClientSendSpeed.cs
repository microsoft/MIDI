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

            var settingsList = GetOtherRemoteClientSettings(out var hasThisRemoteClient);

            if (settingsList is null)
            {
                return;
            }

            if (!hasThisRemoteClient)
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

            SendRemoteClientSettings(settingsList);
        }
    }

}
