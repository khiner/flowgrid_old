#pragma once

#include <state/TracksState.h>
#include "state/Identifiers.h"
#include "unordered_map"

class Push2Colours : ValueTree::Listener {
public:
    class Listener {
    public:
        virtual ~Listener() = default;

        virtual void colourAdded(const Colour &colour, uint8 colourIndex) = 0;

        virtual void trackColourChanged(const String &trackUuid, const Colour &colour) = 0;
    };

    explicit Push2Colours(TracksState &tracks);

    ~Push2Colours() override;

    uint8 findIndexForColourAddingIfNeeded(const Colour &colour);

    void addListener(Listener *listener) { listeners.add(listener); }

    void removeListener(Listener *listener) { listeners.remove(listener); }

    std::unordered_map<String, uint8> indexForColour;
private:
    Array<uint8> availableColourIndexes;
    std::unordered_map<String, uint8> indexForTrackUuid;
    ListenerList<Listener> listeners;
    TracksState &tracks;

    void addColour(const Colour &colour);
    void setColour(uint8 colourIndex, const Colour &colour);

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override;
    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int) override;
    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override;
};
