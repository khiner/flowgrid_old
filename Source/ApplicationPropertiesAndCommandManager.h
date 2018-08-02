#pragma once

#include "JuceHeader.h"

namespace CommandIDs {
    static const int open                   = 0x30000;
    static const int save                   = 0x30001;
    static const int saveAs                 = 0x30002;
    static const int newFile                = 0x30003;
    static const int showPluginListEditor   = 0x30100;
    static const int showAudioMidiSettings  = 0x30200;
    static const int aboutBox               = 0x30300;
    static const int allWindowsForward      = 0x30400;
}

class ApplicationPropertiesAndCommandManager {
public:
    ApplicationPropertiesAndCommandManager() {
        PropertiesFile::Options options;
        options.applicationName = ProjectInfo::projectName;
        options.filenameSuffix = "settings";
        options.osxLibrarySubFolder = "Preferences";
        applicationProperties.setStorageParameters(options);
    }

    ApplicationProperties applicationProperties;
    ApplicationCommandManager commandManager;
};

ApplicationCommandManager& getCommandManager();
ApplicationProperties& getApplicationProperties();
