#pragma once

#include "model/Track.h"
#include "model/View.h"
#include "view/PluginWindow.h"
#include "Stateful.h"
#include "PluginManager.h"
#include "StatefulList.h"
#include "ConnectionType.h"
#include "StatefulAudioProcessorWrappers.h"

namespace TracksIDs {
#define ID(name) const juce::Identifier name(#name);
ID(TRACKS)
#undef ID
}

struct Tracks : public Stateful<Tracks>,
                public StatefulList<Track> {
    struct Listener {
        virtual void trackAdded(Track *track) = 0;
        virtual void trackRemoved(Track *track, int oldIndex) = 0;
        virtual void trackOrderChanged() {}
        virtual void trackPropertyChanged(Track *track, const Identifier &i) {}
    };

    Tracks(View &view, PluginManager &pluginManager, UndoManager &undoManager);

    ~Tracks() override {
        freeObjects();
    }

    static Identifier getIdentifier() { return TracksIDs::TRACKS; }
    void loadFromState(const ValueTree &fromState) override;

    bool isChildType(const ValueTree &tree) const override { return Track::isType(tree); }

    void addTracksListener(Listener *listener) { listeners.add(listener); }
    void removeTracksListener(Listener *listener) { listeners.remove(listener); }

    int indexOfTrack(const ValueTree &track) const { return parent.indexOf(track); }

    bool anyNonMasterTrackHasEffectProcessor(ConnectionType connectionType) {
        for (const auto *track : children)
            if (!track->isMaster())
                for (const auto &processor : track->getProcessorLane())
                    if (Processor::isProcessorAnEffect(processor, connectionType))
                        return true;
        return false;
    }

    // Clears and populates the passed in array
    void copySelectedItemsInto(OwnedArray<Track> &copiedTracks, StatefulAudioProcessorWrappers &processorWrappers) {
        copiedTracks.clear();
        for (auto *track : children) {
            auto copiedTrack = track->copy();
            if (track->isSelected()) {
                copiedTrack.getState().copyPropertiesFrom(track->getState(), nullptr);
                for (auto processor : track->getState())
                    if (Processor::isType(processor))
                        copiedTrack.getState().appendChild(processorWrappers.copyProcessor(processor), nullptr);
            } else {
                copiedTrack.setMaster(track->isMaster());
            }

            auto copiedLanes = ValueTree(ProcessorLanesIDs::PROCESSOR_LANES);
            for (const auto &lane : track->getProcessorLanes()) {
                auto copiedLane = ValueTree(ProcessorLaneIDs::PROCESSOR_LANE);
                ProcessorLane::setSelectedSlotsMask(copiedLane, ProcessorLane::getSelectedSlotsMask(lane));
                for (auto processor : lane)
                    if (track->isSelected() || track->isProcessorSelected(processor))
                        copiedLane.appendChild(processorWrappers.copyProcessor(processor), nullptr);
                copiedLanes.appendChild(copiedLane, nullptr);
            }

            copiedTrack.getState().appendChild(copiedLanes, nullptr);
            copiedTracks.add(std::make_unique<Track>(copiedTrack));
        }
    }

    ValueTree getTrackState(int trackIndex) const { return parent.getChild(trackIndex); }
    int getViewIndexForTrack(const Track *track) const { return track->getIndex() - view.getGridViewTrackOffset(); }
    Track *getTrackWithViewIndex(int trackViewIndex) const { return getChild(trackViewIndex + view.getGridViewTrackOffset()); }
    Track *getMasterTrack() const {
        for (auto *track : children)
            if (track->isMaster())
                return track;
        return {};
    }
    ValueTree getMasterTrackState() const {
        for (auto *track : children)
            if (track->isMaster())
                return track->getState();
        return {};
    }
    int getNumNonMasterTracks() const { return getMasterTrackState().isValid() ? size() - 1 : size(); }

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

    static ValueTree getTrackStateForProcessor(const ValueTree &processor) {
        return Track::isType(processor.getParent()) ? processor.getParent() : processor.getParent().getParent().getParent();
    }
    Track *getTrackForProcessor(const ValueTree &processor) {
        return getChildForState(getTrackStateForProcessor(processor));
    }
    int getTrackIndexForProcessor(const ValueTree &processor) {
        auto *track = getTrackForProcessor(processor);
        return track == nullptr ? -1 : children.indexOf(track);
    }
    ValueTree getFocusedTrackState() const { return getTrackState(view.getFocusedTrackAndSlot().x); }
    Track *getFocusedTrack() const { return getChild(view.getFocusedTrackAndSlot().x); }

    ValueTree getFocusedProcessor() const {
        juce::Point<int> trackAndSlot = view.getFocusedTrackAndSlot();
        const Track *track = getChild(trackAndSlot.x);
        if (track == nullptr) return {};

        return track->getProcessorAtSlot(trackAndSlot.y);
    }

    bool isProcessorFocused(const ValueTree &processor) const { return getFocusedProcessor() == processor; }

    ValueTree findFirstTrackWithSelections() {
        for (auto *track : children)
            if (track->hasSelections())
                return track->getState();
        return {};
    }

    ValueTree findLastTrackWithSelections() const {
        for (int i = children.size() - 1; i >= 0; i--) {
            auto *track = children.getUnchecked(i);
            if (track->hasSelections())
                return track->getState();
        }
        return {};
    }

    Array<BigInteger> getSelectedSlotsMasks() const {
        Array<BigInteger> selectedSlotMasks;
        for (const auto *track : children) {
            const auto &lane = track->getProcessorLane();
            selectedSlotMasks.add(ProcessorLane::getSelectedSlotsMask(lane));
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
    Array<ValueTree> findAllSelectedProcessors() const;

    void showWindow(ValueTree processor, PluginWindow::Type type) {
        processor.setProperty(ProcessorIDs::pluginWindowType, int(type), &undoManager);
    }

    bool doesTrackAlreadyHaveGeneratorOrInstrument(const Track *track) {
        for (const auto &processor : track->getProcessorLane())
            if (auto existingDescription = pluginManager.getDescriptionForIdentifier(Processor::getId(processor)))
                if (PluginManager::isGeneratorOrInstrument(existingDescription.get()))
                    return true;
        return false;
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

    juce::Point<int> selectionPaneTrackAndSlotWithUpDownDelta(int delta) const {
        const auto focusedTrackAndSlot = view.getFocusedTrackAndSlot();
        const auto *focusedTrack = getChild(focusedTrackAndSlot.x);
        if (focusedTrack == nullptr) return INVALID_TRACK_AND_SLOT;

        const ValueTree &focusedProcessor = focusedTrack->getProcessorAtSlot(focusedTrackAndSlot.y);
        // down when track is selected deselects the track
        if (delta > 0 && focusedTrack->isSelected()) return {focusedTrackAndSlot.x, focusedTrackAndSlot.y};

        ValueTree siblingProcessorToSelect;
        if (focusedProcessor.isValid()) {
            siblingProcessorToSelect = focusedProcessor.getSibling(delta);
        } else { // no focused processor - selection is on empty slot
            for (int slot = focusedTrackAndSlot.y + delta; (delta < 0 ? slot >= 0 : slot < view.getNumProcessorSlots(focusedTrack->isMaster())); slot += delta) {
                siblingProcessorToSelect = focusedTrack->getProcessorAtSlot(slot);
                if (siblingProcessorToSelect.isValid())
                    break;
            }
        }
        if (siblingProcessorToSelect.isValid()) return {focusedTrackAndSlot.x, Processor::getSlot(siblingProcessorToSelect)};
        if (delta < 0) return {focusedTrackAndSlot.x, -1};
        return INVALID_TRACK_AND_SLOT;
    }

    juce::Point<int> selectionPaneTrackAndSlotWithLeftRightDelta(int delta) const {
        const auto focusedTrackAndSlot = view.getFocusedTrackAndSlot();
        auto *focusedTrack = getChild(focusedTrackAndSlot.x);
        if (focusedTrack == nullptr) return INVALID_TRACK_AND_SLOT;

        auto *siblingTrackToSelect = getChildForState(focusedTrack->getState().getSibling(delta));
        if (siblingTrackToSelect == nullptr) return INVALID_TRACK_AND_SLOT;

        int siblingTrackIndex = indexOf(siblingTrackToSelect);
        const auto &focusedProcessorLane = focusedTrack->getProcessorLane();
        if (focusedTrack->isSelected() || focusedProcessorLane.getNumChildren() == 0)
            return {siblingTrackIndex, -1};

        if (focusedTrackAndSlot.y != -1) {
            const auto &processorToSelect = siblingTrackToSelect->findProcessorNearestToSlot(focusedTrackAndSlot.y);
            if (processorToSelect.isValid())
                return {siblingTrackIndex, focusedTrackAndSlot.y};
            return {siblingTrackIndex, -1};
        }

        return INVALID_TRACK_AND_SLOT;
    }

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

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (isChildType(tree)) {
            auto *track = getChildForState(tree);
            if (track != nullptr) {
                listeners.call([track, i](Listener &l) { l.trackPropertyChanged(track, i); });
            }
        }
    }

protected:
    Track *createNewObject(const ValueTree &tree) override { return new Track(tree); }
    void deleteObject(Track *track) override { delete track; }
    void newObjectAdded(Track *track) override { listeners.call([track](Listener &l) { l.trackAdded(track); }); }
    void objectRemoved(Track *track, int oldIndex) override { listeners.call([track, oldIndex](Listener &l) { l.trackRemoved(track, oldIndex); }); }
    void objectOrderChanged() override { listeners.call([](Listener &l) { l.trackOrderChanged(); }); }

private:
    View &view;
    PluginManager &pluginManager;
    UndoManager &undoManager;

    ListenerList<Listener> listeners;
};
