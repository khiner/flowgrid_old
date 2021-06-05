#pragma once

#include "model/Output.h"
#include "CreateOrDeleteConnections.h"
#include "ProcessorGraph.h"

struct UpdateProcessorDefaultConnections : public CreateOrDeleteConnections {
    UpdateProcessorDefaultConnections(const Processor *processor, bool makeInvalidDefaultsIntoCustom, Connections &connections, Output &output, ProcessorGraph &processorGraph);
};
