// Copyright (c) Microsoft Corporation. All rights reserved.
#include "common.h"
#include "MidiPin.h"
#include "MidiFilter.h"

// smallest UMP is 4 bytes, smallest bytestream is 1 byte (clock, etc.)
#define MINIMUM_LOOPED_DATASIZE 1

// largest UMP is 16 bytes
#define MAXIMUM_LOOPED_UMP_DATASIZE 16

// largest supported bytestream is 2048
#define MAXIMUM_LOOPED_BYTESTREAM_DATASIZE 2048

// maximum buffer size is 0x100 pages.
#define MAXIMUM_LOOPED_BUFFER_SIZE (PAGE_SIZE * 0x100)
static_assert(    MAXIMUM_LOOPED_BUFFER_SIZE < ULONG_MAX/2, "The maximum looped buffer size may not exceed 1/2 MAX_ULONG");

static const
KSDATARANGE_MUSIC g_MidiStreamDataRangeUMP[] =
{
    {
        {
            sizeof(KSDATARANGE_MUSIC),
            0, // Flags
            0, // SampleSize
            0, // Reserved
            {STATIC_KSDATAFORMAT_TYPE_MUSIC},
            {STATIC_KSDATAFORMAT_SUBTYPE_UNIVERSALMIDIPACKET},
            {STATIC_KSDATAFORMAT_SPECIFIER_NONE}
        },
        {STATIC_KSMUSIC_TECHNOLOGY_PORT},
        0, // Channels
        0, // Notes
        0xFFFF // ChannelMask
    },
};

KSDATARANGE_MUSIC g_MidiStreamDataRangeMIDI[] =
{
    {
        {
            sizeof(KSDATARANGE_MUSIC),
            0, // Flags
            0, // SampleSize
            0, // Reserved
            {STATIC_KSDATAFORMAT_TYPE_MUSIC},
            {STATIC_KSDATAFORMAT_SUBTYPE_MIDI},
            {STATIC_KSDATAFORMAT_SPECIFIER_NONE}
        },
        {STATIC_KSMUSIC_TECHNOLOGY_PORT},
        0, // Channels
        0, // Notes
        0xFFFF // ChannelMask
    },
};

static const
PKSDATARANGE g_MidiStreamDataRangesUMP[] =
{
    PKSDATARANGE(&g_MidiStreamDataRangeUMP[0]),
};

static const
PKSDATARANGE g_MidiStreamDataRangesBytestream[] =
{
    PKSDATARANGE(&g_MidiStreamDataRangeMIDI[0])
};


static
KSDATARANGE g_MidiBridgeDataRange[] =
{
   {
      sizeof(KSDATARANGE),
      0, // Flags
      0, // SampleSize
      0, // Reserved
      STATICGUIDOF(KSDATAFORMAT_TYPE_MUSIC),
      STATICGUIDOF(KSDATAFORMAT_SUBTYPE_MIDI_BUS),
      STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)
   }
};

static const
PKSDATARANGE g_MidiBridgeDataRanges[] =
{
    PKSDATARANGE(&g_MidiBridgeDataRange[0])
};

static
const
KSPIN_INTERFACE g_StreamingInterfaces[] =
{
   {
      STATICGUIDOF(KSINTERFACESETID_Standard),
      KSINTERFACE_STANDARD_STREAMING,
      0
   },
   {
      STATICGUIDOF(KSINTERFACESETID_Standard),
      KSINTERFACE_STANDARD_LOOPED_STREAMING,
      0
   }
};

static const
KSPIN_DISPATCH g_MidiPinDispatch =
{
    MidiPin::Create,
    MidiPin::Close,
    MidiPin::Process,
    nullptr, // PFNKSPINVOID Reset
    nullptr, // PFNKSPINSETDATAFORMAT SetDataFormat
    MidiPin::SetDeviceState,
    nullptr, // PFNKSPIN Connect
    nullptr, // PFNKSPINVOID Disconnect
    nullptr, // PKSCLOCK_DISPATCH Clock
    nullptr  // PKSALLOCATOR_DISPATCH Allocator
};

DEFINE_KSPROPERTY_TABLE(g_LoopedStreamingProperties)
{
    DEFINE_KSPROPERTY_ITEM
    (
        KSPROPERTY_MIDILOOPEDSTREAMING_BUFFER,             // idProperty
        MidiPin::GetLoopedStreamingBuffer,                 // pfnGetHandler
        sizeof(KSMIDILOOPED_BUFFER_PROPERTY),              // cbMinPropertyInput
        sizeof(KSMIDILOOPED_BUFFER),                       // cbMinDataInput
        NULL,                                              // pfnSetHandler
        0,                                                 // Values
        0,                                                 // RelationsCount
        NULL,                                              // Relations
        NULL,                                              // SupportHandler
        0                                                  // SerializedSize
    ),
    DEFINE_KSPROPERTY_ITEM
    (
        KSPROPERTY_MIDILOOPEDSTREAMING_REGISTERS,          // idProperty
        MidiPin::GetLoopedStreamingRegisters,              // pfnGetHandler
        sizeof(KSPROPERTY),                                // cbMinPropertyInput
        sizeof(KSMIDILOOPED_REGISTERS),                    // cbMinDataInput
        NULL,                                              // pfnSetHandler
        0,                                                 // Values
        0,                                                 // RelationsCount
        NULL,                                              // Relations
        NULL,                                              // SupportHandler
        0                                                  // SerializedSize
    ),
    DEFINE_KSPROPERTY_ITEM
    (
        KSPROPERTY_MIDILOOPEDSTREAMING_NOTIFICATION_EVENT, // idProperty
        NULL,                                              // pfnGetHandler
        sizeof(KSPROPERTY),                                // cbMinPropertyInput
        sizeof(KSMIDILOOPED_EVENT2),                        // cbMinDataInput
        MidiPin::SetLoopedStreamingNotificationEvent,      // pfnSetHandler
        0,                                                 // Values
        0,                                                 // RelationsCount
        NULL,                                              // Relations
        NULL,                                              // SupportHandler
        0                                                  // SerializedSize
    )
};


DEFINE_KSPROPERTY_TABLE(g_Midi2EndpointInformationProperties)
{
    DEFINE_KSPROPERTY_ITEM
    (
        KSPROPERTY_MIDI2_GROUP_TERMINAL_BLOCKS,            // idProperty
        MidiPin::GetGroupTerminalBlocks,                   // pfnGetHandler
        sizeof(KSPROPERTY),                                // cbMinPropertyInput
        0,                                                 // cbMinDataInput
        NULL,                                              // pfnSetHandler
        0,                                                 // Values
        0,                                                 // RelationsCount
        NULL,                                              // Relations
        NULL,                                              // SupportHandler
        0                                                  // SerializedSize
    ),
};

DEFINE_KSPROPERTY_SET_TABLE (g_PinPropertySet)
{
    DEFINE_KSPROPERTY_SET
    (
        &KSPROPSETID_MidiLoopedStreaming,
        SIZEOF_ARRAY(g_LoopedStreamingProperties),
        g_LoopedStreamingProperties,
        0,NULL
    ),
    DEFINE_KSPROPERTY_SET
    (
        &KSPROPSETID_MIDI2_ENDPOINT_INFORMATION,
        SIZEOF_ARRAY(g_Midi2EndpointInformationProperties),
        g_Midi2EndpointInformationProperties,
        0,NULL
    )
};

DEFINE_KSAUTOMATION_TABLE (g_PinAutomationTable)
{
    DEFINE_KSAUTOMATION_PROPERTIES (g_PinPropertySet),
    DEFINE_KSAUTOMATION_METHODS_NULL,
    DEFINE_KSAUTOMATION_EVENTS_NULL
};

// Bridge pin data flow out
#define DEFINE_KSPIN_MIDIOUTBRIDGE \
{ \
    &g_MidiPinDispatch, \
    0, \
    { \
        DEFINE_KSPIN_DEFAULT_INTERFACES, \
        DEFINE_KSPIN_DEFAULT_MEDIUMS, \
        SIZEOF_ARRAY(g_MidiBridgeDataRanges), \
        g_MidiBridgeDataRanges,\
        KSPIN_DATAFLOW_OUT, \
        KSPIN_COMMUNICATION_NONE, \
        &KSCATEGORY_AUDIO, \
        nullptr, \
        0 \
    },\
    0, \
    0, \
    0, \
    nullptr, \
    nullptr \
}

