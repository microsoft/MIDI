// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management.Automation;

using Windows.Devices.Midi2.ServiceConfig;
using Windows.Devices.Midi2.Transports.Rtp;

namespace WindowsMidiServices
{
    // Shared by the cmdlets which change the sending speed a host on this PC uses for one device.
    // The service replaces a host's whole list on every change, so each change sends all of it.
    public abstract class MidiRtpRemoteClientSpeedCmdletBase : MidiCmdletBase
    {
        [Parameter(Mandatory = true, Position = 0, ValueFromPipelineByPropertyName = true)]
        public Guid HostId { get; set; }

        // The name the device gives when it connects, which is how the host knows it again
        [Parameter(Mandatory = true, Position = 1, ValueFromPipelineByPropertyName = true)]
        [ValidateNotNullOrWhiteSpace]
        public string RemoteClientName { get; set; } = string.Empty;

        [Parameter]
        public SwitchParameter SaveToConfiguration { get; set; }

        protected string RemoteClientTarget => RemoteClientName.Trim();

        // The host's list without this device, or null when there is no such host. The service
        // ignores case when it matches a device, so this does too.
        protected List<MidiRtpRemoteClientSettings>? GetOtherRemoteClientSettings(out bool hasThisRemoteClient)
        {
            hasThisRemoteClient = false;

            MidiRtpConfiguredHost? host = null;
            var hosts = MidiRtpTransportManager.GetConfiguredHosts();

            if (hosts is not null)
            {
                host = hosts.FirstOrDefault(candidate => candidate is not null && candidate.HostId == HostId);
            }

            if (host is null)
            {
                WriteNonTerminating(
                    new ItemNotFoundException(Format(Strings.HostNotFoundFormat, HostId)),
                    "MidiRtpHostNotFound",
                    ErrorCategory.ObjectNotFound,
                    HostId);

                return null;
            }

            var others = new List<MidiRtpRemoteClientSettings>();

            if (host.RemoteClientSettings is null)
            {
                return others;
            }

            foreach (var settings in host.RemoteClientSettings)
            {
                if (settings is null)
                {
                    continue;
                }

                if (string.Equals(settings.RemoteClientName.Trim(), RemoteClientTarget, StringComparison.OrdinalIgnoreCase))
                {
                    hasThisRemoteClient = true;
                    continue;
                }

                others.Add(settings);
            }

            return others;
        }

        // Applies the list to the running host first, and saves it only when asked
        protected bool SendRemoteClientSettings(IEnumerable<MidiRtpRemoteClientSettings> settingsList)
        {
            var config = new MidiRtpHostRemoteClientSettingsConfig(HostId);

            foreach (var settings in settingsList)
            {
                config.RemoteClientSettings.Add(settings);
            }

            var response = MidiServiceTransportPluginConfigManager.SendUpdate(config);

            if (response is null || response.Status != MidiServiceConfigResponseStatus.Success)
            {
                WriteNonTerminating(
                    new InvalidOperationException(response is null
                        ? Strings.RemoteClientSpeedNotApplied
                        : Format(Strings.RemoteClientSpeedNotAppliedFormat, response.Status)),
                    "MidiRtpRemoteClientSpeedFailed",
                    ErrorCategory.WriteError,
                    RemoteClientTarget);

                return false;
            }

            if (SaveToConfiguration.IsPresent)
            {
                SaveToConfigurationFile(config);
            }
            else
            {
                WriteVerbose(Strings.RemoteClientSpeedTransient);
            }

            return true;
        }
    }
}
