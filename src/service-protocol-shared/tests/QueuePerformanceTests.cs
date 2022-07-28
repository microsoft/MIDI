using MidiService.Protocol.Midi;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace ProtocolTests
{
    [TestClass]
    public class QueuePerformanceTests
    {

        [TestMethod]
        public void PerfProfileUmp128Enqueue()
        {
            int loopCount = 10000;

            int step = 4;
            // in words
            int queueSize = loopCount * Marshal.SizeOf(typeof(Ump128)) / sizeof(uint);

            Guid id = Guid.NewGuid();

            using (IMidiMessageQueue _queue =
                new MidiMessageSharedMemoryQueue(queueSize, id, MidiMessageSharedMemoryQueue.ResizeMode.None))
            {
                System.Diagnostics.Debug.WriteLine($"About to enqueue {queueSize} words in a per-128-bit message mode.\n");

                int startValue = 4206942;

                long startTicks = DateTime.Now.Ticks;
                for (int i = startValue; i < startValue + loopCount; i += step)
                {
                    Ump128 ump;
                    ump.Word1 = (uint)i;
                    ump.Word2 = (uint)i + 1;
                    ump.Word3 = (uint)i + 2;
                    ump.Word4 = (uint)i + 3;

                    _queue.Enqueue(ump);
                }
                long endTicks = DateTime.Now.Ticks;

                long elapsedTicks = endTicks - startTicks;

                System.Diagnostics.Debug.WriteLine($"Start ticks:            {startTicks}");
                System.Diagnostics.Debug.WriteLine($"End ticks:              {endTicks}");
                System.Diagnostics.Debug.WriteLine($"Elapsed:                {elapsedTicks}");
                System.Diagnostics.Debug.WriteLine($" Elapsed Seconds:       {TimeSpan.FromTicks(elapsedTicks).TotalSeconds}");
                System.Diagnostics.Debug.WriteLine($"Ticks Per msg enqueue:  {elapsedTicks / loopCount}");
                System.Diagnostics.Debug.WriteLine($" In Milliseconds:       {TimeSpan.FromTicks(elapsedTicks / loopCount).TotalMilliseconds}");
                System.Diagnostics.Debug.WriteLine($" In Microseconds:       {TimeSpan.FromTicks(elapsedTicks / loopCount).TotalMicroseconds}");
                System.Diagnostics.Debug.WriteLine($" In Nanoseconds:        {TimeSpan.FromTicks(elapsedTicks / loopCount).TotalNanoseconds}");
                System.Diagnostics.Debug.WriteLine($"Ticks Per word:         {elapsedTicks / loopCount / step}");
                System.Diagnostics.Debug.WriteLine($" In Milliseconds:       {TimeSpan.FromTicks(elapsedTicks / loopCount / step).TotalMilliseconds}");
                System.Diagnostics.Debug.WriteLine($" In Microseconds:       {TimeSpan.FromTicks(elapsedTicks / loopCount / step).TotalMicroseconds}");
                System.Diagnostics.Debug.WriteLine($" In Nanoseconds:        {TimeSpan.FromTicks(elapsedTicks / loopCount / step).TotalNanoseconds}");


            }
        }



        [TestMethod]
        public void PerfProfileSingleWordEnqueue()
        {
            int queueSize = 100000;    // in words
            Guid id = Guid.NewGuid();

            using (IMidiMessageQueue _queue =
                new MidiMessageSharedMemoryQueue(queueSize, id, MidiMessageSharedMemoryQueue.ResizeMode.None))
            {
                System.Diagnostics.Debug.WriteLine($"About to enqueue {queueSize} words in the least efficient way allowable.\n");

                uint startValue = 4206942;
                uint count = 100000;

                // this is the most expensive way to add items to the
                // queue and so should take the most time.
                long startTicks = DateTime.Now.Ticks;
                for (uint i = startValue; i < startValue + count; i++)
                {
                    _queue.Enqueue(i);
                }
                long endTicks = DateTime.Now.Ticks;

                long elapsedTicks = endTicks - startTicks;

                System.Diagnostics.Debug.WriteLine($"Start ticks:        {startTicks}");
                System.Diagnostics.Debug.WriteLine($"End ticks:          {endTicks}");
                System.Diagnostics.Debug.WriteLine($"Elapsed:            {elapsedTicks}");
                System.Diagnostics.Debug.WriteLine($" Elapsed Seconds:   {TimeSpan.FromTicks(elapsedTicks).TotalSeconds}");
                System.Diagnostics.Debug.WriteLine($"Ticks Per enqueue:  {elapsedTicks / count}");
                System.Diagnostics.Debug.WriteLine($" In Milliseconds:   {TimeSpan.FromTicks(elapsedTicks / count).TotalMilliseconds}");
                System.Diagnostics.Debug.WriteLine($" In Microseconds:   {TimeSpan.FromTicks(elapsedTicks / count).TotalMicroseconds}");
                System.Diagnostics.Debug.WriteLine($" In Nanoseconds:    {TimeSpan.FromTicks(elapsedTicks / count).TotalNanoseconds}");


            }
        }


    }
}
