
#define WINDOWSMIDISERVICES_EXPORTS




#include "WindowsMidiServicesEnumeration.h"

namespace Microsoft::Windows::Midi::Enumeration
{

	struct MidiDeviceInformationCollection::impl
	{
		int TODO;
	};

	MidiDeviceInformationCollection::MidiDeviceInformationCollection()
	{
		_pimpl = new impl;
	}

	MidiDeviceInformationCollection::~MidiDeviceInformationCollection()
	{
		delete _pimpl;
	}

}









