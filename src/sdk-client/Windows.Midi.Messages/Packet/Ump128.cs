// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

namespace Microsoft.Windows.Midi.Messages.Packet
{
    public sealed class Ump128 : Ump
    {
        public uint Word1;
        public uint Word2;
        public uint Word3;
        public uint Word4;

        public Ump128() { }

        public Ump128(uint word1, uint word2, uint word3, uint word4)
        {
            Word1 = word1;
            Word2 = word2;
            Word3 = word3;
            Word4 = word4;
        }

    }
}
