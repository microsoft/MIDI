using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MidiConfig.Schema
{
    public class Plugin
    {
        public Guid Id { get; set; }

        public string Path { get; set; }

        // TODO: Any other metadata we need? Version info? something else?

    }
}
