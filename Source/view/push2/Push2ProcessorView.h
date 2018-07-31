#pragma once

#include <processors/StatefulAudioProcessorWrapper.h>
#include "JuceHeader.h"
#include "Push2ComponentBase.h"
#include "view/processor_editor/ProcessorEditor.h"

class Push2ProcessorView : public Push2ComponentBase {
public:
    Push2ProcessorView(Project& project, Push2MidiCommunicator& push2MidiCommunicator)
            : Push2ComponentBase(project, push2MidiCommunicator), processorEditor(1) {
        addAndMakeVisible(processorEditor);
    }

    void resized() override {
        processorEditor.setBounds(getLocalBounds());
    }

    void setProcessor(StatefulAudioProcessorWrapper *processor) {
        processorEditor.setProcessor(processor);
    }
    
private:
    ProcessorEditor processorEditor;
};
