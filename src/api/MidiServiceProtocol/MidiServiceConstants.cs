using System.Diagnostics;
using System.Text.Json;

namespace MidiService.Protocol
{
    public class MidiServiceConstants
    {
        private const string PipePrefix = "midi.";

        public const string InitialConnectionPipeName = PipePrefix + "Connect";
        public const string EnumerationServicePipePrefix = PipePrefix + "Enumerator";

        public const string SessionServicePipePrefix = PipePrefix + "Session";  // this gets an ID added to it




    }
}