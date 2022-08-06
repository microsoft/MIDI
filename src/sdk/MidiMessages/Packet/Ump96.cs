// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

namespace Microsoft.Windows.Midi.Messages.Packet
{
    public sealed class Ump96 : Ump
    {
        uint Word1;
        uint Word2;
        uint Word3;

        public Ump96() { }

        public Ump96(uint word1, uint word2, uint word3)
        {
            Word1 = word1;
            Word2 = word2;
            Word3 = word3;
        }

    }
}
