// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

namespace Microsoft.Windows.Midi.Messages
{
    public sealed class Ump128 : Ump
    {
        public UInt32 Word1;
        public UInt32 Word2;
        public UInt32 Word3;
        public UInt32 Word4;

        public Ump128() { }

        public Ump128(UInt32 word1, UInt32 word2, UInt32 word3, UInt32 word4)
        {
            Word1 = word1;
            Word2 = word2;
            Word3 = word3;
            Word4 = word4;
        }

    }
}
