#include "pch.h"

#include "MidiDeviceTable.h"


MidiDeviceTable::MidiDeviceTable() = default;
MidiDeviceTable::~MidiDeviceTable() = default;

MidiDeviceTable& MidiDeviceTable::Current()
{
    // explanation: http://www.modernescpp.com/index.php/thread-safe-initialization-of-data/

    static MidiDeviceTable current;

    return current;
}


MidiLoopbackDevice* MidiDeviceTable::GetBidiDevice()
{
    return &m_bidiDevice;
}

MidiLoopbackDevice* MidiDeviceTable::GetInOutDevice()
{
    return &m_inOutDevice;
}
