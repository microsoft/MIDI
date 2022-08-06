// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

namespace Microsoft.Windows.Midi.Messages
{
    public sealed class Ump32 : Ump
    {
        UInt32 Word1;

        public Ump32() { }

        public Ump32(UInt32 word1)
        {
            Word1 = word1;
        }

    }
}
