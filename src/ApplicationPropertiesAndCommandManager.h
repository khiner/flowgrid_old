#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "FlowGridConfig.h"

using namespace juce;

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

class ApplicationPropertiesAndCommandManager {
public:
    ApplicationPropertiesAndCommandManager() {
        std::cout << "Project name: " << PROJECT_NAME << std::endl;
        std::cout << "Project version: " << PROJECT_VERSION << std::endl;

        PropertiesFile::Options options;
        options.applicationName = PROJECT_NAME;
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
