// disable x-plat warnings as we're dealing with ACL etc here.
#pragma warning disable CA1416



using System.IO.Pipes;
using System.Security.AccessControl;
using System.Security.Principal;


namespace MidiService
{
    // This service is designed to be super fast, with very little traffic. It's a single
    // pipe, so clients will queue when creating sessions. 
    public sealed class ConnectionService : BackgroundService
    {
        private readonly ILogger<ConnectionService> _logger;
        private readonly ServiceState _serviceState;
        private readonly DeviceGraph _deviceGraph;
        private readonly SessionGraph _sessionGraph;

        public ConnectionService(
            ILogger<ConnectionService> logger, ServiceState serviceState, 
            SessionGraph sessionGraph, DeviceGraph deviceGraph) =>
            (_logger, _serviceState, _sessionGraph, _deviceGraph) = 
            (logger, serviceState, sessionGraph, deviceGraph);

        private NamedPipeServerStream? CreatePipe()
        {
            _logger.LogDebug("Opening ConnectionService pipe");

            try
            {
                NamedPipeServerStream pipe;

                SecurityIdentifier securityIdentifier = new SecurityIdentifier(
                    WellKnownSidType.AuthenticatedUserSid, null);

                PipeSecurity security = new PipeSecurity();
                PipeAccessRule accessRule = new PipeAccessRule(
                    securityIdentifier, 
                    PipeAccessRights.ReadWrite, AccessControlType.Allow);

                security.AddAccessRule(accessRule);

                pipe = NamedPipeServerStreamAcl.Create(
                            MidiServiceConstants.InitialConnectionPipeName,
                            PipeDirection.InOut, 
                            1, 
                            PipeTransmissionMode.Message, 
                            PipeOptions.None, 
                            2048, 2048,
                            security);

                _logger.LogDebug("Created ConnectionService pipe server stream.");

                return pipe;
            }
            catch (InvalidOperationException ex)
            {
                // already established
                _logger.LogInformation("ConnectionService Pipe already established. " + ex.Message);


            }
            catch (Exception ex)
            {
                _logger.LogInformation("Could not create ConnectionService pipe. " + ex.Message);
            }

            return null;

        }

        private MidiSessionState SpinUpNewSession(CreateSessionRequestMessage request)
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

                NamedPipeServerStream pipe;

                SecurityIdentifier securityIdentifier = new SecurityIdentifier(
                    WellKnownSidType.AuthenticatedUserSid, null);

                PipeSecurity security = new PipeSecurity();
                PipeAccessRule accessRule = new PipeAccessRule(
                    securityIdentifier,
                    PipeAccessRights.ReadWrite, AccessControlType.Allow);

                security.AddAccessRule(accessRule);

                pipe = NamedPipeServerStreamAcl.Create(
                            state.SessionChannelName,
                            PipeDirection.InOut,
                            1,
                            PipeTransmissionMode.Byte,
                            PipeOptions.None,
                            2048, 2048,
                            security);

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



                    // TEMP
                    Thread.Sleep(10);
                }
            }
            else
            {
                // no session state sent over. Log error
                _logger.LogError("MIDI Session state null in SessionThreadProc.");

            }

        }

        protected override async Task ExecuteAsync(CancellationToken stoppingToken)
        {
            _logger.LogInformation("ConnectionService started running at: {time}", DateTimeOffset.Now);

            try
            {
                using (NamedPipeServerStream? connectionPipe = CreatePipe())
                {
                    if (connectionPipe != null)
                    {
                        try
                        {
                            // get the serializer ready
                            MidiStreamSerializer serializer = new MidiStreamSerializer(connectionPipe);

                            // main service loop
                            while (!stoppingToken.IsCancellationRequested)
                            {
                                // monitor pipe

                                try
                                {
                                    _logger.LogDebug("Waiting for connection");
                                    connectionPipe.WaitForConnection();
                                }
                                catch (IOException)
                                {
                                    // client disconnected. We need to handle this here
                                    // so it doesn't crash the server.
                                    _logger.LogDebug("Client disconnected. Restarting loop.");
                                    connectionPipe.Disconnect();
                                    continue;       // restart the loop. Hey, it's almost as good as a GOTO
                                }

                                _logger.LogDebug("Connection established. Deserializing request message");

                                // deserialize message. We're only expecting one kind of message
                                // on this communications channel
                                CreateSessionRequestMessage request =
                                    serializer.Deserialize<CreateSessionRequestMessage>();

                                _logger.LogInformation("Session create request received:\n"
                                    + "\nProcessName: " + request.ProcessName
                                    + "\nProcessId: " + request.ProcessId
                                    + "\nSessionName: " + request.Name
                                    + "\nClientVersion: " + request.Header.ClientVersion
                                    + "\nClientRequestId: " + request.Header.ClientRequestId
                                    + "\n");

                                //_serviceState.Statistics.SessionServiceIncomingMessagesStatistics<CreateSessionRequestMessage.GetType()>

                                _logger.LogDebug("Spinning up new session.");

                                var sessionState = SpinUpNewSession(request);

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

                                // need to prevent a super tight loop for this to allow SCM to manage it
                                // a 50ms delay in THIS loop is not the end of the world. We 
                                // wouldn't want anything like this in the MIDI message processing
                                // loops, however.
                                Thread.Sleep(50);   
                            }
                        }
                        catch (TaskCanceledException ex)
                        {
                            _logger.LogInformation("ConnectionService Task canceled at {time}", DateTimeOffset.Now);
                        }


                    }
                    else
                    {
                        _logger.LogError("ConnectionService pipe is null");
                    }

                    _logger.LogInformation("Closing ConnectionService pipe.");

                }
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "{Message}", ex.Message);
                Environment.Exit(1);
            }

            _logger.LogInformation("ConnectionService stopped running at: {time}", DateTimeOffset.Now);

        }
    }
}