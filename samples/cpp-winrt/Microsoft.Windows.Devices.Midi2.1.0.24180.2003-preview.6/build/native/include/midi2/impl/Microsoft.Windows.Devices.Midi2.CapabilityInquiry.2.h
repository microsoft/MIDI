// WARNING: Please don't edit this file. It was generated by C++/WinRT v2.0.240405.15

#pragma once
#ifndef WINRT_Microsoft_Windows_Devices_Midi2_CapabilityInquiry_2_H
#define WINRT_Microsoft_Windows_Devices_Midi2_CapabilityInquiry_2_H
#include "winrt/impl/Microsoft.Windows.Devices.Midi2.CapabilityInquiry.1.h"
WINRT_EXPORT namespace winrt::Microsoft::Windows::Devices::Midi2::CapabilityInquiry
{
    struct WINRT_IMPL_EMPTY_BASES MidiUniqueId : winrt::Microsoft::Windows::Devices::Midi2::CapabilityInquiry::IMidiUniqueId
    {
        MidiUniqueId(std::nullptr_t) noexcept {}
        MidiUniqueId(void* ptr, take_ownership_from_abi_t) noexcept : winrt::Microsoft::Windows::Devices::Midi2::CapabilityInquiry::IMidiUniqueId(ptr, take_ownership_from_abi) {}
        MidiUniqueId();
        explicit MidiUniqueId(uint32_t combined28BitValue);
        MidiUniqueId(uint8_t sevenBitByte1, uint8_t sevenBitByte2, uint8_t sevenBitByte3, uint8_t sevenBitByte4);
        [[nodiscard]] static auto ShortLabel();
        [[nodiscard]] static auto LongLabel();
        static auto CreateBroadcast();
        static auto CreateRandom();
    };
}
#endif
