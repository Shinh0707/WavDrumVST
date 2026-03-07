#pragma once

#include "AudioProcessor.h"
#include <juce_core/juce_core.h>

class StateManager {
public:
  static juce::String getStateAsJsonString(WebPluginAudioProcessor &processor,
                                           bool includeAudio);
  static void setStateInformation(WebPluginAudioProcessor &processor,
                                  const void *data, int sizeInBytes);
};
