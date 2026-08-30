// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "ComponentSignatureCacheTests.h"

#include <midi_utils.h>
#include <Feature_Servicing_MIDI2ComponentSignatureCache.h>

#include <thread>
#include <vector>

// midi_utils.h calls into WinTrust and the catalog APIs; the service and transports already
// link this, the test project otherwise has no reason to.
#pragma comment(lib, "wintrust.lib")

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    // The cache is keyed on file paths, so these exercise it through the path-based entry point.
    // That deliberately bypasses the developer mode short circuit in the GUID entry point, which
    // would otherwise make every one of these a no-op on a development machine.
    std::wstring SystemFilePath(std::wstring const& fileName)
    {
        wchar_t systemDirectory[MAX_PATH]{ 0 };

        if (0 == GetSystemDirectoryW(systemDirectory, ARRAYSIZE(systemDirectory)))
        {
            return L"";
        }

        return std::wstring{ systemDirectory } + L"\\" + fileName;
    }

    bool FileExists(std::wstring const& path)
    {
        return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
    }
}

#define SKIP_IF_KIR_DISABLED() \
    if (!Feature_Servicing_MIDI2ComponentSignatureCache::IsEnabled()) \
    { \
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2ComponentSignatureCache is disabled."); \
        return; \
    }


void ComponentSignatureCacheTests::TestSignedSystemFileIsPermitted()
{
    SKIP_IF_KIR_DISABLED();

    auto path = SystemFilePath(L"kernel32.dll");

    VERIFY_IS_FALSE(path.empty());

    VERIFY_SUCCEEDED(internal::IsFilePermittedWithCaching(path));
}

void ComponentSignatureCacheTests::TestRepeatedCallsAgreeWithFirstResult()
{
    SKIP_IF_KIR_DISABLED();

    auto path = SystemFilePath(L"kernel32.dll");

    // The second and later calls are served from the cache. They have to produce the same
    // answer as the verification that populated it.
    auto first = internal::IsFilePermittedWithCaching(path);

    for (int i = 0; i < 10; i++)
    {
        VERIFY_ARE_EQUAL(first, internal::IsFilePermittedWithCaching(path));
    }

    VERIFY_SUCCEEDED(first);
}

void ComponentSignatureCacheTests::TestCachedResultMatchesUncachedResult()
{
    SKIP_IF_KIR_DISABLED();

    // A GUID which is not a registered CLSID. Both entry points must treat it identically,
    // whichever way developer mode happens to be set on this machine.
    GUID unregistered{};

    VERIFY_SUCCEEDED(CoCreateGuid(&unregistered));

    auto uncached = internal::IsComponentPermitted(unregistered);
    auto cached = internal::IsComponentPermittedWithCaching(unregistered);

    VERIFY_ARE_EQUAL(SUCCEEDED(uncached), SUCCEEDED(cached));

    // and repeating it does not change the verdict
    VERIFY_ARE_EQUAL(SUCCEEDED(uncached), SUCCEEDED(internal::IsComponentPermittedWithCaching(unregistered)));
}

void ComponentSignatureCacheTests::TestVerifiedFileIsPinned()
{
    SKIP_IF_KIR_DISABLED();

    // Something signed but not normally loaded into a test host, so the module handle check
    // below actually demonstrates that this code pinned it.
    auto path = SystemFilePath(L"msdmo.dll");

    if (path.empty() || !FileExists(path))
    {
        Log::Result(TestResults::Skipped, L"msdmo.dll is not present on this machine.");
        return;
    }

    bool alreadyLoaded = (GetModuleHandleW(L"msdmo.dll") != nullptr);

    VERIFY_SUCCEEDED(internal::IsFilePermittedWithCaching(path));

    // Pinning is what makes a cached verdict safe to reuse, so its absence is a correctness
    // failure and not just a lost optimization.
    VERIFY_IS_NOT_NULL(GetModuleHandleW(L"msdmo.dll"));

    if (alreadyLoaded)
    {
        Log::Comment(L"msdmo.dll was already loaded, so this only confirms it is still present.");
    }
}

void ComponentSignatureCacheTests::TestMissingFileIsRejectedAndNotCached()
{
    SKIP_IF_KIR_DISABLED();

    auto path = SystemFilePath(L"midi2.no.such.file.for.testing.dll");

    VERIFY_FAILED(internal::IsFilePermittedWithCaching(path));

    // Failures must never be remembered as successes. A file can fail verification because it
    // is being written at that moment, and a sticky failure or a sticky success would both be
    // wrong on the next call.
    VERIFY_FAILED(internal::IsFilePermittedWithCaching(path));
    VERIFY_FAILED(internal::IsFilePermittedWithCaching(path));
}

void ComponentSignatureCacheTests::TestUnregisteredClsidIsRejected()
{
    SKIP_IF_KIR_DISABLED();

    GUID unregistered{};

    VERIFY_SUCCEEDED(CoCreateGuid(&unregistered));

    if (internal::IsDeveloperModeEnabled())
    {
        // developer mode intentionally permits everything, including components that do not exist
        VERIFY_SUCCEEDED(internal::IsComponentPermittedWithCaching(unregistered));

        Log::Comment(L"Developer mode is enabled on this machine, so unsigned components are permitted.");
    }
    else
    {
        VERIFY_FAILED(internal::IsComponentPermittedWithCaching(unregistered));
    }
}

void ComponentSignatureCacheTests::TestConcurrentCallersAllSucceed()
{
    SKIP_IF_KIR_DISABLED();

    // Client connections are built on RPC threads, so several callers can race to populate the
    // same entry.
    auto path = SystemFilePath(L"kernel32.dll");

    const int threadCount = 8;
    const int iterations = 50;

    std::vector<std::thread> threads;
    std::vector<HRESULT> results(threadCount, E_UNEXPECTED);

    for (int i = 0; i < threadCount; i++)
    {
        threads.emplace_back([&results, &path, i, iterations]()
            {
                HRESULT worst = S_OK;

                for (int j = 0; j < iterations; j++)
                {
                    auto hr = internal::IsFilePermittedWithCaching(path);

                    if (FAILED(hr))
                    {
                        worst = hr;
                    }
                }

                results[i] = worst;
            });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    for (auto const& result : results)
    {
        VERIFY_SUCCEEDED(result);
    }
}
