// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

// copied from wdm.h
#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000
#endif

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

    BOOL m_IsLooped{ FALSE };
    BOOL m_IsUMP{ FALSE };

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
        _In_ BOOL,
        _In_ BOOL,
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
        Cleanup();
    }

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
    BOOL m_Running{ TRUE };

    unique_mmcss_handle m_ThreadOwnedMmcssHandle;
};
