#include "Push2ProcessorSelector.h"

Push2ProcessorSelector::ProcessorSelectorRow::ProcessorSelectorRow(Push2 &push2, bool top) {
    for (int i = 0; i < NUM_COLUMNS; i++) {
        auto *label = new Push2Label(i, top, push2);
        addChildComponent(label);
        label->setJustificationType(Justification::centred);
        labels.add(label);
    }
    addAndMakeVisible(selectionRectangleOverlay);
}

void Push2ProcessorSelector::ProcessorSelectorRow::setVisible(bool visible) {
    Component::setVisible(visible);
    updateLabels();
}

KnownPluginList::PluginTree *Push2ProcessorSelector::ProcessorSelectorRow::selectFolder(int index) {
    if (currentTree == nullptr) return nullptr;

    if (index + currentViewOffset < currentTree->subFolders.size()) {
        auto *subfolder = currentTree->subFolders.getUnchecked(index + currentViewOffset);
        selectFolder(subfolder);
        return subfolder;
    }

    return nullptr;
}

void Push2ProcessorSelector::ProcessorSelectorRow::selectFolder(KnownPluginList::PluginTree *subfolder) {
    jassert(currentTree->subFolders.contains(subfolder));

    for (auto *folder: currentTree->subFolders) {
        folder->selected = false;
    }
    subfolder->selected = true;
    updateLabels();
}

KnownPluginList::PluginTree *Push2ProcessorSelector::ProcessorSelectorRow::findSelectedSubfolder() const {
    if (currentTree == nullptr) return nullptr;

    for (int i = 0; i < jmin(getTotalNumberOfTreeItems() - currentViewOffset, NUM_COLUMNS); i++) {
        if (i + currentViewOffset < currentTree->subFolders.size()) {
            auto *subfolder = currentTree->subFolders.getUnchecked(i + currentViewOffset);
            if (subfolder->selected)
                return subfolder;
        }
    }
    if (currentViewOffset < currentTree->subFolders.size()) {
        // default to first subfolder in view
        return currentTree->subFolders.getUnchecked(currentViewOffset);
    }

    return nullptr;
}

const PluginDescription *Push2ProcessorSelector::ProcessorSelectorRow::selectProcessor(int index) const {
    if (currentTree == nullptr) return nullptr;

    if (index + currentViewOffset >= currentTree->subFolders.size()) {
        index -= currentTree->subFolders.size();
        if (index + currentViewOffset < currentTree->plugins.size() && labels[index]->isEnabled()) {
            return &currentTree->plugins.getReference(index + currentViewOffset);
        }
    }

    return nullptr;
}

void Push2ProcessorSelector::ProcessorSelectorRow::setCurrentTree(KnownPluginList::PluginTree *tree) {
    if (currentTree == tree) return;

    if (currentTree != nullptr && currentTree->subFolders.contains(tree)) {
        for (auto *folder: currentTree->subFolders) {
            folder->selected = false;
        }
    }
    currentTree = tree;
    if (currentTree != nullptr)
        currentTree->selected = true;
    setCurrentViewOffset(0);
}

Push2Label *Push2ProcessorSelector::ProcessorSelectorRow::findSelectedLabel() const {
    auto *selectedSubfolder = findSelectedSubfolder();
    if (selectedSubfolder != nullptr)
        return labels.getUnchecked(currentTree->subFolders.indexOf(selectedSubfolder) - currentViewOffset);

    return nullptr;
}

void Push2ProcessorSelector::ProcessorSelectorRow::arrowPressed(int direction) {
    if (direction == Push2::leftArrowDirection)
        setCurrentViewOffset(jmax(0, currentViewOffset - NUM_COLUMNS));
    else if (direction == Push2::rightArrowDirection && currentTree != nullptr)
        setCurrentViewOffset(jmin(currentViewOffset + NUM_COLUMNS,
            getTotalNumberOfTreeItems() - getTotalNumberOfTreeItems() % NUM_COLUMNS));
}

void Push2ProcessorSelector::ProcessorSelectorRow::resized() {
    auto r = getLocalBounds();
    for (auto *label: labels) {
        label->setBounds(r.removeFromLeft(getWidth() / NUM_COLUMNS));
    }
    selectionRectangleOverlay.setRectangle(getLocalBounds().toFloat());
}

