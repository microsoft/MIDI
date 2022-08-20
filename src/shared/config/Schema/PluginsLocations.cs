// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.Config.Schema
{
    public class PluginsLocations
    {
        public string PluginsRootFolder { get; set; }

        public string DevicePluginsSubFolder { get; set; }

        public string ProcessingPluginsSubFolder { get; set; }
    }
}
