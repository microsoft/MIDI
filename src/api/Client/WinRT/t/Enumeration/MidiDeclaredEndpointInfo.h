// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiDeclaredEndpointInfo.g.h"

namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    struct MidiDeclaredEndpointInfo : MidiDeclaredEndpointInfoT<MidiDeclaredEndpointInfo>
    {
        MidiDeclaredEndpointInfo() = default;

        MidiDeclaredEndpointInfo(
            winrt::hstring name,
            winrt::hstring productInstanceId,
            bool supportsMidi10Protocol,
            bool supportsMidi20Protocol,
            bool supportsReceivingJitterReductionTimestamps,
            bool supportsSendingJitterReductionTimestamps,
            bool hasStaticFunctionBlocks,
            uint8_t declaredFunctionBlockCount,
            uint8_t specificationVersionMajor,
            uint8_t specificationVersionMinor
        )
        {
            m_name = name;
            m_productInstanceId = productInstanceId;
            m_supportsMidi10Protocol = supportsMidi10Protocol;
            m_supportsMidi20Protocol = supportsMidi20Protocol;
            m_supportsReceivingJitterReductionTimestamps = supportsReceivingJitterReductionTimestamps;
            m_supportsSendingJitterReductionTimestamps = supportsSendingJitterReductionTimestamps;
            m_hasStaticFunctionBlocks = hasStaticFunctionBlocks;
            m_declaredFunctionBlockCount = declaredFunctionBlockCount;
            m_specificationVersionMajor = specificationVersionMajor;
            m_specificationVersionMinor = specificationVersionMinor;
        }

        bool IsReadOnly() const noexcept { return m_isReadOnly; }

        winrt::hstring Name() const noexcept { return m_name;}
        void Name(hstring const& value)
        {
            if (IsReadOnly()) return;

            m_name = value;
        }
        
        winrt::hstring ProductInstanceId() const noexcept { return m_productInstanceId; }
        void ProductInstanceId(_In_ winrt::hstring const& value)
        {
            if (IsReadOnly()) return;
            m_productInstanceId = value;
        }
        
        bool SupportsMidi10Protocol() const noexcept { return m_supportsMidi10Protocol; }
        void SupportsMidi10Protocol(_In_ bool const value)
        {
            if (IsReadOnly()) return;
            m_supportsMidi10Protocol = value;
        }
        
        bool SupportsMidi20Protocol() const noexcept { return m_supportsMidi20Protocol; }
        void SupportsMidi20Protocol(_In_ bool const value)
        {
            if (IsReadOnly()) return;
            m_supportsMidi20Protocol = value;
        }
        
        bool SupportsReceivingJitterReductionTimestamps() const noexcept { return m_supportsReceivingJitterReductionTimestamps; }
        void SupportsReceivingJitterReductionTimestamps(_In_ bool const value)
        {
            if (IsReadOnly()) return;
            m_supportsReceivingJitterReductionTimestamps = value;
        }
        
        bool SupportsSendingJitterReductionTimestamps() const noexcept { return m_supportsSendingJitterReductionTimestamps; }
        void SupportsSendingJitterReductionTimestamps(_In_ bool const value)
        {
            if (IsReadOnly()) return;
            m_supportsSendingJitterReductionTimestamps = value;
        }
        
        bool HasStaticFunctionBlocks() const noexcept { return m_hasStaticFunctionBlocks; }
        void HasStaticFunctionBlocks(_In_ bool const value)
        {
            if (IsReadOnly()) return;
            m_hasStaticFunctionBlocks = value;
        }
        
        uint8_t DeclaredFunctionBlockCount() const noexcept { return m_declaredFunctionBlockCount; }
        void DeclaredFunctionBlockCount(_In_ uint8_t const value)
        {
            if (IsReadOnly()) return;
            m_declaredFunctionBlockCount = value;
        }
        
        uint8_t SpecificationVersionMajor() const noexcept { return m_specificationVersionMajor; }
        void SpecificationVersionMajor(_In_ uint8_t const value)
        {
            if (IsReadOnly()) return;
            m_specificationVersionMajor = value;
        }
        
        uint8_t SpecificationVersionMinor() const noexcept { return m_specificationVersionMinor; }
        void SpecificationVersionMinor(_In_ uint8_t const value)
        {
            if (IsReadOnly()) return;
            m_specificationVersionMinor = value;
        }

        void InternalSetReadOnly() noexcept { m_isReadOnly = true; }

    private:
        bool m_isReadOnly{ false };
        winrt::hstring m_name{};
        winrt::hstring m_productInstanceId{};
        bool m_supportsMidi10Protocol{ false };
        bool m_supportsMidi20Protocol{ false };
        bool m_supportsReceivingJitterReductionTimestamps{ false };
        bool m_supportsSendingJitterReductionTimestamps{ false };
        bool m_hasStaticFunctionBlocks{ false };
        uint8_t m_declaredFunctionBlockCount{ 0 };
        uint8_t m_specificationVersionMajor{ 0 };
        uint8_t m_specificationVersionMinor{ 0 };
    };
}
namespace winrt::Windows::Devices::Midi2::Enumeration::factory_implementation
{
    struct MidiDeclaredEndpointInfo : MidiDeclaredEndpointInfoT<MidiDeclaredEndpointInfo, implementation::MidiDeclaredEndpointInfo>
    {};
}