#include "pch.h"
#include "Midi2ProgramChangeMessage.h"
#include "Midi2ProgramChangeMessage.g.cpp"


namespace winrt::Windows::Devices::Midi::implementation
{
    bool Midi2ProgramChangeMessage::BankValid()
    {
        throw hresult_not_implemented();
    }
    uint8_t Midi2ProgramChangeMessage::Program()
    {
        throw hresult_not_implemented();
    }
    uint8_t Midi2ProgramChangeMessage::BankMsb()
    {
        throw hresult_not_implemented();
    }
    uint8_t Midi2ProgramChangeMessage::BankLsb()
    {
        throw hresult_not_implemented();
    }
    uint16_t Midi2ProgramChangeMessage::CombinedBank()
    {
        throw hresult_not_implemented();
    }
}
