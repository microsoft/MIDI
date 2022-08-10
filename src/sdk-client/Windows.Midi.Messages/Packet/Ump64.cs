// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

namespace Microsoft.Windows.Midi.Messages.Packet
{
    public sealed class Ump64 : Ump
    {
        uint Word1;
        uint Word2;

        public Ump64() { }

        public Ump64(uint word1, uint word2)
        {
            Word1 = word1;
            Word2 = word2;
        }

    }
}
