// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once


//extern std::unique_ptr<MidiClientInitializer> g_clientInitializer;

//#define MIDI_CLIENT_INITIALIZER_STATIC_STORE_KEY L"Microsoft.Windows.Devices.Midi2.MidiClientInitializer"
//std::mutex g_midiClientInitializerLock{};

// SINGLETON
// using the COM static store as explained here 
// https://devblogs.microsoft.com/oldnewthing/20210210-06/?p=104839 and
// https://devblogs.microsoft.com/oldnewthing/20210215-00/?p=104865

struct MidiClientInitializerSingleton
{
    std::weak_ptr<MidiClientInitializer> weak;
    winrt::slim_mutex lock;

    const winrt::hstring name{ L"MidiClientInitializer" };

    static constexpr decltype(auto) Name()
    {
        return ;
    };

    std::shared_ptr<MidiClientInitializer> Get()
    {
        {
            std::shared_lock guard{ lock };
            if (auto shared = weak.lock()) return shared;
        }

        struct Holder : winrt::implements<Holder, foundation::IInspectable>
        {
            std::shared_ptr<MidiClientInitializer> shared = std::make_shared<MidiClientInitializer>();
        };


        auto holder = winrt::make_self<Holder>();

        winrt::slim_lock_guard guard{ lock };
        if (auto shared = weak.lock()) return shared;

        coreapp::CoreApplication::Properties().Insert(name, *holder);
        weak = holder->shared;

        //// since we created it, we need to initialize it
        //auto hr = holder->shared->Initialize();

        //if (FAILED(hr))
        //{
        //    LOG_IF_FAILED(hr);
        //    return nullptr;
        //}

        return holder->shared;
    }

    void Reset()
    {
        try
        {
            {
                std::shared_lock guard{ lock };
                if (auto shared = weak.lock())
                {
                    shared->Shutdown();
                }
            }

            // let COM know we're done with it
            winrt::Windows::ApplicationModel::Core::
                CoreApplication::Properties().Remove(name);
        }
        catch (winrt::hresult_out_of_bounds const&) 
        {
        }
    }
    
};
