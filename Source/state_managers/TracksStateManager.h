#pragma once

#include "JuceHeader.h"
#include "Identifiers.h"
#include "unordered_map"
#include "StatefulAudioProcessorContainer.h"
#include "state_managers/ViewStateManager.h"
#include "processors/ProcessorManager.h"
#include "StateManager.h"

class TracksStateManager :
        public StateManager,
        private ValueTree::Listener {
public:
    TracksStateManager(ViewStateManager& viewManager, StatefulAudioProcessorContainer& audioProcessorContainer,
                       ProcessorManager& processorManager, UndoManager& undoManager)
            : viewManager(viewManager), audioProcessorContainer(audioProcessorContainer),
              processorManager(processorManager), undoManager(undoManager) {
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

    int getViewIndexForTrack(const ValueTree& track) const {
        return indexOf(track) - viewManager.getGridViewTrackOffset();
    }

    int getNumAvailableSlotsForTrack(const ValueTree &track) const {
        return viewManager.getNumAvailableSlotsForTrack(track);
    }

    int getSlotOffsetForTrack(const ValueTree& track) const {
        return viewManager.getSlotOffsetForTrack(track);
    }

    ValueTree getTrackWithViewIndex(int trackViewIndex) const {
        return getTrack(trackViewIndex + viewManager.getGridViewTrackOffset());
    }

    ValueTree getTrack(int trackIndex) const { return tracks.getChild(trackIndex); }
    ValueTree getMasterTrack() const { return tracks.getChildWithProperty(IDs::isMasterTrack, true); }

    int getMixerChannelSlotForTrack(const ValueTree& track) const {
        return viewManager.getMixerChannelSlotForTrack(track);
    }

    const ValueTree getMixerChannelProcessorForTrack(const ValueTree& track) const {
        return track.getChildWithProperty(IDs::name, MixerChannelProcessor::name());
    }

    const ValueTree getMixerChannelProcessorForFocusedTrack() const {
        return getMixerChannelProcessorForTrack(getFocusedTrack());
    }

    bool isMixerChannelProcessor(const ValueTree& processor) const {
        return processor[IDs::name] == MixerChannelProcessor::name();
    }

    const bool focusedTrackHasMixerChannel() const {
        return getMixerChannelProcessorForFocusedTrack().isValid();
    }

    inline int getNumNonMasterTracks() const {
        return getMasterTrack().isValid() ? tracks.getNumChildren() - 1 : tracks.getNumChildren();
    }

    static inline bool isMasterTrack(const ValueTree& track) {
        return track.hasProperty(IDs::isMasterTrack);
    }

    inline bool isTrackSelected(const ValueTree& track) const {
        if (track[IDs::selected])
            return true;
        return trackHasAnySlotSelected(track);
    }

    ValueTree findTrackWithUuid(const String& uuid) {
        for (const auto& track : tracks) {
            if (track[IDs::uuid] == uuid)
                return track;
        }
        return {};
    }

    inline int findSelectedSlotForTrack(const ValueTree& track) const {
        return getSlotMask(track).getHighestBit();
    }

    // TODO many (if not all) of the usages of this method should be replaced
    // with checking for track _focus_
    inline bool trackHasAnySlotSelected(const ValueTree &track) const {
        return findSelectedSlotForTrack(track) != -1;
    }

    inline const Colour getTrackColour(const ValueTree& track) const {
        return Colour::fromString(track[IDs::colour].toString());
    }

    ValueTree getFocusedTrack() const {
        Point<int> trackAndSlot = viewManager.getFocusedTrackAndSlot();
        return getTrack(trackAndSlot.x);
    }

    ValueTree getFocusedProcessor() const {
        Point<int> trackAndSlot = viewManager.getFocusedTrackAndSlot();
        const ValueTree& track = getTrack(trackAndSlot.x);
        return findProcessorAtSlot(track, trackAndSlot.y);
    }

    inline bool isProcessorSelected(const ValueTree& processor) const {
        return processor.hasType(IDs::PROCESSOR) &&
               isSlotSelected(processor.getParent(), processor[IDs::processorSlot]);
    }

    inline bool isProcessorFocused(const ValueTree& processor) const {
        Point<int> trackAndSlot = viewManager.getFocusedTrackAndSlot();
        const ValueTree& track = getTrack(trackAndSlot.x);
        return processor.hasType(IDs::PROCESSOR) && processor.getParent() == track && trackAndSlot.y == int(processor[IDs::processorSlot]);
    }

    void selectAllTrackSlots(ValueTree& track, UndoManager *undoManager) {
        BigInteger selectedSlotsMask;
        selectedSlotsMask.setRange(0, viewManager.getNumAvailableSlotsForTrack(track), true);
        track.setProperty(IDs::selectedSlotsMask, selectedSlotsMask.toString(2), undoManager);
    }

    void deselectAllTrackSlots(ValueTree& track, UndoManager *undoManager) {
        BigInteger selectedSlotsMask;
        track.setProperty(IDs::selectedSlotsMask, selectedSlotsMask.toString(2), undoManager);
    }

    BigInteger getSlotMask(const ValueTree& track) const {
        BigInteger selectedSlotsMask;
        selectedSlotsMask.parseString(track[IDs::selectedSlotsMask].toString(), 2);
        return selectedSlotsMask;
    }

    static ValueTree findProcessorAtSlot(const ValueTree& track, int slot) {
        return track.getChildWithProperty(IDs::processorSlot, slot);
    }

    bool isSlotSelected(const ValueTree& track, int slot) const {
        return getSlotMask(track)[slot];
    }

    void setProcessorSlot(const ValueTree& track, ValueTree& processor, int newSlot, UndoManager* undoManager) {
        if (!processor.isValid())
            return;

        if (newSlot >= getMixerChannelSlotForTrack(track) && !isMixerChannelProcessor(processor)) {
            if (isMasterTrack(track)) {
                addMasterProcessorSlot();
            } else {
                addProcessorRow();
            }
            newSlot = getMixerChannelSlotForTrack(track) - 1;
        }
        processor.setProperty(IDs::processorSlot, newSlot, undoManager);
    }


    void makeSlotsValid(const ValueTree& parent, UndoManager* undoManager) {
        std::vector<int> slots;
        for (const ValueTree& child : parent) {
            if (child.hasType(IDs::PROCESSOR)) {
                slots.push_back(int(child[IDs::processorSlot]));
            }
        }
        sort(slots.begin(), slots.end());
        for (int i = 1; i < slots.size(); i++) {
            while (slots[i] <= slots[i - 1]) {
                slots[i] += 1;
            }
        }

        auto iterator = slots.begin();
        for (ValueTree child : parent) {
            if (child.hasType(IDs::PROCESSOR)) {
                int newSlot = *(iterator++);
                setProcessorSlot(parent, child, newSlot, undoManager);
            }
        }
    }

    int getParentIndexForProcessor(const ValueTree &parent, const ValueTree &processorState, UndoManager* undoManager) {
        auto slot = int(processorState[IDs::processorSlot]);
        for (ValueTree otherProcessorState : parent) {
            if (processorState == otherProcessorState)
                continue;
            if (otherProcessorState.hasType(IDs::PROCESSOR)) {
                auto otherSlot = int(otherProcessorState[IDs::processorSlot]);
                if (otherSlot == slot) {
                    if (otherProcessorState.getParent() == processorState.getParent()) {
                        // moving within same parent - need to resolve the "tie" in a way that guarantees the child order changes.
                        int currentIndex = parent.indexOf(processorState);
                        int currentOtherIndex = parent.indexOf(otherProcessorState);
                        if (currentIndex < currentOtherIndex) {
                            setProcessorSlot(parent, otherProcessorState, otherSlot - 1, undoManager);
                            return currentIndex + 2;
                        } else {
                            setProcessorSlot(parent, otherProcessorState, otherSlot + 1, undoManager);
                            return currentIndex - 1;
                        }
                    } else {
                        return parent.indexOf(otherProcessorState);
                    }
                } else if (otherSlot > slot) {
                    return parent.indexOf(otherProcessorState);
                }
            }
        }

        // TODO in this and other places, we assume processors are the only type of track child.
        return parent.getNumChildren();
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

    void makeConnectionsSnapshot() {
        slotForNodeIdSnapshot.clear();
        for (const auto& track : tracks) {
            for (const auto& child : track) {
                if (child.hasType(IDs::PROCESSOR)) {
                    slotForNodeIdSnapshot.insert(std::__1::pair<int, int>(child[IDs::nodeId], child[IDs::processorSlot]));
                }
            }
        }
    }

    void restoreConnectionsSnapshot() {
        for (const auto& track : tracks) {
            for (auto child : track) {
                if (child.hasType(IDs::PROCESSOR)) {
                    setProcessorSlot(track, child, slotForNodeIdSnapshot.at(int(child[IDs::nodeId])), nullptr);
                }
            }
        }
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

    void addProcessorToTrack(ValueTree &track, const ValueTree &processor, int insertIndex, UndoManager *undoManager) {
        track.addChild(processor, insertIndex, undoManager);
        makeSlotsValid(track, undoManager);
    }

    ValueTree doCreateAndAddTrack(UndoManager* undoManager, bool addMixer=true, ValueTree nextToTrack={}, bool forceImmediatelyToRight=false) {
        int numTracks = getNumNonMasterTracks();
        const auto& focusedTrack = getFocusedTrack();

        if (!nextToTrack.isValid()) {
            if (focusedTrack.isValid()) {
                // If a track is focused, insert the new track to the left of it if there's no mixer,
                // or to the right of the first track with a mixer if the new track has a mixer.
                nextToTrack = focusedTrack;

                if (addMixer && !forceImmediatelyToRight) {
                    while (nextToTrack.isValid() && !getMixerChannelProcessorForTrack(nextToTrack).isValid())
                        nextToTrack = nextToTrack.getSibling(1);
                }
            }
        }
        if (nextToTrack == getMasterTrack())
            nextToTrack = {};

        bool isSubTrack = nextToTrack.isValid() && !addMixer;

        ValueTree track(IDs::TRACK);
        track.setProperty(IDs::uuid, Uuid().toString(), nullptr);
        track.setProperty(IDs::name, isSubTrack ? makeTrackNameUnique(nextToTrack[IDs::name]) : ("Track " + String(numTracks + 1)), nullptr);
        track.setProperty(IDs::colour, isSubTrack ? nextToTrack[IDs::colour].toString() : Colour::fromHSV((1.0f / 8.0f) * numTracks, 0.65f, 0.65f, 1.0f).toString(), nullptr);

        tracks.addChild(track, nextToTrack.isValid() ? nextToTrack.getParent().indexOf(nextToTrack) + (addMixer || forceImmediatelyToRight ? 1 : 0): numTracks, undoManager);

        if (addMixer)
            doCreateAndAddProcessor(MixerChannelProcessor::getPluginDescription(), track, undoManager);

        return track;
    }

    ValueTree doCreateAndAddMasterTrack() {
        if (getMasterTrack().isValid())
            return {}; // only one master track allowed!

        ValueTree masterTrack(IDs::TRACK);
        masterTrack.setProperty(IDs::isMasterTrack, true, nullptr);
        masterTrack.setProperty(IDs::name, "Master", nullptr);
        masterTrack.setProperty(IDs::colour, Colours::darkslateblue.toString(), nullptr);
        masterTrack.setProperty(IDs::selected, false, nullptr);
        masterTrack.setProperty(IDs::selectedSlotsMask, BigInteger().toString(2), nullptr);

        tracks.addChild(masterTrack, -1, &undoManager);

        doCreateAndAddProcessor(MixerChannelProcessor::getPluginDescription(), masterTrack, &undoManager);

        return masterTrack;
    }

    ValueTree doCreateAndAddProcessor(const PluginDescription &description, ValueTree track, UndoManager *undoManager, int slot=-1) {
        if (description.name == MixerChannelProcessor::name() && getMixerChannelProcessorForTrack(track).isValid())
            return {}; // only one mixer channel per track

        if (ProcessorManager::isGeneratorOrInstrument(&description) &&
            processorManager.doesTrackAlreadyHaveGeneratorOrInstrument(track)) {
            return doCreateAndAddProcessor(description, doCreateAndAddTrack(undoManager, false, track), undoManager, slot);
        }

        ValueTree processor(IDs::PROCESSOR);
        processor.setProperty(IDs::id, description.createIdentifierString(), nullptr);
        processor.setProperty(IDs::name, description.name, nullptr);
        processor.setProperty(IDs::allowDefaultConnections, true, nullptr);

        int insertIndex;
        if (isMixerChannelProcessor(processor)) {
            insertIndex = -1;
            slot = getMixerChannelSlotForTrack(track);
        } else if (slot == -1) {
            if (description.numInputChannels == 0) {
                insertIndex = 0;
                slot = 0;
            } else {
                // Insert new effect processors _right before_ the first MixerChannel processor.
                const ValueTree &mixerChannelProcessor = getMixerChannelProcessorForTrack(track);
                insertIndex = mixerChannelProcessor.isValid() ? track.indexOf(mixerChannelProcessor) : track.getNumChildren();
                slot = insertIndex <= 0 ? 0 : int(track.getChild(insertIndex - 1)[IDs::processorSlot]) + 1;
            }
        } else {
            setProcessorSlot(track, processor, slot, nullptr);
            insertIndex = getParentIndexForProcessor(track, processor, nullptr);
        }
        setProcessorSlot(track, processor, slot, nullptr);
        addProcessorToTrack(track, processor, insertIndex, undoManager);

        return processor;
    }

    Array<ValueTree> findAllSelectedItems() const {
        Array<ValueTree> items;
        for (const auto& track : tracks) {
            if (track[IDs::selected]) {
                items.add(track);
            } else {
                for (const auto& processor : track) {
                    if (isProcessorSelected(processor)) {
                        items.add(processor);
                    }
                }
            }
        }
        return items;
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
    ViewStateManager& viewManager;
    StatefulAudioProcessorContainer& audioProcessorContainer;
    ProcessorManager &processorManager;
    UndoManager &undoManager;

    std::unordered_map<int, int> slotForNodeIdSnapshot;

    int trackWidth {0}, processorHeight {0};

    void addProcessorRow() {
        viewManager.addProcessorRow(&undoManager);
        for (const auto& track : tracks) {
            if (isMasterTrack(track))
                continue;
            auto mixerChannelProcessor = getMixerChannelProcessorForTrack(track);
            if (mixerChannelProcessor.isValid()) {
                setProcessorSlot(track, mixerChannelProcessor, getMixerChannelSlotForTrack(track), &undoManager);
            }
        }
    }

    void addMasterProcessorSlot() {
        viewManager.addMasterProcessorSlot(&undoManager);
        const auto& masterTrack = getMasterTrack();
        auto mixerChannelProcessor = getMixerChannelProcessorForTrack(masterTrack);
        if (mixerChannelProcessor.isValid()) {
            setProcessorSlot(masterTrack, mixerChannelProcessor, getMixerChannelSlotForTrack(masterTrack), &undoManager);
        }
    }

    // NOTE: assumes the track hasn't been added yet!
    const String makeTrackNameUnique(const String& trackName) {
        for (const auto& track : tracks) {
            String otherTrackName = track[IDs::name];
            if (otherTrackName == trackName) {
                if (trackName.contains("-")) {
                    int i = trackName.getLastCharacters(trackName.length() - trackName.lastIndexOf("-") - 1).getIntValue();
                    if (i != 0) {
                        return makeTrackNameUnique(trackName.upToLastOccurrenceOf("-", true, false) + String(i + 1));
                    }
                } else {
                    return makeTrackNameUnique(trackName + "-" + String(1));
                }
            }
        }

        return trackName;
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (tree.hasType(IDs::TRACK) && i == IDs::selected) {
            if (tree[i]) {
                if (!isMasterTrack(tree))
                    viewManager.updateViewTrackOffsetToInclude(indexOf(tree), getNumNonMasterTracks());
            }
        } else if (i == IDs::selectedSlotsMask) {
            if (!isMasterTrack(tree))
                viewManager.updateViewTrackOffsetToInclude(indexOf(tree), getNumNonMasterTracks());
            auto slot = findSelectedSlotForTrack(tree);
            if (slot != -1)
                viewManager.updateViewSlotOffsetToInclude(slot, isMasterTrack(tree));
        }
    }
};
