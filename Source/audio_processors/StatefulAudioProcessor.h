#pragma  once

#include "DefaultAudioProcessor.h"

class StatefulAudioProcessor : public DefaultAudioProcessor {
public:
    StatefulAudioProcessor(int inputChannelCount, int outputChannelCount, UndoManager &undoManager) :
            DefaultAudioProcessor(inputChannelCount, outputChannelCount), state(*this, &undoManager) {

    }

    virtual const String &getParameterIdentifier(int index) = 0;

    AudioProcessorParameter *getParameterByIdentifier(const String& parameterId) {
        return state.getParameter(parameterId);
    }

    AudioProcessorParameter *getParameterByIndex(int parameterIndex) {
        const String& id = getParameterIdentifier(parameterIndex);
        return id != "" ? getParameterByIdentifier(id) : nullptr;
    }

    AudioProcessorValueTreeState *getState() {
        return &state;
    }
protected:
    AudioProcessorValueTreeState state;
};
