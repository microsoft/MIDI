#include "pch.h"
#include "MidiSessionConnectionInformation.h"
#include "MidiSessionConnectionInformation.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{

    _Use_decl_annotations_
    void MidiSessionConnectionInformation::InternalInitialize(
        winrt::hstring const endpointDeviceId,
        uint16_t const instanceCount,
        foundation::DateTime const earliestConnectionTime
        )
    {
        m_endpointDeviceId = internal::NormalizeEndpointInterfaceIdHStringCopy(endpointDeviceId);
        m_instanceCount = instanceCount;
        m_earliestConnectionTime = earliestConnectionTime;
    }



}
