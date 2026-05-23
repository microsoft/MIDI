// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace WindowsMidiServicesInternal
{
    // IMap version
    template<typename T>
    inline T GetDeviceInfoProperty(
        winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, winrt::Windows::Foundation::IInspectable> const& properties,
        winrt::hstring const& key,
        T defaultValue
    ) noexcept
    {
        if (!properties.HasKey(key)) return defaultValue;
        if (properties.Lookup(key) == nullptr) return defaultValue;

        std::optional<T> opt = properties.Lookup(key).try_as<T>();

        if (opt == std::nullopt)
        {
            return defaultValue;
        }
        else
        {
            return opt.value();
        }
    }


    // IMapView version
    template<typename T>
    inline T GetDeviceInfoProperty(
        winrt::Windows::Foundation::Collections::IMap<winrt::hstring, winrt::Windows::Foundation::IInspectable> const& properties,
        winrt::hstring const& key,
        T defaultValue
    ) noexcept
    {
        if (!properties.HasKey(key)) return defaultValue;
        if (properties.Lookup(key) == nullptr) return defaultValue;

        std::optional<T> opt = properties.Lookup(key).try_as<T>();

        if (opt == std::nullopt)
        {
            return defaultValue;
        }
        else
        {
            return opt.value();
        }
    }


}