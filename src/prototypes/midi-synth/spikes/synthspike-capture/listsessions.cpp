// Lists active audio sessions on the default render endpoint with their current peak level.
// Used to find what is making noise before taking a reference capture.

#include <windows.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <psapi.h>

#include <cstdio>
#include <string>

namespace
{
    template <typename T>
    struct ComPtr
    {
        T* Pointer{ nullptr };
        ~ComPtr() { if (Pointer != nullptr) { Pointer->Release(); } }
        T** operator&() noexcept { return &Pointer; }
        T* operator->() const noexcept { return Pointer; }
        explicit operator bool() const noexcept { return Pointer != nullptr; }
    };

    std::wstring ProcessNameFromId(_In_ DWORD processId)
    {
        if (processId == 0)
        {
            return L"(system)";
        }

        const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);

        if (process == nullptr)
        {
            return L"(unknown)";
        }

        wchar_t path[MAX_PATH]{};
        DWORD size = ARRAYSIZE(path);
        std::wstring name = L"(unknown)";

        if (QueryFullProcessImageNameW(process, 0, path, &size))
        {
            const std::wstring full(path);
            const auto slash = full.find_last_of(L'\\');
            name = (slash == std::wstring::npos) ? full : full.substr(slash + 1);
        }

        CloseHandle(process);
        return name;
    }
}

int wmain()
{
    SetConsoleOutputCP(CP_UTF8);
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice> device;
    ComPtr<IAudioSessionManager2> sessionManager;
    ComPtr<IAudioSessionEnumerator> sessions;

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));

    if (SUCCEEDED(hr)) { hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device); }
    if (SUCCEEDED(hr)) { hr = device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&sessionManager)); }
    if (SUCCEEDED(hr)) { hr = sessionManager->GetSessionEnumerator(&sessions); }

    if (FAILED(hr))
    {
        printf("could not enumerate audio sessions: 0x%08X\n", static_cast<unsigned>(hr));
        CoUninitialize();
        return 1;
    }

    int count = 0;
    sessions->GetCount(&count);

    // Any claim about absolute capture level is only valid if these are unity.
    {
        ComPtr<IAudioEndpointVolume> endpointVolume;

        if (SUCCEEDED(device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
            reinterpret_cast<void**>(&endpointVolume))))
        {
            float scalar = 0.0f;
            float decibels = 0.0f;
            BOOL muted = FALSE;

            endpointVolume->GetMasterVolumeLevelScalar(&scalar);
            endpointVolume->GetMasterVolumeLevel(&decibels);
            endpointVolume->GetMute(&muted);

            DWORD hardwareSupport = 0;
            endpointVolume->QueryHardwareSupport(&hardwareSupport);

            printf("Endpoint master volume: %.1f %% (%.2f dB)%s\n",
                scalar * 100.0f, decibels, muted ? "  MUTED" : "");
            printf("  volume implemented in: %s\n\n",
                (hardwareSupport & ENDPOINT_HARDWARE_SUPPORT_VOLUME) ? "hardware" : "software (audio engine)");
        }
    }

    printf("Audio sessions on the default render endpoint: %d\n\n", count);
    printf("  %-10s %-28s %-10s %s\n", "state", "process", "peak", "display name");

    for (int i = 0; i < count; i++)
    {
        ComPtr<IAudioSessionControl> control;

        if (FAILED(sessions->GetSession(i, &control)))
        {
            continue;
        }

        AudioSessionState state = AudioSessionStateInactive;
        control->GetState(&state);

        ComPtr<IAudioSessionControl2> control2;
        DWORD processId = 0;

        if (SUCCEEDED(control.Pointer->QueryInterface(IID_PPV_ARGS(&control2))))
        {
            control2->GetProcessId(&processId);
        }

        ComPtr<IAudioMeterInformation> meter;
        float peak = 0.0f;

        if (SUCCEEDED(control.Pointer->QueryInterface(IID_PPV_ARGS(&meter))))
        {
            meter->GetPeakValue(&peak);
        }

        LPWSTR displayName = nullptr;
        control->GetDisplayName(&displayName);

        const char* stateText =
            (state == AudioSessionStateActive) ? "ACTIVE" :
            (state == AudioSessionStateInactive) ? "inactive" : "expired";

        char peakText[16]{};

        if (peak > 0.0f)
        {
            (void)snprintf(peakText, sizeof(peakText), "%.1f dB", 20.0 * log10(static_cast<double>(peak)));
        }
        else
        {
            (void)snprintf(peakText, sizeof(peakText), "silent");
        }

        printf("  %-10s %-28ws %-10s %ws\n",
            stateText, ProcessNameFromId(processId).c_str(), peakText,
            (displayName != nullptr && displayName[0] != L'\0') ? displayName : L"");

        if (displayName != nullptr)
        {
            CoTaskMemFree(displayName);
        }
    }

    CoUninitialize();
    return 0;
}
