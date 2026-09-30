// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management.Automation;

using Windows.Devices.Midi2.Transports.Rtp;

namespace WindowsMidiServices
{
    // Returns once the service has the entry. Connecting happens in the background, so watch the
    // entry's EntryState with Get-MidiRtpConfiguredClient, or wait for its endpoint to appear.
    [Cmdlet(VerbsCommunications.Connect, "MidiRtpHost", SupportsShouldProcess = true, DefaultParameterSetName = AdvertisedHostParameterSet)]
    [OutputType(typeof(MidiRtpClientConnectResponse))]
    public class CommandConnectMidiRtpHost : MidiCmdletBase
    {
        private const string AdvertisedHostParameterSet = "AdvertisedHost";
        private const string ServiceInstanceNameParameterSet = "ServiceInstanceName";
        private const string AddressParameterSet = "Address";

        [Parameter(Mandatory = true, Position = 0, ValueFromPipeline = true, ParameterSetName = AdvertisedHostParameterSet)]
        public MidiRtpAdvertisedHost? AdvertisedHost { get; set; }

        // Found again by name each time it connects, so the connection survives the remote
        // moving to a new address.
        [Parameter(Mandatory = true, Position = 0, ValueFromPipelineByPropertyName = true, ParameterSetName = ServiceInstanceNameParameterSet)]
        [ValidateNotNullOrWhiteSpace]
        public string ServiceInstanceName { get; set; } = string.Empty;

        [Parameter(Mandatory = true, Position = 0, ParameterSetName = AddressParameterSet)]
        [ValidateNotNullOrWhiteSpace]
        [Alias("IPAddress")]
        public string HostNameOrAddress { get; set; } = string.Empty;

        // The remote's control port. Zero, the default, means 5004, where RTP-MIDI devices listen
        // unless set otherwise.
        [Parameter(Position = 1, ParameterSetName = AddressParameterSet)]
        [ValidateRange(1, 65535)]
        public ushort Port { get; set; }

        // What the remote shows for this PC. Empty uses this PC's name.
        [Parameter]
        public string LocalEndpointName { get; set; } = string.Empty;

        // What Windows calls the endpoint this connection creates. Empty uses the name the
        // remote sends.
        [Parameter]
        public string EndpointName { get; set; } = string.Empty;

        // Reusing the identifier of an existing entry replaces that entry and tries it again now,
        // which is how a connection marked Unavailable is retried.
        [Parameter(ValueFromPipelineByPropertyName = true)]
        public Guid ClientId { get; set; }

        [Parameter]
        public SwitchParameter SaveToConfiguration { get; set; }

        protected override void ProcessRecord()
        {
            RequireMidiServices();
            RequireTransport(MidiRtpTransportManager.IsTransportAvailable, Strings.TransportNameRtp);

            var criteria = new MidiRtpClientMatchCriteria();
            string target;

            if (ParameterSetName == AddressParameterSet)
            {
                var port = Port == 0 ? MidiRtpTransportManager.DefaultHostPort : Port;

                criteria.DirectHostNameOrIPAddress = HostNameOrAddress;
                criteria.DirectPort = port;

                target = $"{HostNameOrAddress}:{port}";
            }
            else if (ParameterSetName == ServiceInstanceNameParameterSet)
            {
                criteria.ServiceInstanceName = ServiceInstanceName;

                target = ServiceInstanceName;
            }
            else
            {
                if (AdvertisedHost is null)
                {
                    ThrowTerminating(
                        new ArgumentNullException(nameof(AdvertisedHost)),
                        "MidiRtpHostRequired",
                        ErrorCategory.InvalidArgument);

                    return;
                }

                criteria.ServiceInstanceName = AdvertisedHost.ServiceInstanceName;

                target = AdvertisedHost.ServiceInstanceName;
            }

            if (!ShouldProcess(target, Strings.RtpConnectAction))
            {
                return;
            }

            var reusingClientId = ClientId != Guid.Empty;

            var config = new MidiRtpClientConnectConfig
            {
                ClientId = reusingClientId ? ClientId : Guid.NewGuid(),
                Name = LocalEndpointName,
                CustomEndpointName = EndpointName,
                MatchCriteria = criteria
            };

            var response = MidiRtpTransportManager.ConnectRtpClientAsync(config).GetAwaiter().GetResult();

            if (response is null || !response.Success)
            {
                WriteNonTerminating(
                    new InvalidOperationException(response is null ? Strings.HostConnectFailed : response.ErrorMessage),
                    "MidiRtpConnectFailed",
                    ErrorCategory.ConnectionError,
                    target);

                return;
            }

            // The service keeps an entry with unchanged settings as it is, so a retry has to be
            // asked for. It does nothing to an entry which is connected or already trying.
            if (reusingClientId)
            {
                var reconnect = MidiRtpTransportManager.ReconnectRtpClientAsync(config.ClientId).GetAwaiter().GetResult();

                if (reconnect is null || !reconnect.Success)
                {
                    WriteWarning(reconnect is null
                        ? Strings.RtpReconnectUnanswered
                        : Format(Strings.RtpReconnectRefusedFormat, reconnect.ErrorMessage));
                }
            }

            if (SaveToConfiguration.IsPresent)
            {
                SaveToConfigurationFile(config);
            }
            else
            {
                WriteVerbose(Strings.ConnectionTransient);
            }

            WriteObject(response);
        }
    }

}
