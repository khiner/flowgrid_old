#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

namespace DeviceManagerUtilities {
static String getAudioChannelName(AudioDeviceManager &deviceManager, int channelIndex, bool input) {
    if (auto *audioDevice = deviceManager.getCurrentAudioDevice()) {
        auto channelNames = input ? audioDevice->getInputChannelNames() : audioDevice->getOutputChannelNames();
        auto activeChannels = input ? audioDevice->getActiveInputChannels()
                                    : audioDevice->getActiveOutputChannels();
        int activeIndex = 0;
        for (int i = 0; i < channelNames.size(); i++) {
            if (activeChannels[i] && activeIndex++ == channelIndex) {
                return channelNames[i];
            }
        }
    }
    return "";
}
}
