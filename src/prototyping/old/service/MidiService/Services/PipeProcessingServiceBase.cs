// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO.Pipes;
using System.Linq;
using System.Linq.Expressions;
using System.Security.AccessControl;
using System.Security.Principal;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.Service.Services
{
    public abstract class PipeProcessingServiceBase : BackgroundService
    {
        protected readonly ILogger _logger;
        private readonly IHostApplicationLifetime _lifetime;

        protected NamedPipeServerStream? _pipe;

        protected string PipeName { get; set; }

        public PipeProcessingServiceBase(
            ILogger logger,
            IHostApplicationLifetime lifetime) =>
            (_logger, _lifetime) = (logger, lifetime);


        protected NamedPipeServerStream CreatePipe(string pipeName)
        {
            NamedPipeServerStream pipe;

            try
            {
                SecurityIdentifier securityIdentifier = new SecurityIdentifier(
                    WellKnownSidType.AuthenticatedUserSid, null);

                PipeSecurity security = new PipeSecurity();
                PipeAccessRule accessRule = new PipeAccessRule(
                    securityIdentifier,
                    PipeAccessRights.ReadWrite, AccessControlType.Allow);

                security.AddAccessRule(accessRule);

                pipe = NamedPipeServerStreamAcl.Create(
                            pipeName,
                            PipeDirection.InOut,
                            1,
                            PipeTransmissionMode.Message,
                            PipeOptions.None,
                            2048, 2048,
                            security);

                return pipe;
            }
            catch (InvalidOperationException ex)
            {
                // already established
                _logger.LogError($"{PipeName} Pipe already established. " + ex.Message);
                throw;
            }
            catch (Exception ex)
            {
                _logger.LogError($"Could not create pipe {PipeName}. This is sometimes because of a quick restart when Windows hasn't yet removed the old pipe instance. Exception: " + ex.Message);
                throw;
            }
        }


        private void CreateMainClassPipe()
        {
            _pipe = CreatePipe(PipeName);

        }



        private void WaitForPipeConnection(CancellationToken stoppingToken)
        {
                try
                {
                    if (_pipe == null)
                    {
                        _logger.LogError("WaitForPipeConnection: Pipe is null.");
                        throw new Exception("Pipe is null in WaitForPipeConnection.");
                    }

                    while (!stoppingToken.IsCancellationRequested)
                    {
                        try
                        {
                            _logger.LogInformation($"Waiting for connection on pipe {PipeName}.");

                            // This will throw TaskCanceledException if service is shut down
                            // It will throw IOException if the client disconnected (this is normal)

                            //await _pipe.WaitForConnectionAsync(stoppingToken);
                            _pipe.WaitForConnection();

                            //_logger.LogDebug($"DEBUG: Connection established on pipe {PipeName}. Returning from WaitForPipeConnection");

                            return;

                        }
                        catch (IOException)
                        {
                            // client disconnected. This is normal. Restart the loop to wait
                            // for the next client to connect
                            //_logger.LogDebug("Client disconnected. Restarting loop.");

                            _pipe.Disconnect();
                            continue;       // restart the loop. Hey, it's almost as good as a GOTO
                        }
                    }

                    //_logger.LogDebug("DEBUG: Exited loop. Returning from WaitForPipeConnection.");

                }
                catch (TaskCanceledException)
                {
                    _logger.LogInformation("Task Canceled: WaitForPipeConnection.");

                    if (_pipe != null && _pipe.IsConnected)
                    {
                        _pipe.Disconnect();
                    }
                    throw;
                }
                catch (Exception ex)
                {
                    _logger.LogError("Exception in WaitForPipeConnection");

                    throw new Exception("Exception waiting for pipe connection. ", ex);
                }
        }




        public override Task StartAsync(CancellationToken cancellationToken)
        {
            // doing this here because ExecuteAsync blocks, despite its name
            // even when you return a task from it. Maybe I was doing something
            // dumb, but recommendation to have multiple background services in
            // a single Windows Service is to do this.

            Task.Run(() => MainLoop(cancellationToken));

            return Task.CompletedTask;
        }

        public override Task StopAsync(CancellationToken cancellationToken)
        {
           // _logger.LogInformation("DEBUG: StopAsync");

            if (_pipe != null)
            {
                _logger.LogInformation("Stopping service. Closing pipe.");

                if (_pipe.IsConnected)
                {
                    _pipe.Disconnect();
                }

                _pipe.Dispose();
                _pipe = null;
            }
            else
            {
                _logger.LogInformation("Stopping service. Pipe was not open.");
            }

            return Task.CompletedTask;
        }

        // This is where the derived class does most of the work
        protected abstract void OnConnectionEstablished(CancellationToken stoppingToken);

        // Helper method to wait for a specific message type and only that message type
        // on this pipe. Good only for pipes with a single request/response pairing.
        protected T? WaitForIncomingMessage<T>(MidiServiceProtocolSerializer serializer) where T : ProtocolMessage
        {
            _logger.LogDebug($"WaitForIncomingMessage '{typeof(T)}'");

            try
            {
                T message = serializer.Deserialize<T>();
                return message;
            }
            catch (JsonException)
            {
                // unexpected message type
                _logger.LogError($"Unexpected or invalid message received on '{PipeName}'. Expected '{typeof(T)}'");
                return null;
            }
            catch (Exception ex)
            {
                // other random error
                _logger.LogError($"Error deserializing '{PipeName}'. Expected '{typeof(T)}' Error: " + ex.ToString());
                return null;
            }
        }

        protected void MainLoop(CancellationToken stoppingToken)
        {
            CreateMainClassPipe();

            if (_pipe == null)
            {
                throw new Exception("Could not start service. Pipe is null.");
            }

            try
            {
                // main service loop
                while (!stoppingToken.IsCancellationRequested)
                {
                    WaitForPipeConnection(stoppingToken);

                    OnConnectionEstablished(stoppingToken);

                    // need to prevent a super tight loop for this to allow SCM to manage it
                    // a 50ms delay in THIS loop is not the end of the world. We 
                    // wouldn't want anything like this in the MIDI message processing
                    // loops, however.
                    //await Task.Delay(50);
                    Thread.Sleep(50);
                }
            }
            catch (TaskCanceledException)
            {
                _logger.LogWarning("Service Task canceled at {time}", DateTimeOffset.Now);

                // Need to shut everything down

                _pipe.Disconnect();
                _pipe.Dispose();
            }
            catch (Exception ex)
            {
                _logger.LogError("Fatal exception. Shutting down service. " + ex.ToString());

                _lifetime.StopApplication();
            }

        }

        protected override async Task ExecuteAsync(CancellationToken stoppingToken)
        {
        }
    }
}
