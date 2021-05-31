#pragma once

#include "Stateful.h"
#include "ProcessorLanes.h"

namespace TrackIDs {
#define ID(name) const juce::Identifier name(#name);
ID(TRACK)
ID(uuid)
ID(colour)
ID(name)
ID(selected)
ID(isMaster)
#undef ID
}

struct Track : public Stateful<Track> {
    Track(): Stateful<Track>() {}
    explicit Track(ValueTree state): Stateful<Track>(std::move(state)) {}

    Track copy() { return Track(state); }

    static Identifier getIdentifier() { return TrackIDs::TRACK; }

    static BigInteger createFullSelectionBitmask(int numSlots) {
        BigInteger selectedSlotsMask;
        selectedSlotsMask.setRange(0, numSlots, true);
        return selectedSlotsMask;
    }

    int getIndex() const { return state.getParent().indexOf(state); }
    String getUuid() const { return state[TrackIDs::uuid]; }
    Colour getColour() const { return Colour::fromString(state[TrackIDs::colour].toString()); }
    Colour getDisplayColour() const { return isSelected() ? getColour().brighter(0.25) : getColour(); }
    String getName() const { return state[TrackIDs::name]; }
    bool isSelected() const { return state[TrackIDs::selected]; }
    bool isMaster() const { return state[TrackIDs::isMaster]; }
    BigInteger getSlotMask() const { return ProcessorLane::getSelectedSlotsMask(getProcessorLane()); }
    bool isSlotSelected(int slot) const { return getSlotMask()[slot]; }
    int firstSelectedSlot() const { return getSlotMask().getHighestBit(); }
    bool hasAnySlotSelected() const { return firstSelectedSlot() != -1; }
    bool hasSelections() const { return isSelected() || hasAnySlotSelected(); }
    bool hasProducerProcessor() {
        for (const auto &processor : getProcessorLane())
            if (Processor::isProcessorAProducer(processor, audio) || Processor::isProcessorAProducer(processor, midi))
                return true;
        return false;
    }
    ValueTree getProcessorAtSlot(int slot) const {
        const auto &lane = getProcessorLane();
        return lane.isValid() ? lane.getChildWithProperty(ProcessorIDs::slot, slot) : ValueTree();
    }
    int getInsertIndexForProcessor(const ValueTree &processor, int insertSlot) const;
    int getNumProcessors() const {
        const auto &lane = getProcessorLane();
        return lane.isValid() ? lane.getNumChildren() : 0;
    }
    ValueTree findProcessorNearestToSlot(int slot) const;
    ValueTree findFirstSelectedProcessor() const;
    ValueTree findLastSelectedProcessor() const;
    Array<ValueTree> findSelectedProcessors() const;
    Array<ValueTree> getAllProcessors() const;
    ValueTree getProcessorLanes() const {
        return state.isValid() ? state.getChildWithName(ProcessorLanesIDs::PROCESSOR_LANES) : ValueTree();
    }
    ValueTree getProcessorLane() const {
        const auto &lanes = getProcessorLanes();
        return lanes.isValid() ? lanes.getChild(0) : ValueTree();
    }
    ValueTree getInputProcessor() const {
        return state.getChildWithProperty(ProcessorIDs::name, InternalPluginFormat::getTrackInputProcessorName());
    }
    ValueTree getOutputProcessor() const {
        return state.getChildWithProperty(ProcessorIDs::name, InternalPluginFormat::getTrackOutputProcessorName());
    }
    ValueTree getFirstProcessor() const {
        const auto &lane = getProcessorLane();
        return lane.getChild(0);
    }
    bool isProcessorSelected(const ValueTree &processor) const {
        return Processor::isType(processor) && isSlotSelected(Processor::getSlot(processor));
    }
    bool isProcessorLeftToRightFlowing(const ValueTree &processor) const {
        return isMaster() && !Processor::isTrackIOProcessor(processor);
    }

    void setUuid(const String &uuid) { state.setProperty(TrackIDs::uuid, uuid, nullptr); }
    void setName(const String &name, UndoManager *undoManager = nullptr) {
        if (undoManager) undoManager->beginNewTransaction();
        state.setProperty(TrackIDs::name, name, undoManager);
    }
    void setColour(const Colour &colour, UndoManager *undoManager = nullptr) { state.setProperty(TrackIDs::colour, colour.toString(), undoManager); }
    void setSelected(bool selected) { state.setProperty(TrackIDs::selected, selected, nullptr); }
    void setMaster(bool isMaster) { state.setProperty(TrackIDs::isMaster, isMaster, nullptr); }

    static ValueTree getProcessorLane(const ValueTree &state) {
        const auto &lanes = state.getChildWithName(ProcessorLanesIDs::PROCESSOR_LANES);
        return lanes.isValid() ? lanes.getChild(0) : ValueTree();
    }
    static ValueTree getProcessorLaneForProcessor(const ValueTree &processor) {
        const auto &parent = processor.getParent();
        return parent.hasType(ProcessorLaneIDs::PROCESSOR_LANE) ? parent : getProcessorLane(parent);
    }
};
