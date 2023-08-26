#pragma once

class MidiLoopbackDevice
{
public:
    void SetCallback(_In_ IMidiCallback* callback);

    HRESULT SendMidiMessage(
        _In_ void*,
        _In_ UINT32,
        _In_ LONGLONG);

    MidiLoopbackDevice();
    ~MidiLoopbackDevice();

private:
    IMidiCallback* m_callback;


};