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

    [Cmdlet(VerbsCommunications.Disconnect, "MidiNetworkHost", SupportsShouldProcess = true, DefaultParameterSetName = ClientIdParameterSet)]
    [OutputType(typeof(MidiNetworkClientDisconnectResponse))]
    public class CommandDisconnectMidiNetworkHost : MidiCmdletBase
    {
        private const string ClientIdParameterSet = "ClientId";
        private const string DeviceIdParameterSet = "DeviceId";
        private const string AddressParameterSet = "Address";

        [Parameter(Mandatory = true, Position = 0, ValueFromPipelineByPropertyName = true, ParameterSetName = ClientIdParameterSet)]
        public Guid ClientId { get; set; }

        [Parameter(Mandatory = true, Position = 0, ValueFromPipelineByPropertyName = true, ParameterSetName = DeviceIdParameterSet)]
        [ValidateNotNullOrWhiteSpace]
        [Alias("MatchDeviceId")]
        public string DeviceId { get; set; } = string.Empty;

        [Parameter(Mandatory = true, Position = 0, ParameterSetName = AddressParameterSet)]
        [ValidateNotNullOrWhiteSpace]
        [Alias("IPAddress")]
        public string HostNameOrAddress { get; set; } = string.Empty;

        [Parameter(Mandatory = true, Position = 1, ParameterSetName = AddressParameterSet)]
        [ValidateRange(1, 65535)]
        public ushort Port { get; set; }

        [Parameter]
        public SwitchParameter PassThru { get; set; }

        protected override void ProcessRecord()
        {
            RequireMidiServices();
            RequireTransport(MidiNetworkTransportManager.IsTransportAvailable, "Network MIDI 2.0");

            var clientIds = ResolveClientIds(out var target);

            if (clientIds.Count == 0)
            {
                WriteNonTerminating(
                    new ItemNotFoundException($"No configured network MIDI client matches {target}."),
                    "MidiNetworkClientNotFound",
                    ErrorCategory.ObjectNotFound,
                    target);

                return;
            }

            foreach (var clientId in clientIds)
            {
                if (!ShouldProcess(clientId.ToString(), "Disconnect network MIDI host"))
                {
                    continue;
                }

                var config = new MidiNetworkClientDisconnectConfig(clientId);

                var response = MidiNetworkTransportManager.DisconnectNetworkClientAsync(config).GetAwaiter().GetResult();

                if (response is null || !response.Success)
                {
                    WriteNonTerminating(
                        new InvalidOperationException(response is null ? "Unable to disconnect the host." : response.ErrorMessage),
                        "MidiNetworkDisconnectFailed",
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

            target = ParameterSetName == DeviceIdParameterSet ? DeviceId : $"{HostNameOrAddress}:{Port}";

            var matches = new List<Guid>();
            var clients = MidiNetworkTransportManager.GetConfiguredClients();

            if (clients is null)
            {
                return matches;
            }

            var port = Port.ToString();

            foreach (var client in clients)
            {
                var isMatch = ParameterSetName == DeviceIdParameterSet
                    ? string.Equals(client.MatchDeviceId, DeviceId, StringComparison.OrdinalIgnoreCase)
                    : IsAddressMatch(client, port);

                if (isMatch)
                {
                    matches.Add(client.ClientId);
                }
            }

            return matches;
        }

        private bool IsAddressMatch(MidiNetworkConfiguredClient client, string port)
        {
            // An entry configured by address is matched on what was configured. One which is
            // connected is also matched on where it actually landed, because a host name was
            // resolved to an address somewhere along the way.
            if (string.Equals(client.ConfiguredDirectAddress, HostNameOrAddress, StringComparison.OrdinalIgnoreCase) &&
                string.Equals(client.ConfiguredDirectPort, port, StringComparison.OrdinalIgnoreCase))
            {
                return true;
            }

            return string.Equals(client.ConnectedRemoteAddress, HostNameOrAddress, StringComparison.OrdinalIgnoreCase) &&
                   string.Equals(client.ConnectedRemotePort, port, StringComparison.OrdinalIgnoreCase);
        }
    }

}
