// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management.Automation;

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.CapabilityInquiry;

namespace WindowsMidiServices
{
    // Shared by the cmdlets which ask a device for a Property Exchange resource. Each call draws
    // its own identifier, looks for devices on one group, asks, and withdraws the identifier again,
    // so nothing is left announced on the endpoint afterwards.
    public abstract class MidiCapabilityInquiryCmdletBase : MidiCmdletBase
    {
        protected const string EndpointDeviceIdParameterSet = "EndpointDeviceId";
        protected const string ConnectionParameterSet = "Connection";

        protected const string ChannelListResourceName = "ChannelList";
        protected const string ProgramListResourceName = "ProgramList";

        [Parameter(Mandatory = true, Position = 0, ValueFromPipelineByPropertyName = true, ParameterSetName = EndpointDeviceIdParameterSet)]
        [ValidateNotNullOrWhiteSpace]
        public string EndpointDeviceId { get; set; } = string.Empty;

        // Not positional: any object converts to a string, so a connection passed by position
        // would otherwise be taken for an endpoint device id.
        [Parameter(Mandatory = true, ValueFromPipeline = true, ParameterSetName = ConnectionParameterSet)]
        public MidiEndpointConnection? Connection { get; set; }

        [Parameter]
        [ValidateRange(0, 15)]
        public byte GroupIndex { get; set; }

        // Applies to each answer, and to each chunk of a long one. Discovery always waits this
        // long, because there is no way to know how many devices are going to reply.
        [Parameter]
        [ValidateRange(100, 60000)]
        public uint ResponseTimeoutMilliseconds { get; set; } = 2000;

        protected string QueriedEndpointDeviceId { get; private set; } = string.Empty;

        // Runs the query once for every responder on the endpoint which offers Property Exchange.
        // An endpoint may hold several, because a responder is a function block, not a device.
        protected void QueryPropertyExchangeResponders(
            Action<MidiCapabilityInquirySession, MidiCapabilityInquiryResponder> query)
        {
            RequireMidiServices();

            MidiTemporaryConnection? temporary = null;

            try
            {
                Windows.Devices.Midi2.MidiEndpointConnection connection;

                if (ParameterSetName == ConnectionParameterSet)
                {
                    connection = RequireOpenConnection(Connection);
                }
                else
                {
                    temporary = OpenTemporaryConnection(EndpointDeviceId);
                    connection = temporary.Connection;
                }

                QueriedEndpointDeviceId = connection.ConnectedEndpointDeviceId;

                // Closed before the temporary connection, so the Invalidate MUID it sends on the
                // way out still has a connection to go out on.
                using var session = MidiCapabilityInquirySession.Create(connection);

                if (session is null)
                {
                    ThrowTerminating(
                        new InvalidOperationException(Strings.CiSessionFailed),
                        "MidiCapabilityInquirySessionFailed",
                        ErrorCategory.ResourceUnavailable,
                        QueriedEndpointDeviceId);

                    return;
                }

                session.Group = new MidiGroup(GroupIndex);
                session.ResponseTimeoutMilliseconds = ResponseTimeoutMilliseconds;

                WriteVerbose(Format(Strings.CiLookingForDevicesFormat, GroupIndex + 1, QueriedEndpointDeviceId));

                var responders = session.DiscoverAsync().GetAwaiter().GetResult();

                if (responders is null || responders.Count == 0)
                {
                    WriteNonTerminating(
                        new ItemNotFoundException(Format(Strings.CiNoResponderFormat, GroupIndex + 1)),
                        "MidiCapabilityInquiryNoResponder",
                        ErrorCategory.ObjectNotFound,
                        QueriedEndpointDeviceId);

                    return;
                }

                var askedAny = false;

                foreach (var responder in responders)
                {
                    if (responder is null || !responder.SupportsPropertyExchange)
                    {
                        continue;
                    }

                    askedAny = true;

                    WriteVerbose(Format(Strings.CiAskingResponderFormat, responder.Muid, responder.FunctionBlockNumber));

                    query(session, responder);
                }

                if (!askedAny)
                {
                    WriteNonTerminating(
                        new NotSupportedException(Strings.CiNoPropertyExchange),
                        "MidiCapabilityInquiryNoPropertyExchange",
                        ErrorCategory.NotImplemented,
                        QueriedEndpointDeviceId);
                }
            }
            finally
            {
                temporary?.Dispose();
            }
        }
    }
}
