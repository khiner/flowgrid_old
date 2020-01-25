#pragma once

#include "JuceHeader.h"
#include "Identifiers.h"
#include "unordered_map"
#include "StatefulAudioProcessorContainer.h"
#include "ValueTreeItems.h"
#include "state_managers/ViewStateManager.h"
#include "processors/ProcessorManager.h"
#include "StateManager.h"

class TracksStateManager :
        public StateManager,
        public ProcessorLifecycleBroadcaster,
        private ValueTree::Listener {
public:
    struct TrackAndSlot {
        TrackAndSlot() : slot(0) {};
        TrackAndSlot(ValueTree track, const ValueTree& processor) : track(std::move(track)), slot(processor[IDs::processorSlot]) {}
        TrackAndSlot(ValueTree track, const int slot) : track(std::move(track)), slot(jmax(-1, slot)) {}
        TrackAndSlot(ValueTree track) : track(std::move(track)), slot(-1) {}

        bool isValid() const { return track.isValid(); }

        void select(TracksStateManager& tracksManager) {
            if (!track.isValid())
                return;

            const auto& focusedTrack = tracksManager.getFocusedTrack();
            if (slot != -1)
                tracksManager.selectProcessorSlot(track, slot);
            if (slot == -1 || (focusedTrack != track && focusedTrack[IDs::selected]))
                tracksManager.setTrackSelected(track, true);
        }

        ValueTree track;
        const int slot;
    };

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

    void setTrackSelected(ValueTree track, bool selected, bool deselectOthers=true, bool allowRectangleSelect=true) {
        // take care of this track
        track.setProperty(IDs::selected, selected, nullptr);
        if (selected) {
            selectAllTrackSlots(track);
            focusOnProcessorSlot(track, -1);
        } else {
            deselectAllTrackSlots(track);
        }
        // take care of other tracks
        selectionEndTrackAndSlot = std::make_unique<TrackAndSlot>(track);
        if (allowRectangleSelect && selectionStartTrackAndSlot != nullptr) { // shift+select
            selectRectangle(track, -1);
        } else {
            if (selected && deselectOthers) {
                for (const auto& otherTrack : tracks) {
                    if (otherTrack != track) {
                        setTrackSelected(otherTrack, false, false);
                    }
                }
            }
        }
    }

    void selectMasterTrack() {
        setTrackSelected(getMasterTrack(), true);
    }

    void selectProcessor(const ValueTree& processor) {
        selectProcessorSlot(processor.getParent(), processor[IDs::processorSlot], true);
    }

    static ValueTree findProcessorAtSlot(const ValueTree& track, int slot) {
        return track.getChildWithProperty(IDs::processorSlot, slot);
    }

    bool isSlotSelected(const ValueTree& track, int slot) const {
        return getSlotMask(track)[slot];
    }

    // Focuses on whatever is in this slot (including nothing).
    // A slot of -1 means to focus on the track, which actually focuses on
    // its first processor, if present, or its first slot (which is empty).
    void focusOnProcessorSlot(const ValueTree& track, int slot) {
        if (slot == -1) {
            const ValueTree &firstProcessor = track.getChild(0);
            slot = firstProcessor.isValid() ? int(firstProcessor[IDs::processorSlot]) : 0;
        }
        viewManager.focusOnProcessorSlot(track, slot);
    }

    void selectProcessorSlot(const ValueTree& track, int slot, bool deselectOthers=true) {
        selectionEndTrackAndSlot = std::make_unique<TrackAndSlot>(track, slot);
        if (selectionStartTrackAndSlot != nullptr) { // shift+select
            selectRectangle(track, slot);
        } else {
            setProcessorSlotSelected(track, slot, true, deselectOthers);
        }
        focusOnProcessorSlot(track, slot);
    }

    void deselectProcessorSlot(const ValueTree& track, int slot) {
        setProcessorSlotSelected(track, slot, false, false);
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


    ValueTree createAndAddMasterTrack() {
        ValueTree masterTrack = doCreateAndAddMasterTrack();
        addSelectionStateToUndoStack();
        return masterTrack;
    }

    ValueTree createAndAddTrack(bool addMixer=true) {
        ValueTree newTrack = doCreateAndAddTrack(&undoManager, addMixer);
        addSelectionStateToUndoStack();
        return newTrack;
    }

    // Assumes we're always creating processors to the currently focused track (which is true as of now!)
    ValueTree createAndAddProcessor(const PluginDescription& description, int slot=-1) {
        const auto& focusedTrack = getFocusedTrack();
        if (focusedTrack.isValid()) {
            ValueTree newProcessor = doCreateAndAddProcessor(description, focusedTrack, &undoManager, slot);
            addSelectionStateToUndoStack();
            return newProcessor;
        } else {
            return {};
        }
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

    void deleteSelectedItems() {
        addSelectionStateToUndoStack();
        const Array<ValueTree> allSelectedItems = findAllSelectedItems();
        for (const auto &selectedItem : allSelectedItems) {
            deleteTrackOrProcessor(selectedItem, &undoManager);
        }
    }

    void duplicateSelectedItems() {
        const Array<ValueTree> allSelectedItems = findAllSelectedItems();
        for (auto selectedItem : allSelectedItems) {
            duplicateItem(selectedItem, &undoManager);
        }
        addSelectionStateToUndoStack();
    }

    void duplicateItem(ValueTree &item, UndoManager* undoManager) {
        if (!item.isValid())
            return;
        if (item.getParent().isValid()) {
            if (item.hasType(IDs::TRACK) && !isMasterTrack(item)) {
                auto copiedTrack = doCreateAndAddTrack(undoManager, false, item, true);
                for (auto processor : item) {
                    saveProcessorStateInformationToState(processor);
                    auto copiedProcessor = processor.createCopy();
                    copiedProcessor.removeProperty(IDs::nodeId, nullptr);
                    addProcessorToTrack(copiedTrack, copiedProcessor, item.indexOf(processor), undoManager);
                }
            } else if (item.hasType(IDs::PROCESSOR) && !isMixerChannelProcessor(item)) {
                saveProcessorStateInformationToState(item);
                auto track = item.getParent();
                auto copiedProcessor = item.createCopy();
                setProcessorSlot(track, copiedProcessor, int(item[IDs::processorSlot]) + 1, nullptr);
                copiedProcessor.removeProperty(IDs::nodeId, nullptr);
                addProcessorToTrack(track, copiedProcessor, track.indexOf(item), undoManager);
            }
        }
    }

    void setCurrentlyDraggingTrack(const ValueTree& currentlyDraggingTrack) {
        this->currentlyDraggingTrack = currentlyDraggingTrack;
    }

    const ValueTree& getCurrentlyDraggingTrack() const {
        return currentlyDraggingTrack;
    }

    void setCurrentlyDraggingProcessor(const ValueTree& currentlyDraggingProcessor) {
        this->currentlyDraggingProcessor = currentlyDraggingProcessor;
    }

    bool isCurrentlyDraggingProcessor() {
        return currentlyDraggingProcessor.isValid();
    }

    ValueTree& getCurrentlyDraggingProcessor() {
        return currentlyDraggingProcessor;
    }

    UndoManager* getDragDependentUndoManager() {
        return !currentlyDraggingProcessor.isValid() ? &undoManager : nullptr;
    }

    bool moveProcessor(int toTrackIndex, int toSlot, UndoManager *undoManager) {
        ValueTree& processor = currentlyDraggingProcessor;
        if (!processor.isValid())
            return false;
        auto toTrack = getTrack(toTrackIndex);
        int fromSlot = processor[IDs::processorSlot];
        if (fromSlot == toSlot && processor.getParent() == toTrack)
            return false;

        setProcessorSlot(processor.getParent(), processor, toSlot, undoManager);

        const int insertIndex = getParentIndexForProcessor(toTrack, processor, undoManager);
        toTrack.moveChildFromParent(processor.getParent(), processor.getParent().indexOf(processor), insertIndex, undoManager);

        makeSlotsValid(toTrack, undoManager);

        return true;
    }

    void deleteTrackOrProcessor(const ValueTree &item, UndoManager *undoManager) {
        if (!item.isValid())
            return;
        if (item.getParent().isValid()) {
            if (item.hasType(IDs::TRACK)) {
                while (item.getNumChildren() > 0)
                    deleteTrackOrProcessor(item.getChild(item.getNumChildren() - 1), undoManager);
            } else if (item.hasType(IDs::PROCESSOR)) {
                sendProcessorWillBeDestroyedMessage(item);
            }
            item.getParent().removeChild(item, undoManager);
            if (item.hasType(IDs::PROCESSOR)) {
                sendProcessorHasBeenDestroyedMessage(item);
            }
        }
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

    TrackAndSlot findItemToSelectWithLeftRightDelta(int delta) const {
        if (viewManager.isGridPaneFocused())
            return findGridItemToSelectWithDelta(delta, 0);
        else
            return findSelectionPaneItemToSelectWithLeftRightDelta(delta);
    }

    TrackAndSlot findItemToSelectWithUpDownDelta(int delta) const {
        if (viewManager.isGridPaneFocused())
            return findGridItemToSelectWithDelta(0, delta);
        else
            return findSelectionPaneItemToFocusWithUpDownDelta(delta);
    }

    void startRectangleSelection() {
        const Point<int> focusedTrackAndSlot = viewManager.getFocusedTrackAndSlot();
        const auto& focusedTrack = getTrack(focusedTrackAndSlot.x);
        if (focusedTrack.isValid()) {
            selectionStartTrackAndSlot = focusedTrack[IDs::selected]
                             ? std::make_unique<TrackAndSlot>(focusedTrack)
                             : std::make_unique<TrackAndSlot>(focusedTrack, focusedTrackAndSlot.y);
        }
    }

    void endRectangleSelection() {
        selectionStartTrackAndSlot.reset();
        selectionEndTrackAndSlot.reset();
    }

    void clear() {
        while (tracks.getNumChildren() > 0) {
            deleteTrackOrProcessor(tracks.getChild(tracks.getNumChildren() - 1), nullptr);
        }
    }

    void setTrackWidth(int trackWidth) { this->trackWidth = trackWidth; }
    void setProcessorHeight(int processorHeight) { this->processorHeight = processorHeight; }

    int getTrackWidth() { return trackWidth; }
    int getProcessorHeight() { return processorHeight; }

    static constexpr int TRACK_LABEL_HEIGHT = 32;
private:
    ValueTree tracks;
    ViewStateManager& viewManager;
    StatefulAudioProcessorContainer& audioProcessorContainer;
    ProcessorManager &processorManager;
    UndoManager &undoManager;

    std::unordered_map<int, int> slotForNodeIdSnapshot;
    std::unique_ptr<TrackAndSlot> selectionStartTrackAndSlot {};
    std::unique_ptr<TrackAndSlot> selectionEndTrackAndSlot {};
    ValueTree currentlyDraggingTrack, currentlyDraggingProcessor;

    int trackWidth {0}, processorHeight {0};

    ValueTree doCreateAndAddMasterTrack() {
        if (getMasterTrack().isValid())
            return {}; // only one master track allowed!

        ValueTree masterTrack(IDs::TRACK);
        masterTrack.setProperty(IDs::isMasterTrack, true, nullptr);
        masterTrack.setProperty(IDs::name, "Master", nullptr);
        masterTrack.setProperty(IDs::colour, Colours::darkslateblue.toString(), nullptr);
        setTrackSelected(masterTrack, false, false);

        tracks.addChild(masterTrack, -1, &undoManager);

        doCreateAndAddProcessor(MixerChannelProcessor::getPluginDescription(), masterTrack, &undoManager);

        return masterTrack;
    }

    ValueTree doCreateAndAddTrack(UndoManager* undoManager, bool addMixer=true, ValueTree nextToTrack={}, bool forceImmediatelyToRight=false) {
        int numTracks = getNumNonMasterTracks();
        const auto& focusedTrack = getFocusedTrack();

        if (!nextToTrack.isValid()) {
            if (focusedTrack.isValid()) {
                // If a track is selected, insert the new track to the left of it if there's no mixer,
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

        setTrackSelected(track, true);

        return track;
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

    Point<int> trackAndSlotToGridPosition(const TrackAndSlot& trackAndSlot) const {
        if (isMasterTrack(trackAndSlot.track))
            return {trackAndSlot.slot + viewManager.getGridViewTrackOffset() - viewManager.getMasterViewSlotOffset(),
                    viewManager.getNumTrackProcessorSlots()};
        else
            return {indexOf(trackAndSlot.track), trackAndSlot.slot};
    }

    const TrackAndSlot gridPositionToTrackAndSlot(const Point<int> gridPosition) const {
        if (gridPosition.y >= viewManager.getNumTrackProcessorSlots())
            return {getMasterTrack(),
                    jmin(gridPosition.x + viewManager.getMasterViewSlotOffset() - viewManager.getGridViewTrackOffset(),
                         viewManager.getNumMasterProcessorSlots() - 1)};
        else
            return {getTrack(jmin(gridPosition.x, getNumNonMasterTracks() - 1)),
                    jmin(gridPosition.y + viewManager.getGridViewSlotOffset(),
                         viewManager.getNumTrackProcessorSlots() - 1)};
    }

    void selectRectangle(const ValueTree &track, int slot) {
        const auto trackIndex = indexOf(track);
        const auto selectionStartTrackIndex = indexOf(selectionStartTrackAndSlot->track);
        const auto gridPosition = trackAndSlotToGridPosition({track, slot});
        Rectangle<int> selectionRectangle(trackAndSlotToGridPosition(*selectionStartTrackAndSlot), gridPosition);
        selectionRectangle.setSize(selectionRectangle.getWidth() + 1, selectionRectangle.getHeight() + 1);

        for (int otherTrackIndex = 0; otherTrackIndex < getNumTracks(); otherTrackIndex++) {
            auto otherTrack = getTrack(otherTrackIndex);
            bool trackSelected = (slot == -1 || selectionStartTrackAndSlot->slot == -1) &&
                                 ((selectionStartTrackIndex <= otherTrackIndex && otherTrackIndex <= trackIndex) ||
                                  (trackIndex <= otherTrackIndex && otherTrackIndex <= selectionStartTrackIndex));
            setTrackSelected(otherTrack, trackSelected, false, false);
            if (!trackSelected) {
                for (int otherSlot = 0; otherSlot < viewManager.getNumAvailableSlotsForTrack(otherTrack); otherSlot++) {
                    const auto otherGridPosition = trackAndSlotToGridPosition({otherTrack, otherSlot});
                    setProcessorSlotSelected(otherTrack, otherSlot, selectionRectangle.contains(otherGridPosition), false);
                }
            }
        }
        if (slot == -1) {
            viewManager.focusOnTrackIndex(trackIndex);
        }
    }

    void addProcessorToTrack(ValueTree &track, const ValueTree &processor, int insertIndex, UndoManager *undoManager) {
        track.addChild(processor, insertIndex, undoManager);
        makeSlotsValid(track, undoManager);
        selectProcessor(processor);
        sendProcessorCreatedMessage(processor);
    }

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

    TrackAndSlot findGridItemToSelectWithDelta(int xDelta, int yDelta) const {
        const auto gridPosition = trackAndSlotToGridPosition(getMostRecentSelectedTrackAndSlot());
        return gridPositionToTrackAndSlot(gridPosition + Point<int>(xDelta, yDelta));
    }

    TrackAndSlot getMostRecentSelectedTrackAndSlot() const {
        if (selectionEndTrackAndSlot != nullptr) {
            return *selectionEndTrackAndSlot;
        } else {
            Point<int> focusedTrackAndSlot = viewManager.getFocusedTrackAndSlot();
            return {getTrack(focusedTrackAndSlot.x), focusedTrackAndSlot.y};
        }
    }

    BigInteger getSlotMask(const ValueTree& track) const {
        BigInteger selectedSlotsMask;
        selectedSlotsMask.parseString(track[IDs::selectedSlotsMask].toString(), 2);
        return selectedSlotsMask;
    }

    void setProcessorSlotSelected(ValueTree track, int slot, bool selected, bool deselectOthers=true) {
        const auto currentSlotMask = getSlotMask(track);
        if (deselectOthers) {
            for (auto anyTrack : tracks) { // also deselect this track!
                setTrackSelected(anyTrack, false, false);
            }
        }
        auto newSlotMask = deselectOthers ? BigInteger() : currentSlotMask;
        newSlotMask.setBit(slot, selected);
        track.setProperty(IDs::selectedSlotsMask, newSlotMask.toString(2), nullptr);
    }

    void selectAllTrackSlots(ValueTree& track) {
        BigInteger selectedSlotsMask;
        selectedSlotsMask.setRange(0, viewManager.getNumAvailableSlotsForTrack(track), true);
        track.setProperty(IDs::selectedSlotsMask, selectedSlotsMask.toString(2), nullptr);
    }

    void deselectAllTrackSlots(ValueTree& track) {
        BigInteger selectedSlotsMask;
        track.setProperty(IDs::selectedSlotsMask, selectedSlotsMask.toString(2), nullptr);
    }

    // Selection & focus events are usually not considered undoable.
    // However, when performing multi-select-aware actions (like deleting multiple selected items),
    // we want an undo/redo to restore what the selection state looked like at the time (for consistency,
    // as well as doing things like re-focusing a processor after undoing a delete)
    // This is accomplished by re-setting all selection/focus properties, passing a flag to allow
    // no-ops into the undo-manager (otherwise re-saving the current state wouldn't be undoable).
    void addSelectionStateToUndoStack() {
        for (auto track : tracks) {
            resetVarAllowingNoopUndo(track, IDs::selected, &undoManager);
            resetVarAllowingNoopUndo(track, IDs::selectedSlotsMask, &undoManager);
        }
        viewManager.addFocusStateToUndoStack(&undoManager);
    }

    TrackAndSlot findSelectionPaneItemToSelectWithLeftRightDelta(int delta) const {
        const Point<int> focusedTrackAndSlot = viewManager.getFocusedTrackAndSlot();
        const ValueTree& focusedTrack = getTrack(focusedTrackAndSlot.x);
        if (!focusedTrack.isValid())
            return {};

        const auto& siblingTrackToSelect = focusedTrack.getSibling(delta);
        if (!siblingTrackToSelect.isValid())
            return {};

        if (focusedTrack[IDs::selected] || focusedTrack.getNumChildren() == 0)
            return {siblingTrackToSelect};

        auto focusedSlot = focusedTrackAndSlot.y;
        if (focusedSlot != -1) {
            const auto& processorToSelect = findProcessorNearestToSlot(siblingTrackToSelect, focusedSlot);
            return processorToSelect.isValid() ? TrackAndSlot(siblingTrackToSelect, processorToSelect) : TrackAndSlot(siblingTrackToSelect);
        }

        return {};
    }

    TrackAndSlot findSelectionPaneItemToFocusWithUpDownDelta(int delta) const {
        const Point<int> focusedTrackAndSlot = viewManager.getFocusedTrackAndSlot();
        const ValueTree& focusedTrack = getTrack(focusedTrackAndSlot.x);
        if (!focusedTrack.isValid())
            return {};

        // XXX broken - EXC_BAD_...
        ValueTree focusedProcessor = {};//getFocusedProcessor();
        if (focusedProcessor.isValid()) {
            if (delta > 0 && focusedTrack[IDs::selected])
                return {focusedTrack, focusedProcessor}; // re-selecting the processor will deselect the parent.

            const auto &siblingProcessorToSelect = focusedProcessor.getSibling(delta);
            if (siblingProcessorToSelect.isValid())
                return {focusedTrack, siblingProcessorToSelect};
        } else { // no focused processor - selection is on empty slot
            auto focusedProcessorSlot = focusedTrackAndSlot.y;
            for (int slot = focusedProcessorSlot;
                 (delta < 0 ? slot >= 0 : slot < viewManager.getNumAvailableSlotsForTrack(focusedTrack));
                 slot += delta) {
                const auto &processor = findProcessorAtSlot(focusedTrack, slot);
                if (processor.isValid())
                    return {focusedTrack, processor};
            }

            return delta < 0 && !focusedTrack[IDs::selected] ? TrackAndSlot(focusedTrack) : TrackAndSlot();
        }
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

    void saveProcessorStateInformationToState(ValueTree &processorState) const {
        if (auto* processorWrapper = audioProcessorContainer.getProcessorWrapperForState(processorState)) {
            MemoryBlock memoryBlock;
            if (auto* processor = processorWrapper->processor) {
                processor->getStateInformation(memoryBlock);
                processorState.setProperty(IDs::state, memoryBlock.toBase64Encoding(), nullptr);
            }
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
