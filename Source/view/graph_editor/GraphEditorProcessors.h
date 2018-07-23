#pragma once

#include <ValueTreeObjectList.h>
#include <Project.h>
#include "GraphEditorProcessor.h"

class GraphEditorProcessors : public Component,
                              public Utilities::ValueTreeObjectList<GraphEditorProcessor> {
public:
    explicit GraphEditorProcessors(GraphEditorTrack &at, ValueTree& state)
            : Utilities::ValueTreeObjectList<GraphEditorProcessor>(state),
              track(at) {
        rebuildObjects();
    }

    ~GraphEditorProcessors() override {
        freeObjects();
    }

    void resized() override {
    }

    void paint(Graphics &g) override {
        auto r = getLocalBounds();
        const int h = r.getHeight() / Project::NUM_AVAILABLE_PROCESSOR_SLOTS;

        g.setColour(findColour(ResizableWindow::backgroundColourId).brighter(0.15));
        for (int i = 0; i < Project::NUM_AVAILABLE_PROCESSOR_SLOTS; i++)
            g.drawRect(r.removeFromTop(h));
    }

    bool isSuitableType(const ValueTree &v) const override {
        return v.hasType(IDs::PROCESSOR);
    }

    GraphEditorProcessor *createNewObject(const ValueTree &v) override {
        auto *ac = new GraphEditorProcessor(v, track);
        addAndMakeVisible(ac);
        return ac;
    }

    void deleteObject(GraphEditorProcessor *ac) override {
        delete ac;
    }

    void newObjectAdded(GraphEditorProcessor *) override { resized(); }

    void objectRemoved(GraphEditorProcessor *) override { resized(); }

    void objectOrderChanged() override { resized(); }

private:
    GraphEditorTrack &track;

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (isSuitableType(v))
            if (i == IDs::start || i == IDs::length)
                resized();

        Utilities::ValueTreeObjectList<GraphEditorProcessor>::valueTreePropertyChanged(v, i);
    }
};
