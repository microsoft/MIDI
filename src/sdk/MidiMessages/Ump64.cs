// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.


namespace Microsoft.Windows.Midi.Messages
{
    public sealed class Ump64 : Ump
    {
        UInt32 Word1;
        UInt32 Word2;

        public Ump64() { }

        public Ump64(UInt32 word1, UInt32 word2)
        {
            Word1 = word1;
            Word2 = word2;
        }

    }
}
