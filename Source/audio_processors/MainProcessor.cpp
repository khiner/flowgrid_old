#include <push2/Push2MidiCommunicator.h>
#include "MainProcessor.h"


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

    for (int sliderIndex = 0; sliderIndex < 8; sliderIndex++) {
        auto slider = std::make_unique<Slider>();
        slider->addListener(this);
        sliders.push_back(std::move(slider));
    }

    mixerAudioSource.addInputSource(toneSource1.get(), false);
    mixerAudioSource.addInputSource(toneSource2.get(), false);
    mixerAudioSource.addInputSource(toneSource3.get(), false);
    mixerAudioSource.addInputSource(toneSource4.get(), false);

    sliders[0]->setComponentID(toneSource1.getAmpParamId());
    sliders[1]->setComponentID(toneSource1.getFreqParamId());
    sliders[2]->setComponentID(toneSource2.getAmpParamId());
    sliders[3]->setComponentID(toneSource2.getFreqParamId());
    sliders[4]->setComponentID(toneSource3.getAmpParamId());
    sliders[5]->setComponentID(toneSource3.getFreqParamId());
    sliders[6]->setComponentID(toneSource4.getAmpParamId());
    sliders[7]->setComponentID(toneSource4.getFreqParamId());
}

void MainProcessor::sliderValueChanged(Slider* slider) {
    treeState.getParameter(slider->getComponentID())->setValueNotifyingHost(static_cast<float>(slider->getValue()));
}

// listened to and called on a non-audio thread, called by MainContentComponent
void MainProcessor::handleControlMidi(const MidiMessage &midiMessage) {
    if (!midiMessage.isController())
        return;

    const int ccNumber = midiMessage.getControllerNumber();
    auto parameterIdEntry = parameterIdForMidiNumber.find(ccNumber);
    if (parameterIdEntry == parameterIdForMidiNumber.end()) {
        return;
    }

    float value = Push2::encoderCcMessageToRotationChange(midiMessage);
    auto param = treeState.getParameter(parameterIdEntry->second);

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

