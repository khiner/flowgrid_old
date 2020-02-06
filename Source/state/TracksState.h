#pragma once


#include "JuceHeader.h"
#include "Identifiers.h"
#include "unordered_map"
#include "StatefulAudioProcessorContainer.h"
#include "state/ViewState.h"
#include "PluginManager.h"
#include "Stateful.h"

class TracksState :
        public Stateful,
        private ValueTree::Listener {
public:
    TracksState(ViewState& view, StatefulAudioProcessorContainer& audioProcessorContainer,
                PluginManager& pluginManager, UndoManager& undoManager)
            : view(view), audioProcessorContainer(audioProcessorContainer),
              pluginManager(pluginManager), undoManager(undoManager) {
        tracks = ValueTree(IDs::TRACKS);
        tracks.addListener(this);
        tracks.setProperty(IDs::name, "Tracks", nullptr);
    }

    void loadFromState(const ValueTree& state) override {
        Utilities::moveAllChildren(state, getState(), nullptr);

        // Re-save all non-string value types,
        // since type information is not saved in XML
        // Also, re-set some vars just to trigger the event (like selected slot mask)
        for (auto track : tracks) {
            if (track.hasProperty(IDs::isMasterTrack)) {
                resetVarToBool(track, IDs::isMasterTrack, this);
            }
            track.sendPropertyChangeMessage(IDs::selectedSlotsMask);
            resetVarToBool(track, IDs::selected, this);
            for (auto processor : track) {
                if (processor.hasType(IDs::PROCESSOR)) {
                    resetVarToInt(processor, IDs::processorSlot, this);
                    resetVarToInt(processor, IDs::nodeId, this);
                    resetVarToInt(processor, IDs::processorInitialized, this);
                    resetVarToBool(processor, IDs::bypassed, this);
                    resetVarToBool(processor, IDs::acceptsMidi, this);
                    resetVarToBool(processor, IDs::producesMidi, this);
                    resetVarToBool(processor, IDs::allowDefaultConnections, this);
                }
            }
        }
    }

    ValueTree& getState() override { return tracks; }

    int getNumTracks() const { return tracks.getNumChildren(); }
    int indexOf(const ValueTree& track) const { return tracks.indexOf(track); }
    ValueTree getTrack(int trackIndex) const { return tracks.getChild(trackIndex); }

    int getViewIndexForTrack(const ValueTree& track) const { return indexOf(track) - view.getGridViewTrackOffset(); }
    int getNumAvailableSlotsForTrack(const ValueTree &track) const { return view.getNumAvailableSlotsForTrack(track); }
    int getSlotOffsetForTrack(const ValueTree& track) const { return view.getSlotOffsetForTrack(track); }

    ValueTree getTrackWithViewIndex(int trackViewIndex) const {
        return getTrack(trackViewIndex + view.getGridViewTrackOffset());
    }

    ValueTree getMasterTrack() const { return tracks.getChildWithProperty(IDs::isMasterTrack, true); }
    static bool isMasterTrack(const ValueTree& track) { return track.hasProperty(IDs::isMasterTrack) && track[IDs::isMasterTrack]; }

    int getMixerChannelSlotForTrack(const ValueTree& track) const { return view.getMixerChannelSlotForTrack(track); }

    ValueTree getMixerChannelProcessorForTrack(const ValueTree& track) const {
        return getProcessorAtSlot(track, getMixerChannelSlotForTrack(track));
    }

    ValueTree getMixerChannelProcessorForFocusedTrack() const {
        return getMixerChannelProcessorForTrack(getFocusedTrack());
    }

    bool focusedTrackHasMixerChannel() const {
        return getMixerChannelProcessorForFocusedTrack().isValid();
    }

    int getNumNonMasterTracks() const {
        return getMasterTrack().isValid() ? tracks.getNumChildren() - 1 : tracks.getNumChildren();
    }

    static bool isMixerChannelProcessor(const ValueTree& processor) {
        return processor[IDs::name] == MixerChannelProcessor::name();
    }

    static bool doesTrackHaveSelections(const ValueTree& track) {
        return track[IDs::selected] || trackHasAnySlotSelected(track);
    }

    bool doesMoreThanOneTrackHaveSelections() const {
        bool foundOne = false;
        for (const auto& track : tracks) {
            if (doesTrackHaveSelections(track)) {
                if (foundOne) return true; // found a second one
                else foundOne = true;
            }
        }
    }

    ValueTree findTrackWithUuid(const String& uuid) {
        for (const auto& track : tracks)
            if (track[IDs::uuid] == uuid)
                return track;
        return {};
    }

    static int firstSelectedSlotForTrack(const ValueTree& track) {
        return getSlotMask(track).getHighestBit();
    }

    static bool trackHasAnySlotSelected(const ValueTree &track) {
        return firstSelectedSlotForTrack(track) != -1;
    }

    static Colour getTrackColour(const ValueTree& track) {
        return Colour::fromString(track[IDs::colour].toString());
    }

    ValueTree getFocusedTrack() const {
        juce::Point<int> trackAndSlot = view.getFocusedTrackAndSlot();
        return getTrack(trackAndSlot.x);
    }

    ValueTree getFocusedProcessor() const {
        juce::Point<int> trackAndSlot = view.getFocusedTrackAndSlot();
        const ValueTree& track = getTrack(trackAndSlot.x);
        return getProcessorAtSlot(track, trackAndSlot.y);
    }

    static bool isProcessorSelected(const ValueTree& processor) {
        return processor.hasType(IDs::PROCESSOR) &&
               isSlotSelected(processor.getParent(), processor[IDs::processorSlot]);
    }

    bool isProcessorFocused(const ValueTree& processor) const {
        return getFocusedProcessor() == processor;
    }

    // TODO vector probably a better choice for these
    Array<String> getSelectedSlotsMasks() const {
        Array<String> selectedSlotMasks;
        for (const auto& track : tracks) {
            selectedSlotMasks.add(track[IDs::selectedSlotsMask]);
        }
        return selectedSlotMasks;
    }

    Array<bool> getTrackSelections() const {
        Array<bool> trackSelections;
        for (const auto& track : tracks) {
            trackSelections.add(track[IDs::selected]);
        }
        return trackSelections;
    }

    static BigInteger getSlotMask(const ValueTree& track) {
        BigInteger selectedSlotsMask;
        selectedSlotsMask.parseString(track[IDs::selectedSlotsMask].toString(), 2);
        return selectedSlotsMask;
    }

    static ValueTree getProcessorAtSlot(const ValueTree& track, int slot) {
        return track.isValid() ? track.getChildWithProperty(IDs::processorSlot, slot) : ValueTree();
    }

    static Array<ValueTree> getSelectedProcessorsForTrack(const ValueTree& track) {
        Array<ValueTree> selectedProcessors;
        auto selectedSlotsMask = getSlotMask(track);
        for (const auto& processor : track) {
                int slot = processor[IDs::processorSlot];
                if (selectedSlotsMask[slot])
                        selectedProcessors.add(processor);
            }
        return selectedProcessors;
    }

    static bool isSlotSelected(const ValueTree& track, int slot) {
        return getSlotMask(track)[slot];
    }

    int getInsertIndexForProcessor(const ValueTree &track, const ValueTree& processor, int insertSlot) {
        bool sameTrack = track == processor.getParent();
        auto handleSameTrack = [sameTrack](int index) -> int { return sameTrack ? std::max(0, index - 1) : index; };
        for (const auto& otherProcessor : track) {
            int otherSlot = otherProcessor[IDs::processorSlot];
            if (otherSlot >= insertSlot && otherProcessor != processor) {
                int otherIndex = track.indexOf(otherProcessor);
                if (sameTrack && track.indexOf(processor) < otherIndex)
                    return handleSameTrack(otherIndex);
                else
                    return otherIndex;
            }
        }
        return handleSameTrack(getMixerChannelProcessorForTrack(track).isValid() ? track.getNumChildren() - 1 : track.getNumChildren());
    }

    // TODO needs update for multi-selection
    bool canDuplicateSelected() const {
        const auto& focusedTrack = getFocusedTrack();
        if (focusedTrack[IDs::selected] && !isMasterTrack(focusedTrack))
            return true;
        auto focusedProcessor = getFocusedProcessor();
        return focusedProcessor.isValid() && !isMixerChannelProcessor(focusedProcessor);
    }

    UndoManager* getUndoManager() {
        return &undoManager;
    }

    void saveProcessorStateInformation() const {
        for (const auto& track : tracks) {
            for (auto processorState : track) {
                saveProcessorStateInformationToState(processorState);
            }
        }
    }

    void saveProcessorStateInformationToState(ValueTree &processorState) const {
        if (auto* processorWrapper = audioProcessorContainer.getProcessorWrapperForState(processorState)) {
            MemoryBlock memoryBlock;
            if (auto* processor = processorWrapper->processor) {
                processor->getStateInformation(memoryBlock);
                processorState.setProperty(IDs::state, memoryBlock.toBase64Encoding(), nullptr);
            }
        }
    }

    void setTrackWidth(int trackWidth) { this->trackWidth = trackWidth; }
    void setProcessorHeight(int processorHeight) { this->processorHeight = processorHeight; }

    int getTrackWidth() { return trackWidth; }
    int getProcessorHeight() { return processorHeight; }

    Array<ValueTree> findAllSelectedItems() const {
        Array<ValueTree> selectedItems;
        for (const auto& track : tracks) {
            if (track[IDs::selected]) {
                selectedItems.add(track);
            } else {
                for (const auto& processor : track) {
                    if (isProcessorSelected(processor)) {
                        selectedItems.add(processor);
                    }
                }
            }
        }
        return selectedItems;
    }

    ValueTree findProcessorNearestToSlot(const ValueTree &track, int slot) const {
        auto nearestSlot = INT_MAX;
        ValueTree nearestProcessor;
        for (const auto& processor : track) {
            int otherSlot = processor[IDs::processorSlot];
            if (otherSlot == slot)
                return processor;
            if (abs(slot - otherSlot) < abs(slot - nearestSlot)) {
                nearestSlot = otherSlot;
                nearestProcessor = processor;
            }
            if (otherSlot > slot)
                break; // processors are ordered by slot.
        }
        return nearestProcessor;
    }

    static constexpr int TRACK_LABEL_HEIGHT = 32; // TODO move to ViewManager?
private:
    ValueTree tracks;
    ViewState& view;
    StatefulAudioProcessorContainer& audioProcessorContainer;
    PluginManager &pluginManager;
    UndoManager &undoManager;

    int trackWidth {0}, processorHeight {0};

    // TODO these state changes should originate from the action that caused them (state changes themselves are not derived from state)
    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (tree.hasType(IDs::TRACK) && i == IDs::selected) {
            if (tree[i]) {
                if (!isMasterTrack(tree))
                    view.updateViewTrackOffsetToInclude(indexOf(tree), getNumNonMasterTracks());
            }
        } else if (i == IDs::selectedSlotsMask) {
            if (!isMasterTrack(tree))
                view.updateViewTrackOffsetToInclude(indexOf(tree), getNumNonMasterTracks());
            auto slot = firstSelectedSlotForTrack(tree);
            if (slot != -1)
                view.updateViewSlotOffsetToInclude(slot, isMasterTrack(tree));
        }
    }
};
