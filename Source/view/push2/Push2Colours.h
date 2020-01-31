#pragma once

#include <state/TracksState.h>
#include "JuceHeader.h"
#include "unordered_map"
#include "state/Identifiers.h"

class Push2Colours : ValueTree::Listener {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void colourAdded(const Colour& colour, uint8 colourIndex) = 0;
        virtual void trackColourChanged(const String &trackUuid, const Colour &colour) = 0;
    };

    explicit Push2Colours(TracksState& tracks) : tracks(tracks) {
        this->tracks.addListener(this);
        for (uint8 colourIndex = 1; colourIndex < CHAR_MAX - 1; colourIndex++) {
            availableColourIndexes.add(colourIndex);
        }
    }

    ~Push2Colours() override {
        tracks.removeListener(this);
    }

    uint8 findIndexForColourAddingIfNeeded(const Colour &colour) {
        auto entry = indexForColour.find(colour.toString());
        if (entry == indexForColour.end()) {
            addColour(colour);
            entry = indexForColour.find(colour.toString());
        }
        jassert(entry != indexForColour.end());
        return entry->second;
    }

    void addListener(Listener* listener) { listeners.add(listener); }
    
    void removeListener(Listener* listener) { listeners.remove(listener); }

    std::unordered_map<String, uint8> indexForColour;
private:
    Array<uint8> availableColourIndexes;
    
    std::unordered_map<String, uint8> indexForTrackUuid;
    
    ListenerList<Listener> listeners;

    TracksState &tracks;
    
    void addColour(const Colour &colour) {
        jassert(!availableColourIndexes.isEmpty());
        setColour(availableColourIndexes.removeAndReturn(0), colour);
    }

    void setColour(uint8 colourIndex, const Colour& colour) {
        jassert(colourIndex > 0 && colourIndex < CHAR_MAX - 1);
        indexForColour[colour.toString()] = colourIndex;
        listeners.call([colour, colourIndex](Listener& listener) { listener.colourAdded(colour, colourIndex); } );
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child.hasType(IDs::TRACK)) {
            const String& uuid = child[IDs::uuid];
            const auto &colour = Colour::fromString(child[IDs::colour].toString());
            auto index = findIndexForColourAddingIfNeeded(colour);
            indexForTrackUuid[uuid] = index;
            listeners.call([uuid, colour](Listener& listener) { listener.trackColourChanged(uuid, colour); } );
        }
    }

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override {
        if (child.hasType(IDs::TRACK)) {
            const auto &uuid = child[IDs::uuid].toString();
            auto index = indexForTrackUuid[uuid];
            availableColourIndexes.add(index);
            indexForTrackUuid.erase(uuid);
        }
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (tree.hasType(IDs::TRACK)) {
            if (i == IDs::colour) {
                const String& uuid = tree[IDs::uuid];
                auto index = indexForTrackUuid[uuid];
                const auto &colour = Colour::fromString(tree[IDs::colour].toString());
                setColour(index, colour);
                listeners.call([uuid, colour](Listener& listener) { listener.trackColourChanged(uuid, colour); } );
            } 
        }
    }
};
