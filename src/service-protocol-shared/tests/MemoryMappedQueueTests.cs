using MidiService.Protocol.Midi;
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

            using (IMidiMessageQueue _queue =
                new MidiMessageSharedMemoryQueue(queueSize, id, MidiMessageSharedMemoryQueue.ResizeMode.None))
            {
                // nothing to do here
            }
        }

        [TestMethod]
        public void EnqueueAndDequeueInQueueOfJustOneElement()
        {
            int queueSize = 10;
            Guid id = Guid.NewGuid();

            using (IMidiMessageQueue _queue =
                new MidiMessageSharedMemoryQueue(queueSize, id, MidiMessageSharedMemoryQueue.ResizeMode.None))
            {
                uint writtenValue = 4206942;
                uint count = 10;

                System.Diagnostics.Debug.WriteLine("about to enqueue just 1");

                Assert.IsTrue(_queue.Enqueue(writtenValue));

                System.Diagnostics.Debug.WriteLine("about to dequeue just 1");

                uint word;

                _queue.Dequeue(out word);

                Assert.AreEqual(writtenValue, word);
            }
        }



        [TestMethod]
        public void EnqueueAndDequeue()
        {
            int queueSize = 1000;
            Guid id = Guid.NewGuid();

            using (IMidiMessageQueue _queue =
                new MidiMessageSharedMemoryQueue(queueSize, id, MidiMessageSharedMemoryQueue.ResizeMode.None))
            {
                uint startValue = 4206942;
                uint count = 10;

                System.Diagnostics.Debug.WriteLine("about to enqueue");

                // this is the most expensive way to add items to the
                // queue and so should take the most time.
                for (uint i = startValue; i < startValue + count; i++)
                {
                    Assert.IsTrue(_queue.Enqueue(i), "Failed to enqueue");
                }

                System.Diagnostics.Debug.WriteLine("about to dequeue");

                // Dequeue and verify same values are there
                for (uint i = startValue; i < startValue + count; i++)
                {
                    uint word;

                    Assert.IsTrue(_queue.Dequeue(out word), "Failed to dequeue");

                    Assert.AreEqual(i, word);
                }
            }
        }


        [TestMethod]
        public void OverCapacityWord()
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

            using (IMidiMessageQueue _queue =
                new MidiMessageSharedMemoryQueue(queueSizeInWords, id, MidiMessageSharedMemoryQueue.ResizeMode.None))
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


        [TestMethod]
        public void OverCapacityUmp128()
        {
            int loopCount = 11;
            int queueStructCount = 10;

            int structSize = Marshal.SizeOf(typeof(Ump128));
            int structSizeInWords = structSize / sizeof(uint);
            int queueSizeInWords = queueStructCount * structSizeInWords;

            System.Diagnostics.Debug.WriteLine($"Calculated queueStructCount   {queueStructCount}");
            System.Diagnostics.Debug.WriteLine($"Calculated structSize         {structSize}");
            System.Diagnostics.Debug.WriteLine($"Calculated structSizeInWords  {structSizeInWords}");
            System.Diagnostics.Debug.WriteLine($"Calculated queueSizeInWords   {queueSizeInWords}");

            Guid id = Guid.NewGuid();

            using (IMidiMessageQueue _queue =
                new MidiMessageSharedMemoryQueue(queueSizeInWords, id, MidiMessageSharedMemoryQueue.ResizeMode.None))
            {
                System.Diagnostics.Debug.WriteLine("about to enqueue");

                for (uint i = 0; i < loopCount; i++)
                {
                    Ump128 ump;
                    ump.Word1 = i;
                    ump.Word2 = i + 1;
                    ump.Word3 = i + 2;
                    ump.Word4 = i + 3;

                    System.Diagnostics.Debug.WriteLine($"Adding structure {i}");

                    bool result = _queue.Enqueue(ump);

                    if (i < queueStructCount)
                        Assert.IsTrue(result);
                    else
                        Assert.IsFalse(result);
                }
            }
        }


        [TestMethod]
        public void EnqueueAndDequeue128()
        {
            int loopCount = 1000;

            uint step = 4;
            // in words
            int structSize = Marshal.SizeOf(typeof(Ump128));
            int structSizeInWords = structSize / sizeof(uint);
            int queueSize = loopCount * structSizeInWords;

            System.Diagnostics.Debug.WriteLine($"Calculated structSize         {structSize}");
            System.Diagnostics.Debug.WriteLine($"Calculated structSizeInWords  {structSizeInWords}");
            System.Diagnostics.Debug.WriteLine($"Calculated queueSize          {queueSize}");

            Guid id = Guid.NewGuid();

            using (IMidiMessageQueue _queue =
                new MidiMessageSharedMemoryQueue(queueSize, id, MidiMessageSharedMemoryQueue.ResizeMode.None))
            {
                uint startValue = 4206942;

                System.Diagnostics.Debug.WriteLine($"about to enqueue {loopCount} structures");

                for (uint i = startValue; i < startValue + loopCount; i += step)
                {
                    Ump128 ump;
                    ump.Word1 = i;
                    ump.Word2 = i + 1;
                    ump.Word3 = i + 2;
                    ump.Word4 = i + 3;

                    Assert.IsTrue(_queue.Enqueue(ump), "Failed to enqueue");
                }


                System.Diagnostics.Debug.WriteLine($"about to dequeue {loopCount} structures");

                // Dequeue and verify same values are there
                for (uint i = startValue; i < startValue + loopCount; i += step)
                {
                    Ump128 ump;

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

        private Ump32 BuildSemiValidUmp32(uint seedData)
        {
            Ump32 ump = default;

            ump.Word1 = UmpMessageType32;

            return ump;
        }

        private Ump64 BuildSemiValidUmp64(uint seedData)
        {
            Ump64 ump = default;

            ump.Word1 = UmpMessageType64;
            ump.Word2 = seedData;

            return ump;
        }
        private Ump96 BuildSemiValidUmp96(uint seedData)
        {
            Ump96 ump = default;

            ump.Word1 = UmpMessageType96;
            ump.Word2 = seedData;
            ump.Word3 = seedData+1;

            return ump;
        }
        private Ump128 BuildSemiValidUmp128(uint seedData)
        {
            Ump128 ump = default;

            ump.Word1 = UmpMessageType128;
            ump.Word2 = seedData;
            ump.Word3 = seedData + 1;
            ump.Word4 = seedData + 2;

            return ump;
        }



        [TestMethod]
        public void EnqueueAndDequeueMixed()
        {
            int queueSize = 20;

            Guid id = Guid.NewGuid();

            using (IMidiMessageQueue queue =
                new MidiMessageSharedMemoryQueue(queueSize, id, MidiMessageSharedMemoryQueue.ResizeMode.None))
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
                            result = queue.Enqueue(BuildSemiValidUmp32(i));
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
                            result = queue.Enqueue(BuildSemiValidUmp64(i));
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
                            result = queue.Enqueue(BuildSemiValidUmp96(i));
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
                            result = queue.Enqueue(BuildSemiValidUmp128(i));
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

                while (dequeueResult)
                {
                    System.Diagnostics.Debug.Write(String.Format("{0:0#} : ", j + 1));

                    switch (queue.PeekNextMessageWordCount())
                    {
                        case 1:
                            Ump32 ump32 = default;
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
                            Ump64 ump64 = default;
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
                            Ump96 ump96 = default;
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
                            Ump128 ump128 = default;
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

                System.Diagnostics.Debug.WriteLine($"Read {messagesRead} structures to the queue comprised of {wordsRead} words in total.\n");



            }
        }


        //[TestMethod]
        //public void MemoryMappedQueueCapacity()
        //{

        //}












    }
}
