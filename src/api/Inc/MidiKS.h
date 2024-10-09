// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

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

    virtual
    HRESULT Initialize(
        _In_ LPCWSTR,
        _In_opt_ HANDLE,
        _In_ UINT,
        _In_ MidiTransport,
        _In_ ULONG&);

    HRESULT OpenStream(_In_ ULONG&);

    HRESULT PinSetState(
        _In_ KSSTATE);

    HRESULT ConfigureLoopedBuffer(_In_ ULONG&);
    HRESULT ConfigureLoopedRegisters();
    HRESULT ConfigureLoopedEvent();

    wil::unique_handle m_Filter;    
    wil::unique_handle m_Pin;
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
        _In_ DWORD*);

    HRESULT SendMidiMessage(
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
        _In_ LONGLONG);

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