void Push2ProcessorSelector::ProcessorSelectorRow::updateLabels() const {
    for (int i = 0; i < labels.size(); i++) {
        Push2Label *label = labels.getUnchecked(i);
        label->setSelected(false);
        label->setVisible(false);
    }

    if (currentTree == nullptr) return;

    for (int i = 0; i < jmin(getTotalNumberOfTreeItems() - currentViewOffset, NUM_COLUMNS); i++) {
        Push2Label *label = labels.getUnchecked(i);
        labels[i]->setVisible(true);
        if (i + currentViewOffset < currentTree->subFolders.size()) {
            KnownPluginList::PluginTree *subFolder = currentTree->subFolders.getUnchecked(i + currentViewOffset);
            label->setText(subFolder->folder, dontSendNotification);
            label->setUnderlined(true);
        } else if (i + currentViewOffset < getTotalNumberOfTreeItems()) {
            const PluginDescription *plugin = &currentTree->plugins.getReference(i + currentViewOffset - currentTree->subFolders.size());
            label->setText(plugin->name, dontSendNotification);
            label->setUnderlined(false);
        }
    }
    if (auto *selectedLabel = findSelectedLabel()) {
        selectedLabel->setSelected(true);
    }
}

Push2ProcessorSelector::Push2ProcessorSelector(View &view, Tracks &tracks, Project &project, Push2MidiCommunicator &push2MidiCommunicator)
    : Push2ComponentBase(view, tracks, push2MidiCommunicator), project(project) {
    topProcessorSelector = std::make_unique<ProcessorSelectorRow>(push2, true);
    bottomProcessorSelector = std::make_unique<ProcessorSelectorRow>(push2, false);
    addChildComponent(topProcessorSelector.get());
    addChildComponent(bottomProcessorSelector.get());

    rootTree = KnownPluginList::PluginTree();

    PluginManager &pluginManager = project.getPluginManager();
    auto internalPluginTree = KnownPluginList::createTree(pluginManager.getInternalPluginDescriptions(), pluginManager.getPluginSortMethod());
    internalPluginTree->folder = "Internal";
    auto externalPluginTree = KnownPluginList::createTree(pluginManager.getExternalPluginDescriptions(), pluginManager.getPluginSortMethod());
    externalPluginTree->folder = "External";
    internalPluginTree->parent = &rootTree;
    externalPluginTree->parent = &rootTree;

    rootTree.subFolders.add(std::move(internalPluginTree));
    rootTree.subFolders.add(std::move(externalPluginTree));

    ProcessorSelectorRow *processorSelectorRow = topProcessorSelector.get();
    processorSelectorRow->setCurrentTree(&rootTree);
    selectProcessorRow(topProcessorSelector.get());
}

void Push2ProcessorSelector::setVisible(bool visible) {
    Push2ComponentBase::setVisible(visible);
    topProcessorSelector->setVisible(visible);
    bottomProcessorSelector->setVisible(visible);
}

const PluginDescription *Push2ProcessorSelector::selectTopProcessor(int index) {
    if (const auto *description = topProcessorSelector->selectProcessor(index))
        return description;

    if (auto *subfolder = topProcessorSelector->selectFolder(index)) {
        ProcessorSelectorRow *processorSelectorRow = bottomProcessorSelector.get();
        processorSelectorRow->setCurrentTree(subfolder);
        selectProcessorRow(bottomProcessorSelector.get());
        updateEnabledPush2Arrows();
    }
    return nullptr;
}

const PluginDescription *Push2ProcessorSelector::selectBottomProcessor(int index) {
    if (const auto *description = bottomProcessorSelector->selectProcessor(index))
        return description;

    if (auto *subfolder = bottomProcessorSelector->selectFolder(index)) {
        ProcessorSelectorRow *processorSelectorRow = topProcessorSelector.get();
        processorSelectorRow->setCurrentTree(bottomProcessorSelector->currentTree);
        topProcessorSelector->setCurrentViewOffset(bottomProcessorSelector->getCurrentViewOffset());
        ProcessorSelectorRow *processorSelectorRow1 = bottomProcessorSelector.get();
        processorSelectorRow1->setCurrentTree(subfolder);
        updateEnabledPush2Arrows();
    }
    return nullptr;
}

