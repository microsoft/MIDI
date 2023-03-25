// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

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

        [TestMethod(), Timeout(8000)]
        public void PerfProfileUmp128Enqueue()
        {
            int loopCount = 500000;

            int step = 4;
            // in words
            int queueSize = loopCount * Marshal.SizeOf(typeof(InternalUmp128)) / sizeof(uint);

            Guid id = Guid.NewGuid();

            using (IMidiUmpMessageQueue queue =
                new MidiUmpMessageSharedQueue(queueSize, id))
            {
                System.Diagnostics.Debug.WriteLine($"Memory-mapped File Queue: About to enqueue {queueSize.ToString("#,###")} words in a per-128-bit message mode.\n");

                int startValue = 4206942;

                long startTicks = DateTime.Now.Ticks;
                for (int i = startValue; i < startValue + loopCount; i += step)
                {
                    InternalUmp128 ump;
                    ump.Word1 = (uint)i;
                    ump.Word2 = (uint)i + 1;
                    ump.Word3 = (uint)i + 2;
                    ump.Word4 = (uint)i + 3;

                    queue.Enqueue(ref ump);
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


            using (IMidiUmpMessageQueue queue =
                new MidiUmpMessageLocalQueue())
            {
                System.Diagnostics.Debug.WriteLine($"Local Queue: About to enqueue {queueSize.ToString("#,###")} words in a per-128-bit message mode.\n");

                int startValue = 4206942;

                long startTicks = DateTime.Now.Ticks;
                for (int i = startValue; i < startValue + loopCount; i += step)
                {
                    InternalUmp128 ump;
                    ump.Word1 = (uint)i;
                    ump.Word2 = (uint)i + 1;
                    ump.Word3 = (uint)i + 2;
                    ump.Word4 = (uint)i + 3;

                    queue.Enqueue(ref ump);
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



        [TestMethod(), Timeout(8000)]
        public void PerfProfileSingleWordEnqueue()
        {
            int queueSize = 100000;    // in words
            Guid id = Guid.NewGuid();

            using (IMidiUmpMessageQueue queue =
                new MidiUmpMessageSharedQueue(queueSize, id))
            {
                System.Diagnostics.Debug.WriteLine($"Memory Mapped File Queue: About to enqueue {queueSize.ToString("#,###")} words in the least efficient way allowable.\n");

                uint startValue = 4206942;
                uint count = 100000;

                // this is the most expensive way to add items to the
                // queue and so should take the most time.
                long startTicks = DateTime.Now.Ticks;
                for (uint i = startValue; i < startValue + count; i++)
                {
                    queue.Enqueue(i);
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

            using (IMidiUmpMessageQueue queue =
                new MidiUmpMessageLocalQueue())
            {
                System.Diagnostics.Debug.WriteLine($"Local Queue: About to enqueue {queueSize.ToString("#,###")} words in the least efficient way allowable.\n");

                uint startValue = 4206942;
                uint count = 100000;

                // this is the most expensive way to add items to the
                // queue and so should take the most time.
                long startTicks = DateTime.Now.Ticks;
                for (uint i = startValue; i < startValue + count; i++)
                {
                    queue.Enqueue(i);
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




        [TestMethod(), Timeout(8000)]
        public void PerfProfileUmp128NativeMemorySpanEnqueue()
        {
            int queueMessageCount = 1000;
            int wordsPerMessage = 4;

            int spanMessageCount = 25;
            // in words
            int queueSize = queueMessageCount * Marshal.SizeOf(typeof(InternalUmp128)) / sizeof(uint);

            Guid id = Guid.NewGuid();

            // we're going to reuse the same memory block each time, much like native code would
            
            Span<InternalUmp128> messageSpan;
            Span<uint> wordSpan;

            IntPtr nativeMemory;

            unsafe
            {
                nativeMemory = Marshal.AllocHGlobal((int)(spanMessageCount * Marshal.SizeOf(typeof(InternalUmp128))));

                messageSpan = new Span<InternalUmp128>(nativeMemory.ToPointer(), spanMessageCount);
                wordSpan = new Span<uint>(nativeMemory.ToPointer(), queueSize);
            }

            using (IMidiUmpMessageQueue queue =
                new MidiUmpMessageSharedQueue(queueSize, id))
            {
                System.Diagnostics.Debug.WriteLine($"Memory-mapped File Queue: About to Span<T> enqueue {queueSize.ToString("#,###")} words in a per-128-bit Span<T> mode.\n");

                int wordCount = 0;

                int startValue = 4206942;
                long startTicks = DateTime.Now.Ticks;
                int step = wordsPerMessage * spanMessageCount;
                bool success = true;

                for (int i = 0; success && (wordCount < queueSize); i++)
                {
                    for (int j = 0; j < spanMessageCount; j++)
                    {
                        InternalUmp128 ump;

                        ump.Word1 = (uint)(startValue + wordCount++);
                        ump.Word2 = (uint)(startValue + wordCount++);
                        ump.Word3 = (uint)(startValue + wordCount++);
                        ump.Word4 = (uint)(startValue + wordCount++);

                        // add the midi message to memory
                        messageSpan[j] = ump;
                    }

                    // now enqueue the words. Same array, different view
                    success = queue.Enqueue(ref wordSpan);

                    if (!success)
                    {
                        System.Diagnostics.Debug.WriteLine($"Failed to enqueue message span {i} wordcount = {wordCount}");
                    }


                }

                long endTicks = DateTime.Now.Ticks;
                long elapsedTicks = endTicks - startTicks;

                System.Diagnostics.Debug.WriteLine($"\nStart ticks:            {startTicks}");
                System.Diagnostics.Debug.WriteLine($"End ticks:              {endTicks}");
                System.Diagnostics.Debug.WriteLine($"Elapsed:                {elapsedTicks}");
                System.Diagnostics.Debug.WriteLine($" Elapsed Seconds:       {TimeSpan.FromTicks(elapsedTicks).TotalSeconds}");
                System.Diagnostics.Debug.WriteLine($"Ticks Per msg enqueue:  {elapsedTicks / queueMessageCount}");
                System.Diagnostics.Debug.WriteLine($" In Milliseconds:       {TimeSpan.FromTicks(elapsedTicks / queueMessageCount).TotalMilliseconds}");
                System.Diagnostics.Debug.WriteLine($" In Microseconds:       {TimeSpan.FromTicks(elapsedTicks / queueMessageCount).TotalMicroseconds}");
                System.Diagnostics.Debug.WriteLine($" In Nanoseconds:        {TimeSpan.FromTicks(elapsedTicks / queueMessageCount).TotalNanoseconds}");
                System.Diagnostics.Debug.WriteLine($"Ticks Per word:         {elapsedTicks / queueMessageCount / wordsPerMessage}");
                System.Diagnostics.Debug.WriteLine($" In Milliseconds:       {TimeSpan.FromTicks(elapsedTicks / queueMessageCount / wordsPerMessage).TotalMilliseconds}");
                System.Diagnostics.Debug.WriteLine($" In Microseconds:       {TimeSpan.FromTicks(elapsedTicks / queueMessageCount / wordsPerMessage).TotalMicroseconds}");
                System.Diagnostics.Debug.WriteLine($" In Nanoseconds:        {TimeSpan.FromTicks(elapsedTicks / queueMessageCount / wordsPerMessage).TotalNanoseconds}");
            }

            Marshal.FreeHGlobal(nativeMemory);

        }


























    }
}
