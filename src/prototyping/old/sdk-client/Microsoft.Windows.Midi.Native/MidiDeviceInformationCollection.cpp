
#define WINDOWSMIDISERVICES_EXPORTS




#include "WindowsMidiServicesEnumeration.h"

namespace Microsoft::Windows::Midi::Enumeration
{

	struct MidiDeviceInformationCollection::implMidiDeviceInformationCollection
	{
		int TODO;
	};

	MidiDeviceInformationCollection::MidiDeviceInformationCollection()
	{
		_pimpl = new implMidiDeviceInformationCollection;
	}

	MidiDeviceInformationCollection::~MidiDeviceInformationCollection()
	{
		delete _pimpl;
	}

}









