// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.



using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.Service.Services
{
    public class MidiRouterService : BackgroundService
    {
        protected readonly ILogger _logger;
        private readonly IHostApplicationLifetime _lifetime;

        private readonly ServiceState _serviceState;
        private readonly MidiDeviceGraph _deviceGraph;
        private readonly SessionGraph _sessionGraph;

        public MidiRouterService(
            ILogger<MidiRouterService> logger,
            IHostApplicationLifetime lifetime,
            ServiceState serviceState,
            SessionGraph sessionGraph, 
            MidiDeviceGraph deviceGraph) =>
                (_logger, _lifetime, _serviceState, _sessionGraph, _deviceGraph) =
                (logger, lifetime, serviceState, sessionGraph, deviceGraph);



        // TODO: This may need multiple threads, or multiple services.
        // See Message Routing doc
        private void MainLoop(CancellationToken stoppingToken)
        {
            // check endpoint queues

            // need to do queue checking on separate threads because 
            // a single blocked queue shouldn't block any others.

            
            

            
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
            //if (_pipe != null)
            //{
            //    _logger.LogInformation("Stopping MIDI Router Service.");

            //    if (_pipe.IsConnected)
            //    {
            //        _pipe.Disconnect();
            //    }

            //    _pipe.Dispose();
            //    _pipe = null;
            //}
            //else
            //{
            //    _logger.LogInformation("Stopping MIDI Router Service. Pipe was not open.");
            //}

            return Task.CompletedTask;
        }

        // everything happens in MainLoop, called from StartAsync
        protected override async Task ExecuteAsync(CancellationToken stoppingToken)
        {
        }

    }
}
