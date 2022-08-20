// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.Config.Schema
{
    public class TrustedDevicePlugin
    {
        public Guid Id { get; set; }

        
        // The subfolder in the shared folder where this plugin lives
        public string SubFolder { get; set; }

        // just the filename in the trusted plugins subfolder. Can't load from anywhere else
        public string FileName { get; set; }

        public string PluginClassName { get; set; }

    }
}
