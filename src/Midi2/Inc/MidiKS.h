// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

// copied from wdm.h
#define PAGE_SIZE 0x1000

#define DWORD_ALIGN(x) ((x+3) & ~3)

class KSMidiDevice
{
public:
    virtual HRESULT Cleanup();
    virtual ~KSMidiDevice();

protected:
    KSMidiDevice() {}

    virtual
    HRESULT Initialize(
        _In_ LPCWSTR,
        _In_opt_ HANDLE,
        _In_ UINT,
        _In_ BOOL,
        _In_ BOOL,
        _In_ ULONG);

    HRESULT OpenStream();

    HRESULT PinSetState(
        _In_ KSSTATE);

    HRESULT ConfigureLoopedBuffer();
    HRESULT ConfigureLoopedRegisters();
    HRESULT ConfigureLoopedEvent();

    wil::unique_handle m_Filter;    
    wil::unique_handle m_Pin;
    wil::unique_cotaskmem_string m_FilterFilename;
    UINT m_PinID {0};

    BOOL m_IsLooped {FALSE};
    BOOL m_IsUMP {FALSE};
    ULONG m_LoopedBufferSize {PAGE_SIZE};

    KSMIDILOOPED_BUFFER m_LoopedBuffer {0};
    KSMIDILOOPED_REGISTERS m_LoopedRegisters {0};

    wil::unique_event m_BufferWriteEvent;

    HRESULT DisableMmcss();
    HRESULT EnableMmcss( 
        _In_ DWORD*);
    HANDLE m_MmcssHandle {NULL};
    DWORD m_MmcssTaskId {0};
};

class KSMidiOutDevice : public KSMidiDevice
{
public:
    HRESULT Initialize(
        _In_ LPCWSTR,
        _In_opt_ HANDLE,
        _In_ UINT,
        _In_ BOOL,
        _In_ BOOL,
        _In_ ULONG,
        _In_ DWORD*);

    HRESULT WriteMidiData(
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
    HRESULT Initialize(
        _In_ LPCWSTR,
        _In_opt_ HANDLE,
        _In_ UINT,
        _In_ BOOL,
        _In_ BOOL,
        _In_ ULONG,
        _In_ DWORD*,
        _In_ IMidiCallback *);

    virtual
    HRESULT Cleanup();

private:
    wil::com_ptr_nothrow<IMidiCallback> m_MidiInCallback;

    static DWORD WINAPI MidiInWorker(
        _In_ LPVOID);

    HRESULT ProcessLoopedMidiIn();
    HRESULT SendRequestToDriver();

    wil::unique_event m_ThreadTerminateEvent;
    wil::unique_event m_ThreadStartedEvent;
    wil::unique_handle m_ThreadHandle;
};
