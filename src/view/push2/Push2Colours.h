#pragma once

#include "model/Tracks.h"
#include "unordered_map"

class Push2Colours : ValueTree::Listener, Tracks::Listener {
public:
    class Listener {
    public:
        virtual ~Listener() = default;

        virtual void colourAdded(const Colour &colour, uint8 colourIndex) = 0;
        virtual void trackColourChanged(const String &trackUuid, const Colour &colour) = 0;
    };

    explicit Push2Colours(Tracks &tracks);

    ~Push2Colours() override;

    uint8 findIndexForColourAddingIfNeeded(const Colour &colour);

    void addListener(Listener *listener) { listeners.add(listener); }
    void removeListener(Listener *listener) { listeners.remove(listener); }

    std::unordered_map<String, uint8> indexForColour;
private:
    Array<uint8> availableColourIndexes;
    std::unordered_map<String, uint8> indexForTrackUuid;
    ListenerList<Listener> listeners;
    Tracks &tracks;

    void addColour(const Colour &colour);
    void setColour(uint8 colourIndex, const Colour &colour);

    void trackAdded(Track *track) override;
    void trackRemoved(Track *track, int oldIndex) override;
    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override;
};
