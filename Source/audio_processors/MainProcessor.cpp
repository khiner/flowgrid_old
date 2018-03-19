#include <push2/Push2MidiCommunicator.h>
#include "MainProcessor.h"

typedef Push2MidiCommunicator Push2;
const auto& ccNumberForControlLabel = Push2::ccNumberForControlLabel;

MainProcessor::MainProcessor(int inputChannelCount, int outputChannelCount):
        toneSource1(new ToneGeneratorAudioSource),
        toneSource2(new ToneGeneratorAudioSource),
        toneSource3(new ToneGeneratorAudioSource),
        toneSource4(new ToneGeneratorAudioSource), treeState(*this, nullptr) {

    this->setLatencySamples(0);

    // if this was a plug-in, the plug-in wrapper code in JUCE would query us
    // for our channel configuration and call the setPlayConfigDetails() method
    // so that things would be set correctly internally as an AudioProcessor
    // object (which are always initialized as zero in, zero out). The sample rate
    // and blockSize values will get sent to us again when our prepareToPlay()
    // method is called before playback begins.
    this->setPlayConfigDetails(inputChannelCount, outputChannelCount, 0, 0);

    treeState.createAndAddParameter("masterVolume", "Volume", "Volume",
                                    NormalisableRange<float>(0.0f, 1.0f),
                                    0.5f,
                                    [](float value) { return String(value*1000) + "ms"; },
                                    [](const String& text) { return text.getFloatValue()/1000.0f; });


    treeState.createAndAddParameter(amp1Id, "Amp1", "Amp1",
                                    NormalisableRange<float>(0.0f, 1.0f),
                                    0.5f,
                                    [](float value) { return String(value*1000) + "ms"; },
                                    [](const String& text) { return text.getFloatValue()/1000.0f; });
    treeState.createAndAddParameter(freq1Id, "Freq1", "Freq1",
                                    NormalisableRange<float> (440.0f, 10000.0f, 0.0f, 0.3f, false),
                                    880.0f,
                                    [](float value) { return String(value*1000) + "ms"; },
                                    [](const String& text) { return text.getFloatValue()/1000.0f; });

    treeState.createAndAddParameter(amp2Id, "Amp2", "Amp2",
                                    NormalisableRange<float>(0.0f, 1.0f),
                                    0.5f,
                                    [](float value) { return String(value*1000) + "ms"; },
                                    [](const String& text) { return text.getFloatValue()/1000.0f; });
    treeState.createAndAddParameter(freq2Id, "Freq2", "Freq2",
                                    NormalisableRange<float> (440.0f, 10000.0f, 0.0f, 0.3f, false),
                                    880.0f,
                                    [](float value) { return String(value*1000) + "ms"; },
                                    [](const String& text) { return text.getFloatValue()/1000.0f; });
    treeState.addParameterListener(amp1Id, this);
    treeState.addParameterListener(freq1Id, this);
    treeState.addParameterListener(amp2Id, this);
    treeState.addParameterListener(freq2Id, this);
}

void MainProcessor::parameterChanged(const String& parameterID, float newValue) {
    if (parameterID == amp1Id) {
        toneSource1->setAmplitude(newValue);
    } else if (parameterID == freq1Id) {
        toneSource1->setFrequency(newValue);
    } else if (parameterID == amp2Id) {
        toneSource2->setAmplitude(newValue);
    } else if (parameterID == freq2Id) {
        toneSource2->setFrequency(newValue);
    }
};

const String& MainProcessor::parameterIndexForMidiCcNumber(const int midiCcNumber) const {
    static const int MASTER_KNOB = ccNumberForControlLabel.at(Push2::ControlLabel::masterKnob);
    static const int KNOB_1 = ccNumberForControlLabel.at(Push2::ControlLabel::topKnob1);
    static const int KNOB_2 = ccNumberForControlLabel.at(Push2::ControlLabel::topKnob2);
    static const int KNOB_3 = ccNumberForControlLabel.at(Push2::ControlLabel::topKnob3);
    static const int KNOB_4 = ccNumberForControlLabel.at(Push2::ControlLabel::topKnob4);
    static const int KNOB_5 = ccNumberForControlLabel.at(Push2::ControlLabel::topKnob5);
    static const int KNOB_6 = ccNumberForControlLabel.at(Push2::ControlLabel::topKnob6);
    static const int KNOB_7 = ccNumberForControlLabel.at(Push2::ControlLabel::topKnob7);
    static const int KNOB_8 = ccNumberForControlLabel.at(Push2::ControlLabel::topKnob8);
    static const int KNOB_9 = ccNumberForControlLabel.at(Push2::ControlLabel::topKnob9);
    static const int KNOB_10 = ccNumberForControlLabel.at(Push2::ControlLabel::topKnob10);

    if (midiCcNumber == MASTER_KNOB) {
        return "";
    } else if (midiCcNumber == KNOB_3) {
        return amp1Id;
    } else if (midiCcNumber == KNOB_4) {
        return freq1Id;
    } else if (midiCcNumber == KNOB_5) {
        return amp2Id;
    } else if (midiCcNumber == KNOB_6) {
        return freq2Id;
    }


    return "";
}
int i = 0;

