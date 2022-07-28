using Microsoft.Windows.Midi.Messages;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.PluginModel
{
     public interface IMidiMessageBuffer : IDisposable
    {
        void SetSharedMemory(IntPtr pointerToAllocatedBuffer, int bufferSizeIn32BitWords);

        void AllocateOwnedBuffer(int bufferSizeIn32BitWords);

        IntPtr MemoryPointer { get; }

        int BufferSizeIn32BitWords { get; }

        public Ump GetNextMessage();

    }
}
