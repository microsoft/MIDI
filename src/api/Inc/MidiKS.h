// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "KsHandleWrapper.h"

// copied from wdm.h
#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000
#endif

#define DWORD_ALIGN(x) ((x+3) & ~3)

class KSMidiDevice
{
public:
    virtual HRESULT Shutdown();
    virtual ~KSMidiDevice();

protected:
    KSMidiDevice() {}

    // Begin remove with Feature_Servicing_MIDI2DeviceRemoval
    virtual
    HRESULT Initialize(
        _In_ LPCWSTR,
        _In_opt_ HANDLE,
        _In_ UINT,
        _In_ MidiTransport,
        _In_ ULONG&,
        _In_ MessageOptionFlags);
    // End remove with Feature_Servicing_MIDI2DeviceRemoval
    virtual
    HRESULT Initialize(
        _In_ LPCWSTR device,
        _In_opt_ HANDLE filter,
        _In_ UINT pinId,
        _In_ MidiTransport transport,
        _In_ ULONG& bufferSize,
        _In_ DWORD* mmcssTaskId,
        _In_ MessageOptionFlags optionFlags,
        _In_ MidiFlow flow,
        _In_ IMidiCallback *callback,
        _In_ LONGLONG context
        );

    // Begin remove with Feature_Servicing_MIDI2DeviceRemoval
    HRESULT OpenStream(_In_ ULONG&, _In_ MessageOptionFlags);
    // End remove with Feature_Servicing_MIDI2DeviceRemoval
    HRESULT OpenStream();

    HRESULT PinSetState(
        _In_ KSSTATE);

    // Begin remove with Feature_Servicing_MIDI2DeviceRemoval
    HRESULT ConfigureLoopedBuffer(_In_ ULONG&);
    // End remove with Feature_Servicing_MIDI2DeviceRemoval
    HRESULT ConfigureLoopedBuffer();
    HRESULT ConfigureLoopedRegisters();
    HRESULT ConfigureLoopedEvent();

    // state and callbacks required to handle removal on a surprise removal
    // or query remove, and restore on a query remove cancel.
    wil::srwlock m_lock;
    MidiFlow m_Flow{ MidiFlowOut };
    bool m_DeviceRemoved {false};
    IMidiCallback * m_Callback {nullptr};
    LONGLONG m_Context {0};
    ULONG m_BufferSize {0};
    MessageOptionFlags m_OptionFlags {MessageOptionFlags_None};
    void OnRemoveCallback();
    void OnRestoreCallback();

    std::unique_ptr<KsHandleWrapper> m_FilterHandleWrapper;
    std::unique_ptr<KsHandleWrapper> m_PinHandleWrapper;

    wil::unique_cotaskmem_string m_FilterFilename;
    UINT m_PinID {0};
    MidiTransport m_Transport {MidiTransport_Invalid};

    std::unique_ptr<MEMORY_MAPPED_PIPE> m_MidiPipe;
    std::unique_ptr<CMidiXProc> m_CrossProcessMidiPump;

    unique_mmcss_handle m_MmcssHandle;
    DWORD m_MmcssTaskId {0};
};

class KSMidiOutDevice : public KSMidiDevice
{
public:

    HRESULT Initialize(
        _In_ LPCWSTR,
        _In_opt_ HANDLE,
        _In_ UINT,
        _In_ MidiTransport,
        _In_ ULONG,
        _In_ DWORD*,
        _In_ MessageOptionFlags);

    HRESULT SendMidiMessage(
        _In_ MessageOptionFlags,
        _In_ void *,
        _In_ UINT32,
        _In_ LONGLONG);

private:
    HRESULT WritePacketMidiData(
        _In_ void *,
        _In_ UINT32,
        _In_ LONGLONG);

    HRESULT WriteLoopedMidiData(
        _In_ void *,
        _In_ UINT32,
        _In_ LONGLONG);
};

class KSMidiInDevice : public KSMidiDevice
{
public:

    virtual ~KSMidiInDevice()
    {
        Shutdown();
    }

    HRESULT Initialize(
        _In_ LPCWSTR,
        _In_opt_ HANDLE,
        _In_ UINT,
        _In_ MidiTransport,
        _In_ ULONG,
        _In_ DWORD*,
        _In_ IMidiCallback *,
        _In_ LONGLONG,
        _In_ MessageOptionFlags);

    virtual
    HRESULT Shutdown();

private:
    wil::com_ptr_nothrow<IMidiCallback> m_MidiInCallback;
    LONGLONG m_MidiInCallbackContext{};

    static DWORD WINAPI MidiInWorker(
        _In_ LPVOID);

    HRESULT ProcessLoopedMidiIn();
    HRESULT SendRequestToDriver();

    wil::unique_event m_ThreadTerminateEvent;
    wil::unique_event m_ThreadStartedEvent;
    wil::unique_handle m_ThreadHandle;
    BOOL m_Running{ TRUE };

    unique_mmcss_handle m_ThreadOwnedMmcssHandle;
};