// listened to and called on a non-audio thread, called by MainContentComponent
void MainProcessor::handleControlMidi(const MidiMessage &midiMessage) {
    if (!midiMessage.isController())
        return;

    const int ccNumber = midiMessage.getControllerNumber();
    const String& parameterIndex = parameterIndexForMidiCcNumber(ccNumber);

    if (parameterIndex == "")
        return;

    float value = Push2::encoderCcMessageToRotationChange(midiMessage);
    auto param = treeState.getParameter(parameterIndex);
    const NormalisableRange<float> &range = treeState.getParameterRange(parameterIndex);

    std::cout << "param val: " << param->getValue() << '\n';
    std::cout << "value: " << value << '\n';
    auto newValue = param->getValue() + value / 10.0f;

    if (newValue > 0)
        param->setValueNotifyingHost(newValue);

}

const String MainProcessor::getName() const {
    return "MainProcessor";
}

void MainProcessor::prepareToPlay(double sampleRate, int estimatedSamplesPerBlock) {
    this->setPlayConfigDetails(getTotalNumInputChannels(), getTotalNumOutputChannels(), sampleRate, estimatedSamplesPerBlock);
}

void MainProcessor::releaseResources() {

}

void MainProcessor::processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) {
    MixerAudioSource mixerAudioSource;
    mixerAudioSource.addInputSource(toneSource1.get(), false);
    mixerAudioSource.addInputSource(toneSource2.get(), false);
    mixerAudioSource.getNextAudioBlock(AudioSourceChannelInfo(buffer));
    //toneSource2->getNextAudioBlock(AudioSourceChannelInfo(buffer));
    //toneSource3->getNextAudioBlock(AudioSourceChannelInfo(buffer));
    //toneSource4->getNextAudioBlock(AudioSourceChannelInfo(buffer));
}

const String MainProcessor::getInputChannelName(int channelIndex) const {
    return "channel " + String(channelIndex);
}

const String MainProcessor::getOutputChannelName(int channelIndex) const {
    return "channel " + String(channelIndex);
}

bool MainProcessor::isInputChannelStereoPair(int index) const {
    return (2 == getTotalNumInputChannels());
}

bool MainProcessor::isOutputChannelStereoPair(int index) const {
    return (2 == getTotalNumOutputChannels());
}

bool MainProcessor::silenceInProducesSilenceOut() const {
    return true;
}

bool MainProcessor::acceptsMidi() const {
    return false;
}

bool MainProcessor::producesMidi() const {
    return false;
}

AudioProcessorEditor* MainProcessor::createEditor() {
    return nullptr;
}

bool MainProcessor::hasEditor() const {
    return false;
}

int MainProcessor::getNumParameters() {
    return 0;
}

const String MainProcessor::getParameterName(int parameterIndex) {
    return "parameter " + String(parameterIndex);
}

const String MainProcessor::getParameterText(int parameterIndex) {
    return "0.0";
}

int MainProcessor::getNumPrograms() {
    return 0;
}

int MainProcessor::getCurrentProgram() {
    return 0;
}

void MainProcessor::setCurrentProgram(int index) {

}

const String MainProcessor::getProgramName(int index) {
    return "program #" + String(index);
}

void MainProcessor::changeProgramName(int index, const String& newName) {

}

void MainProcessor::getStateInformation(juce::MemoryBlock& destData) {

}

void MainProcessor::setStateInformation(const void* data, int sizeInBytes) {

}

double MainProcessor::getTailLengthSeconds() const {
    return 0;
}

