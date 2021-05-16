#pragma once

#include "state/Tracks.h"
#include "state/View.h"

class ContextPane : public Component, private ValueTree::Listener {
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

    Colour getFillColour(const Colour &trackColour, const ValueTree &track, const ValueTree &processor, bool inView, bool slotSelected, bool slotFocused);

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override;

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &child, int index) override;

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override;
};
