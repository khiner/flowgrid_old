#include "CreateProcessor.h"

ValueTree CreateProcessor::createProcessor(const PluginDescription &description) {
    ValueTree processorToCreate(ProcessorIDs::PROCESSOR);
    Processor::setId(processorToCreate, description.createIdentifierString());
    Processor::setName(processorToCreate, description.name);
    Processor::setAllowsDefaultConnections(processorToCreate, true);
    Processor::setPluginWindowType(processorToCreate, static_cast<int>(PluginWindow::Type::none));
    return processorToCreate;
}

static int getInsertSlot(const PluginDescription &description, const ValueTree &track) {
    if (InternalPluginFormat::isTrackIOProcessor(description.name)) return -1;
    if (description.numInputChannels == 0) return 0;

    // Insert new effect processors right after the lane's last processor
    const auto &lane = Track::getProcessorLane(track);
    int indexToInsertProcessor = lane.getNumChildren();
    return indexToInsertProcessor <= 0 ? 0 : Processor::getSlot(lane.getChild(indexToInsertProcessor - 1)) + 1;
}

CreateProcessor::CreateProcessor(ValueTree processorToCreate, int trackIndex, int slot, Tracks &tracks, View &view, ProcessorGraph &processorGraph)
        : trackIndex(trackIndex), slot(slot), processorToCreate(std::move(processorToCreate)),
          pluginWindowType(Processor::getPluginWindowType(this->processorToCreate)),
          insertAction(this->processorToCreate, trackIndex, slot, tracks, view),
          processorGraph(processorGraph) {}

CreateProcessor::CreateProcessor(const PluginDescription &description, int trackIndex, int slot, Tracks &tracks, View &view, ProcessorGraph &processorGraph)
        : CreateProcessor(createProcessor(description), trackIndex, slot, tracks, view, processorGraph) {}

CreateProcessor::CreateProcessor(const PluginDescription &description, int trackIndex, Tracks &tracks, View &view, ProcessorGraph &processorGraph)
        : CreateProcessor(createProcessor(description), trackIndex, getInsertSlot(description, tracks.getTrack(trackIndex)), tracks, view, processorGraph) {}

bool CreateProcessor::perform() {
    performTemporary();
    if (processorToCreate.isValid()) {
        processorGraph.onProcessorCreated(processorToCreate);
        Processor::setPluginWindowType(processorToCreate, pluginWindowType);
    }
    return true;
}

bool CreateProcessor::undo() {
    undoTemporary();
    if (processorToCreate.isValid()) {
        Processor::setPluginWindowType(processorToCreate, static_cast<int>(PluginWindow::Type::none));
        processorGraph.onProcessorDestroyed(processorToCreate);
    }
    return true;
}
