// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.Internal.Config;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.Service.Plugins
{
    internal class TransportPluginFactory
    {
        private ILogger _logger;
        private MidiDeviceGraph _deviceGraph;
        private MidiServicesConfig _config;

        public TransportPluginFactory(ILogger logger, MidiDeviceGraph graph)
        {

        }

        // Read list of trusted plugins from config file
        // Right now, this is all .NET-focused. Next rev
        // is going to likely need C++ support

        void LoadTrustedPlugins()
        {
            // Get plugins path from config
            // Get list of enabled transport plugins from config
            // Try to load each plugin.
            //  - Test each plugin to see if it implements the required transport interface
            //  - Log any failures


            //_config.


        }
    }
}
