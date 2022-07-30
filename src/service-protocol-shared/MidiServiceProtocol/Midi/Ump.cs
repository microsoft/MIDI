using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi.Internal.ServiceProtocol.Midi
{
    // NOTE! This makes an assumption that MidiWord will remain a 32 bit integer
    // The layout here is necessary for sharing with other languages and also
    // for use in memory mapped files

    [StructLayout(LayoutKind.Explicit, Size = 4)]
    public struct Ump32
    {
        [FieldOffset(0)]
        public MidiWord Word1;
    }

    [StructLayout(LayoutKind.Explicit, Size = 8)]
    public struct Ump64
    {
        [FieldOffset(0)]
        public MidiWord Word1;
        [FieldOffset(4)]
        public MidiWord Word2;
    }

    [StructLayout(LayoutKind.Explicit, Size = 12)]
    public struct Ump96
    {
        [FieldOffset(0)]
        public MidiWord Word1;
        [FieldOffset(4)]
        public MidiWord Word2;
        [FieldOffset(8)]
        public MidiWord Word3;
    }

    [StructLayout(LayoutKind.Explicit, Size = 16)]
    public struct Ump128
    {
        [FieldOffset(0)]
        public MidiWord Word1;
        [FieldOffset(4)]
        public MidiWord Word2;
        [FieldOffset(8)]
        public MidiWord Word3;
        [FieldOffset(12)]
        public MidiWord Word4;
    }


}
