#pragma once

#include <ValueTreeObjectList.h>
#include <Project.h>
#include "JuceHeader.h"
#include "GraphEditorTrack.h"

class GraphEditorTracks : public Component, public Utilities::ValueTreeObjectList<GraphEditorTrack> {
public:
    explicit GraphEditorTracks(const ValueTree &state)
            : Utilities::ValueTreeObjectList<GraphEditorTrack>(state) {
        rebuildObjects();
    }

    ~GraphEditorTracks() override {
        freeObjects();
    }

    void resized() override {
        auto r = getLocalBounds();
        const int w = r.getWidth() / Project::NUM_VISIBLE_TRACKS;

        for (auto *at : objects)
            at->setBounds(r.removeFromLeft(w));
    }

    bool isSuitableType(const juce::ValueTree &v) const override {
        return v.hasType(IDs::TRACK) || v.hasType(IDs::MASTER_TRACK);
    }

    GraphEditorTrack *createNewObject(const juce::ValueTree &v) override {
        auto *at = new GraphEditorTrack(v);
        addAndMakeVisible(at);
        return at;
    }

    void deleteObject(GraphEditorTrack *at) override {
        delete at;
    }

    void newObjectAdded(GraphEditorTrack *) override { resized(); }

    void objectRemoved(GraphEditorTrack *) override { resized(); }

    void objectOrderChanged() override { resized(); }
};
