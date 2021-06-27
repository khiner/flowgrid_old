#pragma once

#include "model/Tracks.h"
#include "model/View.h"

class ContextPane : public Component, private ValueTree::Listener, StatefulList<Track>::Listener {
public:
    explicit ContextPane(Tracks &tracks, View &view);

    ~ContextPane() override;

    void paint(Graphics &g) override;
    void resized() override;

private:
    static constexpr int cellWidth = 20, cellHeight = 20;

    Tracks &tracks;
    View &view;
    Path cellPath, trackBorderPath, masterTrackBorderPath;

    Colour getFillColour(const Colour &trackColour, const Track *track, const Processor *processor, bool inView, bool slotSelected, bool slotFocused);

    void onChildAdded(Track *) override { resized(); }
    void onChildRemoved(Track *, int oldIndex) override { resized(); }
    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override;
};