void Push2ProcessorSelector::aboveScreenButtonPressed(int buttonIndex) {
    if (const auto *selectedProcessor = selectTopProcessor(buttonIndex)) {
        project.createProcessor(*selectedProcessor);
    }
}

void Push2ProcessorSelector::belowScreenButtonPressed(int buttonIndex) {
    if (const auto *selectedProcessor = selectBottomProcessor(buttonIndex)) {
        project.createProcessor(*selectedProcessor);
    }
}

void Push2ProcessorSelector::arrowPressed(int direction) {
    if (!canNavigateInDirection(direction)) return;

    if (currentProcessorSelector != nullptr && (direction == Push2::leftArrowDirection || direction == Push2::rightArrowDirection)) {
        currentProcessorSelector->arrowPressed(direction);
    } else if (direction == Push2::downArrowDirection) {
        if (auto *selectedLabel = currentProcessorSelector->findSelectedLabel()) {
            int selectedIndex = currentProcessorSelector->labels.indexOf(selectedLabel);
            if (currentProcessorSelector == topProcessorSelector.get()) {
                selectTopProcessor(selectedIndex);
            } else if (currentProcessorSelector == bottomProcessorSelector.get()) {
                selectBottomProcessor(selectedIndex);
            }
        }
    } else if (direction == Push2::upArrowDirection) {
        if (currentProcessorSelector == bottomProcessorSelector.get()) {
            selectProcessorRow(topProcessorSelector.get());
        } else if (auto *parent = currentProcessorSelector->currentTree->parent) {
            ProcessorSelectorRow *processorSelectorRow = bottomProcessorSelector.get();
            processorSelectorRow->setCurrentTree(topProcessorSelector->currentTree);
            bottomProcessorSelector->setCurrentViewOffset(topProcessorSelector->getCurrentViewOffset());
            bottomProcessorSelector->selectFolder(topProcessorSelector->findSelectedSubfolder());
            ProcessorSelectorRow *processorSelectorRow1 = topProcessorSelector.get();
            processorSelectorRow1->setCurrentTree(parent);
        }
    }
    updateEnabledPush2Arrows();
}

void Push2ProcessorSelector::resized() {
    auto r = getLocalBounds();
    topProcessorSelector->setBounds(r.removeFromTop(30));
    bottomProcessorSelector->setBounds(r.removeFromBottom(30));
}

void Push2ProcessorSelector::updateEnabledPush2Buttons() {
    if (isVisible())
        push2.activateWhiteLedButton(Push2::addDevice);
    else
        push2.enableWhiteLedButton(Push2::addDevice);
    updateEnabledPush2Arrows();
}

void Push2ProcessorSelector::selectProcessorRow(Push2ProcessorSelector::ProcessorSelectorRow *processorSelectorRow) {
    if (currentProcessorSelector != nullptr) {
        currentProcessorSelector->setSelected(false);
    }
    if (processorSelectorRow) {
        currentProcessorSelector = processorSelectorRow;
        currentProcessorSelector->setSelected(true);
    }
}

void Push2ProcessorSelector::updateEnabledPush2Arrows() {
    for (int direction: {Push2::upArrowDirection, Push2::downArrowDirection, Push2::leftArrowDirection, Push2::rightArrowDirection}) {
        if (isVisible() && currentProcessorSelector != nullptr && canNavigateInDirection(direction))
            push2.activateWhiteLedButton(Push2::ccNumberForArrowButton(direction));
        else
            push2.disableWhiteLedButton(Push2::ccNumberForArrowButton(direction));
    }
}

bool Push2ProcessorSelector::canNavigateInDirection(int direction) {
    switch (direction) {
        case Push2::rightArrowDirection:return canNavigateRight();
        case Push2::downArrowDirection:return canNavigateDown();
        case Push2::leftArrowDirection:return canNavigateLeft();
        case Push2::upArrowDirection:return canNavigateUp();
        default:return false;
    }
}
