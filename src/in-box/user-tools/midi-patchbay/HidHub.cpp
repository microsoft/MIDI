// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "HidHub.h"

#include <hidsdi.h>
#include <midi_timestamp.h>

#include <cwctype>

namespace internal = ::WindowsMidiServicesInternal;

namespace midipatchbay
{
    namespace
    {
        // Every 4 ms, in units of 100 ns, the same as the Xbox controllers.
        constexpr int64_t TickIntervalHundredNanoseconds = 40000;
        constexpr DWORD TickIntervalMilliseconds = 4;

        constexpr wchar_t WindowClassName[] = L"MidiPatchbayHidControllers";

        constexpr USHORT ControllerUsages[] = { HidUsageJoystick, HidUsageGamepad, HidUsageMultiAxis };

        bool IsControllerUsage(_In_ USHORT page, _In_ USHORT usage) noexcept
        {
            return page == HidPageGenericDesktop &&
                std::find(std::begin(ControllerUsages), std::end(ControllerUsages), usage) != std::end(ControllerUsages);
        }

        std::wstring DevicePath(_In_ HANDLE handle)
        {
            UINT length{ 0 };

            if (::GetRawInputDeviceInfoW(handle, RIDI_DEVICENAME, nullptr, &length) != 0 || length == 0)
            {
                return {};
            }

            std::wstring path(length, L'\0');

            if (::GetRawInputDeviceInfoW(handle, RIDI_DEVICENAME, path.data(), &length) == static_cast<UINT>(-1))
            {
                return {};
            }

            path.resize(::wcsnlen(path.c_str(), path.size()));

            // Older versions of Windows give the NT form, which CreateFile doesn't take.
            if (path.rfind(L"\\??\\", 0) == 0)
            {
                path[1] = L'\\';
            }

            return path;
        }

        // A USB device is asked for its name over the wire each time, and the status line asks
        // twice a second, so each device is asked once.
        std::wstring ProductName(_In_ std::wstring const& path)
        {
            static std::mutex lock{};
            static std::vector<std::pair<std::wstring, std::wstring>> names{};

            {
                std::lock_guard guard{ lock };

                for (auto const& [known, name] : names)
                {
                    if (known == path)
                    {
                        return name;
                    }
                }
            }

            // No access asked for, so a device another app has open still says its name.
            wil::unique_hfile file{ ::CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr) };

            if (!file)
            {
                return {};
            }

            wchar_t product[256]{};
            std::wstring name{};

            if (::HidD_GetProductString(file.get(), product, sizeof(product) - sizeof(wchar_t)))
            {
                name = product;
            }

            while (!name.empty() && std::iswspace(name.back()))
            {
                name.pop_back();
            }

            std::lock_guard guard{ lock };
            names.emplace_back(path, name);

            return name;
        }

        bool PumpMessages() noexcept
        {
            MSG message{};

            while (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
            {
                if (message.message == WM_QUIT)
                {
                    return false;
                }

                ::TranslateMessage(&message);
                ::DispatchMessageW(&message);
            }

            return true;
        }

        // Sends game controller input to the window, or stops it.
        bool RegisterForControllers(_In_ HWND window, _In_ bool on) noexcept
        {
            RAWINPUTDEVICE devices[std::size(ControllerUsages)]{};

            for (size_t i = 0; i < std::size(ControllerUsages); i++)
            {
                devices[i].usUsagePage = HidPageGenericDesktop;
                devices[i].usUsage = ControllerUsages[i];
                devices[i].dwFlags = on ? (RIDEV_INPUTSINK | RIDEV_DEVNOTIFY) : RIDEV_REMOVE;
                devices[i].hwndTarget = on ? window : nullptr;
            }

            return ::RegisterRawInputDevices(devices, static_cast<UINT>(std::size(devices)), sizeof(RAWINPUTDEVICE)) != FALSE;
        }
    }

    struct HidHub::Subscription
    {
        uint64_t Token{ 0 };
        bool All{ false };
        HidDeviceId Wanted{};
        Listener Callback{};
        AnyListener AllCallback{};

        // Which device it reads, worked out again whenever devices come and go.
        int32_t Index{ -1 };
        uint64_t Generation{ UINT64_MAX };

        std::mutex CallbackLock{};
        bool Active{ true };
    };

    struct HidHub::Device
    {
        enum class ItemKind : uint8_t
        {
            Button,
            Axis,
            Hat,
        };

        struct Item
        {
            ItemKind Kind{ ItemKind::Button };

