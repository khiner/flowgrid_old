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

    Parameter *getParameterObject(int parameterIndex) {
        return dynamic_cast<Parameter *>(getParameters()[parameterIndex]);
    }

    void updateValueTree() {
        for (auto parameter : getParameters()) {
            auto *parameterObject = dynamic_cast<Parameter *>(parameter);
            ValueTree v = getOrCreateChildValueTree(parameterObject->paramId);
            if (!v.hasProperty(IDs::value)) {
                v.setProperty(IDs::value, parameterObject->getDefaultValue(), nullptr);
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
