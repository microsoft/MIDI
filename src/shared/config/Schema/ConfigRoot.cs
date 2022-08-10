// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.Config.Schema
{
    public class ConfigRoot
    {

        public ConfigHeader Header { get; set; } = new ConfigHeader();

        public DataLocations DataLocations { get; set; } = new DataLocations();

        public PluginsLocations PluginsLocations { get; set; } = new PluginsLocations();

        public string CurrentSetupFileNameWithoutPath { get; set; }

        public Settings Settings { get; set; } = new Settings();

        public List<TrustedDevicePlugin> TrustedDevicePlugins { get; set; } = new List<TrustedDevicePlugin>();
        public List<TrustedProcessingPlugin> TrustedProcessingPlugins { get; set; } = new List<TrustedProcessingPlugin>();

    }
}
