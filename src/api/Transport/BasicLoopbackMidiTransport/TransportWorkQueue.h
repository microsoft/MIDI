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

        m_workItems.emplace(TransportWorkItemWorkType::Create, definition);

        return S_OK;
    }

    inline bool GetNextWorkItem(_Inout_ TransportWorkItem& workItem)
    {
        if (!m_workItems.empty())
        {
            // todo: any required locking

            auto item = m_workItems.front();

            workItem.Definition = item.Definition;
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