// bridge pin data flow in
#define DEFINE_KSPIN_MIDIINBRIDGE \
{ \
    &g_MidiPinDispatch, \
    0, \
    { \
        DEFINE_KSPIN_DEFAULT_INTERFACES, \
        DEFINE_KSPIN_DEFAULT_MEDIUMS, \
        SIZEOF_ARRAY(g_MidiBridgeDataRanges), \
        g_MidiBridgeDataRanges, \
        KSPIN_DATAFLOW_IN, \
        KSPIN_COMMUNICATION_NONE, \
        &KSCATEGORY_AUDIO, \
        nullptr, \
        0 \
    }, \
    0, \
    0, \
    0, \
    nullptr, \
    nullptr \
}

// Midi filter consisting of a midi in and midi out pins, 
// supporting only UMP
const
KSPIN_DESCRIPTOR_EX g_MidiPinDescriptorsUMP[4] =
{
    {
        &g_MidiPinDispatch,
        &g_PinAutomationTable, // AutomationTable
        {
            SIZEOF_ARRAY(g_StreamingInterfaces),
            g_StreamingInterfaces,
            DEFINE_KSPIN_DEFAULT_MEDIUMS,
            SIZEOF_ARRAY(g_MidiStreamDataRangesUMP),
            g_MidiStreamDataRangesUMP,
            KSPIN_DATAFLOW_IN,
            KSPIN_COMMUNICATION_SINK,
            &KSCATEGORY_AUDIO,
            &KSAUDFNAME_MIDI, // Localized default midi name
            0 // Reserved
        },
        0, // Flags
        1, // InstancesPossible, when using the global pin handle for loopback, this must not exceed 1.
        0, // InstancesNecessary
        nullptr, // AllocatorFraming
        nullptr // IntersectHandler
    },    
    DEFINE_KSPIN_MIDIOUTBRIDGE,
    {
        &g_MidiPinDispatch,
        &g_PinAutomationTable, // AutomationTable
        {
            SIZEOF_ARRAY(g_StreamingInterfaces),
            g_StreamingInterfaces,
            DEFINE_KSPIN_DEFAULT_MEDIUMS,
            SIZEOF_ARRAY(g_MidiStreamDataRangesUMP),
            g_MidiStreamDataRangesUMP,
            KSPIN_DATAFLOW_OUT,
            KSPIN_COMMUNICATION_SINK,
            &KSCATEGORY_AUDIO,
            &KSAUDFNAME_MIDI, // Localized default midi name
            0 // Reserved
        },
        0, // Flags
        1, // InstancesPossible, when using the global pin handle for loopback, this must not exceed 1.
        0, // InstancesNecessary
        nullptr, // AllocatorFraming
        nullptr // IntersectHandler
    },    
    DEFINE_KSPIN_MIDIINBRIDGE
};

// Midi filter consisting of a midi in and midi out pins, 
// supporting only Bytestream
const
KSPIN_DESCRIPTOR_EX g_MidiPinDescriptorsBytestream[4] =
{
    {
        &g_MidiPinDispatch,
        &g_PinAutomationTable, // AutomationTable
        {
            SIZEOF_ARRAY(g_StreamingInterfaces),
            g_StreamingInterfaces,
            DEFINE_KSPIN_DEFAULT_MEDIUMS,
            SIZEOF_ARRAY(g_MidiStreamDataRangesBytestream),
            g_MidiStreamDataRangesBytestream,
            KSPIN_DATAFLOW_IN,
            KSPIN_COMMUNICATION_SINK,
            &KSCATEGORY_AUDIO,
            &KSAUDFNAME_MIDI, // Localized default midi name
            0 // Reserved
        },
        0, // Flags
        1, // InstancesPossible, when using the global pin handle for loopback, this must not exceed 1.
        0, // InstancesNecessary
        nullptr, // AllocatorFraming
        nullptr // IntersectHandler
    },
    DEFINE_KSPIN_MIDIOUTBRIDGE,
    {
        &g_MidiPinDispatch,
        &g_PinAutomationTable, // AutomationTable
        {
            SIZEOF_ARRAY(g_StreamingInterfaces),
            g_StreamingInterfaces,
            DEFINE_KSPIN_DEFAULT_MEDIUMS,
            SIZEOF_ARRAY(g_MidiStreamDataRangesBytestream),
            g_MidiStreamDataRangesBytestream,
            KSPIN_DATAFLOW_OUT,
            KSPIN_COMMUNICATION_SINK,
            &KSCATEGORY_AUDIO,
            &KSAUDFNAME_MIDI, // Localized default midi name
            0 // Reserved
        },
        0, // Flags
        1, // InstancesPossible, when using the global pin handle for loopback, this must not exceed 1.
        0, // InstancesNecessary
        nullptr, // AllocatorFraming
        nullptr // IntersectHandler
    },
    DEFINE_KSPIN_MIDIINBRIDGE
};

_Use_decl_annotations_
MidiPin::MidiPin(
    PKSPIN pin,
    MidiFilter *filter
) : m_Pin(pin),
    m_Filter(filter)
{
    PAGED_CODE();

    // Looped or streaming data transfer
    m_IsLooped = (pin->ConnectionInterface.Id == KSINTERFACE_STANDARD_LOOPED_STREAMING);

    // legacy bytestream data, or UMP
    m_IsByteStream = IsEqualGUID(pin->ConnectionFormat->SubFormat, KSDATAFORMAT_SUBTYPE_MIDI);

    // When used for loopback "standard" streaming,
    // we use a list_entry structure to store the loopback messages
    InitializeListHead(&m_LoopbackMessageQueue);
}

_Use_decl_annotations_
MidiPin::~MidiPin()
{
    PAGED_CODE();
    Cleanup();
}

_Use_decl_annotations_
NTSTATUS
MidiPin::Create(
    PKSPIN pin,
    PIRP /* Irp */
)
{
    PAGED_CODE();

    KsPinAcquireControl(pin);
    auto releaseDeviceOnExit = wil::scope_exit([pin]() { KsPinReleaseControl(pin); });

    PKSFILTER ksFilter = KsPinGetParentFilter(pin);
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, !ksFilter);

    MidiFilter *filter = (MidiFilter *) ksFilter->Context;
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, !filter);

    PKSDATAFORMAT format = pin->ConnectionFormat;

    // Confirm the format matches expectations
    // we only support looped streaming for UMP, but we support both
    // looped and standard streaming for bytestream.
    BOOL formatMatch = (format->FormatSize >= sizeof(KSDATAFORMAT) &&
                        IsEqualGUID(format->MajorFormat, KSDATAFORMAT_TYPE_MUSIC) && 
                        (((pin->ConnectionInterface.Id == KSINTERFACE_STANDARD_LOOPED_STREAMING) && 
                        IsEqualGUID(format->SubFormat, KSDATAFORMAT_SUBTYPE_UNIVERSALMIDIPACKET)) ||
                        IsEqualGUID(format->SubFormat, KSDATAFORMAT_SUBTYPE_MIDI)) &&
                        IsEqualGUID(format->Specifier, KSDATAFORMAT_SPECIFIER_NONE));
    NT_RETURN_NTSTATUS_IF(STATUS_NO_MATCH, !formatMatch);

    auto lock = filter->m_FilterInstance->MidiPinLock.acquire();

    // If a pin for this flow has already been created, then we should not create another
    if (pin->DataFlow == KSPIN_DATAFLOW_IN)
    {
        NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, filter->m_FilterInstance->MidiOutPin != nullptr);
    }
    else
    {
        NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, filter->m_FilterInstance->MidiInPin != nullptr);
    }

    // Create the pin
    MidiPin* midiPin = new (POOL_FLAG_NON_PAGED) MidiPin(pin, filter);
    NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, !midiPin);

    pin->Context = (PVOID)midiPin;

    // Set this new pin on the device level filter instance
    if (pin->DataFlow == KSPIN_DATAFLOW_IN)
    {
        filter->m_FilterInstance->MidiOutPin = midiPin;
    }
    else
    {
        filter->m_FilterInstance->MidiInPin = midiPin;
    }

    return STATUS_SUCCESS;
}

