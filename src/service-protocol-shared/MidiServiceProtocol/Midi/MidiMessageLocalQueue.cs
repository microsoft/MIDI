using System;
using System.Collections.Generic;
using System.IO.MemoryMappedFiles;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MidiService.Protocol.Midi
{
    public class MidiMessageLocalQueue : IMidiMessageQueue, IDisposable
    { 
        // if needed, this can be optimized to work on a block of
        // local memory. Using the .NET queue for prototying for now

        private Queue<MidiWord>? _queue;
        private bool disposedValue;

        private readonly object _readLock = new object();
        private readonly object _writeLock = new object();



        public bool Enqueue(MidiWord word)
        {
            if (_queue != null)
            {
                lock (_writeLock)
                {
                    _queue.Enqueue(word);
                    return true;
                }
            }
            else
            {
                throw new InvalidOperationException("Queue not initialized");
            }
        }

        public uint Dequeue()
        {
            if (_queue != null)
            {
                lock (_readLock)
                {
                    return _queue.Dequeue();
                }
            }
            else
            {
                throw new InvalidOperationException("Queue not initialized");
            }
        }

        public void Initialize(int maxMidiWords, Guid id)
        {
            _queue = new Queue<MidiWord>(maxMidiWords);
        }






        protected virtual void Dispose(bool disposing)
        {
            if (!disposedValue)
            {
                if (disposing)
                {
                    // TODO: dispose managed state (managed objects)
                }

                // TODO: free unmanaged resources (unmanaged objects) and override finalizer
                // TODO: set large fields to null
                disposedValue = true;
            }
        }

        // // TODO: override finalizer only if 'Dispose(bool disposing)' has code to free unmanaged resources
        // ~MidiMessageLocalQueue()
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

        public bool Enqueue(Ump32 ump)
        {
            throw new NotImplementedException();
        }

        public bool Enqueue(Ump64 ump)
        {
            throw new NotImplementedException();
        }

        public bool Enqueue(Ump96 ump)
        {
            throw new NotImplementedException();
        }

        public bool Enqueue(Ump128 ump)
        {
            throw new NotImplementedException();
        }

        public int EnqueueMany(Stream sourceStream, int count)
        {
            throw new NotImplementedException();
        }

        public bool Dequeue(out uint word)
        {
            throw new NotImplementedException();
        }

        public bool Dequeue(out Ump32 ump)
        {
            throw new NotImplementedException();
        }

        public bool Dequeue(out Ump64 ump)
        {
            throw new NotImplementedException();
        }

        public bool Dequeue(out Ump96 ump)
        {
            throw new NotImplementedException();
        }

        public bool Dequeue(out Ump128 ump)
        {
            throw new NotImplementedException();
        }

        public bool Dequeue(Stream destinationStream, out int wordCount)
        {
            throw new NotImplementedException();
        }

        public bool Dequeue(Span<uint> words, out int wordCount)
        {
            throw new NotImplementedException();
        }

        public int PeekNextMessageWordCount()
        {
            throw new NotImplementedException();
        }

        public uint GetRemainingCapacityInWords()
        {
            throw new NotImplementedException();
        }

        public bool HasMessages()
        {
            throw new NotImplementedException();
        }
    }
}
