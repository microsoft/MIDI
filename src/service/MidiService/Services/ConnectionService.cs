// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.


// disable x-plat warnings as we're dealing with ACL etc here.
#pragma warning disable CA1416


using ProtoBuf;
using System.IO.Pipes;
using System.Security.AccessControl;
using System.Security.Principal;
using System.Text.Json;
using System.Threading;


namespace Microsoft.Windows.Midi.Internal.Service.Services
{
    // This service is designed to be super fast, with very little traffic. It's a single
    // pipe, so clients will queue when creating sessions. 
    public sealed class ConnectionService : PipeProcessingServiceBase
    {

        private readonly ServiceState _serviceState;
        private readonly MidiDeviceGraph _deviceGraph;
        private readonly SessionGraph _sessionGraph;

        public ConnectionService(
            ILogger<ConnectionService> logger, 
            IHostApplicationLifetime lifetime, 
            ServiceState serviceState, 
            SessionGraph sessionGraph, 
            MidiDeviceGraph deviceGraph): base(logger, lifetime)

        {
            _logger.LogDebug("DEBUG: ConnectionService Constructor");

            _serviceState = serviceState;
            _sessionGraph = sessionGraph;
            _deviceGraph = deviceGraph;

            PipeName = MidiServiceConstants.InitialConnectionPipeName;
        }

        private MidiSessionState SpinUpNewMidiSession(CreateSessionRequestMessage request)
        {
            _logger.LogDebug("Creating new session.");

            // create a new session state object and add it to the list
            // of sessions maintained on the server
            MidiSessionState sessionState = new MidiSessionState()
            {
                HeaderClientId = request.Header.ClientId,
                CreatedTime = DateTime.Now,
                ClientVersion = Version.Parse(request.Header.ClientVersion),
                Name = request.Name,
                ProcessId = request.ProcessId,
                ProcessName = request.ProcessName,
                Id = _sessionGraph.GenerateNewSessionId(),
                //LogLevel = request.LogLevel
            };

            // generate the name for the pipe. The thread will create it
            sessionState.SessionChannelName = _sessionGraph.GenerateSessionChannelName(
                sessionState.Id,
                sessionState.ProcessId);

            // create thread and start session
            Thread t = new Thread(SessionThreadProc);
            t.Start(sessionState);

            // store the info
            _sessionGraph.AddSession(sessionState);

            return sessionState;
        }




        private void SessionThreadProc(object? data)
        {
            if (data != null)
            {
                var state = (MidiSessionState)data;

                NamedPipeServerStream pipe = CreatePipe(state.SessionChannelName);

                _logger.LogDebug("Created session pipe server stream.");

                bool sessionActive = true;

                while (sessionActive)
                {
                    try
                    {
                        pipe.WaitForConnection();
                    }
                    catch (IOException)
                    {
                        pipe.Disconnect();
                        continue;
                    }


                    // TODO: Process incoming messages

                    // Messages:
                    // - Update session properties
                    // - Destroy Session
                    // - Open Endpoint
                    // - Close Endpoint




                    // TEMP
                    Thread.Sleep(50);
                }
            }
            else
            {
                // no session state sent over. Log error
                _logger.LogError("MIDI Session state null in SessionThreadProc.");

            }

        }

        //private MidiStreamSerializer _serializer;




        protected override void OnConnectionEstablished(CancellationToken stoppingToken)
        {
            var serializer = new MidiServiceProtocolSerializer(_pipe);

            _logger.LogDebug("Connection established. Deserializing request message");

            // deserialize message. We're only expecting one kind of message
            // on this communications channel
            CreateSessionRequestMessage? request = 
                WaitForIncomingMessage<CreateSessionRequestMessage>(serializer);

            if (request != null)
            {
                _logger.LogInformation("Session create request received:\n"
                    + "\nProcessName: " + request.ProcessName
                    + "\nProcessId: " + request.ProcessId
                    + "\nSessionName: " + request.Name
                    + "\nClientVersion: " + request.Header.ClientVersion
                    + "\nClientRequestId: " + request.Header.ClientRequestId
                    + "\n");

                //_serviceState.Statistics.SessionServiceIncomingMessagesStatistics<CreateSessionRequestMessage.GetType()>

                _logger.LogDebug("Spinning up new session.");

                var sessionState = SpinUpNewMidiSession(request);

                _logger.LogDebug("Responding to client.");

                // Respond back to client with the session details
                // there's a potential race condition here with getting
                // the pipe spun up before client tries to connect to it
                CreateSessionResponseMessage response =
                    new CreateSessionResponseMessage()
                    {
                        Header = new ResponseMessageHeader()
                        {
                            ClientId = sessionState.HeaderClientId,
                            ClientRequestId = request.Header.ClientRequestId,
                            ResponseCode = ResponseCode.Success,
                            ServerVersion = _serviceState.GetServiceVersion().ToString(),
                        },

                        CreatedTime = sessionState.CreatedTime,
                        NewSessionId = sessionState.Id,
                        SessionChannelName = sessionState.SessionChannelName,
                    };

                _logger.LogDebug("About to serialize response");

                try
                {
                    // send the reply message
                    serializer.Serialize<CreateSessionResponseMessage>(response);
                }
                catch (Exception ex)
                {
                    _logger.LogError("Exception serializing response. " + ex.Message);
                }

                _logger.LogDebug("Response complete.");
            }
            else
            {
                // message is null, so not something we expected. Already logged
                // so just continue onwards.
            }

        }
    }







}