// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.Internal.Service.Model;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.Service.Services
{
    public sealed class ManagementService : BackgroundService
    {
        private readonly ILogger<ManagementService> _logger;
        private readonly ServiceState _serviceState;
        private readonly MidiDeviceGraph _deviceGraph;
        private readonly SessionGraph _sessionGraph;

        public ManagementService(
            ILogger<ManagementService> logger, ServiceState serviceState,
            SessionGraph sessionGraph, MidiDeviceGraph deviceGraph) =>
            (_logger, _serviceState, _sessionGraph, _deviceGraph) =
            (logger, serviceState, sessionGraph, deviceGraph);

        protected override async Task ExecuteAsync(CancellationToken stoppingToken)
        {


        }



    }
}
