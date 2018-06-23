#pragma once

#include "JuceHeader.h"
#include <intruments/Instrument.h>

class InstrumentViewComponent : public Component {
public:
    explicit InstrumentViewComponent(Instrument *instrument);

private:
    Instrument* instrument;
};
