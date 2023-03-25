// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.


// disable x-plat warnings as we're dealing with ACL etc here.
#pragma warning disable CA1416



using Microsoft.Windows.Midi.Internal.Service.Model;

namespace Microsoft.Windows.Midi.Internal.Service.Services
{
    // This service is designed to be super fast, with very little traffic. It's a single
    // pipe, so clients will queue when creating sessions. 
    public sealed class PingService : PipeProcessingServiceBase
    {
        private readonly ServiceState _serviceState;

        public PingService(
            ILogger<PingService> logger, 
            IHostApplicationLifetime lifetime, 
            ServiceState serviceState): base(logger, lifetime)

        {
            _logger.LogDebug("DEBUG: PingService Constructor");

            _serviceState = serviceState;

            PipeName = MidiServiceConstants.PingPipeName;
        }

        protected override void OnConnectionEstablished(CancellationToken stoppingToken)
        {
            var serializer = new MidiServiceProtocolSerializer(_pipe);

            _logger.LogDebug("Connection established. Deserializing request message");

            // deserialize message. We're only expecting one kind of message
            // on this communications channel
            ServicePingMessage? request =
                WaitForIncomingMessage<ServicePingMessage>(serializer);

            if (request != null)
            {
                _logger.LogDebug("Responding to ping.");

                ServicePingResponseMessage response =
                    new ServicePingResponseMessage()
                    {
                        Header = new ResponseMessageHeader()
                        {
                            ClientId = request.Header.ClientId,
                            ClientRequestId = request.Header.ClientRequestId,
                            ResponseCode = ResponseCode.Success,
                            ServerVersion = _serviceState.GetServiceVersion().ToString(),
                        },
                    };

                _logger.LogDebug("About to serialize Ping response");

                try
                {
                    // send the reply message
                    serializer.Serialize<ServicePingResponseMessage>(response);
                }
                catch (Exception ex)
                {
                    _logger.LogError("Exception serializing response. " + ex.Message);
                }

                _logger.LogDebug("Response complete.");
            }
            else
            {
                // message is null so likely something unexpected. Already logged.
            }
        }
    }







}