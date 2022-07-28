using System;
using System.Collections.Generic;
using System.IO.MemoryMappedFiles;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace MidiService.Protocol.Midi
{
    /// <summary>
    /// A circular buffer implementation backed by a memory-mapped file, and sharable
    /// across processes. Supports dynamic resizing when full
    /// Can also consider implementing a bip buffer https://www.codeproject.com/articles/3479/the-bip-buffer-the-circular-buffer-with-a-twist
    /// </summary>

    public class MidiMessageSharedMemoryQueue : IMidiMessageQueue, IDisposable
    {
        [StructLayout(LayoutKind.Explicit, Size = 16)]
        internal struct Header
        {
            [FieldOffset(0)]
            internal int TotalCapacityInWords;
            [FieldOffset(4)]
            internal int FrontWordIndex;                  // index of oldest item in queue
            [FieldOffset(8)]
            internal int RearWordIndex;                   // index of space after the most recent item added. This is where new words go
            [FieldOffset(12)]
            internal int IndexOfLastUsedWordInFile;      // we only write contigous messages, and will sometimes end up with some unused words at the bottom of the file.
        }

        public enum ResizeMode
        {
            None,
            Exponential,
            Linear
        }

        private readonly int HeaderSizeInBytes;
        private readonly int MidiWordSizeInBytes;
        private readonly ResizeMode _resizeMode;

        private bool disposedValue;

        private string _fileName;
        private MemoryMappedFile? _file;




        // I hate doing all of this in the constructor, but
        // I hate having an initialze method, too
        public MidiMessageSharedMemoryQueue(int maxMidiWords, Guid id, ResizeMode resizeMode)
        {
            // this returns the header size in bytes
            HeaderSizeInBytes = Marshal.SizeOf(typeof(Header));
            MidiWordSizeInBytes = Marshal.SizeOf(typeof(MidiWord));
            _resizeMode = resizeMode;

            long capacityInBytes = maxMidiWords * MidiWordSizeInBytes;

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
                InitializeHeader(maxMidiWords);

                ReleaseQueueLock();
            }
        }

        private long CalculateByteOffset(int index)
        {
            return (long)(index * MidiWordSizeInBytes + HeaderSizeInBytes);
        }


        #region Header

        private void InitializeHeader(int capacityInWords)
        {
            Header header;

            header.TotalCapacityInWords = capacityInWords;
            header.FrontWordIndex = -1; 
            header.RearWordIndex = -1;
            header.IndexOfLastUsedWordInFile = -1;

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

            return (header.RearWordIndex == header.TotalCapacityInWords - 1 && header.FrontWordIndex == 0) ||
                header.RearWordIndex == header.FrontWordIndex - 1;

        }

        //private uint GetRemainingCapacityInWords(ref Header header)
        //{
        //    // I bet this is off by 1
        //    return header.TotalCapacityInWords - header.RearWordIndex;
        //}

        //public uint GetRemainingCapacityInWords()
        //{
        //    var header = GetFileHeader();
        //    return GetRemainingCapacityInWords(ref header);
        //}

        private bool QueueHasContiguousRoomForData(ref Header header, int numberOfWords)
        {
            System.Diagnostics.Debug.WriteLine($"QueueHasRoomForData");
            System.Diagnostics.Debug.WriteLine($" - Header capacity:    {header.TotalCapacityInWords}");
            System.Diagnostics.Debug.WriteLine($" - Header front:       {header.FrontWordIndex}");
            System.Diagnostics.Debug.WriteLine($" - Header rear:        {header.RearWordIndex}");
            System.Diagnostics.Debug.WriteLine($" - Last used word:     {header.IndexOfLastUsedWordInFile}");
            System.Diagnostics.Debug.WriteLine($" - Requested words:    {numberOfWords}");

            bool result;

            // check we won't go past end of file with this single call
            if (header.RearWordIndex + numberOfWords >= header.TotalCapacityInWords)
            {
                // we're moving past end of file. Don't do this. Need to see if we have room back 
                // at the top

                if (header.FrontWordIndex > numberOfWords)
                {
                    // we have room at the top
                    result = true;
                }
                else
                {
                    // we're full
                    result = false;
                }
            }
            else
            {
                result = true;
            }

            //bool result = GetRemainingCapacityInWords(ref header) >= numberOfWords;

            System.Diagnostics.Debug.WriteLine($" - Result:             {result}");


            return result;
        }

        private bool IsQueueEmpty(ref Header header)
        {
            return (header.FrontWordIndex == -1);
        }


        private bool GetEnqueueIndex(ref Header header, int countWordsToAdd, ref int index)
        {
            // is the queue empty?
            if (IsQueueEmpty(ref header))
            {
                index = 0;

                if (countWordsToAdd <= header.TotalCapacityInWords)
                {
                    // queue is empty, and we have capacity
                    return true;
                }
                else
                {
                    // message is too big
                    return false;
                }
            }

            // is the queue full?
            if ((header.RearWordIndex == header.FrontWordIndex - 1) ||
                (header.FrontWordIndex == 0 && header.RearWordIndex == header.TotalCapacityInWords))
            {
                // queue is full
                index = -1;
                return false;
            }

            // will we wrap over the end?
            if (header.RearWordIndex + countWordsToAdd >= header.TotalCapacityInWords - 1)
            {
                // do we have room at the top of the file?
                if (header.FrontWordIndex > header.TotalCapacityInWords)
                {
                    index = 0;
                    return true;
                }
                else
                {
                    index = -1;
                    return false;
                }
            }

            // the rest of the cases are just business as usual

            index = header.RearWordIndex + 1;
            return true;


        }

        // these handle circular buffer index, as well as the 
        // soft EOF that prevents 
        //private void IncrementRearIndex(ref Header header, int countWordsAdded)
        //{
        //    var newIndex = header.RearWordIndex + countWordsAdded;

        //    if (newIndex >= header.TotalCapacityInWords)
        //    {
        //        // set a soft EOF
        //        header.IndexOfLastUsedWordInFile = header.RearWordIndex;
        //        header.RearWordIndex = 0;
        //    }
        //    else
        //    {
        //        header.RearWordIndex = newIndex % header.TotalCapacityInWords;
        //    }

        //    if (header.IndexOfLastUsedWordInFile < header.RearWordIndex-1)
        //        header.IndexOfLastUsedWordInFile = header.RearWordIndex-1;

        //}

        //private void IncrementFrontIndex(ref Header header, int countWordsAdded)
        //{
        //    var newIndex = header.FrontWordIndex + count;

        //    if (newIndex >= header.TotalCapacityInWords)
        //    {
        //        // set a soft EOF
        //        header.IndexOfLastUsedWordInFile = header.FrontWordIndex;
        //        header.FrontWordIndex = 0;
        //    }
        //    else
        //    {
        //        header.FrontWordIndex = newIndex % header.TotalCapacityInWords;
        //    }

        //}


        // this is private because we only want the queue to support
        // certain data types. We don't want to allow adding just 
        // anything to the queue. But there are some assumptions in
        // her about MIDI messages being composed of words, for example
        private bool Enqueue<T>(T t) where T: struct
        {
            if (!LockQueue()) return false;

            var header = GetFileHeader();

            var messageSizeInWords = Marshal.SizeOf(typeof(T)) / MidiWordSizeInBytes;

            //System.Diagnostics.Debug.WriteLine($"messageSizeInWords={messageSizeInWords}");

            //if (!QueueHasContiguousRoomForData(ref header, messageSizeInWords)) return false;

            int index = 0;
            if (GetEnqueueIndex(ref header, messageSizeInWords, ref index))
            {
                using (var accessor = _file.CreateViewAccessor())
                {
                    // get rear of queue
                    var offset = CalculateByteOffset(index);

                    System.Diagnostics.Debug.WriteLine($"front={header.FrontWordIndex}, rear={header.RearWordIndex}, queue at index={index}, byte offset={offset}");

                    accessor.Write<T>(offset, ref t);

                    if (header.FrontWordIndex < 0)
                        header.FrontWordIndex = 0;

                    header.RearWordIndex = index + messageSizeInWords-1;

                    header.IndexOfLastUsedWordInFile = 
                        Math.Max(header.IndexOfLastUsedWordInFile, header.RearWordIndex);

                    System.Diagnostics.Debug.WriteLine($" - Added {messageSizeInWords} words at index {index}");

                }
            }

            // write updated header data
            PutFileHeader(ref header);

            ReleaseQueueLock();

            return true;
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
                t = default;
                return false;
            }

            var header = GetFileHeader();

            if (IsQueueEmpty(ref header))
            {
                t = default;
                return false;
            }

            int wordsRead = 0;
            int messageSizeInWords = (Marshal.SizeOf(typeof(T)) / MidiWordSizeInBytes);

            using (var accessor = _file.CreateViewAccessor())
            {
                long offset = CalculateByteOffset(header.FrontWordIndex);

                System.Diagnostics.Debug.WriteLine($"front={header.FrontWordIndex}, rear={header.RearWordIndex}, byte offset={offset}");

                // get front of queue
                accessor.Read<T>(offset, out t);
                wordsRead += messageSizeInWords;

                // TODO: Implement circular buffer read ------------------------------
                // TODO: I don't properly handle draining the queue completely.

                // increment offset
                header.FrontWordIndex += wordsRead;

                // write data
                PutFileHeader(ref header);
            }

            ReleaseQueueLock();

            return true;
        }




        public int PeekNextMessageWordCount()
        {
            var header = GetFileHeader();

            MidiWord word;

            using (var accessor = _file.CreateViewAccessor())
            {
                // get front of queue
                accessor.Read<MidiWord>(CalculateByteOffset(header.FrontWordIndex), out word);
            }

            // See section 2.1.4 of the UMP protocol specification
            // message type is the first 4 most significant bits of the first word
            //
            // TODO: Move this out to a common class that is available
            // everywhere
            byte messageType =  (byte)((word >> 28) & 0x0F); // the mask isn't necessary, but it makes me feel better inside

            switch (messageType)
            {
                case 0x0:   // Utility message
                case 0x1:   // system real time and system common
                case 0x2:   // MIDI 1.0 channel voice
                    return 1;

                case 0x3:   // data message
                case 0x4:   // MIDI 2.0 channel voice
                    return 2;

                case 0x5:   // Data
                    return 4;


                // future reserved message types --------

                case 0x6:   // reserved
                case 0x7:   // reserved
                    return 1;
                case 0x8:   // reserved
                case 0x9:   // reserved
                case 0xA:   // reserved
                    return 2;
                case 0xB:   // reserved
                case 0xC:   // reserved
                    return 3;
                case 0xD:   // reserved
                case 0xE:   // reserved
                case 0xF:   // reserved
                    return 4;
                default:
                    throw new InvalidDataException($"Next word contains invalid messageType {messageType}");
            }
        }



        public int EnqueueMany(Stream sourceStream, int count)
        {
            throw new NotImplementedException();
        }

        public bool Dequeue(Stream destinationStream, out int count)
        {
            throw new NotImplementedException();
        }

        public bool Dequeue(Span<uint> words, out int wordCount)
        {
            throw new NotImplementedException();
        }


        #region Individual UMP types

        // writing one word at a time is the most expensive way to 
        // enqueue items
        public bool Enqueue(MidiWord word)
        {
            return Enqueue<MidiWord>(word);
        }

        public bool Enqueue(Ump128 ump)
        {
            return Enqueue<Ump128>(ump);
        }

        public bool Enqueue(Ump96 ump)
        {
            return Enqueue<Ump96>(ump);
        }

        public bool Enqueue(Ump64 ump)
        {
            return Enqueue<Ump64>(ump);
        }

        public bool Enqueue(Ump32 ump)
        {
            return Enqueue<Ump32>(ump);
        }


        public bool Dequeue(out Ump128 ump)
        {
            return Dequeue<Ump128>(out ump);
        }

        public bool Dequeue(out Ump96 ump)
        {
            return Dequeue<Ump96>(out ump);
        }

        public bool Dequeue(out Ump64 ump)
        {
            return Dequeue<Ump64>(out ump);
        }

        public bool Dequeue(out Ump32 ump)
        {
            return Dequeue<Ump32>(out ump);
        }

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
