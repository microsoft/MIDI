using System;
using System.Collections.Generic;
using System.IO.MemoryMappedFiles;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace MidiService.Protocol.Midi
{
    internal struct Header
    {
        internal uint TotalCapacityInWords;
        internal uint DataStartOffset;
        internal uint FrontWordIndex;                 // index of oldest item in queue
        internal uint RearWordIndex;                  // index of newest item in queue
    }

    public class MidiMessageSharedMemoryQueue : IMidiMessageQueue, IDisposable
    {
        private bool disposedValue;

        private string _fileName;
        private MemoryMappedFile? _file;


        // TODO: Fix this to make the class thread-safe
        // Everything in an instance of this class must be performed on the same
        // thread. Otherwise the mutex code won't work.
        //
        // Intent is to have one instance of any given queue per process and to have

        private readonly int HeaderSizeInFileInBytes;

        private readonly uint DataOffsetInWords;

        private readonly int Ump128SizeInBytes = Marshal.SizeOf(typeof(Ump128));

        // I hate doing all of this in the constructor, but
        // I hate having an initialze method too
        public MidiMessageSharedMemoryQueue(uint maxMidiWords, Guid id)
        {
            // this returns the header size in bytes
            HeaderSizeInFileInBytes = Marshal.SizeOf(typeof(Header));

            // 
            DataOffsetInWords = (uint)(HeaderSizeInFileInBytes / Marshal.SizeOf(typeof(MidiWord)));

            uint capacity = maxMidiWords * sizeof(MidiWord);

            _fileName = "midi.queue." + id.ToString("D");

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

                _file = MemoryMappedFile.CreateNew(_fileName, (long)capacity + HeaderSizeInFileInBytes);
            }

            CreateMutex();  // needs the filename to be set before calling

            InitializeHeader(maxMidiWords);
        }

       private uint CalculateByteOffset(uint index)
        {
            return (uint)(index * sizeof(MidiWord) + HeaderSizeInFileInBytes);
        }


        private void InitializeHeader(uint capacityInWords)
        {
            // TODO: Need to figure out if we're first consumer
            // initialize the header ONLY if we're the one creating it

            Header header;

            header.TotalCapacityInWords = capacityInWords;
            header.DataStartOffset = DataOffsetInWords;
            header.FrontWordIndex = 0;
            header.RearWordIndex = 0;

            PutFileHeader(header);

        }

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



        // TODO: Need to read lock this?
        private Header GetFileHeader()
        {
            Header header;

            using (var accessor = _file.CreateViewAccessor(0, HeaderSizeInFileInBytes))
            {
                accessor.Read<Header>(0, out header);
            }

            return header;
        }

        private void PutFileHeader(Header header)
        {
            using (var accessor = _file.CreateViewAccessor(0, HeaderSizeInFileInBytes))
            {
                accessor.Write<Header>(0, ref header);
            }
        }

        // I considered having separate mutex for header and file contents
        // but the two would always be used together, so single it is
        private Mutex _mutex;

        // TODO: Need to handle exceptions
        // https://docs.microsoft.com/en-us/dotnet/api/system.threading.waithandle.waitone?view=net-6.0#system-threading-waithandle-waitone
        private bool LockFile()
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

        private void ReleaseFile()
        {
            if (_mutex != null)
                _mutex.ReleaseMutex();
        }



        public bool HasMessages()
        {
            var header = GetFileHeader();

            // This is definitely off by 1
            return header.RearWordIndex != header.FrontWordIndex;

        }


        private uint GetRemainingCapacityInWords(Header header)
        {
            // I bet this is off by 1
            return header.TotalCapacityInWords - header.RearWordIndex;
        }

        public uint GetRemainingCapacityInWords()
        {
            var header = GetFileHeader();
            return GetRemainingCapacityInWords(header);
        }


        private bool QueueHasRoomForData (Header header, int numberOfWords)
        {
            //System.Diagnostics.Debug.WriteLine($"QueueHasRoomForData");
            //System.Diagnostics.Debug.WriteLine($" - Header capacity:    {header.TotalCapacityInWords}");
            //System.Diagnostics.Debug.WriteLine($" - Header data offset: {header.DataStartOffset}");
            //System.Diagnostics.Debug.WriteLine($" - Header front:       {header.FrontWordIndex}");
            //System.Diagnostics.Debug.WriteLine($" - Header rear:        {header.RearWordIndex}");
            //System.Diagnostics.Debug.WriteLine($" - Requested words:    {numberOfWords}");

            bool result = GetRemainingCapacityInWords(header) >= numberOfWords;

            //System.Diagnostics.Debug.WriteLine($" - Result:             {result}");


            return result;
        }


        // this is private because we only want the queue to support
        // certain data types. We don't want to allow adding just 
        // anything to the queue
        private bool Enqueue<T>(T t) where T: struct
        {
            if (!LockFile()) return false;

            var header = GetFileHeader();

            var messageSizeInWords = Marshal.SizeOf(typeof(T)) / sizeof(MidiWord);

            if (!QueueHasRoomForData(header, messageSizeInWords)) return false;

            uint wordsWritten = 0;
            using (var accessor = _file.CreateViewAccessor())
            {
                // get rear of queue

                accessor.Write<T>(CalculateByteOffset(header.RearWordIndex), ref t);
                wordsWritten += (uint)messageSizeInWords;

                // TODO: Implement circular buffer

                // increment offset
                header.RearWordIndex += wordsWritten;
            }

            // write updated header data
            PutFileHeader(header);

            ReleaseFile();

            return true;
        }


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




        // TODO: Need an automatic dequeue which reads the message type
        // and returns the appropriate type from that.
        // Maybe also need an option to validate on the other ones

        private bool Dequeue<T>(out T t) where T:struct
        {
            if (!LockFile())
            {
                t = default;
                return false;
            }

            var header = GetFileHeader();
            uint wordsRead = 0;
            uint messageSizeInWords = (uint)(Marshal.SizeOf(typeof(T)) / sizeof(MidiWord));

            using (var accessor = _file.CreateViewAccessor())
            {
                // get front of queue

                accessor.Read<T>(CalculateByteOffset(header.FrontWordIndex), out t);
                wordsRead += messageSizeInWords;

                // TODO: Implement circular buffer

                // increment offset
                header.FrontWordIndex += wordsRead;

                // write data
                PutFileHeader(header);
            }

            ReleaseFile();

            return true;
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

    }
}
