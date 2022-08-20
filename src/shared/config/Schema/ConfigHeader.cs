// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using System.Text.Json.Serialization;

namespace Microsoft.Windows.Midi.Internal.Config.Schema
{
    public class ConfigHeader
    {
        [JsonInclude]
        private const string ThisFileType = "MIDI Configuration";

        public string FileType { get; set; } = ThisFileType;

        public Version SchemaVersion { get; set; }



    }
}