            // The button's bit, or the axis.
            uint16_t Index{ 0 };

            UCHAR ReportId{ 0 };
            LONG LogicalMinimum{ 0 };
            LONG LogicalMaximum{ 0 };
            USHORT BitSize{ 0 };
            double Step{ 0.0 };
        };

        HANDLE Handle{ nullptr };
        HidDeviceInfo Info{};
        std::vector<uint8_t> Preparsed{};
        bool UsesReportIds{ false };

        // By data index, which every input in a report has.
        std::vector<std::optional<Item>> Items{};
        std::vector<HIDP_DATA> Data{};

        HidReading Reading{};

        // Which way each axis last moved.
        std::array<int8_t, HidAxisCount> Directions{};

        PHIDP_PREPARSED_DATA Parsed() noexcept { return reinterpret_cast<PHIDP_PREPARSED_DATA>(Preparsed.data()); }
    };

    HidHub::HidHub() noexcept
    {
        (void)m_wake.create(wil::EventOptions::None);
        (void)m_stop.create(wil::EventOptions::ManualReset);
    }

    HidHub& HidHub::Current()
    {
        static HidHub hub{};
        return hub;
    }

    HidHub::~HidHub()
    {
        if (m_stop)
        {
            m_stop.SetEvent();
        }

        if (m_thread.joinable())
        {
            m_thread.join();
        }
    }

