#pragma once

#include <utility>
#include "JuceHeader.h"
#include "Identifiers.h"

struct Parameter : public AudioProcessorParameterWithID {
    Parameter(ValueTree state, UndoManager &undoManager, String parameterID, String paramName, const String &labelText, NormalisableRange<double> range,
                  float defaultVal, std::function<String (const float)> valueToTextFunction,
                  std::function<float(const String &)> textToValueFunction)
            : AudioProcessorParameterWithID(parameterID, paramName, labelText, Category::genericParameter),
              state(std::move(state)), undoManager(undoManager), paramId(std::move(parameterID)), paramName(std::move(paramName)),
              labelText(labelText), range(std::move(range)),
              valueToTextFunction(std::move(valueToTextFunction)),
              textToValueFunction(std::move(textToValueFunction)), defaultValue(defaultVal) {}

    ValueTree getState() const {
        return state.getChildWithProperty(IDs::id, paramId);
    }

    float getValue() const override {
        return (float) range.convertTo0to1(getRawValue());
    }

    void setValue (float newValue) override {
        float clampedNewValue = newValue < 0 ? 0 : (newValue > 1 ? 1 : newValue);
        getState().setProperty(IDs::value, range.convertFrom0to1(clampedNewValue), &undoManager);
    }

    float getRawValue() const {
        return getState().getProperty(IDs::value);
    }

    Value getValueObject() const {
        return getState().getPropertyAsValue(IDs::value, &undoManager);
    }

    void attachSlider(Slider *slider, Label *label=nullptr) {
        if (label != nullptr) {
            label->setText(paramName, dontSendNotification);
        }
        slider->getValueObject().referTo({});
        slider->setNormalisableRange(range);
        slider->textFromValueFunction = valueToTextFunction;
        slider->valueFromTextFunction = textToValueFunction;
        slider->getValueObject().referTo(getValueObject());
    }

    float getDefaultValue() const override {
        //return range.convertTo0to1 (defaultValue);
        return defaultValue;
    }

    float getValueForText(const String& text) const override {
        return (float) range.convertTo0to1(textToValueFunction != nullptr ? textToValueFunction(text) : text.getFloatValue());
    }

    ValueTree state;
    UndoManager &undoManager;

    const String paramId;
    const String paramName;
    const String labelText;
    NormalisableRange<double> range;
    std::function<String(const float)> valueToTextFunction;
    std::function<float(const String &)> textToValueFunction;

private:
    float defaultValue;
};