__drv_maxIRQL(PASSIVE_LEVEL)
PAGED_CODE_SEG
NTSTATUS
MidiPin::Cleanup()
{
    PAGED_CODE();

    // shut down the worker thread and wait for it to close,
    // if it exists.
    if (m_WorkerThread)
    {
        m_ThreadExitEvent.set();
        m_ThreadExitedEvent.wait();
        KeWaitForSingleObject(m_WorkerThread, Executive, KernelMode, FALSE, NULL);
        ObDereferenceObject(m_WorkerThread);
        m_WorkerThread = nullptr;
    }

    // unmap and free the registers that are mapped to user mode,
    // if they exist.
    CleanupSingleBufferMapping(&m_Registers);
    m_ReadRegister = nullptr;
    m_WriteRegister = nullptr;

    // unmap and free the buffer that is double mapped to user mode
    // and kernel mode, if the buffer exists and is mapped.
    CleanupDoubleBufferMapping(&m_KernelBufferMapping);
    CleanupDoubleBufferMapping(&m_ClientBufferMapping);

    // clean up the write event that was provided to us by user mode.
    if (m_WriteEvent)
    {
        ObDereferenceObject(m_WriteEvent);
        m_WriteEvent = nullptr;
    }

    if (m_ReadEvent)
    {
        ObDereferenceObject(m_ReadEvent);
        m_ReadEvent = nullptr;
    }

    // For standard streaming loopback, we have a list of the messages
    // being looped back, if it's not empty, free any remaining messages.
    while(!IsListEmpty(&m_LoopbackMessageQueue))
    {
        PLOOPBACK_MESSAGE message = (PLOOPBACK_MESSAGE) RemoveHeadList(&m_LoopbackMessageQueue);
        ExFreePoolWithTag(message, MINMIDI_POOLTAG);
    }

    if (m_Process)
    {
        ObDereferenceObject(m_Process);
        m_Process = nullptr;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
MidiPin::Close(
    PKSPIN pin,
    PIRP /* Irp */
)
{
    PAGED_CODE();

    KsPinAcquireControl(pin);
    auto releaseDeviceOnExit = wil::scope_exit([pin]() { KsPinReleaseControl(pin); });

    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, pin == nullptr);
    MidiPin * This = (MidiPin *)pin->Context;
    if (This)
    {
        {
            auto lock = This->m_Filter->m_FilterInstance->MidiPinLock.acquire();

            // Remove this pin from the device level filter instance
            if (pin->DataFlow == KSPIN_DATAFLOW_IN)
            {
                This->m_Filter->m_FilterInstance->MidiOutPin = nullptr;
            }
            else
            {
                This->m_Filter->m_FilterInstance->MidiInPin = nullptr;
            }
        }

        This->Cleanup();

        delete This;
        pin->Context = nullptr;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
MidiPin::Process(
    PKSPIN pin
)
{
    // This function handles both sending and receiving midi messages
    // when standard streaming is being used, and it loops the messages that 
    // are received back out using the m_LoopbackMessageQueue.
    PAGED_CODE();

    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;

    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, pin == nullptr);
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_DEVICE_STATE, pin->DeviceState != KSSTATE_RUN);
    MidiPin * This = (MidiPin *)pin->Context;
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, This == nullptr);

    // This should not be called for a looped pin instance, this used for
    // packet based only
    NT_RETURN_NTSTATUS_IF(STATUS_NOT_IMPLEMENTED, TRUE == This->m_IsLooped);

    if (pin->DataFlow == KSPIN_DATAFLOW_IN)
    {
        // application is sending a midi message out, the data is flowing into
        // the driver, this is traditionally called midi out.
    
        PKSSTREAM_POINTER streamPtr = KsPinGetLeadingEdgeStreamPointer(pin, KSSTREAM_POINTER_STATE_LOCKED);
        if ( nullptr != streamPtr )
        {
            // confirm there is at least KSMUSICFORMAT worth of data
            if(streamPtr->OffsetIn.Remaining > sizeof(KSMUSICFORMAT))
            {
                PKSMUSICFORMAT musicHeader = (PKSMUSICFORMAT) streamPtr->OffsetIn.Data;
    
                // we have a music header now, confirm that there's more data than just the header
                if ( musicHeader->ByteCount && (streamPtr->OffsetIn.Remaining >= (sizeof(KSMUSICFORMAT) + musicHeader->ByteCount)))
                {
                    PKSPIN midiInPin {nullptr};

                    // valid device request, process the message.
                    status = STATUS_SUCCESS;

                    // Acquire the lock in order to access the midi in pin.
                    // the lock needs to be acquired for the duration of accessing the midi in pin
                    // to ensure that it's not removed in the middle.
                    // scope ensures that we hold the in pin while adding a loopback message,
                    // but it doesn't need to be held while advancing the pointers.
                    {
                        auto lock = This->m_Filter->m_FilterInstance->MidiPinLock.acquire();
                        // if there's a midi in pin to loopback to
                        if (nullptr != This->m_Filter->m_FilterInstance->MidiInPin &&
                            KSSTATE_RUN == This->m_Filter->m_FilterInstance->MidiInPin->m_DeviceState)
                        {
                            // pointer to the data that follows the music header
                            PUCHAR pData = (PUCHAR)(musicHeader + 1);
        
                            // allocate the loopback buffer that will be added to the list
                            PLOOPBACK_MESSAGE message = (PLOOPBACK_MESSAGE) ExAllocatePool2(POOL_FLAG_NON_PAGED, 
                                                                        sizeof(LOOPBACK_MESSAGE) + musicHeader->ByteCount - 1,
                                                                        MINMIDI_POOLTAG);
                            if (nullptr != message)
                            {
                                // copy the data in and add it to the list
                                message->Size = musicHeader->ByteCount;
                                message->Position = streamPtr->StreamHeader->PresentationTime.Time;
                                message->BufferIndex = (BYTE *) message->Buffer;
                                RtlCopyMemory(message->Buffer, pData, musicHeader->ByteCount);
                                InsertTailList(&This->m_Filter->m_FilterInstance->MidiInPin->m_LoopbackMessageQueue, &message->ListEntry);

                                // the kspin that we need to process the loopback on
                                midiInPin = This->m_Filter->m_FilterInstance->MidiInPin->m_Pin;

                            }
                            else
                            {
                                // allocation failed, return a failure
                                status = STATUS_INSUFFICIENT_RESOURCES;
                            }
                        }
                    }

                    if (NT_SUCCESS(status))
                    {
                        KsStreamPointerAdvanceOffsets( streamPtr, streamPtr->OffsetIn.Remaining, 0, FALSE );

                        if (midiInPin)
                        {
                            // trigger the midi in loopback
                            Process(midiInPin);
                        }
                    }
                }
            }

            if (!NT_SUCCESS(status))
            {
                KsStreamPointerSetStatusCode(streamPtr, status);
                KsStreamPointerAdvanceOffsets(streamPtr, streamPtr->OffsetIn.Remaining, 0, FALSE);
            }
        }
    }
    else
    {
        // Because this is doing loopback, and the above code calls Process(midiInPin),
        // this can be called by a midi out thread along with a midi in worker thread.
        // We don't want both processing at the exact same time, only 1 at a time.
        auto streamingLock = (This->m_StandardStreamingLock).acquire();

        // midi message to be sent to the application, this is traditionally called midi
        // in.
        PKSSTREAM_POINTER streamPtr = KsPinGetLeadingEdgeStreamPointer(pin, KSSTREAM_POINTER_STATE_LOCKED);
        while(streamPtr)
        {
            PKSMUSICFORMAT musicHeader = (PKSMUSICFORMAT) streamPtr->OffsetOut.Data;
            PLOOPBACK_MESSAGE message = nullptr;

            // retrieve a loopback message from the queue, if one exists.
            // only hold the lock while retrieving the item, to limit
            // contention with writing messages to the queue.
            {
                auto midiInlock = This->m_Filter->m_FilterInstance->MidiPinLock.acquire();
                if (!IsListEmpty(&This->m_LoopbackMessageQueue))
                {
                    message = (PLOOPBACK_MESSAGE) RemoveHeadList(&This->m_LoopbackMessageQueue);
                }
            }

            // if there is a message, copy it over, free the allocation,
            // and advance the stream pointer offset so the calling app
            // completes.
            if (message)
            {
                BYTE *midiMessage = (BYTE *) (musicHeader + 1);
                ULONG numBytesToCopy = min(streamPtr->OffsetOut.Remaining - sizeof(KSMUSICFORMAT), message->Size);
                ULONG copySize = sizeof(KSMUSICFORMAT) + numBytesToCopy;
                ULONG copySizeAligned = (copySize + 3) & ~3;

                // if we exceed the output buffer after aligning the buffer,
                // remove the unaligned portion from the copy, do that in the next
                // pass.
                if (copySizeAligned > streamPtr->OffsetOut.Remaining)
                {
                    numBytesToCopy -= (copySize & ~3);
                    copySizeAligned = copySize = sizeof(KSMUSICFORMAT) + numBytesToCopy;
                }

                RtlCopyMemory(midiMessage, message->BufferIndex, numBytesToCopy);

                streamPtr->StreamHeader->PresentationTime.Time = message->Position;

                if (numBytesToCopy < message->Size)
                {
                    auto midiInlock = This->m_Filter->m_FilterInstance->MidiPinLock.acquire();

                    // unable to send the entire message, subtract the portion sent
                    // and put it back on the queue
                    message->Size -= numBytesToCopy;
                    message->BufferIndex += numBytesToCopy;
                    InsertHeadList(&This->m_Filter->m_FilterInstance->MidiInPin->m_LoopbackMessageQueue, &message->ListEntry);
                }
                else
                {
                    // message has been sent, free it
                    ExFreePoolWithTag(message, MINMIDI_POOLTAG);
                    message = nullptr;
                }
    
                musicHeader->ByteCount = numBytesToCopy;

                musicHeader->TimeDeltaMs = 0;

                KsStreamPointerAdvanceOffsetsAndUnlock(streamPtr, 0, copySizeAligned, TRUE);

                // Get the stream pointer for the next request, if one exists.
                streamPtr = KsPinGetLeadingEdgeStreamPointer(pin, KSSTREAM_POINTER_STATE_LOCKED);
            }
            else
            {
                status = STATUS_PENDING;

                // unlock and pend because we have no data to send
                KsStreamPointerSetStatusCode(streamPtr, status);
                KsStreamPointerUnlock(streamPtr, FALSE);
                streamPtr = nullptr;
            }
        }
    }

    return status;
}

