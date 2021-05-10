#pragma once

#include "state/OutputState.h"
#include "CreateOrDeleteConnectionsAction.h"
#include "StatefulAudioProcessorContainer.h"

struct UpdateProcessorDefaultConnectionsAction : public CreateOrDeleteConnectionsAction {
    UpdateProcessorDefaultConnectionsAction(const ValueTree &processor, bool makeInvalidDefaultsIntoCustom,
                                            ConnectionsState &connections, OutputState &output,
                                            StatefulAudioProcessorContainer &audioProcessorContainer);
};
