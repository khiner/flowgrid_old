#include "PluginManager.h"

PluginManager::PluginManager() {
    if (auto savedPluginList = getUserSettings()->getXmlValue(PLUGIN_LIST_FILE_NAME))
        knownPluginListExternal.recreateFromXml(*savedPluginList);

    for (auto &pluginType : getInternalPluginDescriptions()) {
        knownPluginListInternal.addType(pluginType);
        if (!InternalPluginFormat::isIoProcessor(pluginType.name) &&
            !InternalPluginFormat::isTrackIOProcessor(pluginType.name))
            userCreatablePluginListInternal.addType(pluginType);
    }

    pluginSortMethod = (KnownPluginList::SortMethod) getUserSettings()->getIntValue("pluginSortMethod", KnownPluginList::sortByCategory);
    knownPluginListExternal.addChangeListener(this);

    formatManager.addDefaultFormats();
    formatManager.addFormat(new InternalPluginFormat());
}

PluginListComponent *PluginManager::makePluginListComponent() {
    const File &deadMansPedalFile = getUserSettings()->getFile().getSiblingFile("RecentlyCrashedPluginsList");
    return new PluginListComponent(formatManager, knownPluginListExternal, deadMansPedalFile, getUserSettings(), true);
}

std::unique_ptr<PluginDescription> PluginManager::getDescriptionForIdentifier(const String &identifier) {
    auto description = knownPluginListInternal.getTypeForIdentifierString(identifier);
    return description != nullptr ? std::move(description) : knownPluginListExternal.getTypeForIdentifierString(identifier);
}

void PluginManager::addPluginsToMenu(PopupMenu &menu, const ValueTree &track) {
    PopupMenu internalSubMenu;
    PopupMenu externalSubMenu;

    externalPluginDescriptions = knownPluginListExternal.getTypes();
    userCreatableInternalPluginDescriptions = userCreatablePluginListInternal.getTypes();

    KnownPluginList::addToMenu (internalSubMenu, userCreatableInternalPluginDescriptions, pluginSortMethod);
    KnownPluginList::addToMenu (externalSubMenu, externalPluginDescriptions, pluginSortMethod, String(), getInternalPluginDescriptions().size());

    menu.addSubMenu("Internal", internalSubMenu, true);
    menu.addSeparator();
    menu.addSubMenu("External", externalSubMenu, true);
}

PluginDescription PluginManager::getChosenType(const int menuId) {
    int internalPluginListIndex = KnownPluginList::getIndexChosenByMenu(userCreatableInternalPluginDescriptions, menuId);
    if (internalPluginListIndex != -1)
        return userCreatableInternalPluginDescriptions[internalPluginListIndex];
    int externalPluginListIndex = KnownPluginList::getIndexChosenByMenu(externalPluginDescriptions, menuId - getInternalPluginDescriptions().size());
    if (externalPluginListIndex != -1)
        return externalPluginDescriptions[externalPluginListIndex];
    return {};
}

void PluginManager::changeListenerCallback(ChangeBroadcaster *changed) {
    if (changed == &knownPluginListExternal) {
        // save the plugin list every time it gets changed, so that if we're scanning
        // and it crashes, we've still saved the previous ones
        if (auto savedPluginList = knownPluginListExternal.createXml()) {
            getUserSettings()->setValue(PLUGIN_LIST_FILE_NAME, savedPluginList.get());
            getApplicationProperties().saveIfNeeded();
        }
    }
}
