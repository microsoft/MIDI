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
    // Ends the connection and removes the entry from the running service. An entry saved in the
    // configuration file comes back when the service restarts.
    [Cmdlet(VerbsCommunications.Disconnect, "MidiRtpHost", SupportsShouldProcess = true, DefaultParameterSetName = ClientIdParameterSet)]
    [OutputType(typeof(MidiRtpClientDisconnectResponse))]
    public class CommandDisconnectMidiRtpHost : MidiCmdletBase
    {
        private const string ClientIdParameterSet = "ClientId";
        private const string ServiceInstanceNameParameterSet = "ServiceInstanceName";
        private const string AddressParameterSet = "Address";

        [Parameter(Mandatory = true, Position = 0, ValueFromPipelineByPropertyName = true, ParameterSetName = ClientIdParameterSet)]
        public Guid ClientId { get; set; }

        [Parameter(Mandatory = true, Position = 0, ValueFromPipelineByPropertyName = true, ParameterSetName = ServiceInstanceNameParameterSet)]
        [ValidateNotNullOrWhiteSpace]
        [Alias("RemoteServiceInstanceName")]
        public string ServiceInstanceName { get; set; } = string.Empty;

        [Parameter(Mandatory = true, Position = 0, ParameterSetName = AddressParameterSet)]
        [ValidateNotNullOrWhiteSpace]
        [Alias("IPAddress")]
        public string HostNameOrAddress { get; set; } = string.Empty;

        // Zero, the default, means 5004, matching Connect-MidiRtpHost.
        [Parameter(Position = 1, ParameterSetName = AddressParameterSet)]
        [ValidateRange(1, 65535)]
        public ushort Port { get; set; }

        [Parameter]
        public SwitchParameter PassThru { get; set; }

        protected override void ProcessRecord()
        {
            RequireMidiServices();
            RequireTransport(MidiRtpTransportManager.IsTransportAvailable, Strings.TransportNameRtp);

            var clientIds = ResolveClientIds(out var target);

            if (clientIds.Count == 0)
            {
                WriteNonTerminating(
                    new ItemNotFoundException(Format(Strings.RtpClientNotFoundFormat, target)),
                    "MidiRtpClientNotFound",
                    ErrorCategory.ObjectNotFound,
                    target);

                return;
            }

            foreach (var clientId in clientIds)
            {
                if (!ShouldProcess(clientId.ToString(), Strings.RtpDisconnectAction))
                {
                    continue;
                }

                var config = new MidiRtpClientDisconnectConfig(clientId);

                var response = MidiRtpTransportManager.DisconnectRtpClientAsync(config).GetAwaiter().GetResult();

                if (response is null || !response.Success)
                {
                    WriteNonTerminating(
                        new InvalidOperationException(response is null ? Strings.HostDisconnectFailed : response.ErrorMessage),
                        "MidiRtpDisconnectFailed",
                        ErrorCategory.ConnectionError,
                        clientId);

                    continue;
                }

                if (PassThru.IsPresent)
                {
                    WriteObject(response);
                }
            }
        }

        // Only the client identifier reaches the service, so the other parameter sets resolve
        // through the configured client list first.
        private List<Guid> ResolveClientIds(out string target)
        {
            if (ParameterSetName == ClientIdParameterSet)
            {
                target = ClientId.ToString();

                return ClientId == Guid.Empty ? [] : [ClientId];
            }

            var port = Port == 0 ? MidiRtpTransportManager.DefaultHostPort : Port;

            target = ParameterSetName == ServiceInstanceNameParameterSet ? ServiceInstanceName : $"{HostNameOrAddress}:{port}";

            var matches = new List<Guid>();
            var clients = MidiRtpTransportManager.GetConfiguredClients();

            if (clients is null)
            {
                return matches;
            }

            foreach (var client in clients)
            {
                var isMatch = ParameterSetName == ServiceInstanceNameParameterSet
                    ? string.Equals(client.RemoteServiceInstanceName, ServiceInstanceName, StringComparison.OrdinalIgnoreCase)
                    : IsAddressMatch(client, port);

                if (isMatch)
                {
                    matches.Add(client.ClientId);
                }
            }

            return matches;
        }

        private bool IsAddressMatch(MidiRtpConfiguredClient client, ushort port)
        {
            // An entry configured by address is matched on what was configured. One which is
            // connected is also matched on where it actually landed, because a host name was
            // resolved to an address somewhere along the way.
            if (client.IsDirectConnection &&
                string.Equals(client.ConfiguredDirectAddress, HostNameOrAddress, StringComparison.OrdinalIgnoreCase) &&
                client.ConfiguredDirectPort == port)
            {
                return true;
            }

            var connection = client.Connection;

            return connection is not null &&
                   string.Equals(connection.RemoteAddress, HostNameOrAddress, StringComparison.OrdinalIgnoreCase) &&
                   connection.RemotePort == port;
        }
    }

}
