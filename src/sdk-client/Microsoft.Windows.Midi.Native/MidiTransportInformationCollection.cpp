#define WINDOWSMIDISERVICES_EXPORTS


#include "WindowsMidiServicesEnumeration.h"

namespace Microsoft::Windows::Midi::Enumeration
{

	struct MidiTransportInformationCollection::impl
	{
		int TODO;
	};

	MidiTransportInformationCollection::MidiTransportInformationCollection()
	{
		_pimpl = new impl;
	}

	MidiTransportInformationCollection::~MidiTransportInformationCollection()
	{
		delete _pimpl;
	}

}

