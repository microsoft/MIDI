// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

//using Microsoft.Windows.Midi.Messages.Packet;
using Microsoft.Windows.Midi.PluginModel;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;


namespace Microsoft.Windows.Midi.Internal.Service.Processing
{
    // TODO: Move this to an external library


    // Buffer to use in the service when communicating between the service code
    // and transports.
    // The data is a contiguous chunk of memory containing UMP messages. UMP
    // messages vary in length depending upon the message type, but are all made
    // up of 1 to 4 32-bit words.
    //
    // The idea here is to prevent copying of data as much as possible. Ideally,
    // messages that have no required processing / filtering only get copied when
    // sent to the client via the named pipe.
    // So driver -> transport -> service should ideally all share the same
    // memory or otherwise minimize the number of memory allocations/copies.
    //
    // For drivers:
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/kernel/managing-memory-for-drivers
    //public class SharedMemoryMidiMessageBuffer : IMidiMessageBuffer
    //{
    //    //   private Memory<UInt32> _messageBuffer;

    //    // TODO: If this is a circular buffer, we'll do it this way. Not sure how to 
    //    // reasonably sync that with teh driver,. though
    //    //private IntPtr _head;
    //    //private IntPtr _tail;

    //    // TODO: Implement an IEnumerator to go through the buffer

    //    // This is a pointer to the memory. In the case of driver-based
    //    // transports, it's shared with the kernel driver. In the case
    //    // of all user-code transports, it's a bit of overkill, but we'll
    //    // use the same data sharing pattern
    //    private IntPtr _memoryBufferPointer;
    //    private int _bufferSizeIn32BitWords; 

    //    // True if we're responsible for freeing this memory
    //    private bool _ownMemory;

    //    public nint MemoryPointer => _memoryBufferPointer;

    //    public int BufferSizeIn32BitWords => _bufferSizeIn32BitWords;


    //    public void Dispose()
    //    {
    //        if (_ownMemory && _memoryBufferPointer != IntPtr.Zero)
    //        {
    //            Marshal.FreeHGlobal(_memoryBufferPointer);
    //        }

    //        _memoryBufferPointer = IntPtr.Zero;
    //    }

    //    public void SetSharedMemory(nint pointerToAllocatedBuffer, int bufferSizeIn32BitWords)
    //    {
    //        throw new NotImplementedException();
    //    }

    //    // Need to experiment a bit to figure out sizes here. May want to make
    //    // them configurable in the settings
    //    public void AllocateOwnedBuffer(int bufferSizeIn32BitWords)
    //    {
    //        int bufferSizeInBytes = sizeof(UInt32) * bufferSizeIn32BitWords;

    //        _memoryBufferPointer = Marshal.AllocHGlobal(bufferSizeInBytes);
    //        _bufferSizeIn32BitWords = bufferSizeIn32BitWords;
    //        _ownMemory = true;
    //    }

    //    public Ump GetNextMessage()
    //    {
    //        throw new NotImplementedException();
    //    }
    //}
}
