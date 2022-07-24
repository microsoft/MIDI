using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using System.Text.Json.Serialization;

namespace MidiConfig.Schema
{
    public class Header
    {
        [JsonInclude]
        private const string ThisFileType = "MIDI Configuration";

        public string FileType { get; set; } = ThisFileType;

        public Version SchemaVersion { get; set; }



    }
}