_Use_decl_annotations_
NONPAGED_CODE_SEG
void MidiPin::WorkerThread(PVOID context)
{
    auto midiPin = reinterpret_cast<MidiPin*>(context);
    midiPin->HandleIo();
}

__drv_maxIRQL(PASSIVE_LEVEL)
NONPAGED_CODE_SEG
void
MidiPin::HandleIo()
{
    // This function handles both sending and receiving midi messages
    // when cyclic buffering is being used.
    // This implememtation loops the midi out data back to midi in.
    NTSTATUS status = STATUS_SUCCESS;

    // start with the even reset to indicate that the thread is running
    m_ThreadExitedEvent.clear();

    if (m_Pin->DataFlow == KSPIN_DATAFLOW_IN)
    {
        // application is sending a midi message out, this is traditionally called
        // midi out.            
        PVOID waitObjects[] = { m_ThreadExitEvent.get(), m_WriteEvent };

        do
        {
            // run until we get a thread exit event.
            // wait for either a write event indicating data is ready to move, or thread exit.
            status = KeWaitForMultipleObjects(2, waitObjects, WaitAny, Executive, KernelMode, FALSE, nullptr, nullptr);
            if (STATUS_WAIT_1 == status)
            {
                do
                {
                    // process data in the cyclic buffer until either the read buffer is empty
                    // or the write buffer is full.

                    // In order to loopback data, we have to have a midi in pin.
                    // The job of this lock is to synchronize availability and
                    // state of m_MidiInPin, so it must be held for the duration of
                    // use of m_MidiInPin. There should be very little contention,
                    // as the only other time this is used is when midi in
                    // transitions between run and paused.
                    auto lock = m_Filter->m_FilterInstance->MidiPinLock.acquire();

                    // we have a write event, there should be data available to move.
                    if (nullptr != m_Filter->m_FilterInstance->MidiInPin &&
                        KSSTATE_RUN == m_Filter->m_FilterInstance->MidiInPin->m_DeviceState)
                    {
                        ULONG bytesAvailableToRead = 0;
                        ULONG bytesAvailable = 0;

                        KeResetEvent(m_WriteEvent);

                        // Retrieve the midi out position for the buffer we are reading from. The data between the read position
                        // and write position is valid. (read position is our last read position, write position is their last written).
                        // Retrieve our read position first since we know it won't be changing, get their last write position second,
                        // so we can get as much data as possible.
                        ULONG midiOutReadPosition = (ULONG) InterlockedCompareExchange((LONG *)m_ReadRegister, 0, 0);
                        ULONG midiOutWritePosition = (ULONG) InterlockedCompareExchange((LONG *)m_WriteRegister, 0, 0);

                        // first figure out how much data there is to read, taking
                        // into account the looping buffer.
                        if (midiOutReadPosition <= midiOutWritePosition)
                        {
                            // if the read position is less than the write position, 
                            // then we haven't looped around so the difference between
                            // the read and write position is the amount of data to read.
                            bytesAvailableToRead = midiOutWritePosition - midiOutReadPosition;
                        }
                        else
                        {
                            // the write position is behind the read position, so
                            // we looped around. The difference between the read position and 
                            // the write position is the available space, so the total buffer
                            // size minus the available space gives us the amount of data to read.
                            bytesAvailableToRead = m_BufferSize - (midiOutReadPosition - midiOutWritePosition);
                        }

                        if (bytesAvailableToRead == 0)
                        {
                            // nothing to copy, so we are finished with this pass
                            break;
                        }

                        PVOID startingReadAddress = (PVOID)(((PBYTE)m_KernelBufferMapping.Buffer1.m_BufferClientAddress) + midiOutReadPosition);
                        PLOOPEDDATAFORMAT header = (PLOOPEDDATAFORMAT)(startingReadAddress);
                        UINT32 dataSize = header->ByteCount;

                        if (dataSize < MINIMUM_LOOPED_DATASIZE || (m_IsByteStream?(dataSize > MAXIMUM_LOOPED_BYTESTREAM_DATASIZE):(dataSize > MAXIMUM_LOOPED_UMP_DATASIZE)))
                        {
                            // TBD: need to log an abort

                            // data is malformed, abort.
                            goto cleanup;
                        }

                        ULONG bytesToCopy = dataSize + sizeof(LOOPEDDATAFORMAT);
                        ULONG finalReadPosition = (midiOutReadPosition + bytesToCopy) % m_BufferSize;

                        if (bytesAvailableToRead < bytesToCopy)
                        {
                            // if the full contents of this buffer isn't yet available,
                            // wait for more data to come in.
                            // Client will set an event when the write position advances.
                            break;
                        }

                        // reset the client read event, in case we find out later the buffer is full.
                        KeResetEvent(m_Filter->m_FilterInstance->MidiInPin->m_ReadEvent);

                        // Retrieve the midi in position for the buffer that we are writing to. The data between the write position
                        // and read position is empty. (read position is their last read position, write position is our last written).
                        // retrieve our write position first since we know it won't be changing, and their read position second,
                        // so we can have as much free space as possible.
                        ULONG midiInWritePosition = (ULONG) InterlockedCompareExchange((LONG *)m_Filter->m_FilterInstance->MidiInPin->m_WriteRegister, 0, 0);
                        ULONG midiInReadPosition = (ULONG) InterlockedCompareExchange((LONG *)m_Filter->m_FilterInstance->MidiInPin->m_ReadRegister, 0, 0);

                        // Now we need to calculate the available space, taking into account the looping
                        // buffer.
                        if (midiInReadPosition <= midiInWritePosition)
                        {
                            // if the read position is less than the write position, then the difference between
                            // the read and write position is the buffer in use, same as above. So, the total
                            // buffer size minus the used buffer gives us the available space.
                            bytesAvailable = m_Filter->m_FilterInstance->MidiInPin->m_BufferSize - (midiInWritePosition - midiInReadPosition);
                        }
                        else
                        {
                            // we looped around, the write position is behind the read position.
                            // The difference between the read position and the write position
                            // is the available space, which is exactly what we want.
                            bytesAvailable = midiInReadPosition - midiInWritePosition;
                        }

                        // Note, if we fill the buffer up 100%, then write position == read position,
                        // which is the same as when the buffer is empty and everything in the buffer
                        // would be lost.
                        // Reserve 1 byte so that when the buffer is full the write position will trail
                        // the read position.
                        // Because of this reserve, and the above calculation, the true bytesAvailable 
                        // count can never be 0.
                        ASSERT(bytesAvailable != 0);
                        bytesAvailable--;

                        if (bytesToCopy > bytesAvailable)
                        {
                            PVOID midiInWaitObjects[] = { m_ThreadExitEvent.get(), m_Filter->m_FilterInstance->MidiInPin->m_ReadEvent };

                            // Wait for either room in the output buffer, or thread exit.
                            status = KeWaitForMultipleObjects(2, midiInWaitObjects, WaitAny, Executive, KernelMode, FALSE, nullptr, nullptr);
                            if (STATUS_WAIT_1 == status)
                            {
                                // we have room, retry writing this pass
                                continue;
                            }
                            break;
                        }

                        // There's enough space available, calculate our write position
                        PVOID startingWriteAddress = (PVOID)(((PBYTE)m_Filter->m_FilterInstance->MidiInPin->m_KernelBufferMapping.Buffer1.m_BufferClientAddress)+midiInWritePosition);

                        // copy the data. This works (reading/writing past the end of the buffer)
                        // because we have mapped the same buffer twice, so reading or writing
                        // past the end has the effect of looping around. We're safe to do this
                        // up to the client address plus 2x the buffer size, which will never happen
                        // because everything above is constrained to the single buffer plus a little bit
                        // of overlap.
                        RtlCopyMemory
                        (
                            startingWriteAddress,
                            startingReadAddress,
                            bytesToCopy
                        );

                        // now calculate the new position that the buffer has been written up to.
                        // this will be the original write position, plus the bytes copied, again modululs
                        // the buffer size to take into account the loop.
                        ULONG finalWritePosition = (midiInWritePosition + bytesToCopy) % m_Filter->m_FilterInstance->MidiInPin->m_BufferSize;

                        // finalize by advancing the registers and setting the write event

                        // advance our read position
                        InterlockedExchange((LONG *)m_ReadRegister, finalReadPosition);

                        // signal that we've read the data, in case a client is waiting for buffer space to
                        // come available.
                        KeSetEvent(m_ReadEvent, 0, 0);

                        // advance the write position for the loopback and signal that there's data available
                        InterlockedExchange((LONG *)m_Filter->m_FilterInstance->MidiInPin->m_WriteRegister, finalWritePosition);
                        KeSetEvent(m_Filter->m_FilterInstance->MidiInPin->m_WriteEvent, 0, 0);
                    }
                    else
                    {
                        // nothing to do with the data, so just update the read position up to the write position
                        // to indicate that it was "processed", so that the midi out writer won't fill up
                        // the buffer
                        InterlockedExchange((LONG *)(m_ReadRegister), *((ULONG*)(m_WriteRegister)));
                        break;
                    }
                }while(true);
            }
            else
            {
                // exit event or failure, exit the loop to terminate the thread.
                break;
            }
        }while(true);
    }
    // else, this is a midi in pin, nothing to do for this worker thread that is just
    // looping the midi out data back to midi in.

cleanup:
    m_ThreadExitedEvent.set();
    PsTerminateSystemThread(status);
}

