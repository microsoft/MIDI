
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

bool MidiEndpointIdHelperTests::ClassSetup()
{
    std::cout << "MidiEndpointIdHelperTests::ClassSetup" << std::endl;

    winrt::init_apartment();

    return m_initializer.InitializeSdkRuntime();
}

bool MidiEndpointIdHelperTests::ClassCleanup()
{
    std::cout << "MidiEndpointIdHelperTests::ClassCleanup" << std::endl;

    m_initializer.ShutdownSdkRuntime();

    winrt::uninit_apartment();

    return true;
}


void MidiEndpointIdHelperTests::TestCreateShortIdFromFullId()
{
    winrt::hstring result{};

    result = MidiEndpointDeviceIdHelper::GetShortIdFromFullId(L"\\\\?\\swd#midisrv#midiu_ksa_9447707571394916916#{e7cce071-3c03-423f-88d3-f1045d02552b}");
    std::cout << "Result: " << winrt::to_string(result) << std::endl;
    VERIFY_ARE_EQUAL(result, L"ksa_9447707571394916916");

    // this will be normalized before checking, so will return as lowercase
    result = MidiEndpointDeviceIdHelper::GetShortIdFromFullId(L"\\\\?\\SWD#MIDISRV#MIDIU_KSA_9447707571394916916#{E7CCE071-3C03-423f-88D3-F1045D02552B}");
    std::cout << "Result: " << winrt::to_string(result) << std::endl;
    VERIFY_ARE_EQUAL(result, L"ksa_9447707571394916916");

    result = MidiEndpointDeviceIdHelper::GetShortIdFromFullId(L"\\\\?\\swd#midisrv#midiu_loop_b_default_loopback_b#{e7cce071-3c03-423f-88d3-f1045d02552b}");
    std::cout << "Result: " << winrt::to_string(result) << std::endl;
    VERIFY_ARE_EQUAL(result, L"loop_b_default_loopback_b");

    result = MidiEndpointDeviceIdHelper::GetShortIdFromFullId(L"\\\\?\\swd#midisrv#midiu_ks_0600f00659b80802#{e7cce071-3c03-423f-88d3-f1045d02552b}");
    std::cout << "Result: " << winrt::to_string(result) << std::endl;
    VERIFY_ARE_EQUAL(result, L"ks_0600f00659b80802");

    // this is an invalid one
    result = MidiEndpointDeviceIdHelper::GetShortIdFromFullId(L"\\\\?\\swd#mmdevapi#midiu_ks_0600f00659b80802#{62f9c741-b25a-46ce-b54c-9bccce08b6f2}");
    std::cout << "Result: " << winrt::to_string(result) << std::endl;
    VERIFY_ARE_EQUAL(result, L"");

    result = MidiEndpointDeviceIdHelper::GetShortIdFromFullId(L"this is an invalid id");
    std::cout << "Result: " << winrt::to_string(result) << std::endl;
    VERIFY_ARE_EQUAL(result, L"");

    // also not a midisrv UMP endpoint
    result = MidiEndpointDeviceIdHelper::GetShortIdFromFullId(L"\\\\?\\SWD#MMDEVAPI#MIDII_43F4F143.BLE10#{62f9c741-b25a-46ce-b54c-9bccce08b6f2}");
    std::cout << "Result: " << winrt::to_string(result) << std::endl;
    VERIFY_ARE_EQUAL(result, L"");
}

void MidiEndpointIdHelperTests::TestCreateFullIdFromShortId()
{
    winrt::hstring result{};

    result = MidiEndpointDeviceIdHelper::GetFullIdFromShortId(L"ksa_9447707571394916916");
    VERIFY_ARE_EQUAL(result, L"\\\\?\\swd#midisrv#midiu_ksa_9447707571394916916#{e7cce071-3c03-423f-88d3-f1045d02552b}");

    result = MidiEndpointDeviceIdHelper::GetFullIdFromShortId(L"LOOP_B_DEFAULT_LOOPBACK_B");
    VERIFY_ARE_EQUAL(result, L"\\\\?\\swd#midisrv#midiu_loop_b_default_loopback_b#{e7cce071-3c03-423f-88d3-f1045d02552b}");
}

void MidiEndpointIdHelperTests::TestIsWindowsMidiServicesId()
{
    VERIFY_IS_TRUE(MidiEndpointDeviceIdHelper::IsWindowsMidiServicesEndpointDeviceId(L"\\\\?\\swd#midisrv#midiu_ksa_9447707571394916916#{e7cce071-3c03-423f-88d3-f1045d02552b}"));

    // this is an invalid one
    VERIFY_IS_FALSE(MidiEndpointDeviceIdHelper::IsWindowsMidiServicesEndpointDeviceId(L"\\\\?\\swd#mmdevapi#midiu_ks_0600f00659b80802#{62f9c741-b25a-46ce-b54c-9bccce08b6f2}"));

    // as is this
    VERIFY_IS_FALSE(MidiEndpointDeviceIdHelper::IsWindowsMidiServicesEndpointDeviceId(L"this is an invalid id"));
}

void MidiEndpointIdHelperTests::TestNormalizeId()
{
    winrt::hstring result{};

    result = MidiEndpointDeviceIdHelper::NormalizeFullId(L"\\\\?\\swd#midisrv#midiu_ksa_9447707571394916916#{e7cce071-3c03-423f-88d3-f1045d02552b}");
    VERIFY_ARE_EQUAL(result, L"\\\\?\\swd#midisrv#midiu_ksa_9447707571394916916#{e7cce071-3c03-423f-88d3-f1045d02552b}");

    result = MidiEndpointDeviceIdHelper::NormalizeFullId(L"     \\\\?\\SWD#MIDISRV#MIDIU_KSA_9447707571394916916#{E7CCE071-3C03-423f-88D3-F1045D02552B}  ");
    VERIFY_ARE_EQUAL(result, L"\\\\?\\swd#midisrv#midiu_ksa_9447707571394916916#{e7cce071-3c03-423f-88d3-f1045d02552b}");
}

