#pragma once

#include "model/Output.h"
#include "CreateOrDeleteConnections.h"
#include "ProcessorGraph.h"

struct UpdateProcessorDefaultConnections : public CreateOrDeleteConnections {
    UpdateProcessorDefaultConnections(const Processor *, bool makeInvalidDefaultsIntoCustom, Connections &, Output &, AllProcessors &, ProcessorGraph &);
};