_Use_decl_annotations_
NTSTATUS
MidiPin::SetDeviceState(
    PKSPIN pin,
    KSSTATE newState,
    KSSTATE oldState
)
{        
    PAGED_CODE();

    KsPinAcquireControl(pin);
    auto releaseDeviceOnExit = wil::scope_exit([pin]() { KsPinReleaseControl(pin); });

    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, pin == nullptr);
    MidiPin * This = (MidiPin *)pin->Context;
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, This == nullptr);

    UNREFERENCED_PARAMETER(oldState);
    ASSERT(oldState == This->m_DeviceState);

    switch(newState)
    {
        case KSSTATE_STOP:
        case KSSTATE_ACQUIRE:
            // nothing to do at this time for this sample driver.
            break;
        case KSSTATE_PAUSE:
            if (KSSTATE_RUN == This->m_DeviceState)
            {
                // transition from run to paused
                if (pin->DataFlow == KSPIN_DATAFLOW_IN)
                {
                    // Midi out
                    if (This->m_IsLooped)
                    {
                        // This is only applicable for looped cyclic buffer,
                        // standard doesn't use the worker thread.

                        // shut down and clean up the worker thread.
                        This->m_ThreadExitEvent.set();
                        This->m_ThreadExitedEvent.wait();
                        KeWaitForSingleObject(This->m_WorkerThread, Executive, KernelMode, FALSE, NULL);
                        ObDereferenceObject(This->m_WorkerThread);
                        This->m_WorkerThread = nullptr;
                    }
                }
            }
            break;
        case KSSTATE_RUN:
            if (KSSTATE_PAUSE == This->m_DeviceState)
            {
                // transition from paused to run
                if (pin->DataFlow == KSPIN_DATAFLOW_IN)
                {
                    // midi out
                    if (This->m_IsLooped)
                    {
                        This->m_ThreadExitEvent.clear();

                        // Create and start the midi out worker thread for the cyclic buffer.
                        HANDLE handle = NULL;
                        NT_RETURN_IF_NTSTATUS_FAILED(PsCreateSystemThread(&handle, THREAD_ALL_ACCESS, 0, 0, 0, MidiPin::WorkerThread, This));

                        // we use the handle from ObReferenceObjectByHandleWithTag, so we can close this handle
                        // when this goes out of scope.
                        auto closeHandle = wil::scope_exit([&handle]() {
                                ZwClose(handle);
                            });

                        // reference the thread using our pooltag, for easier diagnostics later on in the event of
                        // a leak.
                        NT_RETURN_IF_NTSTATUS_FAILED(ObReferenceObjectByHandleWithTag(handle, THREAD_ALL_ACCESS, nullptr, KernelMode, MINMIDI_POOLTAG, (PVOID*)&(This->m_WorkerThread), nullptr));

                        // boost the worker thread priority, to ensure that the write events are handled as
                        // timely as possible.
                        KeSetPriorityThread(This->m_WorkerThread, HIGH_PRIORITY);
                    }
                }
            }
            break;
        default:
            return STATUS_INVALID_PARAMETER;
    }

    {
        auto lock = This->m_Filter->m_FilterInstance->MidiPinLock.acquire();
        This->m_DeviceState = newState;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
MidiPin::GetSingleBufferMapping(
    UINT32 bufferSize,
    KPROCESSOR_MODE mode,
    BOOL lockPages,
    PSINGLE_BUFFER_MAPPING baseMapping,
    PSINGLE_BUFFER_MAPPING mapping
)
{
    PAGED_CODE();

    // The allocation must not be 0
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, bufferSize == 0);

    // The allocation must be a multiple of the page size,
    // as entire pages are mapped.
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, (bufferSize % PAGE_SIZE) != 0);

    auto cleanupOnFailure = wil::scope_exit([&]() {
            CleanupSingleBufferMapping(mapping);
        });

    mapping->m_Mode = mode;

    if (baseMapping)
    {
        // We save the buffer address with a flag indicating we don't own it, so it's available
        // for future reference if needed.
        mapping->m_BufferAddress = baseMapping->m_BufferAddress;
        mapping->m_OwnsAllocation = FALSE;

        mapping->m_BufferMdl = IoAllocateMdl(MmGetMdlVirtualAddress(baseMapping->m_BufferMdl), bufferSize, FALSE, FALSE, NULL);
        NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, nullptr == mapping->m_BufferMdl);

        // build the new mdl using the base mdl as a template
        IoBuildPartialMdl( baseMapping->m_BufferMdl, mapping->m_BufferMdl, MmGetMdlVirtualAddress(baseMapping->m_BufferMdl), bufferSize );
    }
    else
    {
        // otherwise, we're creating a whole new double mapping,
        // allocate the buffer we're going to double-map to the user space.
        mapping->m_BufferAddress = ExAllocatePool2(POOL_FLAG_NON_PAGED, bufferSize, MINMIDI_POOLTAG);
        NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, nullptr == mapping->m_BufferAddress);
        mapping->m_OwnsAllocation = TRUE;        

        mapping->m_BufferMdl = IoAllocateMdl(mapping->m_BufferAddress, bufferSize, FALSE, FALSE, NULL);
        NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, nullptr == mapping->m_BufferMdl);

        // This is the original MDL for this, and will be used as the base mdl for any future mdl's.
        // We are responsible for building the MDL.
        MmBuildMdlForNonPagedPool(mapping->m_BufferMdl);
    }

    if (lockPages)
    {
        __try
        {
            // map the read register to user/kernel space.
            mapping->m_BufferClientAddress = MmMapLockedPagesSpecifyCache(mapping->m_BufferMdl, mode, MmNonCached, NULL, FALSE, NormalPagePriority | MdlMappingNoExecute);
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            mapping->m_BufferClientAddress = NULL;
        }
        NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, nullptr == mapping->m_BufferClientAddress);
    }

    // success, so do not perform a failure clean up when the function exits.
    cleanupOnFailure.release();

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
MidiPin::CleanupSingleBufferMapping(
    PSINGLE_BUFFER_MAPPING mapping
)
{
    PAGED_CODE();

    // m_Process tracks the process which requested the buffer
    // mapping, if there is no process then there cannot be a
    // mapping.
    if (m_Process != nullptr)
    {
        BOOL attachedToProcess {FALSE};
        KAPC_STATE apcState {0};

        // if the buffer was mapped from a different process,
        // we must attach to that process in order to unmap.
        if (IoGetCurrentProcess() != m_Process)
        {
            KeStackAttachProcess((PRKPROCESS) m_Process, &apcState);
            attachedToProcess = TRUE;
        }

        // detach from the requesting process on exit if needed
        auto detachFromProcess = wil::scope_exit([&]() {
                if (attachedToProcess)
                {
                    KeUnstackDetachProcess(&apcState);
                    attachedToProcess = FALSE;
                }
            });

        // having a client address means it has been mapped,
        // so unmap it.
        if (mapping->m_BufferClientAddress)
        {
            if (mapping->m_Mode == UserMode)
            {
                // when mapping user mode, we map with MmMapLockedPagesSpecifyCache,
                // which gets freed with MmUnmapLockedPages.
                MmUnmapLockedPages(mapping->m_BufferClientAddress, mapping->m_BufferMdl);
            }
            else
            {
                // when mapping kernel mode, we map with MmMapLockedPagesWithReservedMapping,
                // which gets freed with MmUnmapReservedMapping.
                MmUnmapReservedMapping(mapping->m_BufferClientAddress, MINMIDI_POOLTAG, mapping->m_BufferMdl);
            }
            mapping->m_BufferClientAddress = nullptr;
        }

        // clean up the mdl if it is present
        if (mapping->m_BufferMdl)
        {
            IoFreeMdl(mapping->m_BufferMdl);
            mapping->m_BufferMdl = nullptr;
        }

        // if this mapping owns the allocation, free the memory
        if (mapping->m_MappingAddress)
        {
            MmFreeMappingAddress(mapping->m_MappingAddress, MINMIDI_POOLTAG);
            mapping->m_MappingAddress = nullptr;
        }

        // if this mapping owns the allocation, free the memory
        if (mapping->m_BufferAddress && mapping->m_OwnsAllocation)
        {
            ExFreePoolWithTag(mapping->m_BufferAddress, MINMIDI_POOLTAG);
        }

        mapping->m_BufferAddress = nullptr;
        mapping->m_OwnsAllocation = FALSE;
    }

    return STATUS_SUCCESS;
}

