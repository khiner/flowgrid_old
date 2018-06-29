#pragma  once

#include "DefaultAudioProcessor.h"
#include <unordered_map>
#include <Parameter.h>

class StatefulAudioProcessor : public DefaultAudioProcessor {
public:
    StatefulAudioProcessor(int inputChannelCount, int outputChannelCount, ValueTree &state, UndoManager &undoManager) :
            DefaultAudioProcessor(inputChannelCount, outputChannelCount), state(state), undoManager(undoManager) {
    }

    virtual const String &getParameterIdentifier(int index) = 0;

    virtual Parameter *getParameterInfo(int parameterIndex) = 0;

    void updateValueTree() {
        for (int i = 0; i < getNumParameters(); i++) {
            ValueTree v = getOrCreateChildValueTree(getParameterIdentifier(i));
            if (!v.hasProperty(IDs::value)) {
                v.setProperty(IDs::value, getParameterInfo(i)->defaultValue, nullptr);
            }
            v.sendPropertyChangeMessage(IDs::value);
        }
    }

    ValueTree getOrCreateChildValueTree(const String& paramID) {
        ValueTree v(state.getChildWithProperty(IDs::id, paramID));

        if (!v.isValid()) {
            v = ValueTree(IDs::PARAM);
            v.setProperty(IDs::id, paramID, nullptr);
            state.appendChild(v, nullptr);
        }

        return v;
    }

    UndoManager &undoManager;

    ValueTree &state;
};
