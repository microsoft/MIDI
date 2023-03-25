#define WINDOWSMIDISERVICES_EXPORTS


#include "WindowsMidiServicesEnumeration.h"

namespace Microsoft::Windows::Midi::Enumeration
{


	struct MidiEndpointInformationCollection::implMidiEndpointmInformationCollection
	{
		int TODO;
	};

	MidiEndpointInformationCollection::MidiEndpointInformationCollection()
	{
		_pimpl = new implMidiEndpointInformationCollection;
	}

	MidiEndpointInformationCollection::~MidiEndpointInformationCollection()
	{
		delete _pimpl;
	}


}