// irql is restored through scope_exit, which is not detected properly by
// static analysis.
#pragma prefast(push)
#pragma prefast(disable:28167)
_Use_decl_annotations_
NTSTATUS
MidiPin::GetDoubleBufferMapping(
    UINT32 bufferSize,
    KPROCESSOR_MODE mode,
    PSINGLE_BUFFER_MAPPING baseMapping,
    PDOUBLE_BUFFER_MAPPING mapping
)
{

    // this code creates two single buffer mappings, back to back,
    // in either user mode or kernel mode. Being a cyclic buffer,
    // this has the benefit of allowing us to read or write off of the
    // end of the first buffer with the effect of looping back to the beginning
    // of the buffer. We can read/write past by up to 1 bufferSize.

    PAGED_CODE();

    // In the event of failure, be sure to clean up anything
    // allocated during this call.
    auto cleanupOnFailure = wil::scope_exit([&]() {
            // clean up in the opposite order of creation,
            // since Buffer2 may have MDL's that reference
            // Buffer 1 memory.
            CleanupSingleBufferMapping(&mapping->Buffer2);
            CleanupSingleBufferMapping(&mapping->Buffer1);
        });

    // if we have a "baseMapping", we need to build Buffer1 using baseMapping, otherwise baseMapping will be null
    // and we'll build a new mapping. Don't lock it.
    NT_RETURN_IF_NTSTATUS_FAILED(GetSingleBufferMapping(bufferSize, mode, FALSE, baseMapping, &mapping->Buffer1));

    // if we have a baseMapping, we also want to build Buffer2 using that baseMapping.
    // If we don't have a baseMapping, then we want to use Buffer1 as the base mapping.
    // Don't lock it.
    NT_RETURN_IF_NTSTATUS_FAILED(GetSingleBufferMapping(bufferSize, mode, FALSE, baseMapping?baseMapping:&mapping->Buffer1, &mapping->Buffer2));

    // MmMapLockedPagesSpecifyCache is used to map to user mode, and it takes a requested address to use.
    // MmMapLockedPagesSpecifyCache does not respect the requested address for kernel mode, so kernel mode
    // mapping is done differently.
    if (UserMode == mode)
    {
        SINGLE_BUFFER_MAPPING doubleBuffer;
        KIRQL OldIrql;

        // Create a full mapping, with an allocation, of a buffer 2x the required buffer size.
        // We aren't going to keep this, this is just to get the memory manager to set aside some space
        // and provide us the virtual address for it. Don't lock it yet.
        NT_RETURN_IF_NTSTATUS_FAILED(GetSingleBufferMapping(bufferSize * 2, mode, FALSE, nullptr, &doubleBuffer));

        // A double sized buffer is created to reserve the block of memory
        // needed to map the real buffer back to back. Once we're finished
        // allocating the buffer and mapping it twice, all of the double
        // buffer allocations can be freed.
        auto cleanupOnExit = wil::scope_exit([&]() {
                CleanupSingleBufferMapping(&doubleBuffer);
           });

        {
            // the following scope will have a raised IRQL, to help ensure the mapping succeeds.
            KeRaiseIrql(APC_LEVEL, &OldIrql);

            // restore the irql when we exit this block, 
            // whether or not the mapping has succeeded.
            auto restoreIrql = wil::scope_exit([&]()
            {
                KeLowerIrql(OldIrql);
            });

            // retry the mapping up to 8 times, until we have a successful client address for both mappings.
            // When we unmap it's possible that something will come in and use the space that was just unmapped, causing
            // the double mapping to fail.
            for (int i = 0; i < 8 && ((nullptr == mapping->Buffer1.m_BufferClientAddress) || (nullptr == mapping->Buffer2.m_BufferClientAddress)); i++)
            {
                // First lock the double buffer
                if (nullptr == doubleBuffer.m_BufferClientAddress)
                {
                    __try
                    {
                        doubleBuffer.m_BufferClientAddress = MmMapLockedPagesSpecifyCache(doubleBuffer.m_BufferMdl, mode, MmNonCached, NULL, FALSE, NormalPagePriority | MdlMappingNoExecute);
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER)
                    {
                        doubleBuffer.m_BufferClientAddress = nullptr;
                    }
                    NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, nullptr == doubleBuffer.m_BufferClientAddress);
                }

                // save off the address that was used for the new locking
                PVOID doubleBufferMappedAddress = doubleBuffer.m_BufferClientAddress;

                // Unmap the double so we can try to map the other two buffers into its place,
                // hopefully the memory block isn't taken before we complete the mapping-> If it is, we'll retry.
                MmUnmapLockedPages(doubleBuffer.m_BufferClientAddress, doubleBuffer.m_BufferMdl);
                doubleBuffer.m_BufferClientAddress = nullptr;

                __try
                {
                    // map the data buffer at the doubleBufferMappedAddress that we just had mapped, and then unmapped.
                    mapping->Buffer1.m_BufferClientAddress = MmMapLockedPagesSpecifyCache(mapping->Buffer1.m_BufferMdl, mode, MmNonCached, doubleBufferMappedAddress, FALSE, NormalPagePriority | MdlMappingNoExecute);
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                    mapping->Buffer1.m_BufferClientAddress = nullptr;
                }

                // if we mapped the buffer the first time, map the same buffer a second time
                if (nullptr != mapping->Buffer1.m_BufferClientAddress)
                {
                    __try
                    {
                        // map the same data buffer a second time, at an address immediately following the first mapping->
                        mapping->Buffer2.m_BufferClientAddress = MmMapLockedPagesSpecifyCache(mapping->Buffer2.m_BufferMdl, mode, MmNonCached, ((PVOID)(((PBYTE)(mapping->Buffer1.m_BufferClientAddress)) + bufferSize)), FALSE, NormalPagePriority | MdlMappingNoExecute);
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER)
                    {
                        mapping->Buffer2.m_BufferClientAddress = nullptr;
                    }

                    // if we failed mapping the second time, we may be trying again,
                    // unmap the first mapping in preparation for the next try
                    if (nullptr == mapping->Buffer2.m_BufferClientAddress)
                    {
                        MmUnmapLockedPages(mapping->Buffer1.m_BufferClientAddress, mapping->Buffer1.m_BufferMdl);
                        mapping->Buffer1.m_BufferClientAddress = nullptr;
                    }

                    // not back to back, try again
                    if (mapping->Buffer2.m_BufferClientAddress != (((PBYTE)(mapping->Buffer1.m_BufferClientAddress)) + bufferSize))
                    {
                        MmUnmapLockedPages(mapping->Buffer1.m_BufferClientAddress, mapping->Buffer1.m_BufferMdl);
                        mapping->Buffer1.m_BufferClientAddress = nullptr;
                        MmUnmapLockedPages(mapping->Buffer2.m_BufferClientAddress, mapping->Buffer2.m_BufferMdl);
                        mapping->Buffer2.m_BufferClientAddress = nullptr;
                    }

                }
            }

            // if mapping failed, we failed.
            NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, nullptr == mapping->Buffer1.m_BufferClientAddress);
            NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, nullptr == mapping->Buffer2.m_BufferClientAddress);
        }
    } 
    else
    {
        // kernel mode needs to map slightly differently,
        // first, we allocate the mapping address space needed for both buffers,
        // then we map them to the reserved mapping.
        mapping->Buffer1.m_MappingAddress = MmAllocateMappingAddressEx(((SIZE_T) bufferSize) * 2, MINMIDI_POOLTAG, MM_MAPPING_ADDRESS_DIVISIBLE);
        NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, nullptr == mapping->Buffer1.m_MappingAddress);

        mapping->Buffer1.m_BufferClientAddress = MmMapLockedPagesWithReservedMapping(mapping->Buffer1.m_MappingAddress, MINMIDI_POOLTAG, mapping->Buffer1.m_BufferMdl, MmNonCached);
        NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, nullptr == mapping->Buffer1.m_BufferClientAddress);

        mapping->Buffer2.m_BufferClientAddress = MmMapLockedPagesWithReservedMapping(((PVOID)(((PBYTE)(mapping->Buffer1.m_MappingAddress))+bufferSize)), MINMIDI_POOLTAG, mapping->Buffer2.m_BufferMdl, MmNonCached);
        NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, nullptr == mapping->Buffer2.m_BufferClientAddress);

        // if the mapping isn't back to back, we have a problem.
        NT_RETURN_NTSTATUS_IF(STATUS_INSUFFICIENT_RESOURCES, mapping->Buffer2.m_BufferClientAddress != (((PBYTE)(mapping->Buffer1.m_BufferClientAddress))+bufferSize));
    }

    // success, so do not perform a failure clean up when the function exits.
    cleanupOnFailure.release();

    return STATUS_SUCCESS;
}
#pragma prefast(pop)

