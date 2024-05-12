// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

namespace WindowsMidiServicesInternal
{
    winrt::hstring BuildFunctionBlockPropertyKey(_In_ uint8_t functionBlockNumber);
    winrt::hstring BuildFunctionBlockNamePropertyKey(_In_ uint8_t functionBlockNumber);

    bool PropertyMapContainsAnyFunctionBlockProperty(_In_ collections::IMapView<winrt::hstring, foundation::IInspectable> map);
}
