#pragma once

#include "JuceHeader.h"

namespace CommandIDs {
    static const int open                   = 0x10000;
    static const int save                   = 0x10001;
    static const int saveAs                 = 0x10002;
    static const int newFile                = 0x10003;
    static const int undo                   = 0x20000;
    static const int redo                   = 0x20001;
    static const int deleteSelected         = 0x20002;
    static const int showPush2MirrorWindow  = 0x30000;
    static const int showPluginListEditor   = 0x40000;
    static const int showAudioMidiSettings  = 0x40001;
    static const int aboutBox               = 0x40002;
    static const int allWindowsForward      = 0x40003;
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
