// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

class CMidi2KSMidi
{
public:

    HRESULT Initialize(_In_ LPCWSTR, _In_ MidiFlow, _In_ DWORD *, _In_opt_ IMidiCallback *);
    HRESULT SendMidiMessage(_In_ PVOID , _In_ UINT , _In_ LONGLONG);
    HRESULT Cleanup();

private:
    std::unique_ptr<KSMidiInDevice> m_MidiInDevice;
    std::unique_ptr<KSMidiOutDevice> m_MidiOutDevice;
};

