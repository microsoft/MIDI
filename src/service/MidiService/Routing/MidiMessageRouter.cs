using MidiService.Protocol.Midi;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;


namespace MidiService.Routing
{
    internal class MidiMessageRouter
    {
        private const int MainMidiMessageQueueInitialSize = 5000;

        //private Queue<IRoutableMidiMessage> _incomingQueue =
        //    new Queue<IRoutableMidiMessage>(MainMidiMessageQueueInitialSize);






        // Really REALLY need to test performance here.
        // if pipes end up too slow, will need to go with
        // shared memory with a bunch of allocated buffers
        //https://www.techmikael.com/2010/02/blazing-fast-ipc-in-net-4-wcf-vs.html
        //https://henry-chong.com/cross-process-threading-synchronization-eventwaithandles-and-mutexes
        //https://www.red-gate.com/simple-talk/development/dotnet-development/sharing-caring-using-memory-mapped-files-net/
        //https://docs.microsoft.com/en-us/dotnet/api/system.io.memorymappedfiles.memorymappedfile?view=net-6.0

        // start with pipes first, because that's fairly easy.
        // To reduce complexity when looking at shared memory, could
        // have shared memory buffers per session.

        // Need to use shared memory for publishing the device graph anyway
        // Should probably just sort out the mechanism and use it for MIDI messages



    }
}
