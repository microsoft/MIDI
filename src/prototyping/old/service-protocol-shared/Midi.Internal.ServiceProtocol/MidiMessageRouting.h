#pragma once

#include <vector>

// Internal service code for MIDI message routing. Move this into the service itself.


class MidiMessageRouterService
{
	// TODO: list of all the routing objects
	// TODO: Add/remove methods. On remove, check to see if 
	// the queue no longer has any listeners, and if so, it can
	// be closed down.
};


class MidiMessageQueue {}; // to be defined elsewhere

class MidiMessageRouter
{
protected:

public:

};


// Straight-through message router. Highly optimize for speed
class BasicMessageRouter : public MidiMessageRouter
{
protected:
	MidiMessageQueue _from;
	MidiMessageQueue _to;

public:

};





class MergeMessageRouter : public MidiMessageRouter
{
protected:
	std::vector<MidiMessageQueue> _from;
	MidiMessageQueue _to;

public:

};






// may need to rethink this one as the reltionship between group/channel is more complex
struct ParsingMessageRouterSource
{
	MidiMessageQueue Queue;
	std::vector<int> OnlyGroups;
	std::vector<int> OnlyChannels;
	bool AllGroups;
	bool AllChannels;
}; 
struct ParsingMessageRouterTarget
{
	MidiMessageQueue Queue;
	int Group;
	int Channel;
	bool PreserveGroup;
	bool PreserveChannel;
};

// routes only from specific channels/groups to specific channels/groups
class ParsingMessageRouter : public MidiMessageRouter
{
protected:
	ParsingMessageRouterTarget _from;
	ParsingMessageRouterTarget _to;

public:

};



struct SplitPointTarget
{
	MidiMessageQueue ToQueue;
	int ToGroup;
	int ToChannel;

	bool PreserveGroup;			// if true, ignores ToGroup
	bool PreserveChannel;		// if true, ignores ToChannel

	int StartNoteNumber;		// TODO: Support UMP pitches?
	int EndNoteNumber;
};

// virtual split points
class SplitPointRouter : public MidiMessageRouter
{
protected:
	MidiMessageQueue _from;
	std::vector<SplitPointTarget> _to; 

public:

};






enum PolyChainAlgorithm
{
	PolyChainAlgorithmRoundRobin,
	PolyChainAlgorithmRandom,
	PolyChainAlgorithmRandomWithPriority
};

struct PolyChainTarget
{
	MidiMessageQueue ToQueue;
	int ToGroup;
	int ToChannel;
	int NoteCount;
	int Priority;
};

// virtual split points
class PolyChainRouter : public MidiMessageRouter
{
protected:
	MidiMessageQueue _from;
	std::vector<PolyChainTarget> _to;

public:

};



