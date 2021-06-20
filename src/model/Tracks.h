#pragma once

#include "model/Track.h"
#include "model/View.h"
#include "view/PluginWindowType.h"
#include "Stateful.h"
#include "StatefulList.h"
#include "ConnectionType.h"
#include "StatefulAudioProcessorWrappers.h"

namespace TracksIDs {
#define ID(name) const juce::Identifier name(#name);
ID(TRACKS)
#undef ID
}

struct Tracks : public Stateful<Tracks>,
                public StatefulList<Track>,
                private Track::Listener {
    struct Listener {
        virtual void trackAdded(Track *track) {}
        virtual void trackRemoved(Track *track, int oldIndex) {}
        virtual void trackOrderChanged() {}
        virtual void trackPropertyChanged(Track *track, const Identifier &i) {}
        virtual void processorAdded(Processor *processor) {}
        virtual void processorRemoved(Processor *processor, int oldIndex) {}
        virtual void processorPropertyChanged(Processor *processor, const Identifier &) {}
    };

    void addTracksListener(Listener *listener) { listeners.add(listener); }
    void removeTracksListener(Listener *listener) { listeners.remove(listener); }

    Tracks(View &view, UndoManager &undoManager, AudioDeviceManager &deviceManager);

    ~Tracks() override {
        freeObjects();
    }

    static Identifier getIdentifier() { return TracksIDs::TRACKS; }
    void loadFromState(const ValueTree &fromState) override;
    bool isChildType(const ValueTree &tree) const override { return Track::isType(tree); }

    bool anyNonMasterTrackHasEffectProcessor(ConnectionType connectionType);

    // Clears and populates the passed in array
    void copySelectedItemsInto(OwnedArray<Track> &copiedTracks, StatefulAudioProcessorWrappers &processorWrappers);

    int getViewIndexForTrack(const Track *track) const { return track->getIndex() - view.getGridViewTrackOffset(); }
    Track *getMostRecentlyCreatedTrack() const { return mostRecentlyCreatedTrack; }
    Processor *getMostRecentlyCreatedProcessor() const { return mostRecentlyCreatedProcessor; }
    Track *getTrackWithViewIndex(int trackViewIndex) const { return getChild(trackViewIndex + view.getGridViewTrackOffset()); }
    Track *getMasterTrack() const {
        for (auto *track : children)
            if (track->isMaster())
                return track;
        return {};
    }
    int getNumNonMasterTracks() const { return getMasterTrack() != nullptr ? size() - 1 : size(); }

    bool anyTrackSelected() const {
        for (const auto *track : children)
            if (track->isSelected())
                return true;
        return false;
    }

    bool anyTrackHasSelections() const {
        for (const auto *track : children)
            if (track->hasSelections())
                return true;
        return false;
    }

    bool moreThanOneTrackHasSelections() const {
        bool foundOne = false;
        for (const auto *track : children) {
            if (track->hasSelections()) {
                if (foundOne) return true; // found a second one
                else foundOne = true;
            }
        }
        return false;
    }

    Track *findTrackWithUuid(const String &uuid) {
        for (auto *track : children)
            if (track->getUuid() == uuid)
                return track;
        return {};
    }
    Track *getTrackForProcessor(const Processor *processor) {
        return processor != nullptr ? getTrackForProcessorState(processor->getState()) : nullptr;
    }
    Track *getTrackForProcessorState(const ValueTree &processor) {
        const auto &trackState = Track::isType(processor.getParent()) ? processor.getParent() : processor.getParent().getParent().getParent();
        return getChildForState(trackState);
    }
    Track *getFocusedTrack() const { return getChild(view.getFocusedTrackAndSlot().x); }

    Processor *getFocusedProcessor() const {
        juce::Point<int> trackAndSlot = view.getFocusedTrackAndSlot();
        const Track *track = getChild(trackAndSlot.x);
        if (track == nullptr) return nullptr;

        return track->getProcessorAtSlot(trackAndSlot.y);
    }

    Processor *getProcessorByNodeId(juce::AudioProcessorGraph::NodeID nodeId) const {
        for (auto *track : children)
            if (auto *processor = track->getProcessorByNodeId(nodeId))
                return processor;
        return nullptr;
    }

    Processor *getProcessorAt(int trackIndex, int slot) const {
        const Track *track = getChild(trackIndex);
        if (track == nullptr) return nullptr;

        return track->getProcessorAtSlot(slot);
    }

    Processor *getProcessorAt(juce::Point<int> trackAndSlot) const {
        return getProcessorAt(trackAndSlot.x, trackAndSlot.y);
    }

    bool isProcessorFocused(const Processor *processor) const { return getFocusedProcessor() == processor; }

    Track *findFirstTrackWithSelections() {
        for (auto *track : children)
            if (track->hasSelections())
                return track;
        return nullptr;
    }

    Track *findLastTrackWithSelections() const {
        for (int i = children.size() - 1; i >= 0; i--) {
            auto *track = children.getUnchecked(i);
            if (track->hasSelections())
                return track;
        }
        return nullptr;
    }

    Array<BigInteger> getSelectedSlotsMasks() const {
        Array<BigInteger> selectedSlotMasks;
        for (const auto *track : children) {
            selectedSlotMasks.add(track->getProcessorLane()->getSelectedSlotsMask());
        }
        return selectedSlotMasks;
    }

    Array<bool> getTrackSelections() const {
        Array<bool> trackSelections;
        for (const auto *track : children) {
            trackSelections.add(track->isSelected());
        }
        return trackSelections;
    }

    Array<Track *> findAllSelectedTracks() const;
    Array<Processor *> findAllSelectedProcessors() const;

    void showWindow(Processor *processor, PluginWindowType type) {
        processor->setPluginWindowType(int(type), &undoManager);
    }

    juce::Point<int> trackAndSlotWithLeftRightDelta(int delta) const {
        return view.isGridPaneFocused() ? trackAndSlotWithGridDelta(delta, 0) : selectionPaneTrackAndSlotWithLeftRightDelta(delta);
    }

    juce::Point<int> trackAndSlotWithUpDownDelta(int delta) const {
        return view.isGridPaneFocused() ? trackAndSlotWithGridDelta(0, delta) : selectionPaneTrackAndSlotWithUpDownDelta(delta);
    }

    juce::Point<int> trackAndSlotWithGridDelta(int xDelta, int yDelta) const {
        auto focusedTrackAndSlot = view.getFocusedTrackAndSlot();
        const auto *focusedTrack = getChild(focusedTrackAndSlot.x);
        if (focusedTrack != nullptr && focusedTrack->isSelected())
            focusedTrackAndSlot.y = -1;

        const auto fromGridPosition = trackAndSlotToGridPosition(focusedTrackAndSlot);
        return gridPositionToTrackAndSlot(fromGridPosition + juce::Point(xDelta, yDelta), focusedTrack != nullptr && focusedTrack->isMaster());
    }

    juce::Point<int> selectionPaneTrackAndSlotWithUpDownDelta(int delta) const;

    juce::Point<int> selectionPaneTrackAndSlotWithLeftRightDelta(int delta) const;

    juce::Point<int> trackAndSlotToGridPosition(const juce::Point<int> trackAndSlot) const {
        const auto *track = getChild(trackAndSlot.x);
        if (track != nullptr && track->isMaster())
            return {trackAndSlot.y + view.getGridViewTrackOffset() - view.getMasterViewSlotOffset(), view.getNumProcessorSlots()};
        return trackAndSlot;
    }

    juce::Point<int> gridPositionToTrackAndSlot(juce::Point<int> gridPosition, bool allowUpFromMaster = false) const;

    int getNumSlotsForTrack(const Track *track) const { return view.getNumProcessorSlots(track != nullptr && track->isMaster()); }
    int findSlotAt(juce::Point<int> position, const Track *track) const;

    static constexpr juce::Point<int> INVALID_TRACK_AND_SLOT = {-1, -1};

protected:
    Track *createNewObject(const ValueTree &tree) override {
        return new Track(tree, undoManager, deviceManager);
    }
    void deleteObject(Track *track) override { delete track; }
    void newObjectAdded(Track *track) override {
        mostRecentlyCreatedTrack = track;
        track->addTrackListener(this);
        listeners.call(&Listener::trackAdded, track);
    }
    void objectRemoved(Track *track, int oldIndex) override {
        listeners.call(&Listener::trackRemoved, track, oldIndex);
        track->removeTrackListener(this);
        if (track == mostRecentlyCreatedTrack) mostRecentlyCreatedTrack = nullptr;
    }
    void objectOrderChanged() override { listeners.call(&Listener::trackOrderChanged); }
    void objectChanged(Track *track, const Identifier &i) override { listeners.call(&Listener::trackPropertyChanged, track, i); }

private:
    View &view;
    UndoManager &undoManager;
    AudioDeviceManager &deviceManager;

    ListenerList<Listener> listeners;

    Track *mostRecentlyCreatedTrack = nullptr;
    Processor *mostRecentlyCreatedProcessor = nullptr;

    void processorAdded(Processor *processor) override {
        mostRecentlyCreatedProcessor = processor;
        listeners.call(&Listener::processorAdded, processor);
    }
    void processorRemoved(Processor *processor, int oldIndex) override {
        listeners.call(&Listener::processorRemoved, processor, oldIndex);
        if (processor == mostRecentlyCreatedProcessor) mostRecentlyCreatedProcessor = nullptr;
    }
    void processorPropertyChanged(Processor *processor, const Identifier &i) override {
        listeners.call(&Listener::processorPropertyChanged, processor, i);
    }
};
