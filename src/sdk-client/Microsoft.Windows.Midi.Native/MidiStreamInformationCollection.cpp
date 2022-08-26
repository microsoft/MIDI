#define WINDOWSMIDISERVICES_EXPORTS


#include "WindowsMidiServicesEnumeration.h"

namespace Microsoft::Windows::Midi::Enumeration
{


	struct MidiStreamInformationCollection::implMidiStreamInformationCollection
	{
		int TODO;
	};

	MidiStreamInformationCollection::MidiStreamInformationCollection()
	{
		_pimpl = new implMidiStreamInformationCollection;
	}

	MidiStreamInformationCollection::~MidiStreamInformationCollection()
	{
		delete _pimpl;
	}


}

