// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using System.Collections;
using System.Reflection;
using Microsoft.Windows.Midi.Internal.Service.Services;
using Microsoft.Windows.Midi.Internal.Config;

namespace Microsoft.Windows.Midi.Internal.Service.Model
{
    public class ServiceState
    {
        private readonly ILogger _logger;
        private readonly MidiServicesConfig _config;

        public ServiceState(ILogger<ConnectionService> logger, MidiServicesConfig config)
        {
            _logger = logger;
            _config = config;

            //_logger.LogDebug("DEBUG: ServiceState constructor");

            // not keen on this being in the constructor, but easier to deal with the DI this way
            _config.Load();
        }

        //private DeviceGraph _deviceGraph = new DeviceGraph();


        public Version? GetServiceVersion()
        {
            return Assembly.GetExecutingAssembly().GetName().Version;
        }

        public Statistics Statistics { get; } = new Statistics();

        //public MidiServicesConfig Config { get; set; }


    }
}