_Use_decl_annotations_
NTSTATUS
MidiPin::CleanupDoubleBufferMapping(
    PDOUBLE_BUFFER_MAPPING mapping
)
{
    PAGED_CODE();

    // Buffer 1 typically owns the memory, so unmap
    // and remove idl's in the opposite order of creation,
    // buffer 2 and then buffer 1.
    CleanupSingleBufferMapping(&mapping->Buffer2);
    CleanupSingleBufferMapping(&mapping->Buffer1);

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
MidiPin::GetLoopedStreamingBuffer
(
    PIRP                  Irp,
    PKSMIDILOOPED_BUFFER_PROPERTY Request,
    PKSMIDILOOPED_BUFFER  Buffer
)
{
    // handle incoming property call to retrieve the looped streaming buffer.
    // this is registered as a get handler in the ksproperty table.
    // The incoming property and outgoing buffer sizes were also specified,
    // and sizing will be enforced by KS. basic support is also handled
    // by avstream, so no additional handling is required.
    PAGED_CODE();

    BOOL attachedToProcess {FALSE};
    KAPC_STATE apcState {0};

    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, nullptr == Irp);
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, nullptr == Request);
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, nullptr == Buffer);

    PKSPIN pin = KsGetPinFromIrp(Irp);
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, nullptr == pin);

    KsPinAcquireControl(pin);
    auto releaseDeviceOnExit = wil::scope_exit([pin]() { KsPinReleaseControl(pin); });

    MidiPin * This = (MidiPin *)pin->Context;
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, nullptr == This);

    // this is only applicable if this is a looped pin instance.
    NT_RETURN_NTSTATUS_IF(STATUS_NOT_IMPLEMENTED, FALSE == This->m_IsLooped);

    // if we are already initialized, cannot be initialized again.
    NT_RETURN_NTSTATUS_IF(STATUS_ALREADY_INITIALIZED, nullptr != This->m_Process);

    // In the event of failure, be sure to clean up anything
    // allocated during this call.
    auto cleanupOnFailure = wil::scope_exit([&]() {
            This->CleanupDoubleBufferMapping(&This->m_KernelBufferMapping);
            This->CleanupDoubleBufferMapping(&This->m_ClientBufferMapping);

            if (This->m_Process)
            {
                ObDereferenceObject(This->m_Process);
                This->m_Process = nullptr;
            }
        });

    // cache the current process
    This->m_Process = IoGetRequestorProcess(Irp);
    ObReferenceObjectWithTag(This->m_Process, MINMIDI_POOLTAG);

    // if the current process is not the requesting process,
    // attach to the requesting process
    if (IoGetCurrentProcess() != This->m_Process)
    {
        KeStackAttachProcess((PRKPROCESS)This->m_Process, &apcState);
        attachedToProcess = TRUE;
    }

    // detach from the calling process if needed
    auto detachFromProcess = wil::scope_exit([&]() {
            if (attachedToProcess)
            {
                KeUnstackDetachProcess(&apcState);
                attachedToProcess = FALSE;
            }
        });

    // Start by using the requested buffer size. 
    // The actual buffer size must be some multiple of PAGE_SIZE
    // so that we can double map it on the page boundary, so we're
    // going to round this number up to the nearest page.
    ULONG bufferSize = Request->RequestedBufferSize;
    ULONG testResult {0};

    // The allocation must not be 0
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, bufferSize == 0);

    // if adding a page to the buffer size causes it to overflow, the requested size is
    // an invalid parameter.
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, !NT_SUCCESS(RtlULongAdd(bufferSize, PAGE_SIZE, &testResult)));

    // round to the nearest page
    bufferSize = ROUND_TO_PAGES(bufferSize);

    // If the adjusted buffer size is larger than the maximum permitted, fail the request.
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, bufferSize > MAXIMUM_LOOPED_BUFFER_SIZE);

    // If the adjusted buffer size cannot be doubled, fail the request.
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, !NT_SUCCESS(RtlULongMult(bufferSize, 2, &testResult)));

    // Create our mapping to user mode w/ the double buffer.
    NT_RETURN_IF_NTSTATUS_FAILED(This->GetDoubleBufferMapping(bufferSize, UserMode, nullptr, &This->m_ClientBufferMapping));

    // now build a double buffer mapping in kernel mode
    NT_RETURN_IF_NTSTATUS_FAILED(This->GetDoubleBufferMapping(bufferSize, KernelMode, &This->m_ClientBufferMapping.Buffer1, &This->m_KernelBufferMapping));

    // Success, we now have a single buffer mapped to the client space back to back
    // return the client address pointer to the first one
    This->m_BufferSize = Buffer->ActualBufferSize = bufferSize;
    Buffer->BufferAddress = This->m_ClientBufferMapping.Buffer1.m_BufferClientAddress;

    // success, so do not perform a failure clean up when the function exits.
    cleanupOnFailure.release();

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
MidiPin::GetLoopedStreamingRegisters
(
    PIRP                      irp,
    PKSPROPERTY               request,
    PKSMIDILOOPED_REGISTERS   buffer
)
{
    // handle incoming property call to retrieve the looped streaming registers.
    // this is registered as a get handler in the ksproperty table.
    // The incoming property and outgoing buffer sizes were also specified,
    // and sizing will be enforced by KS. basic support is also handled
    // by avstream, so no additional handling is required.

    PAGED_CODE();

    BOOL attachedToProcess {FALSE};
    KAPC_STATE apcState {0};

    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, nullptr == irp);
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, nullptr == request);
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, nullptr == buffer);

    PKSPIN pin = KsGetPinFromIrp(irp);
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, nullptr == pin);

    KsPinAcquireControl(pin);
    auto releaseDeviceOnExit = wil::scope_exit([pin]() { KsPinReleaseControl(pin); });

    MidiPin * This = (MidiPin *)pin->Context;
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, nullptr == This);

    // this is only applicable if this is a looped pin instance.
    NT_RETURN_NTSTATUS_IF(STATUS_NOT_IMPLEMENTED, FALSE == This->m_IsLooped);

    // registers already created, can't create again.
    NT_RETURN_NTSTATUS_IF(STATUS_ALREADY_INITIALIZED, nullptr != This->m_ReadRegister);

    // a different process created the buffer mapping, registers must be created by the
    // same process
    NT_RETURN_NTSTATUS_IF(STATUS_ALREADY_INITIALIZED, IoGetRequestorProcess(irp) != This->m_Process);

    // if the current process is not the requesting process,
    // attach to the requesting process
    if (IoGetCurrentProcess() != This->m_Process)
    {
        KeStackAttachProcess((PRKPROCESS)This->m_Process, &apcState);
        attachedToProcess = TRUE;
    }
    
    // detach from the requesting process on exit if needed
    auto detachFromProcess = wil::scope_exit([&]() {
            if (attachedToProcess)
            {
                KeUnstackDetachProcess(&apcState);
                attachedToProcess = FALSE;
            }
        });

    // in the event of failure, clean up any partial mappings.
    auto cleanupOnFailure = wil::scope_exit([&]() {
            This->CleanupSingleBufferMapping(&This->m_Registers);
            This->m_ReadRegister = nullptr;
            This->m_WriteRegister = nullptr;
        });

    // When mapping into user mode, the mapping happens on page-size alignments,
    // which means you need to allocate a page to ensure you own the entire memory
    // that's mapped into user mode (to prevent other unrelated kernel allocations
    // from being mapped into user)
    NT_RETURN_IF_NTSTATUS_FAILED(This->GetSingleBufferMapping(PAGE_SIZE, UserMode, TRUE, nullptr, &This->m_Registers));

    This->m_ReadRegister = (PULONG) This->m_Registers.m_BufferAddress;
    This->m_WriteRegister = This->m_ReadRegister + 1;

    buffer->ReadPosition = (PULONG) This->m_Registers.m_BufferClientAddress;
    buffer->WritePosition = ((PULONG) This->m_Registers.m_BufferClientAddress) + 1;

    // success, so do not perform a failure clean up when the function exits.
    cleanupOnFailure.release();

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
MidiPin::SetLoopedStreamingNotificationEvent
(
    PIRP                   irp,
    PKSPROPERTY            request,
    PKSMIDILOOPED_EVENT2   buffer
)
{
    // handle incoming property call to set the looped streaming events.
    // this is registered as a set handler in the ksproperty table.
    // The incoming property and outgoing buffer sizes were also specified,
    // and sizing will be enforced by KS. basic support is also handled
    // by avstream, so no additional handling is required.

    PAGED_CODE();

    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, nullptr == irp);
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, nullptr == request);
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, nullptr == buffer);

    PKSPIN pin = KsGetPinFromIrp(irp);
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, nullptr == pin);

    KsPinAcquireControl(pin);
    auto releaseDeviceOnExit = wil::scope_exit([pin]() { KsPinReleaseControl(pin); });

    MidiPin * This = (MidiPin *)pin->Context;
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, nullptr == This);

    // event already registered, can't register again.
    NT_RETURN_NTSTATUS_IF(STATUS_ALREADY_INITIALIZED, nullptr != This->m_WriteEvent);
    NT_RETURN_NTSTATUS_IF(STATUS_ALREADY_INITIALIZED, nullptr != This->m_ReadEvent);

    // a different process created the buffer mapping, the event must come from the
    // same process
    NT_RETURN_NTSTATUS_IF(STATUS_ALREADY_INITIALIZED, IoGetRequestorProcess(irp) != This->m_Process);

    // this is only applicable if this is a looped pin instance.
    NT_RETURN_NTSTATUS_IF(STATUS_NOT_IMPLEMENTED, FALSE == This->m_IsLooped);

    // reference the notification event provided by the client,
    // For midi out will use this event as the signal that midi out data has been written,
    // For midi in this will be the signal that we will set when midi in data has been
    // written.
    NT_RETURN_IF_NTSTATUS_FAILED(ObReferenceObjectByHandle(buffer->WriteEvent,
                                              EVENT_MODIFY_STATE,
                                              *ExEventObjectType,
                                              UserMode,
                                              (PVOID*)&This->m_WriteEvent,
                                              NULL));

    NT_RETURN_IF_NTSTATUS_FAILED(ObReferenceObjectByHandle(buffer->ReadEvent,
                                              EVENT_MODIFY_STATE,
                                              *ExEventObjectType,
                                              UserMode,
                                              (PVOID*)&This->m_ReadEvent,
                                              NULL));

    return STATUS_SUCCESS;
}

