// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi
{
    internal static class ClientState
    {
        public static Guid ClientId { get; set; }

        static ClientState()
        {
            ClientId = Guid.NewGuid();
        }

        // This is not accurate. Need to manage whole-API version elsewhere, otherwise
        // it will be different depending upon the assembly.
        public static Version ApiVersion
        {
            get
            {
                var version = Assembly.GetExecutingAssembly().GetName().Version;

                if (version == null)
                    version = new Version(0, 0, 0);

                return version;
            }
        }


    }
}
