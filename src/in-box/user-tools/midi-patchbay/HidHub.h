// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "HidControllers.h"

namespace midipatchbay
{
    // Joysticks, wheels and gamepads that aren't Xbox controllers, through Raw Input, which keeps
    // reading while another app is in front. Runs its own thread once anything listens.
    class HidHub
    {
    public:
        // On the hub's thread, every few milliseconds whether anything changed or not.
        using Listener = std::function<void(uint64_t nowMilliseconds, uint64_t timestamp, HidReading const& reading)>;

        // For Learn: every connected device, every few milliseconds.
        using AnyListener = std::function<void(HidDeviceId const& device, HidReading const& reading)>;

        static HidHub& Current();

        ~HidHub();

        HidHub(HidHub const&) = delete;
        HidHub& operator=(HidHub const&) = delete;

        // Returns 0 when it can't listen.
        uint64_t Listen(_In_ HidDeviceId const& device, _In_ Listener listener) noexcept;
        uint64_t ListenToAll(_In_ AnyListener listener) noexcept;

        // Once this returns the listener is never called again, so never call it from a listener.
        void StopListening(_In_ uint64_t token) noexcept;

        // The HID game controllers connected now, on the caller's thread, in a steady order.
        static std::vector<HidDeviceInfo> Devices() noexcept;

    private:
        HidHub() noexcept;

        struct Subscription;
        struct Device;

        uint64_t Add(_In_ std::shared_ptr<Subscription> subscription) noexcept;

        static std::unique_ptr<Device> LoadDevice(_In_ HANDLE handle) noexcept;
        static void Parse(_Inout_ Device& device, _In_reads_bytes_(length) BYTE const* report, _In_ ULONG length) noexcept;

        void ThreadWorker() noexcept;
        Device* FindOrLoad(_In_ HANDLE handle) noexcept;
        void ReloadDevices() noexcept;
        void OnInput(_In_ LPARAM input) noexcept;
        void OnDeviceChange(_In_ WPARAM change, _In_ HANDLE handle) noexcept;
        void Tick(_In_ std::vector<std::shared_ptr<Subscription>> const& listening) noexcept;

        static LRESULT CALLBACK WindowProcedure(_In_ HWND window, _In_ UINT message, _In_ WPARAM wParam, _In_ LPARAM lParam);

        std::mutex m_lock{};
        std::vector<std::shared_ptr<Subscription>> m_subscriptions{};
        uint64_t m_nextToken{ 1 };

        std::thread m_thread{};
        wil::unique_event_nothrow m_wake{};
        wil::unique_event_nothrow m_stop{};

        // Only the hub's thread touches these.
        std::vector<std::unique_ptr<Device>> m_devices{};
        std::vector<HidDeviceId> m_present{};
        uint64_t m_generation{ 0 };
        std::vector<HANDLE> m_ignored{};
        std::vector<uint8_t> m_inputBuffer{};
    };
}
