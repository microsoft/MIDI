// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.IO.MemoryMappedFiles;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.ServiceProtocol.Midi
{
    public class MidiUmpMessageLocalQueue : IMidiUmpMessageQueue, IDisposable
    { 
        // if needed, this can be optimized to work on a block of
        // local memory. Using the .NET queue for prototying for now

        private Queue<MidiWord> _queue = new Queue<MidiWord>();
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

        public bool IsFull()
        {
            return false;   // this can't be full
        }

        public bool IsEmpty()
        {
            return _queue.Count == 0;
        }



        public bool Enqueue(ref InternalUmp32 ump)
        {
            _queue.Enqueue(ump.Word1);

            return true;
        }

        public bool Enqueue(ref InternalUmp64 ump)
        {
            _queue.Enqueue(ump.Word1);
            _queue.Enqueue(ump.Word2);

            return true;
        }

        public bool Enqueue(ref InternalUmp96 ump)
        {
            _queue.Enqueue(ump.Word1);
            _queue.Enqueue(ump.Word2);
            _queue.Enqueue(ump.Word3);

            return true;
        }

        public bool Enqueue(ref InternalUmp128 ump)
        {
            _queue.Enqueue(ump.Word1);
            _queue.Enqueue(ump.Word2);
            _queue.Enqueue(ump.Word3);
            _queue.Enqueue(ump.Word4);

            return true;
        }



        public bool Dequeue(out uint word)
        {
            try
            {
                word = _queue.Dequeue();
                return true;
            }
            catch (InvalidOperationException)
            {
                word = default;
                return false;
            }
        }

        public bool Dequeue(out InternalUmp32 ump)
        {
            try
            {
                ump.Word1 = _queue.Dequeue();

                return true;
            }
            catch (InvalidOperationException)
            {
                ump = default;
                return false;
            }
        }

        public bool Dequeue(out InternalUmp64 ump)
        {
            try
            {
                ump.Word1 = _queue.Dequeue();
                ump.Word2 = _queue.Dequeue();

                return true;
            }
            catch (InvalidOperationException)
            {
                ump = default;
                return false;
            }
        }

        public bool Dequeue(out InternalUmp96 ump)
        {
            try
            {
                ump.Word1 = _queue.Dequeue();
                ump.Word2 = _queue.Dequeue();
                ump.Word3 = _queue.Dequeue();

                return true;
            }
            catch (InvalidOperationException)
            {
                ump = default;
                return false;
            }
        }

        public bool Dequeue(out InternalUmp128 ump)
        {
            try
            {
                ump.Word1 = _queue.Dequeue();
                ump.Word2 = _queue.Dequeue();
                ump.Word3 = _queue.Dequeue();
                ump.Word4 = _queue.Dequeue();

                return true;
            }
            catch (InvalidOperationException)
            {
                ump = default;
                return false;
            }
        }

        public bool Dequeue(ref Span<uint> words, out int wordCount)
        {
            // TODO: Dequeue with Span

            throw new NotImplementedException();
        }

        public int PeekNextMessageWordCount()
        {
            var word = _queue.Peek();

            return MidiMessageUtilityInternal.UmpLengthFromFirstWord(word);

        }


        public bool Enqueue(ref Span<uint> words)
        {
            int i = 0;

            foreach (MidiWord word in words)
            {
                _queue.Enqueue(word);
                i++;
            }

            return true;
        }
    }
}
