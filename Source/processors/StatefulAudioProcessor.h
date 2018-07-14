#pragma  once

#include "DefaultAudioProcessor.h"
#include <unordered_map>
#include <Parameter.h>
#include <Utilities.h>

class StatefulAudioProcessor : public DefaultAudioProcessor, public Utilities::ValueTreePropertyChangeListener {
public:
    StatefulAudioProcessor(int inputChannelCount, int outputChannelCount, ValueTree state, UndoManager &undoManager) :
            DefaultAudioProcessor(inputChannelCount, outputChannelCount), state(std::move(state)), undoManager(undoManager) {
    }

    virtual void valueTreePropertyChanged(ValueTree& tree, const Identifier& p) = 0;

    virtual Parameter *getParameterInfo(int parameterIndex) = 0;

    const String &getParameterIdentifier(int parameterIndex) {
        Parameter *parameter = getParameterInfo(parameterIndex);
        return parameter != nullptr ? parameter->paramId : IDs::PARAM_NA.toString();
    }

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

    ValueTree state;
    UndoManager &undoManager;
};
