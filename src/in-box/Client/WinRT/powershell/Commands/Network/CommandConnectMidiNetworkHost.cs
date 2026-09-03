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

    [Cmdlet(VerbsCommunications.Connect, "MidiNetworkHost", SupportsShouldProcess = true, DefaultParameterSetName = AdvertisedHostParameterSet)]
    [OutputType(typeof(MidiNetworkClientConnectResponse))]
    public class CommandConnectMidiNetworkHost : MidiCmdletBase
    {
        private const string AdvertisedHostParameterSet = "AdvertisedHost";
        private const string DeviceIdParameterSet = "DeviceId";
        private const string AddressParameterSet = "Address";

        [Parameter(Mandatory = true, Position = 0, ValueFromPipeline = true, ParameterSetName = AdvertisedHostParameterSet)]
        public MidiNetworkAdvertisedHost? AdvertisedHost { get; set; }

        [Parameter(Mandatory = true, Position = 0, ValueFromPipelineByPropertyName = true, ParameterSetName = DeviceIdParameterSet)]
        [ValidateNotNullOrWhiteSpace]
        public string DeviceId { get; set; } = string.Empty;

        [Parameter(Mandatory = true, Position = 0, ParameterSetName = AddressParameterSet)]
        [ValidateNotNullOrWhiteSpace]
        [Alias("IPAddress")]
        public string HostNameOrAddress { get; set; } = string.Empty;

        [Parameter(Mandatory = true, Position = 1, ParameterSetName = AddressParameterSet)]
        [ValidateRange(1, 65535)]
        public ushort Port { get; set; }

        // The name this PC announces to the remote. Empty derives it from the machine name.
        [Parameter]
        public string LocalEndpointName { get; set; } = string.Empty;

        // What Windows calls the endpoint this connection creates. Empty uses the name the
        // remote announces.
        [Parameter]
        public string EndpointName { get; set; } = string.Empty;

        // Reusing the identifier of an existing entry re-arms that entry rather than adding a
        // second one, which is how a direct connection marked Unavailable is retried.
        [Parameter(ValueFromPipelineByPropertyName = true)]
        public Guid ClientId { get; set; }

        [Parameter]
        public SwitchParameter CreateOnlyUmpEndpoints { get; set; }

        [Parameter]
        public SwitchParameter SaveToConfiguration { get; set; }

        protected override void ProcessRecord()
        {
            RequireMidiServices();
            RequireTransport(MidiNetworkTransportManager.IsTransportAvailable, "Network MIDI 2.0");

            var criteria = new MidiNetworkClientMatchCriteria();
            string target;

            if (ParameterSetName == AddressParameterSet)
            {
                criteria.DirectHostNameOrIPAddress = HostNameOrAddress;
                criteria.DirectPort = Port;

                target = $"{HostNameOrAddress}:{Port}";
            }
            else if (ParameterSetName == DeviceIdParameterSet)
            {
                criteria.DeviceId = DeviceId;

                target = DeviceId;
            }
            else
            {
                if (AdvertisedHost is null)
                {
                    ThrowTerminating(
                        new ArgumentNullException(nameof(AdvertisedHost)),
                        "MidiNetworkHostRequired",
                        ErrorCategory.InvalidArgument);

                    return;
                }

                // Matching on the device id lets the service re-resolve the address from the
                // advertisement, so the connection survives the remote moving. The product
                // instance id and endpoint name are the device's own identity, which is what
                // still resolves the entry when its advertised label changes.
                criteria.DeviceId = AdvertisedHost.DeviceId;
                criteria.ProductInstanceId = AdvertisedHost.ProductInstanceId;
                criteria.UmpEndpointName = AdvertisedHost.UmpEndpointName;

                target = string.IsNullOrEmpty(AdvertisedHost.DeviceName) ? AdvertisedHost.DeviceId : AdvertisedHost.DeviceName;
            }

            if (!ShouldProcess(target, "Connect to network MIDI host"))
            {
                return;
            }

            var config = new MidiNetworkClientConnectConfig
            {
                ClientId = ClientId == Guid.Empty ? Guid.NewGuid() : ClientId,
                UmpEndpointName = LocalEndpointName,
                CustomEndpointName = EndpointName,
                CreateOnlyUmpEndpoints = CreateOnlyUmpEndpoints.IsPresent,
                MatchCriteria = criteria
            };

            var response = MidiNetworkTransportManager.ConnectNetworkClientAsync(config).GetAwaiter().GetResult();

            if (response is null || !response.Success)
            {
                WriteNonTerminating(
                    new InvalidOperationException(response is null ? "Unable to connect to the host." : response.ErrorMessage),
                    "MidiNetworkConnectFailed",
                    ErrorCategory.ConnectionError,
                    target);

                return;
            }

            if (SaveToConfiguration.IsPresent)
            {
                SaveToConfigurationFile(config);
            }
            else
            {
                WriteVerbose("This connection is transient and will be lost when the service restarts. Use -SaveToConfiguration to keep it.");
            }

            WriteObject(response);
        }
    }

}