    _Use_decl_annotations_
    uint64_t HidHub::Listen(HidDeviceId const& device, Listener listener) noexcept
    {
        try
        {
            if (listener == nullptr || device.IsEmpty())
            {
                return 0;
            }

            auto subscription = std::make_shared<Subscription>();
            subscription->Wanted = device;
            subscription->Callback = std::move(listener);

            return Add(std::move(subscription));
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to listen to a game controller.")

        return 0;
    }

    _Use_decl_annotations_
    uint64_t HidHub::ListenToAll(AnyListener listener) noexcept
    {
        try
        {
            if (listener == nullptr)
            {
                return 0;
            }

            auto subscription = std::make_shared<Subscription>();
            subscription->All = true;
            subscription->AllCallback = std::move(listener);

            return Add(std::move(subscription));
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to listen to game controllers.")

        return 0;
    }

    _Use_decl_annotations_
    uint64_t HidHub::Add(std::shared_ptr<Subscription> subscription) noexcept
    {
        try
        {
            if (!m_wake || !m_stop)
            {
                return 0;
            }

            std::lock_guard lock{ m_lock };

            if (!m_thread.joinable())
            {
                m_thread = std::thread([this]() { ThreadWorker(); });
            }

            subscription->Token = m_nextToken++;
            m_subscriptions.push_back(std::move(subscription));

            m_wake.SetEvent();

            return m_subscriptions.back()->Token;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to listen to game controllers.")

        return 0;
    }

    _Use_decl_annotations_
    void HidHub::StopListening(uint64_t token) noexcept
    {
        if (token == 0)
        {
            return;
        }

        std::shared_ptr<Subscription> found{};

        {
            std::lock_guard lock{ m_lock };

            auto const it = std::find_if(m_subscriptions.begin(), m_subscriptions.end(),
                [token](std::shared_ptr<Subscription> const& subscription) { return subscription->Token == token; });

            if (it != m_subscriptions.end())
            {
                found = *it;
                m_subscriptions.erase(it);
            }
        }

        if (found != nullptr)
        {
            std::lock_guard lock{ found->CallbackLock };
            found->Active = false;
        }
    }

    std::vector<HidDeviceInfo> HidHub::Devices() noexcept
    {
        std::vector<HidDeviceInfo> found{};

        try
        {
            std::vector<RAWINPUTDEVICELIST> list{};

            // A device can arrive between asking how many there are and asking for them.
            for (int attempt = 0; attempt < 3; attempt++)
            {
                UINT count{ 0 };

                if (::GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) != 0)
                {
                    return found;
                }

                list.resize(count);

                auto const got = ::GetRawInputDeviceList(list.data(), &count, sizeof(RAWINPUTDEVICELIST));

                if (got != static_cast<UINT>(-1))
                {
                    list.resize(got);
                    break;
                }

                list.clear();
            }

            for (auto const& item : list)
            {
                if (item.dwType != RIM_TYPEHID)
                {
                    continue;
                }

                if (auto device = LoadDevice(item.hDevice))
                {
                    found.push_back(device->Info);
                }
            }

            std::sort(found.begin(), found.end(),
                [](HidDeviceInfo const& a, HidDeviceInfo const& b) { return a.Id.Path < b.Id.Path; });
        }
        catch (...)
        {
            found.clear();
        }

        return found;
    }

    _Use_decl_annotations_
    std::unique_ptr<HidHub::Device> HidHub::LoadDevice(HANDLE handle) noexcept
    {
        try
        {
            RID_DEVICE_INFO info{};
            info.cbSize = sizeof(info);

            UINT size = sizeof(info);

            if (::GetRawInputDeviceInfoW(handle, RIDI_DEVICEINFO, &info, &size) == static_cast<UINT>(-1) ||
                info.dwType != RIM_TYPEHID ||
                !IsControllerUsage(info.hid.usUsagePage, info.hid.usUsage))
            {
                return nullptr;
            }

            auto device = std::make_unique<Device>();
            auto& id = device->Info.Id;

            device->Handle = handle;
            id.Path = DevicePath(handle);

            if (id.Path.empty() || IsXInputDevicePath(id.Path))
            {
                return nullptr;
            }

            id.VendorId = static_cast<uint16_t>(info.hid.dwVendorId);
            id.ProductId = static_cast<uint16_t>(info.hid.dwProductId);
            id.Name = ProductName(id.Path);

            size = 0;

            if (::GetRawInputDeviceInfoW(handle, RIDI_PREPARSEDDATA, nullptr, &size) != 0 || size == 0)
            {
                return nullptr;
            }

            device->Preparsed.resize(size);

            if (::GetRawInputDeviceInfoW(handle, RIDI_PREPARSEDDATA, device->Preparsed.data(), &size) == static_cast<UINT>(-1))
            {
                return nullptr;
            }

            auto const parsed = device->Parsed();

            HIDP_CAPS caps{};

            if (::HidP_GetCaps(parsed, &caps) != HIDP_STATUS_SUCCESS)
            {
                return nullptr;
            }

            device->Items.resize(caps.NumberInputDataIndices);

            auto const place = [&device](USHORT index, Device::Item const& item)
                {
                    if (index < device->Items.size())
                    {
                        device->Items[index] = item;
                        device->UsesReportIds = device->UsesReportIds || item.ReportId != 0;
                    }
                };

            USHORT buttonCapCount = caps.NumberInputButtonCaps;
            std::vector<HIDP_BUTTON_CAPS> buttonCaps(buttonCapCount);

            if (buttonCapCount != 0 && ::HidP_GetButtonCaps(HidP_Input, buttonCaps.data(), &buttonCapCount, parsed) == HIDP_STATUS_SUCCESS)
            {
                buttonCaps.resize(buttonCapCount);

                for (auto const& cap : buttonCaps)
                {
                    if (cap.UsagePage != HidPageButton)
                    {
                        continue;
                    }

                    uint32_t const firstUsage = cap.IsRange ? cap.Range.UsageMin : cap.NotRange.Usage;
                    uint32_t const lastUsage = cap.IsRange ? cap.Range.UsageMax : cap.NotRange.Usage;
                    uint32_t const firstIndex = cap.IsRange ? cap.Range.DataIndexMin : cap.NotRange.DataIndex;

                    for (uint32_t usage = (std::max)(firstUsage, 1u); usage <= lastUsage && usage <= HidMaximumButtons; usage++)
                    {
                        place(static_cast<USHORT>(firstIndex + (usage - firstUsage)),
                            Device::Item{ Device::ItemKind::Button, static_cast<uint16_t>(usage - 1), cap.ReportID, 0, 1, 1 });

                        device->Info.ButtonCount = (std::max)(device->Info.ButtonCount, static_cast<uint16_t>(usage));
                    }
                }
            }

            USHORT valueCapCount = caps.NumberInputValueCaps;
            std::vector<HIDP_VALUE_CAPS> valueCaps(valueCapCount);

            if (valueCapCount != 0 && ::HidP_GetValueCaps(HidP_Input, valueCaps.data(), &valueCapCount, parsed) == HIDP_STATUS_SUCCESS)
            {
                valueCaps.resize(valueCapCount);

                for (auto const& cap : valueCaps)
                {
                    // A relative value, such as a mouse wheel's, has no place to rest or end.
                    if (!cap.IsAbsolute || cap.LogicalMin == cap.LogicalMax)
                    {
                        continue;
                    }

                    uint32_t const firstUsage = cap.IsRange ? cap.Range.UsageMin : cap.NotRange.Usage;
                    uint32_t const lastUsage = cap.IsRange ? cap.Range.UsageMax : cap.NotRange.Usage;
                    uint32_t const firstIndex = cap.IsRange ? cap.Range.DataIndexMin : cap.NotRange.DataIndex;

                    for (uint32_t usage = firstUsage; usage <= lastUsage && usage <= 0xFFFF; usage++)
                    {
                        auto const index = static_cast<USHORT>(firstIndex + (usage - firstUsage));

                        if (cap.UsagePage == HidPageGenericDesktop && usage == HidUsageHatSwitch)
                        {
                            if (!device->Info.HasHat)
                            {
                                place(index, Device::Item{ Device::ItemKind::Hat, 0, cap.ReportID, cap.LogicalMin, cap.LogicalMax, cap.BitSize });
                                device->Info.HasHat = true;
                            }

                            continue;
                        }

                        auto const second = cap.UsagePage == HidPageGenericDesktop &&
                            usage == HidAxisUsage(HidAxis::Slider).Usage &&
                            device->Info.Axes.test(static_cast<size_t>(HidAxis::Slider));
                        auto const axis = HidAxisFromUsage(cap.UsagePage, static_cast<uint16_t>(usage), second);

                        if (!axis.has_value() || device->Info.Axes.test(static_cast<size_t>(*axis)))
                        {
                            continue;
                        }

                        place(index, Device::Item{ Device::ItemKind::Axis, static_cast<uint16_t>(*axis), cap.ReportID, cap.LogicalMin, cap.LogicalMax, cap.BitSize,
                            HidValueStep(cap.LogicalMin, cap.LogicalMax, cap.BitSize) });
                        device->Info.Axes.set(static_cast<size_t>(*axis));
                    }
                }
            }

            device->Data.resize(::HidP_MaxDataListLength(HidP_Input, parsed));

            // Axes stay NaN until the first report says where they are.
            device->Reading.Connected = true;

            return device;
        }
        catch (...)
        {
        }

        return nullptr;
    }

    _Use_decl_annotations_
    void HidHub::Parse(Device& device, BYTE const* report, ULONG length) noexcept
    {
        if (device.Data.empty() || length == 0)
        {
            return;
        }

        auto count = static_cast<ULONG>(device.Data.size());

        if (::HidP_GetData(HidP_Input, device.Data.data(), &count, device.Parsed(),
            reinterpret_cast<PCHAR>(const_cast<BYTE*>(report)), length) != HIDP_STATUS_SUCCESS)
        {
            return;
        }

        auto& reading = device.Reading;
        UCHAR const reportId = device.UsesReportIds ? report[0] : 0;

        // A report lists only the buttons in it that are down.
        for (auto const& item : device.Items)
        {
            if (item.has_value() && item->Kind == Device::ItemKind::Button && item->ReportId == reportId)
            {
                reading.Buttons.reset(item->Index);
            }
        }

        for (ULONG i = 0; i < count && i < device.Data.size(); i++)
        {
            auto const& data = device.Data[i];

            if (data.DataIndex >= device.Items.size() || !device.Items[data.DataIndex].has_value())
            {
                continue;
            }

            auto const& item = *device.Items[data.DataIndex];

            switch (item.Kind)
            {
            case Device::ItemKind::Button:
                if (data.On)
                {
                    reading.Buttons.set(item.Index);
                }
                break;

            case Device::ItemKind::Axis:
                reading.Axes[item.Index] = SteadyHidValue(reading.Axes[item.Index],
                    NormalizeHidValue(data.RawValue, item.LogicalMinimum, item.LogicalMaximum, item.BitSize),
                    item.Step, device.Directions[item.Index]);
                break;

            case Device::ItemKind::Hat:
                reading.Hat = HatPosition(data.RawValue, item.LogicalMinimum, item.LogicalMaximum);
                break;
            }
        }
    }

    _Use_decl_annotations_
    HidHub::Device* HidHub::FindOrLoad(HANDLE handle) noexcept
    {
        try
        {
            for (auto const& device : m_devices)
            {
                if (device->Handle == handle)
                {
                    return device.get();
                }
            }

            if (std::find(m_ignored.begin(), m_ignored.end(), handle) != m_ignored.end())
            {
                return nullptr;
            }

            auto device = LoadDevice(handle);

            if (device == nullptr)
            {
                m_ignored.push_back(handle);
                return nullptr;
            }

            m_present.push_back(device->Info.Id);
            m_devices.push_back(std::move(device));
            m_generation++;

            return m_devices.back().get();
        }
        catch (...)
        {
        }

        return nullptr;
    }

    void HidHub::ReloadDevices() noexcept
    {
        try
        {
            m_devices.clear();
            m_present.clear();
            m_ignored.clear();
            m_generation++;

            UINT count{ 0 };

            if (::GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) != 0 || count == 0)
            {
                return;
            }

            std::vector<RAWINPUTDEVICELIST> list(count);
            auto const got = ::GetRawInputDeviceList(list.data(), &count, sizeof(RAWINPUTDEVICELIST));

            for (UINT i = 0; got != static_cast<UINT>(-1) && i < got; i++)
            {
                if (list[i].dwType == RIM_TYPEHID)
                {
                    (void)FindOrLoad(list[i].hDevice);
                }
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void HidHub::OnInput(LPARAM input) noexcept
    {
        try
        {
            UINT size{ 0 };

            if (::GetRawInputData(reinterpret_cast<HRAWINPUT>(input), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0 ||
                size < sizeof(RAWINPUTHEADER))
            {
                return;
            }

            if (m_inputBuffer.size() < size)
            {
                m_inputBuffer.resize(size);
            }

            if (::GetRawInputData(reinterpret_cast<HRAWINPUT>(input), RID_INPUT, m_inputBuffer.data(), &size, sizeof(RAWINPUTHEADER)) != size)
            {
                return;
            }

            auto const* raw = reinterpret_cast<RAWINPUT const*>(m_inputBuffer.data());

            if (raw->header.dwType != RIM_TYPEHID || size < offsetof(RAWINPUT, data.hid.bRawData))
            {
                return;
            }

            auto* device = FindOrLoad(raw->header.hDevice);

            if (device == nullptr)
            {
                return;
            }

            auto const& hid = raw->data.hid;
            auto const available = static_cast<uint64_t>(size) - offsetof(RAWINPUT, data.hid.bRawData);

            if (hid.dwSizeHid == 0 || static_cast<uint64_t>(hid.dwSizeHid) * hid.dwCount > available)
            {
                return;
            }

            for (DWORD i = 0; i < hid.dwCount; i++)
            {
                Parse(*device, hid.bRawData + static_cast<size_t>(i) * hid.dwSizeHid, hid.dwSizeHid);
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void HidHub::OnDeviceChange(WPARAM change, HANDLE handle) noexcept
    {
        try
        {
            if (change == GIDC_ARRIVAL)
            {
                (void)FindOrLoad(handle);
                return;
            }

            if (change != GIDC_REMOVAL)
            {
                return;
            }

            std::erase(m_ignored, handle);

            auto const it = std::find_if(m_devices.begin(), m_devices.end(),
                [handle](std::unique_ptr<Device> const& device) { return device->Handle == handle; });

            if (it != m_devices.end())
            {
                m_present.erase(m_present.begin() + (it - m_devices.begin()));
                m_devices.erase(it);
                m_generation++;
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void HidHub::Tick(std::vector<std::shared_ptr<Subscription>> const& listening) noexcept
    {
        auto const frequency = internal::GetMidiTimestampFrequency();
        auto const timestamp = internal::GetCurrentMidiTimestamp();
        auto const now = frequency == 0 ? 0 : internal::ConvertTimestampToWholeMilliseconds(timestamp, frequency);

        HidReading const missing{};

        for (auto const& subscription : listening)
        {
            std::lock_guard lock{ subscription->CallbackLock };

            if (!subscription->Active)
            {
                continue;
            }

            // One listener's trouble must not stop the others hearing anything.
            try
            {
                if (subscription->All)
                {
                    for (auto const& device : m_devices)
                    {
                        subscription->AllCallback(device->Info.Id, device->Reading);
                    }

                    continue;
                }

                if (subscription->Generation != m_generation)
                {
                    subscription->Index = ChooseHidDevice(m_present, subscription->Wanted);
                    subscription->Generation = m_generation;
                }

                auto const index = subscription->Index;

                subscription->Callback(now, timestamp,
                    index >= 0 && static_cast<size_t>(index) < m_devices.size() ? m_devices[static_cast<size_t>(index)]->Reading : missing);
            }
            catch (...)
            {
                MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(L"A game controller listener failed.");
            }
        }
    }

    void HidHub::ThreadWorker() noexcept
    {
        HWND window{ nullptr };
        auto const instance = ::GetModuleHandleW(nullptr);
        bool registered{ false };

        try
        {
            ::SetThreadDescription(::GetCurrentThread(), L"Patchbay HID game controllers");
            ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

            WNDCLASSEXW windowClass{};
            windowClass.cbSize = sizeof(windowClass);
            windowClass.lpfnWndProc = WindowProcedure;
            windowClass.hInstance = instance;
            windowClass.lpszClassName = WindowClassName;

            (void)::RegisterClassExW(&windowClass);

            // Never shown. Raw Input from the background needs a window, and a message-only one
            // doesn't get it.
            window = ::CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, WindowClassName, L"", WS_POPUP,
                0, 0, 0, 0, nullptr, nullptr, instance, this);

            if (window == nullptr)
            {
                MIDI_PATCHBAY_LOG_WARNING(L"Unable to make the window that reads game controllers.");
                return;
            }

            wil::unique_handle timer{ ::CreateWaitableTimerExW(nullptr, nullptr,
                CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS) };

            std::vector<std::shared_ptr<Subscription>> listening{};
            bool triedToRegister{ false };
            bool stopping{ false };

            while (!stopping)
            {
                {
                    std::lock_guard lock{ m_lock };
                    listening = m_subscriptions;
                }

                if (listening.empty())
                {
                    // Input stops while nothing listens, so a gamepad that reports all the time
                    // doesn't keep waking this thread.
                    if (registered)
                    {
                        (void)RegisterForControllers(window, false);
                        registered = false;
                    }

                    triedToRegister = false;

                    HANDLE idle[] = { m_stop.get(), m_wake.get() };
                    auto const result = ::MsgWaitForMultipleObjectsEx(2, idle, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);

                    stopping = result == WAIT_OBJECT_0 || result == WAIT_FAILED ||
                        (result == WAIT_OBJECT_0 + 2 && !PumpMessages());
                    continue;
                }

                if (!registered && !triedToRegister)
                {
                    triedToRegister = true;
                    registered = RegisterForControllers(window, true);

                    if (!registered)
                    {
                        MIDI_PATCHBAY_LOG_WARNING(L"Unable to register for game controller input.");
                    }

                    // Nothing says which controllers came or went while input was stopped.
                    ReloadDevices();
                }

                Tick(listening);
                listening.clear();

                if (timer)
                {
                    LARGE_INTEGER due{};
                    due.QuadPart = -TickIntervalHundredNanoseconds;
                    ::SetWaitableTimer(timer.get(), &due, 0, nullptr, nullptr, FALSE);
                }

                // Input that arrives before the next tick is read now, and goes out at the tick.
                while (true)
                {
                    HANDLE waits[] = { m_stop.get(), timer ? timer.get() : m_wake.get() };
                    auto const result = ::MsgWaitForMultipleObjectsEx(2, waits, timer ? INFINITE : TickIntervalMilliseconds,
                        QS_ALLINPUT, MWMO_INPUTAVAILABLE);

                    if (result == WAIT_OBJECT_0 || result == WAIT_FAILED)
                    {
                        stopping = true;
                        break;
                    }

                    if (result == WAIT_OBJECT_0 + 2)
                    {
                        if (!PumpMessages())
                        {
                            stopping = true;
                            break;
                        }

                        continue;
                    }

                    break;
                }
            }
        }
        catch (...)
        {
            MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(L"Game controllers stopped being read.");
        }

        if (registered)
        {
            (void)RegisterForControllers(window, false);
        }

        if (window != nullptr)
        {
            ::DestroyWindow(window);
        }

        (void)::UnregisterClassW(WindowClassName, instance);
    }

    _Use_decl_annotations_
    LRESULT CALLBACK HidHub::WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_NCCREATE)
        {
            auto const* create = reinterpret_cast<CREATESTRUCTW const*>(lParam);
            ::SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        }

        auto* hub = reinterpret_cast<HidHub*>(::GetWindowLongPtrW(window, GWLP_USERDATA));

        if (hub != nullptr)
        {
            if (message == WM_INPUT)
            {
                hub->OnInput(lParam);
            }
            else if (message == WM_INPUT_DEVICE_CHANGE)
            {
                hub->OnDeviceChange(wParam, reinterpret_cast<HANDLE>(lParam));
                return 0;
            }
        }

        // WM_INPUT has to reach DefWindowProc, which cleans up after it.
        return ::DefWindowProcW(window, message, wParam, lParam);
    }
}
