#define WINDOWSMIDISERVICES_EXPORTS


#include "WindowsMidiServicesEnumeration.h"

namespace Microsoft::Windows::Midi::Enumeration
{

	struct MidiTransportInformationCollection::implMidiTransportInformationCollection
	{
		int TODO;
	};

	MidiTransportInformationCollection::MidiTransportInformationCollection()
	{
		_pimpl = new implMidiTransportInformationCollection;
	}

	MidiTransportInformationCollection::~MidiTransportInformationCollection()
	{
		delete _pimpl;
	}

}

