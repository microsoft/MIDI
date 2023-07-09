// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

#include "pch.h"
#include <functional>
#include <iostream>

using InternalMidiInCallbackFunc = std::function<void(PVOID, UINT32, LONGLONG)>;
using InternalMidiInCallbackFuncVector = std::vector<InternalMidiInCallbackFunc>;

#define MIDIIN_CALLBACK_FUNC_DECL(n) void n (PVOID data, UINT size, LONGLONG position)



#define IMPLEMENT_MIDI_MESSAGES_RECEIVED_EVENT \
private:\
 winrt::event<winrt::Windows::Foundation::EventHandler<winrt::Windows::Devices::Midi2::MidiMessagesReceivedEventArgs>> _messagesReceivedEvent;\
public:\
 STDMETHOD(Callback)(_In_ PVOID Data, _In_ UINT Size, _In_ LONGLONG Position) override;\
 inline winrt::event_token MessagesReceived(winrt::Windows::Foundation::EventHandler<winrt::Windows::Devices::Midi2::MidiMessagesReceivedEventArgs> const& handler)\
 { return _messagesReceivedEvent.add(handler); }\
 inline void MessagesReceived(winrt::event_token const& token) noexcept\
 { _messagesReceivedEvent.remove(token); }\
 



namespace Windows::Devices::Midi2::Internal
{

	class InternalMidiDeviceConnection : public IMidiCallback
	{
	public:
		hstring DeviceId{};

		// only one of these will be valid at a time
		com_ptr<IMidiBiDi> BidirectionalConnection{ nullptr };
		com_ptr<IMidiIn> InConnection{ nullptr };
		com_ptr<IMidiOut> OutputConnection{ nullptr };

		bool _useMmcss{ true };
		DWORD _mmcssTaskId{ 0 };

		// TODO: Reference to the WinRT endpoints to fire off the event

		InternalMidiInCallbackFuncVector RegisteredListeners{};


		STDMETHOD(Callback)(_In_ PVOID Data, _In_ UINT Size, _In_ LONGLONG Position)
		{
			std::cout << __FUNCTION__ << ": message received. About to loop through registered listeners." << std::endl;

			std::for_each(RegisteredListeners.begin(),
				RegisteredListeners.end(),
				[&Data, &Size, &Position](const InternalMidiInCallbackFunc& func)
				{ 
					func(Data, Size, Position); 
				}
			);

			return S_OK;
		}

		STDMETHODIMP QueryInterface(REFIID, void**) { return S_OK; }

		// probably need to change these to actual ref counting. TBD.
		STDMETHODIMP_(ULONG) AddRef() { return 1; }
		STDMETHODIMP_(ULONG) Release() { return 1; }
	};

}