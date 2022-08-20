// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Messages.Packet
{
    /// <summary>
    /// Cast this to an int to get the number of words in the UMP
    /// </summary>
    public enum UmpType
    {
        Unknown = 0,
        Ump32 = 1,
        Ump64 = 2,
        Ump96 = 3,
        Ump128 = 4
    }
}
