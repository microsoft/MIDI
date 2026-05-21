using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.Marshalling;
using System.Text;

namespace Microsoft.Windows.Devices.Midi2
{
    // The C# implementation of these methods automatically translate the HRESULT returns into
    // exceptions, so calling code needs to handle those exceptions.

    [GeneratedComInterface(Options = ComInterfaceOptions.ComObjectWrapper, StringMarshalling = StringMarshalling.Utf16)]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [System.Runtime.InteropServices.Guid("8087b303-0519-31d1-31d1-000000000010")]
    public partial interface IMidiEndpointConnectionMessagesReceivedCallback
    {
        // This is a non-allocating callback that can return any number of
        // messages in a single call. The implementor of the interface is
        // required to quickly take the messages and add them to an internal
        // processing queue before returning. The data pointer is only valid
        // for the duration of this call.
        public unsafe void MessagesReceived(
            Guid sessionId,
            Guid connectionId,
            UInt64 timestamp,
            UInt32 wordCount,
            UInt32* messages
            );
    }


    [GeneratedComInterface(Options = ComInterfaceOptions.ComObjectWrapper, StringMarshalling = StringMarshalling.Utf16)]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [System.Runtime.InteropServices.Guid("8087b303-0519-31d1-31d1-000000000020")]
    public partial interface IMidiEndpointConnectionRaw
    {
        // transmission limit for a single call
        [PreserveSig]
        public UInt32 GetSupportedMaxMidiWordsPerTransmission();

        // returns true if the buffer contains only valid UMP lengths
        // between messages and messages+wordcount. It does not 
        // validate anything else about the UMPs.
        [return: MarshalAs(UnmanagedType.Bool)]
        [PreserveSig]
        public unsafe bool ValidateBufferHasOnlyCompleteUmps(
            UInt32 wordCount,
            UInt32* messages
            );

        // before sending a buffer of messages, the caller is responsible
        // for confirming that the buffer has only complete UMPs, and that
        // the buffer is smaller than or equal to the transmission limit
        public unsafe void SendMidiMessagesRaw(
            UInt64 timestamp,
            UInt32 wordCount,
            UInt32* completeMessages
            );

        // Wire up your callback handler. When this is in play, the normal
        // WinRT message received events including those on listeners 
        // associated with the connection, will not fire. This is designed
        // solely to be a super fast and efficient callback. You can only
        // have one callback handler for a given connection.
        public unsafe void SetMessagesReceivedCallback(
            nint messagesReceivedCallback
            );

        // Remove your callback handler and reinstant normal event routing.
        // Do this before adding a new callback handler, or when you are
        // cleaning up your connection.
        public void RemoveMessagesReceivedCallback();

    }



}
