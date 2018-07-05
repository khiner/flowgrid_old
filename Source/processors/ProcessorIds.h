#pragma once

#include "JuceHeader.h"
#include "GainProcessor.h"
#include "BalanceAndGainProcessor.h"
#include "SineBank.h"

const static StringArray processorIds {GainProcessor::name(), BalanceAndGainProcessor::name(), SineBank::name()};
