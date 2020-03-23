#pragma once

#include "JuceHeader.h"
#include "processors/InternalPluginFormat.h"
#include "state/Identifiers.h"
#include "ApplicationPropertiesAndCommandManager.h"

class PluginManager : private ChangeListener {
public:
    PluginManager() {
        if (auto savedPluginList = getUserSettings()->getXmlValue(PLUGIN_LIST_FILE_NAME))
            knownPluginListExternal.recreateFromXml(*savedPluginList);

        for (auto &pluginType : internalPluginDescriptions) {
            knownPluginListInternal.addType(pluginType);
            if (!InternalPluginFormat::isIoProcessorName(pluginType.name))
                userCreatablePluginListInternal.addType(pluginType);
        }

        pluginSortMethod = (KnownPluginList::SortMethod) getUserSettings()->getIntValue("pluginSortMethod", KnownPluginList::sortByCategory);
        knownPluginListExternal.addChangeListener(this);

        formatManager.addDefaultFormats();
        formatManager.addFormat(new InternalPluginFormat());
    }

    PluginListComponent *makePluginListComponent() {
        const File &deadMansPedalFile = getUserSettings()->getFile().getSiblingFile("RecentlyCrashedPluginsList");
        return new PluginListComponent(formatManager, knownPluginListExternal, deadMansPedalFile, getUserSettings(), true);
    }

    std::unique_ptr<PluginDescription> getDescriptionForIdentifier(const String &identifier) {
        auto description = knownPluginListInternal.getTypeForIdentifierString(identifier);
        return description != nullptr ? std::move(description) : knownPluginListExternal.getTypeForIdentifierString(identifier);
    }

    PluginDescription &getAudioInputDescription() {
        return internalFormat.audioInDesc;
    }

    PluginDescription &getAudioOutputDescription() {
        return internalFormat.audioOutDesc;
    }

    KnownPluginList &getKnownPluginListExternal() {
        return knownPluginListExternal;
    }

    KnownPluginList &getUserCreatablePluginListInternal() {
        return userCreatablePluginListInternal;
    }

    KnownPluginList::SortMethod getPluginSortMethod() const {
        return pluginSortMethod;
    }

    AudioPluginFormatManager &getFormatManager() {
        return formatManager;
    }

    void setPluginSortMethod(const KnownPluginList::SortMethod pluginSortMethod) {
        this->pluginSortMethod = pluginSortMethod;
    }

    // TODO https://github.com/WeAreROLI/JUCE/commit/c88611e5c8e4449012cfdf523177ed50922b9bcc#diff-9353bec3542b3a3f80210989f8a0ac2b
    void addPluginsToMenu(PopupMenu &menu, const ValueTree &track) const {
        StringArray disabledPluginIds;

        PopupMenu internalSubMenu;
        PopupMenu externalSubMenu;

        userCreatablePluginListInternal.addToMenu(internalSubMenu, getPluginSortMethod(), disabledPluginIds);
        knownPluginListExternal.addToMenu(externalSubMenu, getPluginSortMethod(), {}, String(), userCreatablePluginListInternal.getNumTypes());

        menu.addSubMenu("Internal", internalSubMenu, true);
        menu.addSeparator();
        menu.addSubMenu("External", externalSubMenu, true);
    }

    const PluginDescription getChosenType(const int menuId) const {
        int internalPluginListIndex = userCreatablePluginListInternal.getIndexChosenByMenu(menuId);
        if (internalPluginListIndex != -1)
            return userCreatablePluginListInternal.getTypes()[internalPluginListIndex];
        int externalPluginListIndex = knownPluginListExternal.getIndexChosenByMenu(menuId - userCreatablePluginListInternal.getNumTypes());
        if (externalPluginListIndex != -1)
            return knownPluginListExternal.getTypes()[externalPluginListIndex];
        return {};
    }

    static bool isGeneratorOrInstrument(const PluginDescription *description) {
        return description->isInstrument || description->category.equalsIgnoreCase("generator") || description->category.equalsIgnoreCase("synth");
    }

    bool doesTrackAlreadyHaveGeneratorOrInstrument(const ValueTree &track) {
        for (const auto &child : track)
            if (auto existingDescription = getDescriptionForIdentifier(child.getProperty(IDs::id)))
                if (isGeneratorOrInstrument(existingDescription.get()))
                    return true;
        return false;
    }

private:
    const String PLUGIN_LIST_FILE_NAME = "pluginList";

    InternalPluginFormat internalFormat;
    KnownPluginList knownPluginListExternal;
    KnownPluginList knownPluginListInternal;
    KnownPluginList userCreatablePluginListInternal;

    KnownPluginList::SortMethod pluginSortMethod;
    AudioPluginFormatManager formatManager;

    void changeListenerCallback(ChangeBroadcaster *changed) override {
        if (changed == &knownPluginListExternal) {
            // save the plugin list every time it gets changed, so that if we're scanning
            // and it crashes, we've still saved the previous ones
            if (auto savedPluginList = knownPluginListExternal.createXml()) {
                getUserSettings()->setValue(PLUGIN_LIST_FILE_NAME, savedPluginList.get());
                getApplicationProperties().saveIfNeeded();
            }
        }
    }
};
