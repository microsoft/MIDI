#define WINDOWSMIDISERVICES_EXPORTS


#include "WindowsMidiServicesEnumeration.h"

namespace Microsoft::Windows::Midi::Enumeration
{


	struct MidiStreamInformationCollection::impl
	{
		int TODO;
	};

	MidiStreamInformationCollection::MidiStreamInformationCollection()
	{
		_pimpl = new impl;
	}

	MidiStreamInformationCollection::~MidiStreamInformationCollection()
	{
		delete _pimpl;
	}


}

