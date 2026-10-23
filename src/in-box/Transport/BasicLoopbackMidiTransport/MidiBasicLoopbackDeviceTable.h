// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once


// What listEntries reports about one loopback. The definition is the stored configuration; the
// message count is live and belongs to the device rather than to its definition, so the two are
// gathered together here instead of being folded into the definition itself.
struct MidiBasicLoopbackDeviceSnapshot
{
    MidiBasicLoopbackDeviceDefinition Definition{};

    uint64_t MessageCount{ 0 };

    // what is actually running, which is not always what the definition asked for
    bool FeedbackProtectionEnabled{ false };
    MidiBasicLoopbackFeedback::Status FeedbackStatus{};
};


// Why a create was refused before anything was asked of the service
enum class MidiBasicLoopbackReservation
{
    Reserved = 0,
    AssociationIdInUse,
    UniqueIdInUse,
    UniqueIdOverlaps,
    NameInUse,
};


class MidiBasicLoopbackDeviceTable
{
private:
    // unlike GUID, winrt::guid has built-in comparison so it can be used as a key in std::map
    std::map<winrt::guid, std::shared_ptr<MidiBasicLoopbackDevice>> m_devices;

    // A loopback between being accepted and being added. Creating one takes the service about a
    // second, and nothing stops two requests arriving together, so what it will use is held here
    // from the moment it is checked.
    struct Reservation
    {
        std::wstring CleanUniqueIdentifier{};

        // empty when names are not enforced for this create
        std::wstring CleanName{};
    };

    std::map<winrt::guid, Reservation> m_reservations;

    // guards m_devices and m_reservations. Accessed from the data path
    // (GetDeviceById via SendMidiMessage), the activation path (AddDevice),
    // and teardown (RemoveDevice / Shutdown), so it must be synchronized.
    // Taken before any device's own lock.
    wil::srwlock m_devicesLock;

    // Caller must hold m_devicesLock.
    bool IsNameTakenLocked(
        _In_ std::wstring const& cleanName,
        _In_ MidiBasicLoopbackDevice const* ignoredDevice);


public:
    std::vector<MidiBasicLoopbackDeviceSnapshot> GetDeviceListSnapshot();

    std::shared_ptr<MidiBasicLoopbackDevice> GetDevice(_In_ winrt::guid const& associationId);
    std::shared_ptr<MidiBasicLoopbackDevice> GetDeviceById(_In_ std::wstring const& endpointDeviceId);

    // Checks the association id, the unique id and, when one is given, the name against every
    // loopback, created or still being created, and holds them in the same step. Unique ids and
    // names are compared without regard to case, because the service's device ids aren't.
    // checkOverlap also refuses a unique id that starts with another one, or that another one
    // starts with, because the service removes endpoints by the start of their instance id.
    MidiBasicLoopbackReservation Reserve(
        _In_ winrt::guid const& associationId,
        _In_ std::wstring const& uniqueIdentifier,
        _In_ std::wstring const& endpointName,
        _In_ bool const checkOverlap);

    void ReleaseReservation(_In_ winrt::guid const& associationId);

    // Publishes the device and drops its reservation. Refuses, and changes nothing, if a device
    // already has this association id.
    HRESULT AddDevice(_In_ winrt::guid const& associationId, _In_ std::shared_ptr<MidiBasicLoopbackDevice> device);

    void RemoveDevice(_In_ winrt::guid const& associationId);

    // Checks the new name against every other loopback and swaps in the renamed definition in one
    // step, so two renames, or a rename and a create, can't both end up with one name.
    // HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS) when the name is taken; E_NOTFOUND when the device
    // has been shut down. previous is the definition that was replaced.
    HRESULT RenameDevice(
        _In_ std::shared_ptr<MidiBasicLoopbackDevice> const& device,
        _In_ std::wstring const& endpointName,
        _In_ std::wstring const& endpointDescription,
        _In_ std::wstring const& imageFileName,
        _Out_ std::shared_ptr<MidiBasicLoopbackDeviceDefinition const>& previous);

    HRESULT Shutdown();
};
