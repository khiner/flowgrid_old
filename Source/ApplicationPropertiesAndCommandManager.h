#pragma once

#include "JuceHeader.h"

namespace CommandIDs {
    static const int
            open = 0x10000,
            save = 0x10001,
            saveAs = 0x10002,
            newFile = 0x10003,
            undo = 0x20000,
            redo = 0x20001,
            copySelected = 0x20002,
            insert = 0x20003,
            duplicateSelected = 0x20004,
            deleteSelected = 0x20005,
            insertTrack = 0x30000,
            insertProcessorLane = 0x30001,
            createMasterTrack = 0x30002,
            showPush2MirrorWindow = 0x40000,
            navigateLeft = 0x40001,
            navigateRight = 0x40002,
            navigateUp = 0x40003,
            navigateDown = 0x40004,
            showPluginListEditor = 0x50000,
            showAudioMidiSettings = 0x50001,
            togglePaneFocus = 0x50002,
            aboutBox = 0x50003,
            allWindowsForward = 0x50004;
}

const String push2MidiDeviceName = "Ableton Push 2 Live Port";

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

ApplicationCommandManager &getCommandManager();

ApplicationProperties &getApplicationProperties();

PropertiesFile *getUserSettings();
