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
#pragma warning(push)
#pragma warning(disable: 26816, justification: "stack-allocated return value is fine for inline function like this.")
    // IMap version
    template<typename T>
    __forceinline T GetDeviceInfoProperty(
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
    __forceinline T GetDeviceInfoProperty(
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
#pragma warning(pop)

}