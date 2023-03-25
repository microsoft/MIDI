// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.Internal.ServiceProtocol.Midi;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace ProtocolTests
{
    [TestClass]
    public class MemoryMappedQueueTests
    {

        [TestMethod]
        public void CreateMemoryMappedQueue()
        {
            int queueSize = 1000;
            Guid id = Guid.NewGuid();

            using (IMidiUmpMessageQueue _queue =
                new MidiUmpMessageSharedQueue(queueSize, id))
            {
                // nothing to do here
            }
        }

        [TestMethod(), Timeout(3000)]
        public void WordEnqueueAndDequeueSingleElement()
        {
            int queueSize = 10;
            Guid id = Guid.NewGuid();

            using (IMidiUmpMessageQueue _queue =
                new MidiUmpMessageSharedQueue(queueSize, id))
            {
                uint writtenValue = 4206942;

                System.Diagnostics.Debug.WriteLine("about to enqueue just 1");

                Assert.IsTrue(_queue.Enqueue(writtenValue));

                System.Diagnostics.Debug.WriteLine("about to dequeue just 1");

                uint word;

                _queue.Dequeue(out word);

                Assert.AreEqual(writtenValue, word);
            }
        }

        [TestMethod(), Timeout(3000)]
        public void WordDequeueFromEmptyQueue()
        {
            int queueSize = 10;
            Guid id = Guid.NewGuid();

            using (IMidiUmpMessageQueue _queue =
                new MidiUmpMessageSharedQueue(queueSize, id))
            {
                System.Diagnostics.Debug.WriteLine("about to dequeue from an empty queue");

                uint word;

                Assert.IsFalse(_queue.Dequeue(out word));
            }
        }

        [TestMethod(), Timeout(3000)]
        public void Ump128DequeueFromEmptyQueue()
        {
            int queueSize = 10;
            Guid id = Guid.NewGuid();

            using (IMidiUmpMessageQueue _queue =
                new MidiUmpMessageSharedQueue(queueSize, id))
            {
                System.Diagnostics.Debug.WriteLine("about to dequeue from an empty queue");

                InternalUmp128 ump;

                Assert.IsFalse(_queue.Dequeue(out ump));
            }
        }


        [TestMethod(), Timeout(3000)]
        public void WordEnqueueAndDequeue()
        {
            int queueSize = 12;
            Guid id = Guid.NewGuid();

            using (IMidiUmpMessageQueue _queue =
                new MidiUmpMessageSharedQueue(queueSize, id))
            {
                int startValue = 4206942;
                int count = queueSize;

                System.Diagnostics.Debug.WriteLine($"About to enqueue {count} words in queue with capacity {queueSize}");

                // this is the most expensive way to add items to the
                // queue and so should take the most time.
                for (int i = 0; i < count; i++)
                {
                    System.Diagnostics.Debug.Write(i + " ");

                    Assert.IsTrue(_queue.Enqueue((uint)(i + startValue)), "Failed to enqueue");
                }

                System.Diagnostics.Debug.WriteLine($"\n\nAbout to dequeue {count} words");

                // Dequeue and verify same values are there
                for (int i = 0; i < count; i++)
                {
                    uint word;

                    System.Diagnostics.Debug.Write(i + " ");

                    Assert.IsTrue(_queue.Dequeue(out word), "Failed to dequeue");

                    Assert.AreEqual((uint)(i + startValue), word);
                }
            }
        }


        [TestMethod(), Timeout(3000)]
        public void WordEnqueueOverCapacity()
        {
            int loopCount = 11;
            int queueStructCount = 10;

            int wordSize = Marshal.SizeOf(typeof(uint));
            int wordSizeInWords = wordSize / sizeof(uint);     // just keeping this the same as other methods
            int queueSizeInWords = queueStructCount * wordSizeInWords;

            System.Diagnostics.Debug.WriteLine($"Calculated queueStructCount   {queueStructCount}");
            System.Diagnostics.Debug.WriteLine($"Calculated wordSize           {wordSize}");
            System.Diagnostics.Debug.WriteLine($"Calculated wordSizeInWords    {wordSizeInWords}");
            System.Diagnostics.Debug.WriteLine($"Calculated queueSizeInWords   {queueSizeInWords}");

            Guid id = Guid.NewGuid();

            using (IMidiUmpMessageQueue _queue =
                new MidiUmpMessageSharedQueue(queueSizeInWords, id))
            {
                System.Diagnostics.Debug.WriteLine("about to enqueue");

                for (uint i = 0; i < loopCount; i++)
                {
                    uint word;
                    word = i;

                    System.Diagnostics.Debug.WriteLine($"\nAdding word {i}");

                    bool result = _queue.Enqueue(word);

                    if (i < queueStructCount)
                        Assert.IsTrue(result, $"Item {i} should have been within the queue capacity");
                    else
                        Assert.IsFalse(result, $"Item {i} should have been over the queue capacity.");
                }
            }
        }


        [TestMethod(), Timeout(3000)]
        public void Ump128EnqueueOverCapacity()
        {
            int loopCount = 11;
            int queueStructCount = 10;

            int structSize = Marshal.SizeOf(typeof(InternalUmp128));
            int structSizeInWords = structSize / sizeof(uint);
            int queueSizeInWords = queueStructCount * structSizeInWords;

            System.Diagnostics.Debug.WriteLine($"Calculated queueStructCount   {queueStructCount}");
            System.Diagnostics.Debug.WriteLine($"Calculated structSize         {structSize}");
            System.Diagnostics.Debug.WriteLine($"Calculated structSizeInWords  {structSizeInWords}");
            System.Diagnostics.Debug.WriteLine($"Calculated queueSizeInWords   {queueSizeInWords}");

            Guid id = Guid.NewGuid();

            using (IMidiUmpMessageQueue _queue =
                new MidiUmpMessageSharedQueue(queueSizeInWords, id))
            {
                System.Diagnostics.Debug.WriteLine("about to enqueue");

                for (uint i = 0; i < loopCount; i++)
                {
                    InternalUmp128 ump;
                    ump.Word1 = i;
                    ump.Word2 = i + 1;
                    ump.Word3 = i + 2;
                    ump.Word4 = i + 3;

                    System.Diagnostics.Debug.WriteLine($"Adding structure {i}");

                    bool result = _queue.Enqueue(ref ump);

                    if (i < queueStructCount)
                        Assert.IsTrue(result);
                    else
                        Assert.IsFalse(result);
                }
            }
        }


        [TestMethod(), Timeout(3000)]
        public void Ump128EnqueueAndDequeueMultiple()
        {
            int loopCount = 1000;

            uint step = 4;
            // in words
            int structSize = Marshal.SizeOf(typeof(InternalUmp128));
            int structSizeInWords = structSize / sizeof(uint);
            int queueSize = loopCount * structSizeInWords;

            System.Diagnostics.Debug.WriteLine($"Calculated structSize         {structSize}");
            System.Diagnostics.Debug.WriteLine($"Calculated structSizeInWords  {structSizeInWords}");
            System.Diagnostics.Debug.WriteLine($"Calculated queueSize          {queueSize}");

            Guid id = Guid.NewGuid();

            using (IMidiUmpMessageQueue _queue =
                new MidiUmpMessageSharedQueue(queueSize, id))
            {
                uint startValue = 4206942;

                System.Diagnostics.Debug.WriteLine($"about to enqueue {loopCount} structures");

                for (uint i = startValue; i < startValue + loopCount; i += step)
                {
                    InternalUmp128 ump;
                    ump.Word1 = i;
                    ump.Word2 = i + 1;
                    ump.Word3 = i + 2;
                    ump.Word4 = i + 3;

                    Assert.IsTrue(_queue.Enqueue(ref ump), "Failed to enqueue");
                }


                System.Diagnostics.Debug.WriteLine($"about to dequeue {loopCount} structures");

                // Dequeue and verify same values are there
                for (uint i = startValue; i < startValue + loopCount; i += step)
                {
                    InternalUmp128 ump;

                    Assert.IsTrue(_queue.Dequeue(out ump));

                    Assert.AreEqual(i, ump.Word1);
                    Assert.AreEqual(i+1, ump.Word2);
                    Assert.AreEqual(i+2, ump.Word3);
                    Assert.AreEqual(i+3, ump.Word4);
                }


                // TODO: Verify new capacity is equal to the original capacity


            }
        }

        // this is not exhaustive. Just for testing
        // See 2.1.4 of UMP format spec
        private const uint UmpMessageType32 = (uint)0x0 << 28;
        private const uint UmpMessageType64 = (uint)0x3 << 28;
        private const uint UmpMessageType96 = (uint)0xC << 28;
        private const uint UmpMessageType128 = (uint)0x5 << 28;

        private InternalUmp32 BuildSemiValidUmp32(uint seedData)
        {
            InternalUmp32 ump = default;

            ump.Word1 = UmpMessageType32;

            return ump;
        }

        private InternalUmp64 BuildSemiValidUmp64(uint seedData)
        {
            InternalUmp64 ump = default;

            ump.Word1 = UmpMessageType64;
            ump.Word2 = seedData;

            return ump;
        }
        private InternalUmp96 BuildSemiValidUmp96(uint seedData)
        {
            InternalUmp96 ump = default;

            ump.Word1 = UmpMessageType96;
            ump.Word2 = seedData;
            ump.Word3 = seedData+1;

            return ump;
        }
        private InternalUmp128 BuildSemiValidUmp128(uint seedData)
        {
            InternalUmp128 ump = default;

            ump.Word1 = UmpMessageType128;
            ump.Word2 = seedData;
            ump.Word3 = seedData + 1;
            ump.Word4 = seedData + 2;

            return ump;
        }



        [TestMethod(), Timeout(3000)]
        public void UmpMixedEnqueueRandomAndDequeue()
        {
            int queueSize = 15;

            Guid id = Guid.NewGuid();

            using (IMidiUmpMessageQueue queue =
                new MidiUmpMessageSharedQueue(queueSize, id))
            {
                System.Diagnostics.Debug.WriteLine("\nNot a performance test.");
                System.Diagnostics.Debug.WriteLine("This tests writing messages with a valid UMP message type, ");
                System.Diagnostics.Debug.WriteLine("and then dequeuing them based only on that message type.\n");

                System.Diagnostics.Debug.WriteLine($"About to enqueue random structures. Capacity is {queueSize} words.\n");

                var rnd = new Random();

                bool okToContinue = true;
                uint wordsWritten = 0;

                uint messagesWritten = 0;
                uint i = 0;

                while (okToContinue)
                {
                    bool result = false;

                    System.Diagnostics.Debug.Write(String.Format("{0:0#} : ", i+1));

                    switch (rnd.Next(1, 4))
                    {
                        //case 0:
                        //    System.Diagnostics.Debug.WriteLine("word");
                        //    uint word = i;
                        //    result = queue.Enqueue(word);
                        //    if (!result)
                        //    {
                        //        Assert.IsTrue(queue.GetRemainingCapacityInWords() < 1);
                        //    }
                        //    break;
                        case 1:
                            var ump32 = BuildSemiValidUmp32(i);
                            result = queue.Enqueue(ref ump32);
                            if (!result)
                            {
                                //Assert.IsTrue(queue.GetRemainingCapacityInWords() < 1);
                                System.Diagnostics.Debug.WriteLine("Unable to add Ump32 (1 word)");
                            }
                            else
                            {
                                System.Diagnostics.Debug.WriteLine("Ump32 (1 word)");
                                wordsWritten += 1;
                                messagesWritten++;
                            }
                            break;
                        case 2:
                            var ump64 = BuildSemiValidUmp64(i);
                            result = queue.Enqueue(ref ump64);
                            if (!result)
                            {
                                //Assert.IsTrue(queue.GetRemainingCapacityInWords() < 2);
                                System.Diagnostics.Debug.WriteLine("Unable to add Ump64 (2 words)");
                            }
                            else
                            {
                                System.Diagnostics.Debug.WriteLine("Ump64 (2 words)");
                                wordsWritten += 2;
                                messagesWritten++;
                            }
                            break;
                        case 3:
                            var ump96 = BuildSemiValidUmp96(i);
                            result = queue.Enqueue(ref ump96);
                            if (!result)
                            {
                                //Assert.IsTrue(queue.GetRemainingCapacityInWords() < 3);
                                System.Diagnostics.Debug.WriteLine("Unable to add Ump96 (3 words)");
                            }
                            else
                            {
                                System.Diagnostics.Debug.WriteLine("Ump96 (3 words)");
                                wordsWritten += 3;
                                messagesWritten++;
                            }
                            break;
                        case 4:
                            var ump128 = BuildSemiValidUmp128(i);
                            result = queue.Enqueue(ref ump128);
                            if (!result)
                            {
                                //Assert.IsTrue(queue.GetRemainingCapacityInWords() < 4);
                                System.Diagnostics.Debug.WriteLine("Unable to add Ump128 (4 words)");
                            }
                            else
                            {
                                System.Diagnostics.Debug.WriteLine("Ump128 (4 words)");
                                wordsWritten += 4;
                                messagesWritten++;
                            }
                            break;
                    }

                    i++;
                    okToContinue = result;
                }

                System.Diagnostics.Debug.WriteLine($"Added {messagesWritten} structures to the queue comprised of {wordsWritten} words in total.\n");

                System.Diagnostics.Debug.WriteLine($"Beginning read\n");

                uint wordsRead = 0;

                int messagesRead = 0;
                int j = 0;
                bool dequeueResult = true;

                while (dequeueResult && !queue.IsEmpty())
                {
                    System.Diagnostics.Debug.Write(String.Format("{0:0#} : ", j + 1));

                    switch (queue.PeekNextMessageWordCount())
                    {
                        case 1:
                            InternalUmp32 ump32 = default;
                            dequeueResult = queue.Dequeue(out ump32);
                            if (!dequeueResult)
                            {
                                System.Diagnostics.Debug.WriteLine("Unable to read Ump32 (1 word)");
                                Assert.Fail();
                            }
                            else
                            {
                                System.Diagnostics.Debug.WriteLine("Ump32 (1 word)");
                                wordsRead += 1;
                                messagesRead++;
                            }
                            break;
                        case 2:
                            InternalUmp64 ump64 = default;
                            dequeueResult = queue.Dequeue(out ump64);
                            if (!dequeueResult)
                            {
                                System.Diagnostics.Debug.WriteLine("Unable to read Ump64 (2 words)");
                                Assert.Fail();
                            }
                            else
                            {
                                System.Diagnostics.Debug.WriteLine("Ump64 (2 words)");
                                wordsRead += 2;
                                messagesRead++;
                            }
                            break;
                        case 3:
                            InternalUmp96 ump96 = default;
                            dequeueResult = queue.Dequeue(out ump96);
                            if (!dequeueResult)
                            {
                                System.Diagnostics.Debug.WriteLine("Unable to read Ump96 (3 words)");
                                Assert.Fail();
                            }
                            else
                            {
                                System.Diagnostics.Debug.WriteLine("Ump96 (3 words)");
                                wordsRead += 3;
                                messagesRead++;
                            }
                            break;
                        case 4:
                            InternalUmp128 ump128 = default;
                            dequeueResult = queue.Dequeue(out ump128);
                            if (!dequeueResult)
                            {
                                System.Diagnostics.Debug.WriteLine("Unable to read Ump128 (4 words)");
                                Assert.Fail();
                            }
                            else
                            {
                                System.Diagnostics.Debug.WriteLine("Ump128 (4 words)");
                                wordsRead += 4;
                                messagesRead++;
                            }
                            break;

                    }

                    // TODO: Validate contents of packet

                    j++;
                }

                System.Diagnostics.Debug.WriteLine($"Read {messagesRead} structures from the queue comprised of {wordsRead} words in total.\n");



            }
        }

        Random _random = new Random();

        private bool EnqueueRandomUmp(IMidiUmpMessageQueue q, uint data)
        {
            switch (_random.Next(0, 3))
            {
                case 1:
                    var ump64 = BuildSemiValidUmp64(data);
                    System.Diagnostics.Debug.WriteLine($"Enqueuing Ump64 with value {data}");
                    return q.Enqueue(ref ump64);
                case 2:
                    var ump96 = BuildSemiValidUmp96(data);
                    System.Diagnostics.Debug.WriteLine($"Enqueuing Ump96 with value {data}");
                    return q.Enqueue(ref ump96);
                case 3:
                    var ump128 = BuildSemiValidUmp128(data);
                    System.Diagnostics.Debug.WriteLine($"Enqueuing Ump128 with value {data}");
                    return q.Enqueue(ref ump128);
                default:
                    var ump32 = BuildSemiValidUmp32(data);
                    System.Diagnostics.Debug.WriteLine($"Enqueuing Ump32 with value {data}");
                    return q.Enqueue(ref ump32);
            }
        }

        private bool DequeueNextUmp(IMidiUmpMessageQueue q)
        {
            bool dequeueResult = false;

            switch (q.PeekNextMessageWordCount())
            {
                case 1:
                    InternalUmp32 ump32 = default;
                    dequeueResult = q.Dequeue(out ump32);
                    if (!dequeueResult)
                    {
                        System.Diagnostics.Debug.WriteLine("Unable to read Ump32 (1 word)");
                        return false;
                    }
                    else
                    {
                        System.Diagnostics.Debug.WriteLine("Dequeued Ump32");
                        return true;
                    }
                case 2:
                    InternalUmp64 ump64 = default;
                    dequeueResult = q.Dequeue(out ump64);
                    if (!dequeueResult)
                    {
                        System.Diagnostics.Debug.WriteLine("Unable to read Ump64 (2 words)");
                        return false;
                    }
                    else
                    {
                        System.Diagnostics.Debug.WriteLine("Ump64 (2 words)");
                        return true;
                    }
                case 3:
                    InternalUmp96 ump96 = default;
                    dequeueResult = q.Dequeue(out ump96);
                    if (!dequeueResult)
                    {
                        System.Diagnostics.Debug.WriteLine("Unable to read Ump96 (3 words)");
                        return false;
                    }
                    else
                    {
                        System.Diagnostics.Debug.WriteLine("Ump96 (3 words)");
                        return true;
                    }
                case 4:
                    InternalUmp128 ump128 = default;
                    dequeueResult = q.Dequeue(out ump128);
                    if (!dequeueResult)
                    {
                        System.Diagnostics.Debug.WriteLine("Unable to read Ump128 (4 words)");
                        return false;
                    }
                    else
                    {
                        System.Diagnostics.Debug.WriteLine("Ump128 (4 words)");
                        return true;
                    }
                default:
                    System.Diagnostics.Debug.WriteLine("Peek returned an unexpected result.");
                    return false;
            }
        }



        // What this test does:
        // - adds and removes items to cause the queue pointers to move and wrap. 
        //   It's really just testing if it's truly circular.
        // what this test doesn't do
        // - validate that the data is accurate

        [TestMethod(), Timeout(3000)]
        public void UmpLoopedMixedEnqueueRandomAndDequeue()
        {
            int queueSize = 20;

            Guid id = Guid.NewGuid();

            using (IMidiUmpMessageQueue queue =
                new MidiUmpMessageSharedQueue(queueSize, id))
            {
                System.Diagnostics.Debug.WriteLine("\nNot a performance test.");
                System.Diagnostics.Debug.WriteLine("This tests interleaved reading/writing messages with a valid UMP message type, ");

                System.Diagnostics.Debug.WriteLine($"About to enqueue random structures. Capacity is {queueSize} words.\n");

                int iterations = 1000;

                for (int i = 1; i <= iterations; i++)
                {
                    // queue 4 random messages
                    for (int j = 0; j < 4; j++)
                    {
                        EnqueueRandomUmp(queue, (uint)(j * i * 2));
                    }

                    // dequeue 3 random messages
                    for (int j = 0; j < 3; j++)
                    {
                        DequeueNextUmp(queue);
                    }

                    // queue 3 random messages
                    for (int j = 0; j < 3; j++)
                    {
                        EnqueueRandomUmp(queue, (uint)(j * i * 4));
                    }

                    // dequeue 4 random messages. This should drain the queue
                    for (int j = 0; j < 4; j++)
                    {
                        DequeueNextUmp(queue);
                    }

                    Assert.IsTrue(queue.IsEmpty(), "Queue should have been empty");
                }
            }
        }










    }
}
