// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections;
using System.Collections.Generic;
using System.IO.MemoryMappedFiles;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.ServiceProtocol.Midi
{
    // Even without the memory-mapped file and resizing support, this queue ended up more
    // complex than the typical ring buffer/queue implementation.
    //
    // Things that would have made this easier, and reasons why those were rejected:
    //
    // 1. Aligning all elements on 16 byte boundaries. This makes queue management much easier, but
    //    most messages will not be Ump128 (16 byte) messages, so a lot of space ends up wasted.
    //
    // 2. Storing the size of the message as a word ahead of the message itself. Again, this wastes
    //    a lot of space, and we can calculate this by reading the first 4 bits of the UMP anyway.
    //
    // 3. Just treating everything as words and not handling messages. This will involve more reads/
    //    writes and puts the onus on the consuming code to assemble the messages. We need to minimize
    //    the number of times we lock/read/unlock and lock/write/unlock the file, so larger atomic
    //    operations are preferred.

    // ---------------------------------------------
    // TODO: Auto-resizing is not yet implemented
    // ---------------------------------------------

    public class MidiUmpMessageSharedQueue : IMidiUmpMessageQueue, IDisposable
    {
        [StructLayout(LayoutKind.Explicit, Size = 20)]
        internal struct Header
        {
            [FieldOffset(0)]
            internal int TotalCapacityInWords;
            [FieldOffset(4)]
            internal int FrontMessageIndex;
            [FieldOffset(8)]
            internal int RearMessageIndex;
            [FieldOffset(12)]
            internal int RearMessageSize;               // this is a convenience to prevent doing an additional read
            [FieldOffset(16)]
            internal int IndexOfLastUsedWordInFile;      // we only write contigous messages, and will sometimes end up with some unused words at the bottom of the file.
        }

        // TODO: This queue does not yet support resizing

        public enum ResizeMode
        {
            None,
            Exponential,
            Linear
        }

        private readonly int HeaderSizeInBytes;     // calculated size of header in bytes
        private readonly int MidiWordSizeInBytes;   // calculated size of an element in bytes

        private readonly ResizeMode _resizeMode;    // how the queue can grow
        private readonly int _maxQueueSizeInWords;   // absolute limit on growth

        private bool disposedValue;

        private string _fileName;
        private MemoryMappedFile? _file;


        // TODO: Do resize mode and max need to be stored in the file header? Only creating end should own these
        
        // TODO: Need to x-process signal changes in the queue


        // I hate doing all of this in the constructor, but
        // I hate having an initialze method, too
        public MidiUmpMessageSharedQueue(int initialQueueSizeInWords, Guid id, ResizeMode resizeMode, int maxQueueSizeInWords)
        {
            // this returns the header size in bytes
            HeaderSizeInBytes = Marshal.SizeOf(typeof(Header));
            MidiWordSizeInBytes = Marshal.SizeOf(typeof(MidiWord));
            
            _resizeMode = resizeMode;
            _maxQueueSizeInWords = maxQueueSizeInWords;

            long capacityInBytes = initialQueueSizeInWords * MidiWordSizeInBytes;

            _fileName = "midi.queue." + id.ToString("D");

            CreateMutex();  // needs the filename to be set before calling

            try
            {
                _file = MemoryMappedFile.OpenExisting(_fileName);
            }
            catch (FileNotFoundException)
            {
                // potential race condition here with two processes creating
                // the queue at the same time
                //
                // For this to work properly, really need the server to 
                // be the owner of all these queues, and dole them out
                // to the clients after creation
                //
                // That's also why this is specific to the use case, and 
                // not a generally reusable memory mapped queue :)

                LockQueue();

                _file = MemoryMappedFile.CreateNew(_fileName, capacityInBytes + HeaderSizeInBytes);

                // we only init the header if we're creating the file
                InitializeHeader(initialQueueSizeInWords);

                ReleaseQueueLock();
            }
        }


        public MidiUmpMessageSharedQueue(int initialQueueSizeInWords, Guid id) : 
            this (initialQueueSizeInWords, id, ResizeMode.None, -1)
        { }


        private long CalculateByteOffset(int index)
        {
            return (long)(index * MidiWordSizeInBytes + HeaderSizeInBytes);
        }


        #region Header

        private void InitializeHeader(int capacityInWords)
        {
            Header header;

            header.TotalCapacityInWords = capacityInWords;
            header.FrontMessageIndex = -1; 
            header.RearMessageIndex = -1;
            header.IndexOfLastUsedWordInFile = -1;
            header.RearMessageSize = -1;    // this is a convenience

            PutFileHeader(ref header);
        }



        // TODO: Need to read lock this?
        private Header GetFileHeader()
        {
            Header header;

            using (var accessor = _file.CreateViewAccessor(0, HeaderSizeInBytes))
            {
                accessor.Read<Header>(0, out header);
            }

            return header;
        }

        private void PutFileHeader(ref Header header)
        {
            using (var accessor = _file.CreateViewAccessor(0, HeaderSizeInBytes))
            {
                accessor.Write<Header>(0, ref header);
            }
        }

        #endregion

        #region Mutex

        // I considered having separate mutex for header and file contents
        // but the two would always be used together, so single it is
        private Mutex _mutex;

        // depends upon filename
        private void CreateMutex()
        {
            string mutexName = _fileName + ".mutex";

            if (Mutex.TryOpenExisting(mutexName, out _mutex))
            {
                // mutex already exists, so we're good
            }
            else
            {
                // create the mutex without owning it
                _mutex = new Mutex(false, mutexName);
            }
        }
        
        // TODO: Need to handle exceptions
        // https://docs.microsoft.com/en-us/dotnet/api/system.threading.waithandle.waitone?view=net-6.0#system-threading-waithandle-waitone
        private bool LockQueue()
        {
            bool result;

            // TODO: Can have a timeout here
            if (_mutex != null)
            {
                result = _mutex.WaitOne();
            }
            else
            {
                result = false;
            }

  //          System.Diagnostics.Debug.WriteLine($"LockFile() {result}");

            return result;
        }

        private void ReleaseQueueLock()
        {
            if (_mutex != null)
                _mutex.ReleaseMutex();
        }

        #endregion


        public bool IsFull()
        {
            var header = GetFileHeader();

            return IsQueueFull(ref header);
        }

        public bool IsEmpty()
        {
            var header = GetFileHeader();

            return IsQueueEmpty(ref header);
        }


        private bool IsQueueEmpty(ref Header header)
        {
            return (header.FrontMessageIndex == -1 && header.RearMessageIndex == -1);
        }

        private bool IsQueueFull(ref Header header)
        {
            if (header.RearMessageIndex + header.RearMessageSize == header.FrontMessageIndex - 1)
                return true;
            else if (header.FrontMessageIndex == 0 && header.RearMessageIndex + header.RearMessageSize-1 >=
                header.TotalCapacityInWords)
                return true;
            else
                return false;
        }

        private bool GetEnqueueIndex(ref Header header, int countWordsToAdd, out int enqueueIndex)
        {
            // is the queue empty?
            if (IsQueueEmpty(ref header))
            {
                enqueueIndex = 0;

                if (countWordsToAdd <= header.TotalCapacityInWords)
                {
                    // queue is empty, and we have capacity
                    //System.Diagnostics.Debug.WriteLine("queue is empty and has room");
                    return true;
                }
                else
                {
                    // message is larger than the queue itself
                    //System.Diagnostics.Debug.WriteLine("queue is empty but message too large");
                    return false;
                }
            }

            // is the queue full?
            if (IsQueueFull(ref header))
            {
                // queue is full
                enqueueIndex = -1;
                //System.Diagnostics.Debug.WriteLine("queue is full");
                return false;
            }

            // will we wrap over the end?
            if (header.RearMessageIndex + header.RearMessageSize-1 + countWordsToAdd >= 
                header.TotalCapacityInWords)
            {
                // do we have room at the top of the file?
                if (header.FrontMessageIndex >= countWordsToAdd)
                {
                    //System.Diagnostics.Debug.WriteLine("wrap: good room at top of file");
                    enqueueIndex = 0;
                    return true;
                }
                else
                {
                    //System.Diagnostics.Debug.WriteLine("wrap: no room at top of file");
                    enqueueIndex = -1;
                    return false;
                }
            }

            // is there enough contiguous room for this message?
            // we've already checked for cases where we're going to wrap, so can ignore that case.

            // we've already wrapped, so see if there's room
            if (header.RearMessageIndex < header.FrontMessageIndex)
            {
                if (header.FrontMessageIndex - (header.RearMessageIndex + header.RearMessageSize) >= countWordsToAdd)
                {
                    //System.Diagnostics.Debug.WriteLine("wrap: previously wrapped, but there's room");
                    enqueueIndex = header.RearMessageIndex + 1;
                    return true;
                }
                else
                {
                    //System.Diagnostics.Debug.WriteLine("wrap: previously wrapped, but no room");
                    // no room
                    enqueueIndex = -1;
                    return false;
                }
            }





            // the rest of the cases are just business as usual

            //System.Diagnostics.Debug.WriteLine("Normal enqueueIndex case");
            enqueueIndex = header.RearMessageIndex + header.RearMessageSize;
            return true;
        }

        private bool GetDequeueIndex(ref Header header, int countWordsToRead, out int dequeueIndex)
        {
            if (IsQueueEmpty(ref header))
            {
                //System.Diagnostics.Debug.WriteLine("Queue Empty");

                dequeueIndex = 0;
                return false;
            }
            else
            {
                //System.Diagnostics.Debug.WriteLine("Normal dequeue");

                dequeueIndex = header.FrontMessageIndex;
                return true;
            }
        }


 


        public int PeekNextMessageWordCount()
        {
            var header = GetFileHeader();

            MidiWord word;

            using (var accessor = _file.CreateViewAccessor())
            {
                // get front of queue
                accessor.Read<MidiWord>(CalculateByteOffset(header.FrontMessageIndex), out word);
            }

            return MidiMessageUtilityInternal.UmpLengthFromFirstWord(word);
        }


        // we don't use the usual % approach here because of variable-sized elements.
        // just clearler to do it this way.
        private void IncrementFrontIndex(ref Header header, int countWordsRemoved)
        {
            var newIndex = header.FrontMessageIndex + countWordsRemoved;

            // check to see if we'll wrap around
            if (newIndex > header.IndexOfLastUsedWordInFile)
            {
                // set a soft EOF at the current end word. 
                header.IndexOfLastUsedWordInFile = header.RearMessageIndex + header.RearMessageSize-1;

                // wrap around to top of file
                header.FrontMessageIndex = 0;
            }
            else
            {
                // otherwise, normal move forward
                header.FrontMessageIndex = newIndex;
            }

        }


        // this is private because we only want the queue to support
        // certain data types. We don't want to allow adding just 
        // anything to the queue. But there are some assumptions in
        // her about MIDI messages being composed of words, for example
        private bool Enqueue<T>(ref T t) where T: struct
        {
            if (!LockQueue())
            {
                //System.Diagnostics.Debug.WriteLine($"Could not acquire lock");
                return false;
            }

            var header = GetFileHeader();

            var messageSizeInWords = Marshal.SizeOf(typeof(T)) / MidiWordSizeInBytes;

            //System.Diagnostics.Debug.WriteLine($"messageSizeInWords={messageSizeInWords}");

            //if (!QueueHasContiguousRoomForData(ref header, messageSizeInWords)) return false;

            bool success = false;
            int index;
            if (GetEnqueueIndex(ref header, messageSizeInWords, out index))
            {
                using (var accessor = _file.CreateViewAccessor())
                {
                    // get rear of queue
                    var offset = CalculateByteOffset(index);

                   // System.Diagnostics.Debug.WriteLine($"Enqueue");
                    //System.Diagnostics.Debug.WriteLine($" - Capacity:            {header.TotalCapacityInWords}");
                    //System.Diagnostics.Debug.WriteLine($" - Front Message index: {header.FrontMessageIndex}");
                    //System.Diagnostics.Debug.WriteLine($" - Rear Message index:  {header.RearMessageIndex}");
                    //System.Diagnostics.Debug.WriteLine($" - Rear Message size:   {header.RearMessageSize}");
                    //System.Diagnostics.Debug.WriteLine($" - Last used word:      {header.IndexOfLastUsedWordInFile}");
                    //System.Diagnostics.Debug.WriteLine($" - Enqueue Index:       {index}");
                    //System.Diagnostics.Debug.WriteLine($" - Message size words:  {messageSizeInWords}");
                    //System.Diagnostics.Debug.WriteLine($" - Byte offset:         {offset}");

                    accessor.Write<T>(offset, ref t);

                    // queue no longer empty
                    if (header.FrontMessageIndex == -1)
                        header.FrontMessageIndex = 0;

                    header.RearMessageIndex = index;
                    header.RearMessageSize = messageSizeInWords;

                    header.IndexOfLastUsedWordInFile =
                        Math.Max(header.IndexOfLastUsedWordInFile, 
                                 header.RearMessageIndex + header.RearMessageSize-1);

                    //System.Diagnostics.Debug.WriteLine($" - Added {messageSizeInWords} words at index {index}");

                    //System.Diagnostics.Debug.WriteLine($" - New Rear Message index:  {header.RearMessageIndex}");
                    //System.Diagnostics.Debug.WriteLine($" - New Rear Message size:   {header.RearMessageSize}");
                    //System.Diagnostics.Debug.WriteLine($" - New Last used word:      {header.IndexOfLastUsedWordInFile}");

                }

                // write updated header data
                PutFileHeader(ref header);
                success = true;
            }
            else
            {
                //System.Diagnostics.Debug.WriteLine($"Could not get enqueue index");
                success = false;
            }

            ReleaseQueueLock();

            return success;
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        private bool DequeueWillDrainQueue(ref Header header, int messageSizeInWords)
        {
            // this check doesn't account for wrapping
            return (header.FrontMessageIndex == header.RearMessageIndex);
        }


            // TODO: Need an automatic dequeue which reads the message type
            // and returns the appropriate type from that.
            // Maybe also need an option to validate on the other ones
            // so that it checks the message type to ensure it matches with the
            // UMP the call is expecting
            // This would be a great place for polymorphic return types, or an 
            // interface, but both screw up the ability to use structs for 
            // performance

            private bool Dequeue<T>(out T t) where T:struct
        {
            if (!LockQueue())
            {
                //System.Diagnostics.Debug.WriteLine($"Could not lock queue");

                t = default;
                return false;
            }

            var header = GetFileHeader();

            int messageSizeInWords = (Marshal.SizeOf(typeof(T)) / MidiWordSizeInBytes);

            int index = 0;

            if (!GetDequeueIndex(ref header, messageSizeInWords, out index))
            {
                //System.Diagnostics.Debug.WriteLine($"Could not get dequeue index");

                t = default;
                return false;
            }

            using (var accessor = _file.CreateViewAccessor())
            {
                long offset = CalculateByteOffset(index);

                //System.Diagnostics.Debug.WriteLine($"Dequeue");
                //System.Diagnostics.Debug.WriteLine($" - Capacity:            {header.TotalCapacityInWords}");
                //System.Diagnostics.Debug.WriteLine($" - Front Message index: {header.FrontMessageIndex}");
                //System.Diagnostics.Debug.WriteLine($" - Rear Message index:  {header.RearMessageIndex}");
                //System.Diagnostics.Debug.WriteLine($" - Rear Message size:   {header.RearMessageSize}");
                //System.Diagnostics.Debug.WriteLine($" - Last used word:      {header.IndexOfLastUsedWordInFile}");

                //System.Diagnostics.Debug.WriteLine($" - Dequeue Index:       {index}");
                //System.Diagnostics.Debug.WriteLine($" - Byte offset:         {offset}");
                //System.Diagnostics.Debug.WriteLine($" - Message size words:  {messageSizeInWords}");

                // get front of queue
                accessor.Read<T>(offset, out t);

                //System.Diagnostics.Debug.WriteLine($" - Dequeued {messageSizeInWords} words at index {index}");

                if (DequeueWillDrainQueue(ref header, messageSizeInWords))
                {
                    // With this dequeue, we're draining the queue. Set the postion flags

                    //System.Diagnostics.Debug.WriteLine("Drained queue");

                    header.FrontMessageIndex = -1;
                    header.RearMessageIndex = -1;
                    header.IndexOfLastUsedWordInFile = -1;
                    header.RearMessageSize = 0;
                }
                else
                {
                    // Norm dequeue. Increment offset
                    IncrementFrontIndex(ref header, messageSizeInWords);
                }

                //System.Diagnostics.Debug.WriteLine($" - New Front Message index: {header.FrontMessageIndex}");

                // write data
                PutFileHeader(ref header);
            }

            ReleaseQueueLock();

            return true;
        }



        private bool EnqueueMany<T>(ref Span<T> messages) where T:struct
        {
            if (!LockQueue())
            {
                return false;
            }

            var header = GetFileHeader();

            var messageSizeInWords = Marshal.SizeOf(typeof(T)) / MidiWordSizeInBytes;

            //System.Diagnostics.Debug.WriteLine($"EnqueueMany: messageSizeInWords={messageSizeInWords}");

            bool success = true;

            using (var accessor = _file.CreateViewAccessor())
            {
                // not using a foreach here because those can't be used
                // in ref calls, meaning they get copied anyway. See CS1657
                int i = 0;
                while (success && i < messages.Length)
                {
                    //System.Diagnostics.Debug.WriteLine($"EnqueueMany: i={i}");

                    T t = messages[i];

                    int index;
                    if (GetEnqueueIndex(ref header, messageSizeInWords, out index))
                    {
                        // get rear of queue
                        var offset = CalculateByteOffset(index);

                        // System.Diagnostics.Debug.WriteLine($"Enqueue");
                        //System.Diagnostics.Debug.WriteLine($" - Capacity:            {header.TotalCapacityInWords}");
                        //System.Diagnostics.Debug.WriteLine($" - Front Message index: {header.FrontMessageIndex}");
                        //System.Diagnostics.Debug.WriteLine($" - Rear Message index:  {header.RearMessageIndex}");
                        //System.Diagnostics.Debug.WriteLine($" - Rear Message size:   {header.RearMessageSize}");
                        //System.Diagnostics.Debug.WriteLine($" - Last used word:      {header.IndexOfLastUsedWordInFile}");
                        //System.Diagnostics.Debug.WriteLine($" - Enqueue Index:       {index}");
                        //System.Diagnostics.Debug.WriteLine($" - Message size words:  {messageSizeInWords}");
                        //System.Diagnostics.Debug.WriteLine($" - Byte offset:         {offset}");

                        accessor.Write<T>(offset, ref t);

                        // queue no longer empty
                        if (header.FrontMessageIndex == -1)
                            header.FrontMessageIndex = 0;

                        header.RearMessageIndex = index;
                        header.RearMessageSize = messageSizeInWords;

                        header.IndexOfLastUsedWordInFile =
                            Math.Max(header.IndexOfLastUsedWordInFile,
                                        header.RearMessageIndex + header.RearMessageSize - 1);

                        //System.Diagnostics.Debug.WriteLine($" - Added {messageSizeInWords} words at index {index}");

                        //System.Diagnostics.Debug.WriteLine($" - New Rear Message index:  {header.RearMessageIndex}");
                        //System.Diagnostics.Debug.WriteLine($" - New Rear Message size:   {header.RearMessageSize}");
                        //System.Diagnostics.Debug.WriteLine($" - New Last used word:      {header.IndexOfLastUsedWordInFile}");

                        success = true;
                    }
                    else
                    {
                        System.Diagnostics.Debug.WriteLine($"Enqueue span: Could not get enqueue index for element {i}. Queue may be full. ");
                        success = false;
                        break;
                    }

                    i++;

                }
            }

            // write updated header data
            PutFileHeader(ref header);

            ReleaseQueueLock();

            return success;

        }


        public bool Enqueue(ref Span<MidiWord> words)
        {
            return EnqueueMany<MidiWord>(ref words);
        }

        public bool Dequeue(ref Span<uint> words, out int wordCount)
        {
            throw new NotImplementedException();
        }


        #region Individual UMP types

        //[MethodImpl(MethodImplOptions.AggressiveInlining)]
        public bool Enqueue(MidiWord word)
        {
            return Enqueue<MidiWord>(ref word);
        }

        //[MethodImpl(MethodImplOptions.AggressiveInlining)]
        public bool Enqueue(ref InternalUmp128 ump)
        {
            return Enqueue<InternalUmp128>(ref ump);
        }

        //[MethodImpl(MethodImplOptions.AggressiveInlining)]
        public bool Enqueue(ref InternalUmp96 ump)
        {
            return Enqueue<InternalUmp96>(ref ump);
        }

        //[MethodImpl(MethodImplOptions.AggressiveInlining)]
        public bool Enqueue(ref InternalUmp64 ump)
        {
            return Enqueue<InternalUmp64>(ref ump);
        }

        //[MethodImpl(MethodImplOptions.AggressiveInlining)]
        public bool Enqueue(ref InternalUmp32 ump)
        {
            return Enqueue<InternalUmp32>(ref ump);
        }



        //[MethodImpl(MethodImplOptions.AggressiveInlining)]
        public bool Dequeue(out InternalUmp128 ump)
        {
            return Dequeue<InternalUmp128>(out ump);
        }

        //[MethodImpl(MethodImplOptions.AggressiveInlining)]
        public bool Dequeue(out InternalUmp96 ump)
        {
            return Dequeue<InternalUmp96>(out ump);
        }

        //[MethodImpl(MethodImplOptions.AggressiveInlining)]
        public bool Dequeue(out InternalUmp64 ump)
        {
            return Dequeue<InternalUmp64>(out ump);
        }

        //[MethodImpl(MethodImplOptions.AggressiveInlining)]
        public bool Dequeue(out InternalUmp32 ump)
        {
            return Dequeue<InternalUmp32>(out ump);
        }

        //[MethodImpl(MethodImplOptions.AggressiveInlining)]
        public bool Dequeue(out MidiWord word)
        {
            return Dequeue<MidiWord>(out word);
        }

        #endregion



        protected virtual void Dispose(bool disposing)
        {
            if (!disposedValue)
            {
                if (disposing)
                {
                    // TODO: dispose managed state (managed objects)
                    if (_mutex != null)
                        _mutex.Dispose();
                }

                // TODO: free unmanaged resources (unmanaged objects) and override finalizer
                // TODO: set large fields to null
                disposedValue = true;
            }
        }

        // // TODO: override finalizer only if 'Dispose(bool disposing)' has code to free unmanaged resources
        // ~MidiMessageSharedMemoryQueue()
        // {
        //     // Do not change this code. Put cleanup code in 'Dispose(bool disposing)' method
        //     Dispose(disposing: false);
        // }

        public void Dispose()
        {
            // Do not change this code. Put cleanup code in 'Dispose(bool disposing)' method
            Dispose(disposing: true);
            GC.SuppressFinalize(this);
        }


    }
}
