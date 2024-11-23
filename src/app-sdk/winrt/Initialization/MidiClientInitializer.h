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
    STDMETHOD(Initialize)() = 0;
    STDMETHOD(Shutdown)() = 0;

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
struct __declspec(uuid("c3263827-c3b0-bdbd-2500-ce63a3f3f2c3")) MidiClientInitializer : winrt::implements<MidiClientInitializer, IMidiClientInitializer>
{
    STDMETHOD(Initialize)() noexcept override;
    STDMETHOD(Shutdown)() noexcept override;

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


private:
    bool m_initialized{ false };
};

extern winrt::com_ptr<IMidiClientInitializer> g_clientInitializer;

// uuid
// famous phone number
// famous album
// famous musical (same letter/number swaps as others here)
// infamous synthesizer
// Web devs should have no problem decoding the last part
struct __declspec(uuid("18675309-5150-ca75-0b12-5648616c656e")) MidiClientInitializerFactory : winrt::implements<MidiClientInitializerFactory, IClassFactory>
{
    STDMETHOD(CreateInstance)(IUnknown* outer, GUID const& iid, void** result) noexcept final
    {
        *result = nullptr;

        try
        {
            // no aggregation support
            if (outer)
            {
                return CLASS_E_NOAGGREGATION;
            }

            // we don't make a new instance here, because we want only
            // a single instance of this class per-process
            return g_clientInitializer->QueryInterface(iid, result);

        }
        catch (...)
        {
            return winrt::to_hresult();
        }
    }

    STDMETHOD(LockServer)(BOOL) noexcept final
    {
        return S_OK;
    }
};


