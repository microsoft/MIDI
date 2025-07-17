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

        m_workItems.emplace(TransportWorkItemWorkType::Create, definitionA, definitionB);

        return S_OK;
    }

    inline bool GetNextWorkItem(_Inout_ TransportWorkItem& workItem)
    {
        if (!m_workItems.empty())
        {
            // todo: any required locking

            auto item = m_workItems.front();

            workItem.DefinitionA = item.DefinitionA;
            workItem.DefinitionB = item.DefinitionB;
            workItem.Type = item.Type;

            m_workItems.pop();

            return true;
        }

        return false;
    }

    bool IsEmpty() { return m_workItems.empty(); }

private:
    std::queue<TransportWorkItem> m_workItems{};
};
