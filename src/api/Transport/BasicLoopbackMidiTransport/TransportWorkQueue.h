// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once


enum TransportWorkItemWorkType
{
    Create,
    Update,
    Delete
};

struct TransportWorkItem
{
    TransportWorkItemWorkType Type{ TransportWorkItemWorkType::Create };
    std::shared_ptr<MidiBasicLoopbackDeviceDefinition> Definition{ nullptr };

    TransportWorkItem() { }
    TransportWorkItem(
        _In_ TransportWorkItemWorkType type,
        _In_ std::shared_ptr<MidiBasicLoopbackDeviceDefinition> definition) :
        Type(type), Definition(definition)
    { }
};



class TransportWorkQueue
{
public:
    inline HRESULT CreateEndpoint(
        _In_ std::shared_ptr<MidiBasicLoopbackDeviceDefinition> definition)
    {
        if (definition == nullptr)
        {
            return E_INVALIDARG;
        }

        auto lock = m_lock.lock_exclusive();

        m_workItems.emplace(TransportWorkItemWorkType::Create, definition);

        return S_OK;
    }

    inline bool GetNextWorkItem(_Inout_ TransportWorkItem& workItem)
    {
        auto lock = m_lock.lock_exclusive();

        if (m_workItems.empty())
        {
            return false;
        }

        workItem = m_workItems.front();
        m_workItems.pop();

        return true;
    }

    bool IsEmpty()
    {
        auto lock = m_lock.lock_shared();
        return m_workItems.empty();
    }

private:
    wil::srwlock m_lock;
    std::queue<TransportWorkItem> m_workItems{};
};
