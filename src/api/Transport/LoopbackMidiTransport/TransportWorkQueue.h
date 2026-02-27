// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include <mutex>

enum TransportWorkItemWorkType
{
    Create,
    Update,
    Delete
};

struct TransportWorkItem
{
    TransportWorkItemWorkType Type{ TransportWorkItemWorkType::Create };
    std::shared_ptr<MidiLoopbackDeviceDefinition> DefinitionA{ nullptr };
    std::shared_ptr<MidiLoopbackDeviceDefinition> DefinitionB{ nullptr };

    TransportWorkItem() { }
    TransportWorkItem(
        _In_ TransportWorkItemWorkType type,
        _In_ std::shared_ptr<MidiLoopbackDeviceDefinition> definitionA,
        _In_ std::shared_ptr<MidiLoopbackDeviceDefinition> definitionB) :
        Type(type), DefinitionA(definitionA), DefinitionB(definitionB)
    { }
};



class TransportWorkQueue
{
public:
    inline HRESULT CreateEndpointPair(
        _In_ std::shared_ptr<MidiLoopbackDeviceDefinition> definitionA,
        _In_ std::shared_ptr<MidiLoopbackDeviceDefinition> definitionB)
    {
        if (definitionA == nullptr || definitionB == nullptr)
        {
            return E_INVALIDARG;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_workItems.emplace(TransportWorkItemWorkType::Create, definitionA, definitionB);

        return S_OK;
    }

    inline bool GetNextWorkItem(_Inout_ TransportWorkItem& workItem)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_workItems.empty())
        {
            auto item = m_workItems.front();

            workItem.DefinitionA = item.DefinitionA;
            workItem.DefinitionB = item.DefinitionB;
            workItem.Type = item.Type;

            m_workItems.pop();

            return true;
        }

        return false;
    }

    inline bool IsEmpty()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_workItems.empty();
    }

private:
    std::queue<TransportWorkItem> m_workItems{};
    std::mutex m_mutex;
};
