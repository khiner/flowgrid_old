#include <push2/Push2MidiCommunicator.h>
#include "MainProcessor.h"

typedef Push2MidiCommunicator Push2;
typedef Push2::ControlLabel Push2CL;
const static auto& ccForCL = Push2::ccNumberForControlLabel;

MainProcessor::MainProcessor(int inputChannelCount, int outputChannelCount):
        treeState(*this, nullptr),
        toneSource1(treeState, "1"),
        toneSource2(treeState, "2"),
        toneSource3(treeState, "3"),
        toneSource4(treeState, "4"),
        masterVolumeParamId("masterVolume") {

    this->setLatencySamples(0);

    // if this was a plug-in, the plug-in wrapper code in JUCE would query us
    // for our channel configuration and call the setPlayConfigDetails() method
    // so that things would be set correctly internally as an AudioProcessor
    // object (which are always initialized as zero in, zero out). The sample rate
    // and blockSize values will get sent to us again when our prepareToPlay()
    // method is called before playback begins.
    this->setPlayConfigDetails(inputChannelCount, outputChannelCount, 0, 0);

    masterVolumeParam = treeState.createAndAddParameter("masterVolume", "Volume", "Volume",
                                    NormalisableRange<float>(0.0f, 1.0f),
                                    0.5f,
                                    [](float value) { return String(value*1000) + "ms"; },
                                    [](const String& text) { return text.getFloatValue()/1000.0f; });

    mixerAudioSource.addInputSource(toneSource1.get(), false);
    mixerAudioSource.addInputSource(toneSource2.get(), false);
    mixerAudioSource.addInputSource(toneSource3.get(), false);
    mixerAudioSource.addInputSource(toneSource4.get(), false);
}

const StringRef MainProcessor::parameterIdForMidiCcNumber(const int midiCcNumber) const {
    static const int MASTER_KNOB = ccForCL.at(Push2::ControlLabel::masterKnob);
    static const int KNOB_1 = ccForCL.at(Push2CL::topKnob1);
    static const int KNOB_2 = ccForCL.at(Push2CL::topKnob2);
    static const int KNOB_3 = ccForCL.at(Push2CL::topKnob3);
    static const int KNOB_4 = ccForCL.at(Push2CL::topKnob4);
    static const int KNOB_5 = ccForCL.at(Push2CL::topKnob5);
    static const int KNOB_6 = ccForCL.at(Push2CL::topKnob6);
    static const int KNOB_7 = ccForCL.at(Push2CL::topKnob7);
    static const int KNOB_8 = ccForCL.at(Push2CL::topKnob8);
    static const int KNOB_9 = ccForCL.at(Push2CL::topKnob9);
    static const int KNOB_10 = ccForCL.at(Push2CL::topKnob10);

    if (midiCcNumber == MASTER_KNOB) {
        return masterVolumeParamId;
    } else if (midiCcNumber == KNOB_3) {
        return toneSource1.getAmpParamId();
    } else if (midiCcNumber == KNOB_4) {
        return toneSource1.getFreqParamdId();
    } else if (midiCcNumber == KNOB_5) {
        return toneSource2.getAmpParamId();
    } else if (midiCcNumber == KNOB_6) {
        return toneSource2.getFreqParamdId();
    } else if (midiCcNumber == KNOB_7) {
        return toneSource3.getAmpParamId();
    } else if (midiCcNumber == KNOB_8) {
        return toneSource3.getFreqParamdId();
    } else if (midiCcNumber == KNOB_9) {
        return toneSource4.getAmpParamId();
    } else if (midiCcNumber == KNOB_10) {
        return toneSource4.getFreqParamdId();
    }

    const static String emptyString {};
    return emptyString;
}

// listened to and called on a non-audio thread, called by MainContentComponent
void MainProcessor::handleControlMidi(const MidiMessage &midiMessage) {
    if (!midiMessage.isController())
        return;

    const int ccNumber = midiMessage.getControllerNumber();
    const StringRef parameterId = parameterIdForMidiCcNumber(ccNumber);

    if (parameterId.isEmpty())
        return;

    float value = Push2::encoderCcMessageToRotationChange(midiMessage);
    auto param = treeState.getParameter(parameterId);

    auto newValue = param->getValue() + value / 5.0f; // todo move manual scaling to param

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
    mixerAudioSource.getNextAudioBlock(AudioSourceChannelInfo(buffer));
    buffer.applyGain(masterVolumeParam->getValue() * 50.0f); // todo get rid of manual scaling and replace with LinearSmoothedValue (or dsp::Gain object)
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

