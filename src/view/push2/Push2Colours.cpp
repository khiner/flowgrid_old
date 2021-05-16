#include "Push2Colours.h"

Push2Colours::Push2Colours(TracksState &tracks) : tracks(tracks) {
    this->tracks.addListener(this);
    for (uint8 colourIndex = 1; colourIndex < CHAR_MAX - 1; colourIndex++) {
        availableColourIndexes.add(colourIndex);
    }
}

Push2Colours::~Push2Colours() {
    tracks.removeListener(this);
}

uint8 Push2Colours::findIndexForColourAddingIfNeeded(const Colour &colour) {
    auto entry = indexForColour.find(colour.toString());
    if (entry == indexForColour.end()) {
        addColour(colour);
        entry = indexForColour.find(colour.toString());
    }
    jassert(entry != indexForColour.end());
    return entry->second;
}

void Push2Colours::addColour(const Colour &colour) {
    jassert(!availableColourIndexes.isEmpty());
    setColour(availableColourIndexes.removeAndReturn(0), colour);
}

void Push2Colours::setColour(uint8 colourIndex, const Colour &colour) {
    jassert(colourIndex > 0 && colourIndex < CHAR_MAX - 1);
    indexForColour[colour.toString()] = colourIndex;
    listeners.call([colour, colourIndex](Listener &listener) { listener.colourAdded(colour, colourIndex); });
}

void Push2Colours::valueTreeChildAdded(ValueTree &parent, ValueTree &child) {
    if (TrackState::isType(child)) {
        const String &uuid = child[TrackStateIDs::uuid];
        const auto &colour = Colour::fromString(child[TrackStateIDs::colour].toString());
        auto index = findIndexForColourAddingIfNeeded(colour);
        indexForTrackUuid[uuid] = index;
        listeners.call([uuid, colour](Listener &listener) { listener.trackColourChanged(uuid, colour); });
    }
}

void Push2Colours::valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) {
    if (TrackState::isType(child)) {
        const auto &uuid = child[TrackStateIDs::uuid].toString();
        auto index = indexForTrackUuid[uuid];
        availableColourIndexes.add(index);
        indexForTrackUuid.erase(uuid);
    }
}

void Push2Colours::valueTreePropertyChanged(ValueTree &tree, const Identifier &i) {
    if (TrackState::isType(tree)) {
        if (i == TrackStateIDs::colour) {
            const String &uuid = tree[TrackStateIDs::uuid];
            auto index = indexForTrackUuid[uuid];
            const auto &colour = Colour::fromString(tree[TrackStateIDs::colour].toString());
            setColour(index, colour);
            listeners.call([uuid, colour](Listener &listener) { listener.trackColourChanged(uuid, colour); });
        }
    }
}
