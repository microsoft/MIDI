// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

typedef enum
{
    Platform_x64 = 1,
    //    Platform_Arm64 = 2,
    //    Platform_Arm64EC = 3,
    Platform_Arm64X = 4,
} MidiAppSDKPlatform;

// uuid
// famous drum machine + famous groove box (7=T)
// famous sampler (5=S)
// famous pedal
// famous nord
// famous arp + famous roland + famous yamaha (5=S)
struct __declspec(uuid("8087b303-d551-bce2-1ead-a2500d50c580")) IMidiClientInitializer : ::IUnknown
{

    STDMETHOD(GetInstalledWindowsMidiServicesSdkVersion)(
        _Out_opt_ MidiAppSDKPlatform* buildPlatform,
        _Out_opt_ DWORD* versionMajor,
        _Out_opt_ DWORD* versionMinor,
        _Out_opt_ DWORD* versionRevision,

        _Out_opt_ DWORD* versionDateNumber,
        _Out_opt_ DWORD* versionTimeNumber,

        _Out_opt_ LPWSTR* buildSource,
        _Out_opt_ LPWSTR* versionName,
        _Out_opt_ LPWSTR* versionFullString
    ) = 0;

    // demand-starts the service if present
    STDMETHOD(EnsureServiceAvailable)() = 0;
};

#define MIDI_CLIENT_INITIALIZER_CLASS_NAME      L"MidiClientInitializer"
#define MIDI_CLIENT_INITIALIZER_PROG_ID         L"Microsoft.Windows.Devices.Midi2.MidiClientInitializer"

// uuid 
// famous trash compactor
// famous droid (b=p)
// famous synthesizer used in a sci-fi movie
// famous tune played on that synthesizer (first letter is not a note, 6=g)
struct __declspec(uuid("c3263827-c3b0-bdbd-2500-ce63a3f3f2c3")) MidiClientInitializer : public IMidiClientInitializer
{
    STDMETHOD(Initialize)() noexcept;       // not part of COM interface
    STDMETHOD(Shutdown)() noexcept;         // not part of COM interface
    bool CanUnloadNow() noexcept;           // not part of COM interface

    STDMETHOD(GetInstalledWindowsMidiServicesSdkVersion)(
        _Out_opt_ MidiAppSDKPlatform* buildPlatform,
        _Out_opt_ DWORD* versionMajor,
        _Out_opt_ DWORD* versionMinor,
        _Out_opt_ DWORD* versionRevision,

        _Out_opt_ DWORD* versionDateNumber,
        _Out_opt_ DWORD* versionTimeNumber,

        _Out_opt_ LPWSTR* buildSource,
        _Out_opt_ LPWSTR* versionName,
        _Out_opt_ LPWSTR* versionFullString
        ) noexcept override;

    STDMETHOD(EnsureServiceAvailable)() noexcept override;

    ULONG __stdcall AddRef() noexcept override;
    ULONG __stdcall Release() noexcept override;
    HRESULT __stdcall QueryInterface(const IID& iid, void** ppv) noexcept override;

    MidiClientInitializer();
    ~MidiClientInitializer();

private:
    ULONG m_cRef{ 0 };

    bool m_initialized{ false };
    wil::critical_section m_initializeLock{};

    wil::com_ptr_nothrow<IMidiTransport> m_serviceTransport{ nullptr };
};