WCHAR outputBlockName [] = L"FBO0";

UMP_GROUP_TERMINAL_BLOCK_HEADER outputBlock ={
        sizeof(UMP_GROUP_TERMINAL_BLOCK_HEADER) + sizeof(outputBlockName),  // Size of the whole UMP_GROUP_TERMINAL_BLOCK_DEFINITION
        0,                                                                  // Number
        MIDI_GROUP_TERMINAL_BLOCK_BIDIRECTIONAL,                            // Direction
        0,                                                                  // FirstGroupIndex
        1,                                                                  // GroupCount
        0,                                                                  // Protocol
        0,                                                                  // MaxInputBandwidth
        0                                                                   // MaxOutputBandwidth
    };

_Use_decl_annotations_
NTSTATUS
MidiPin::GetGroupTerminalBlocks
(
    PIRP                      irp,
    PKSP_PIN                  request,
    PVOID                     buffer
)
{
    PAGED_CODE();

    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, nullptr == irp);
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, nullptr == request);

    PIO_STACK_LOCATION  irpStack              = IoGetCurrentIrpStackLocation( irp );
    ULONG               inputBufferLength     = irpStack->Parameters.DeviceIoControl.InputBufferLength;
    ULONG               outputBufferLength    = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, inputBufferLength < sizeof(KSP_PIN));
    NT_RETURN_NTSTATUS_IF(STATUS_NOT_SUPPORTED, (request->Property.Flags & KSPROPERTY_TYPE_GET) == 0);
    NT_RETURN_NTSTATUS_IF(STATUS_NOT_SUPPORTED, (request->Property.Flags & KSPROPERTY_TYPE_SET) != 0);

    // total return data size, multiple item plus all UMP_GROUP_TERMINAL_BLOCK_DEFINITIONs
    ULONG gtbOutputSize = sizeof(KSMULTIPLE_ITEM) + sizeof(outputBlock) + sizeof(outputBlockName);

    irp->IoStatus.Information = gtbOutputSize;
    NT_RETURN_NTSTATUS_IF(STATUS_BUFFER_OVERFLOW, 0 == outputBufferLength);
    NT_RETURN_NTSTATUS_IF(STATUS_BUFFER_TOO_SMALL, outputBufferLength < gtbOutputSize);

    // If we've gotten this far, we have to have an output buffer.
    NT_RETURN_NTSTATUS_IF(STATUS_INVALID_PARAMETER, nullptr == buffer);

    // fill out the leading multiple item structure
    PKSMULTIPLE_ITEM multItem = (PKSMULTIPLE_ITEM) buffer;
    multItem->Size = gtbOutputSize;
    multItem->Count = 1;

    // copy over the gtb header data, note that outputBlock->Size includes the size of the 
    // name that follows the header.
    UMP_GROUP_TERMINAL_BLOCK_HEADER *gtb = (UMP_GROUP_TERMINAL_BLOCK_HEADER*) (multItem + 1);
    memcpy(gtb, &outputBlock, sizeof(outputBlock));

    // copy over the name just after the gtb header
    PWCHAR name = (PWCHAR) (gtb + 1);
    memcpy(name, &outputBlockName, sizeof(outputBlockName));

    return STATUS_SUCCESS;
}

