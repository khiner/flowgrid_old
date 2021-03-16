#include <Utilities.h>
#include "view/push2/Push2Component.h"
#include "ApplicationPropertiesAndCommandManager.h"
#include <view/graph_editor/GraphEditor.h>
#include <view/CustomColourIds.h>
#include "view/BasicWindow.h"
#include "DeviceChangeMonitor.h"

class FlowGridApplication : public JUCEApplication, public MenuBarModel, private ChangeListener, private Utilities::ValueTreePropertyChangeListener {
public:
    FlowGridApplication() : project(undoManager, pluginManager, deviceManager),
                            tracks(project.getTracks()),
                            push2Colours(tracks),
                            push2MidiCommunicator(project, push2Colours),
                            processorGraph(project, tracks, project.getConnections(), project.getInput(), project.getOutput(),
                                               undoManager, deviceManager, push2MidiCommunicator) {}

    const String getApplicationName() override { return ProjectInfo::projectName; }

    const String getApplicationVersion() override { return ProjectInfo::versionString; }

    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const String &) override {
        Process::makeForegroundProcess();

        project.addChangeListener(this);
        undoManager.addChangeListener(this);

        deviceChangeMonitor = std::make_unique<DeviceChangeMonitor>(deviceManager);

        setMacMainMenu(this);

        auto &lookAndFeel = LookAndFeel::getDefaultLookAndFeel();
        lookAndFeel.setDefaultSansSerifTypeface(Typeface::createSystemTypefaceFor(
                BinaryData::AbletonSansMediumRegular_otf, BinaryData::AbletonSansMediumRegular_otfSize));
        lookAndFeel.setColour(CustomColourIds::unfocusedOverlayColourId, Colours::black.withAlpha(0.15f));
        lookAndFeel.setColour(CustomColourIds::defaultAudioConnectionColourId, Colours::yellow);
        lookAndFeel.setColour(CustomColourIds::customAudioConnectionColourId, Colours::grey); // TODO surround with white instead
        lookAndFeel.setColour(CustomColourIds::defaultMidiConnectionColourId, Colours::orange);
        lookAndFeel.setColour(CustomColourIds::customMidiConnectionColourId, Colours::red); // TODO surround with white instead

        lookAndFeel.setColour(Slider::rotarySliderOutlineColourId,
                              lookAndFeel.findColour(Slider::rotarySliderOutlineColourId).brighter(0.06));

        pluginListComponent = std::unique_ptr<PluginListComponent>(pluginManager.makePluginListComponent());

        auto savedAudioState = getUserSettings()->getXmlValue("audioDeviceState");
        deviceManager.initialise(256, 256, savedAudioState.get(), true);

        deviceManager.addChangeListener(this);

        mainWindow = std::make_unique<MainWindow>(*this, "FlowGrid", new GraphEditor(processorGraph, project));
        mainWindow->setBoundsRelative(0.02, 0.02, 0.96, 0.96);

        push2Component = std::make_unique<Push2Component>(project, push2MidiCommunicator);

        push2MidiCommunicator.setPush2Listener(push2Component.get());

        player.setProcessor(&processorGraph);
        deviceManager.addAudioCallback(&player);

        project.initialize();
        processorGraph.removeIllegalConnections();
        undoManager.clearUndoHistory();

        getCommandManager().registerAllCommandsForTarget(this);
        push2Component->setVisible(true);
    }

    void shutdown() override {
        push2Component = nullptr;
        push2Window = nullptr;
        deviceChangeMonitor = nullptr;
        deviceManager.removeAudioCallback(&player);
        undoManager.removeChangeListener(this);
        project.removeChangeListener(this);
        setMacMainMenu(nullptr);
    }

    void systemRequestedQuit() override {
        // This is called when the app is being asked to quit: you can ignore this
        // request and let the app carry on running, or call quit() to allow the app to close.
        if (mainWindow != nullptr)
            mainWindow->tryToQuitApplication();
        else
            quit();
    }

    void anotherInstanceStarted(const String &commandLine) override {
        // When another instance of the app is launched while this one is running,
        // this method is invoked, and the commandLine parameter tells you what
        // the other instance's command-line arguments were.
    }

    StringArray getMenuBarNames() override {
        StringArray names{"File", "Edit", "Create", "View", "Options"};
        return names;
    }

    PopupMenu getMenuForIndex(int topLevelMenuIndex, const String & /*menuName*/) override {
        PopupMenu menu;

        if (topLevelMenuIndex == 0) { // File menu
            menu.addCommandItem(&getCommandManager(), CommandIDs::newFile);
            menu.addCommandItem(&getCommandManager(), CommandIDs::open);

            RecentlyOpenedFilesList recentFiles;
            recentFiles.restoreFromString(getUserSettings()->getValue("recentProjectFiles"));

            PopupMenu recentFilesMenu;
            recentFiles.createPopupMenuItems(recentFilesMenu, 100, true, true);
            menu.addSubMenu("Open recent project", recentFilesMenu);

            menu.addCommandItem(&getCommandManager(), CommandIDs::save);
            menu.addCommandItem(&getCommandManager(), CommandIDs::saveAs);
            menu.addSeparator();
            menu.addCommandItem(&getCommandManager(), StandardApplicationCommandIDs::quit);
        } else if (topLevelMenuIndex == 1) { // Edit menu
            menu.addCommandItem(&getCommandManager(), CommandIDs::undo);
            menu.addCommandItem(&getCommandManager(), CommandIDs::redo);
            menu.addSeparator();
            menu.addCommandItem(&getCommandManager(), CommandIDs::copySelected);
            menu.addCommandItem(&getCommandManager(), CommandIDs::insert);
            menu.addCommandItem(&getCommandManager(), CommandIDs::duplicateSelected);
            menu.addSeparator();
            menu.addCommandItem(&getCommandManager(), CommandIDs::deleteSelected);
        } else if (topLevelMenuIndex == 2) { // Create menu
            menu.addCommandItem(&getCommandManager(), CommandIDs::insertTrack);
            menu.addCommandItem(&getCommandManager(), CommandIDs::insertProcessorLane);
            menu.addCommandItem(&getCommandManager(), CommandIDs::createMasterTrack);
        } else if (topLevelMenuIndex == 3) { // View menu
            menu.addCommandItem(&getCommandManager(), CommandIDs::showPush2MirrorWindow);
        } else if (topLevelMenuIndex == 4) { // Options menu
            menu.addCommandItem(&getCommandManager(), CommandIDs::showAudioMidiSettings);
            menu.addCommandItem(&getCommandManager(), CommandIDs::showPluginListEditor);

            const auto &pluginSortMethod = pluginManager.getPluginSortMethod();

            PopupMenu sortTypeMenu;
            sortTypeMenu.addItem(200, "List plugins in default order", true, pluginSortMethod == KnownPluginList::defaultOrder);
            sortTypeMenu.addItem(201, "List plugins in alphabetical order", true, pluginSortMethod == KnownPluginList::sortAlphabetically);
            sortTypeMenu.addItem(202, "List plugins by category", true, pluginSortMethod == KnownPluginList::sortByCategory);
            sortTypeMenu.addItem(203, "List plugins by manufacturer", true, pluginSortMethod == KnownPluginList::sortByManufacturer);
            sortTypeMenu.addItem(204, "List plugins based on the directory structure", true, pluginSortMethod == KnownPluginList::sortByFileSystemLocation);
            menu.addSubMenu("Plugin menu type", sortTypeMenu);
        }

        return menu;
    }

    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override {
        if (topLevelMenuIndex == 0) { // File menu
            if (menuItemID >= 100 && menuItemID < 200) {
                RecentlyOpenedFilesList recentFiles;
                recentFiles.restoreFromString(getUserSettings()->getValue("recentProjectFiles"));

                if (project.saveIfNeededAndUserAgrees() == FileBasedDocument::savedOk) {
                    project.loadFrom(recentFiles.getFile(menuItemID - 100), true);
                }
            }
        } else if (topLevelMenuIndex == 1) { // Edit menu
        } else if (topLevelMenuIndex == 2) { // Create menu
        } else if (topLevelMenuIndex == 3) { // View menu
        } else if (topLevelMenuIndex == 4) { // Options menu
            if (menuItemID == 1) {
                showAudioMidiSettings();
            } else if (menuItemID >= 200 && menuItemID < 210) {
                if (menuItemID == 200) pluginManager.setPluginSortMethod(KnownPluginList::defaultOrder);
                else if (menuItemID == 201) pluginManager.setPluginSortMethod(KnownPluginList::sortAlphabetically);
                else if (menuItemID == 202) pluginManager.setPluginSortMethod(KnownPluginList::sortByCategory);
                else if (menuItemID == 203) pluginManager.setPluginSortMethod(KnownPluginList::sortByManufacturer);
                else if (menuItemID == 204) pluginManager.setPluginSortMethod(KnownPluginList::sortByFileSystemLocation);

                getUserSettings()->setValue("pluginSortMethod", (int) pluginManager.getPluginSortMethod());
                menuItemsChanged();
            }
        }
    }

    void menuBarActivated(bool isActivated) override {}

    void getAllCommands(Array<CommandID> &commands) override {
        static const CommandID ids[] = {
                CommandIDs::newFile,
                CommandIDs::open,
                CommandIDs::save,
                CommandIDs::saveAs,
                CommandIDs::undo,
                CommandIDs::redo,
                CommandIDs::deleteSelected,
                CommandIDs::copySelected,
                CommandIDs::insert,
                CommandIDs::duplicateSelected,
                CommandIDs::insertTrack,
                CommandIDs::insertProcessorLane,
                CommandIDs::createMasterTrack,
                CommandIDs::showPush2MirrorWindow,
                CommandIDs::navigateLeft,
                CommandIDs::navigateRight,
                CommandIDs::navigateUp,
                CommandIDs::navigateDown,
                CommandIDs::showPluginListEditor,
                CommandIDs::showAudioMidiSettings,
                CommandIDs::togglePaneFocus,
//                CommandIDs::aboutBox,
//                CommandIDs::allWindowsForward,
        };

        commands.addArray(ids, numElementsInArray(ids));
    }

    void getCommandInfo(const CommandID commandID, ApplicationCommandInfo &result) override {
        const String category("General");

        switch (commandID) {
            case CommandIDs::newFile:
                result.setInfo("New", "Creates a new project", category, 0);
                result.defaultKeypresses.add(KeyPress('n', ModifierKeys::commandModifier, 0));
                break;
            case CommandIDs::open:
                result.setInfo("Open...", "Opens a project", category, 0);
                result.defaultKeypresses.add(KeyPress('o', ModifierKeys::commandModifier, 0));
                break;
            case CommandIDs::save:
                result.setInfo("Save", "Saves the current project", category, 0);
                result.defaultKeypresses.add(KeyPress('s', ModifierKeys::commandModifier, 0));
                result.setActive(project.hasChangedSinceSaved());
                break;
            case CommandIDs::saveAs:
                result.setInfo("Save As...", "Saves a copy of the current project", category, 0);
                result.defaultKeypresses.add(KeyPress('s', ModifierKeys::shiftModifier | ModifierKeys::commandModifier, 0));
                break;
            case CommandIDs::undo:
                result.setInfo("Undo", String(), category, 0);
                result.addDefaultKeypress('z', ModifierKeys::commandModifier);
                result.setActive(undoManager.canUndo());
                break;
            case CommandIDs::redo:
                result.setInfo("Redo", String(), category, 0);
                result.addDefaultKeypress('z', ModifierKeys::commandModifier | ModifierKeys::shiftModifier);
                result.setActive(undoManager.canRedo());
                break;
            case CommandIDs::copySelected:
                result.setInfo("Copy", String(), category, 0);
                result.addDefaultKeypress('c', ModifierKeys::commandModifier);
                result.setActive(tracks.anyTrackHasSelections());
                break;
            case CommandIDs::insert:
                result.setInfo("Insert", String(), category, 0);
                result.addDefaultKeypress('v', ModifierKeys::commandModifier);
                result.setActive(project.hasCopy());
                break;
            case CommandIDs::duplicateSelected:
                result.setInfo("Duplicate selected item(s)", String(), category, 0);
                result.addDefaultKeypress('d', ModifierKeys::commandModifier);
                result.setActive(tracks.anyTrackHasSelections());
                break;
            case CommandIDs::deleteSelected:
                result.setInfo("Delete selected item(s)", String(), category, 0);
                result.addDefaultKeypress(KeyPress::deleteKey, ModifierKeys::noModifiers);
                result.addDefaultKeypress(KeyPress::backspaceKey, ModifierKeys::noModifiers);
                result.setActive(tracks.getFocusedTrack().isValid());
                break;
            case CommandIDs::insertTrack:
                result.setInfo("Insert track", String(), category, 0);
                result.addDefaultKeypress('t', ModifierKeys::commandModifier);
                break;
            case CommandIDs::insertProcessorLane:
                result.setInfo("Insert processor lane", String(), category, 0);
                result.addDefaultKeypress('t', ModifierKeys::commandModifier | ModifierKeys::shiftModifier);
                break;
            case CommandIDs::createMasterTrack:
                result.setInfo("Create master track", String(), category, 0);
                result.addDefaultKeypress('m', ModifierKeys::commandModifier | ModifierKeys::shiftModifier);
                result.setActive(!tracks.getMasterTrack().isValid());
                break;
            case CommandIDs::showPush2MirrorWindow:
                result.setInfo("Open a window mirroring a Push 2 display", String(), category, 0);
                break;
            case CommandIDs::navigateLeft:
                result.setInfo("Select whatever is to the left of the current selection", String(), category, 0);
                result.addDefaultKeypress(KeyPress::leftKey, ModifierKeys::noModifiers);
                result.addDefaultKeypress(KeyPress::leftKey, ModifierKeys::shiftModifier);
                break;
            case CommandIDs::navigateRight:
                result.setInfo("Select whatever is to the right of the current selection", String(), category, 0);
                result.addDefaultKeypress(KeyPress::rightKey, ModifierKeys::noModifiers);
                result.addDefaultKeypress(KeyPress::rightKey, ModifierKeys::shiftModifier);
                break;
            case CommandIDs::navigateUp:
                result.setInfo("Select whatever is above the current selection", String(), category, 0);
                result.addDefaultKeypress(KeyPress::upKey, ModifierKeys::noModifiers);
                result.addDefaultKeypress(KeyPress::upKey, ModifierKeys::shiftModifier);
                break;
            case CommandIDs::navigateDown:
                result.setInfo("Select whatever is below the current selection", String(), category, 0);
                result.addDefaultKeypress(KeyPress::downKey, ModifierKeys::noModifiers);
                result.addDefaultKeypress(KeyPress::downKey, ModifierKeys::shiftModifier);
                break;
            case CommandIDs::showPluginListEditor:
                result.setInfo("Edit the list of available plugins", String(), category, 0);
                result.addDefaultKeypress('p', ModifierKeys::commandModifier);
                break;
            case CommandIDs::showAudioMidiSettings:
                result.setInfo("Change the audio device settings", String(), category, 0);
                result.addDefaultKeypress('a', ModifierKeys::commandModifier);
                break;
            case CommandIDs::togglePaneFocus: // keypress only; not in menu
                result.setInfo("Change the focused pane", String(), category, 0);
                result.addDefaultKeypress(KeyPress::tabKey, ModifierKeys::noModifiers);
                break;
//            case CommandIDs::aboutBox:
//                result.setInfo ("About...", String(), category, 0);
//                break;
//            case CommandIDs::allWindowsForward:
//                result.setInfo ("All Windows Forward", "Bring all plug-in windows forward", category, 0);
//                result.addDefaultKeypress ('w', ModifierKeys::commandModifier);
//                break;
            default:
                break;
        }
    }

    bool perform(const InvocationInfo &info) override {
        switch (info.commandID) {
            case CommandIDs::newFile:
                if (project.saveIfNeededAndUserAgrees() == FileBasedDocument::savedOk)
                    project.newDocument();
                break;
            case CommandIDs::open:
                if (project.saveIfNeededAndUserAgrees() == FileBasedDocument::savedOk)
                    project.loadFromUserSpecifiedFile(true);
                break;
            case CommandIDs::save:
                project.save(true, true);
                break;
            case CommandIDs::saveAs:
                project.saveAs(File(), true, true, true);
                break;
            case CommandIDs::undo:
                project.undo();
                break;
            case CommandIDs::redo:
                project.redo();
                break;
            case CommandIDs::copySelected:
                project.copySelectedItems();
                applicationCommandListChanged(); // enable paste menu item
                break;
            case CommandIDs::insert:
                project.insert();
                break;
            case CommandIDs::duplicateSelected:
                project.duplicateSelectedItems();
                break;
            case CommandIDs::deleteSelected:
                project.deleteSelectedItems();
                break;
            case CommandIDs::insertTrack:
                project.createTrack(false);
                break;
            case CommandIDs::insertProcessorLane:
                // TODO
                break;
            case CommandIDs::createMasterTrack:
                project.createTrack(true);
                break;
            case CommandIDs::showPush2MirrorWindow:
                showPush2MirrorWindow();
                break;
            case CommandIDs::navigateLeft:
                project.navigateLeft();
                break;
            case CommandIDs::navigateRight:
                project.navigateRight();
                break;
            case CommandIDs::navigateUp:
                project.navigateUp();
                break;
            case CommandIDs::navigateDown:
                project.navigateDown();
                break;
            case CommandIDs::showPluginListEditor:
                showPluginList();
                break;
            case CommandIDs::showAudioMidiSettings:
                showAudioMidiSettings();
                break;
            case CommandIDs::togglePaneFocus:
                project.getView().togglePaneFocus();
                break;
//            case CommandIDs::aboutBox:
//                // TODO
//                break;
//            case CommandIDs::allWindowsForward:
//            {
//                auto& desktop = Desktop::getInstance();
//
//                for (int i = 0; i < desktop.getNumComponents(); ++i)
//                    desktop.getComponent (i)->toBehind (this);
//
//                break;
//            }
            default:
                return false;
        }

        return true;
    }

    class MainWindow : public DocumentWindow, public FileDragAndDropTarget {
    public:
        explicit MainWindow(FlowGridApplication &owner, const String &name, GraphEditor *graphEditor) :
                DocumentWindow(name, Colours::lightgrey, DocumentWindow::allButtons), owner(owner), graphEditor(graphEditor) {
            graphEditor->setSize(1, 1); // nonzero size to avoid warnings
            setContentOwned(graphEditor, true);
            setResizable(true, true);

            centreWithSize(getWidth(), getHeight());
            setVisible(true);
            setBackgroundColour(getLookAndFeel().findColour(ResizableWindow::backgroundColourId).brighter(0.04));
            addKeyListener(getCommandManager().getKeyMappings());
        }

        void modifierKeysChanged(const ModifierKeys &modifiers) override {
            DocumentWindow::modifierKeysChanged(modifiers);
            owner.project.setShiftHeld(modifiers.isShiftDown());
            owner.project.setAltHeld(modifiers.isAltDown());
        }

        void closeButtonPressed() override {
            tryToQuitApplication();
        }

        KeyboardFocusTraverser *createFocusTraverser() override {
            return nullptr;
        }

        struct AsyncQuitRetrier : private Timer {
            AsyncQuitRetrier() { startTimer(500); }

            void timerCallback() override {
                stopTimer();
                delete this;

                if (auto app = JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            }
        };

        void tryToQuitApplication() {
            if (owner.project.saveIfNeededAndUserAgrees() != FileBasedDocument::savedOk)
                return;

            if (graphEditor->closeAnyOpenPluginWindows()) {
                // Really important thing to note here: if the last call just deleted any plugin windows,
                // we won't exit immediately - instead we'll use our AsyncQuitRetrier to let the message
                // loop run for another brief moment, then try again. This will give any plugins a chance
                // to flush any GUI events that may have been in transit before the app forces them to be unloaded.
                new AsyncQuitRetrier();
            } else if (ModalComponentManager::getInstance()->cancelAllModalComponents()) {
                new AsyncQuitRetrier();
            } else {
                // Some plug-ins do not want [NSApp stop] to be called
                // before the plug-ins are deallocated.
//                owner.releaseGraph();
                JUCEApplication::quit();
            }
        }

        bool isInterestedInFileDrag(const StringArray &files) override {
            return true;
        }

        void filesDropped(const StringArray &files, int x, int y) override {
            if (files.size() == 1 && File(files[0]).hasFileExtension(Project::getFilenameSuffix())) {
                if (owner.project.saveIfNeededAndUserAgrees() == FileBasedDocument::savedOk)
                    owner.project.loadFrom(File(files[0]), true);
            }
        }

        void fileDragEnter(const StringArray &files, int, int) override {}

        void fileDragMove(const StringArray &files, int, int) override {}

        void fileDragExit(const StringArray &files) override {}

    private:
        FlowGridApplication &owner;
        GraphEditor *graphEditor;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    ApplicationPropertiesAndCommandManager applicationPropertiesAndCommandManager;

private:
    PluginManager pluginManager;

    std::unique_ptr<Push2Component> push2Component;
    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<DocumentWindow> push2Window;
    std::unique_ptr<PluginListComponent> pluginListComponent;

    UndoManager undoManager;
    AudioDeviceManager deviceManager;

    Project project;
    TracksState &tracks;
    Push2Colours push2Colours;
    Push2MidiCommunicator push2MidiCommunicator;
    ProcessorGraph processorGraph;
    AudioProcessorPlayer player;

    std::unique_ptr<DeviceChangeMonitor> deviceChangeMonitor;

    void showAudioMidiSettings() {
        auto *audioSettingsComponent = new AudioDeviceSelectorComponent(deviceManager, 2, 256, 2, 256, true, true, true, false);
        audioSettingsComponent->setSize(500, 450);

        DialogWindow::LaunchOptions o;
        o.content.setOwned(audioSettingsComponent);
        o.dialogTitle = "Audio Settings";
        o.componentToCentreAround = mainWindow.get();
        o.dialogBackgroundColour = mainWindow->findColour(ResizableWindow::backgroundColourId);
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = false;
        o.resizable = true;

        auto *w = o.create();
        w->enterModalState(true, nullptr, true);
    }

    void showPluginList() {
        DialogWindow::LaunchOptions o;
        o.content.setNonOwned(pluginListComponent.get());
        o.dialogTitle = "Available Plugins";
        o.componentToCentreAround = mainWindow.get();
        o.dialogBackgroundColour = mainWindow->findColour(ResizableWindow::backgroundColourId);
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = false;
        o.resizable = true;

        auto *w = o.create();
        w->enterModalState(true, ModalCallbackFunction::create([this](int) {}), true);
    }

    void showPush2MirrorWindow() {
        if (push2Window == nullptr) {
            push2Window = std::make_unique<BasicWindow>("Push 2 Mirror", push2Component.get(), false, [this]() { push2Window = nullptr; });
            push2Window->setBackgroundColour(Colours::black);
            push2Window->setBounds(100, 100, Push2Display::WIDTH, Push2Display::HEIGHT + mainWindow->getTitleBarHeight());
            push2Window->setResizable(false, false);
            push2Window->setAlwaysOnTop(true);
        }
    }

    void changeListenerCallback(ChangeBroadcaster *source) override {
        if (source == &project) {
            mainWindow->setName(project.getDocumentTitle());
        } else if (source == &undoManager) {
            applicationCommandListChanged();
        } else if (source == &deviceManager) {
            if (!push2MidiCommunicator.isInitialized() && MidiInput::getDevices().contains(push2MidiDeviceName, true)) {
                auto midiInput = MidiInput::openDevice(MidiInput::getDevices().indexOf(push2MidiDeviceName, true), &push2MidiCommunicator);
                auto midiOutput = MidiOutput::openDevice(MidiOutput::getDevices().indexOf(push2MidiDeviceName, true));
                push2MidiCommunicator.setMidiInputAndOutput(std::move(midiInput), std::move(midiOutput));
                push2Component->setVisible(true); // refreshes button lights

                // Always enable Push 2 as a midi input device when it's connected (even if it's been disabled, for simplicity)
                deviceManager.setMidiInputEnabled(push2MidiDeviceName, true);
            } else if (push2MidiCommunicator.isInitialized() && !MidiInput::getDevices().contains(push2MidiDeviceName, true)) {
                push2MidiCommunicator.setMidiInputAndOutput(nullptr, nullptr);
            }
            auto audioState = deviceManager.createStateXml();
            getUserSettings()->setValue("audioDeviceState", audioState.get());
            getUserSettings()->saveIfNeeded();
        }
    }
};

static FlowGridApplication &getApp() { return *dynamic_cast<FlowGridApplication *>(JUCEApplication::getInstance()); }

ApplicationProperties &getApplicationProperties() { return getApp().applicationPropertiesAndCommandManager.applicationProperties; }

PropertiesFile *getUserSettings() { return getApplicationProperties().getUserSettings(); }

ApplicationCommandManager &getCommandManager() { return getApp().applicationPropertiesAndCommandManager.commandManager; }

// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (FlowGridApplication)
