// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.ServiceProtocol.Midi
{
    public interface IMidiUmpMessageQueue : IDisposable
    {
        //int Count { get; }

        //uint GetRemainingCapacityInWords();
        //bool HasMessages();

        bool IsFull();

        bool IsEmpty();



       // void Initialize(int maxMidiWords, Guid id);

        // ump32, or just single words
        bool Enqueue(MidiWord word);

        bool Enqueue(ref InternalUmp32 ump);

        // ump64, or any two words
        bool Enqueue(ref InternalUmp64 ump);

        // ump96, or any three words
        bool Enqueue(ref InternalUmp96 ump);

        // ump128 or any four words
        bool Enqueue(ref InternalUmp128 ump);


        bool Enqueue(ref Span<MidiWord> words);


        bool Dequeue(out MidiWord word);

        bool Dequeue(out InternalUmp32 ump);
        bool Dequeue(out InternalUmp64 ump);
        bool Dequeue(out InternalUmp96 ump);
        bool Dequeue(out InternalUmp128 ump);

        bool Dequeue(ref Span<MidiWord> words, out int wordCount);


        // method to help with removing a full message
        int PeekNextMessageWordCount();


        // TODO: Add methods for waiting on new messages
        // internally using WaitForSingleObject or similar




        // may want to provide methods which operate on spans
        // and other types for block-copy


        // TODO: Provide a DequeueWholeMessage which will pull a message out using the 
        // message type to figure out how many words to read.





    }
}
