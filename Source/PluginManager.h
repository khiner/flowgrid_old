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

    Array<PluginDescription> &getInternalPluginDescriptions() {
        return internalPluginDescriptions;
    }

    Array<PluginDescription> &getExternalPluginDescriptions() {
        return externalPluginDescriptions;
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

    void addPluginsToMenu(PopupMenu &menu, const ValueTree &track) {
        PopupMenu internalSubMenu;
        PopupMenu externalSubMenu;

        externalPluginDescriptions = knownPluginListExternal.getTypes();

        KnownPluginList::addToMenu (internalSubMenu, internalPluginDescriptions, pluginSortMethod);
        KnownPluginList::addToMenu (externalSubMenu, externalPluginDescriptions, pluginSortMethod, String(), internalPluginDescriptions.size());

        menu.addSubMenu("Internal", internalSubMenu, true);
        menu.addSeparator();
        menu.addSubMenu("External", externalSubMenu, true);
    }

    const PluginDescription getChosenType(const int menuId) const {
        int internalPluginListIndex = KnownPluginList::getIndexChosenByMenu(internalPluginDescriptions, menuId);
        if (internalPluginListIndex != -1)
            return internalPluginDescriptions[internalPluginListIndex];
        int externalPluginListIndex = KnownPluginList::getIndexChosenByMenu(externalPluginDescriptions, menuId - internalPluginDescriptions.size());
        if (externalPluginListIndex != -1)
            return externalPluginDescriptions[externalPluginListIndex];
        return {};
    }

    static bool isGeneratorOrInstrument(const PluginDescription *description) {
        return description->isInstrument || description->category.equalsIgnoreCase("generator") || description->category.equalsIgnoreCase("synth");
    }

private:
    const String PLUGIN_LIST_FILE_NAME = "pluginList";

    InternalPluginFormat internalFormat;
    KnownPluginList knownPluginListExternal;
    KnownPluginList knownPluginListInternal;
    KnownPluginList userCreatablePluginListInternal;

    KnownPluginList::SortMethod pluginSortMethod;
    AudioPluginFormatManager formatManager;

    Array<PluginDescription> externalPluginDescriptions;

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
