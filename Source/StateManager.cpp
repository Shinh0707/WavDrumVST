#include "StateManager.h"

juce::String
StateManager::getStateAsJsonString(WebPluginAudioProcessor &processor,
                                   bool includeAudio) {
  juce::Array<juce::var> padsArray;

  std::lock_guard<std::mutex> lock(processor.audioMutex);
  for (int i = 0; i < processor.sampler.getNumSounds(); ++i) {
    if (auto *drumSound =
            dynamic_cast<DrumSound *>(processor.sampler.getSound(i).get())) {
      juce::DynamicObject::Ptr padObj = new juce::DynamicObject();
      padObj->setProperty("midiNote", drumSound->getMidiNote());
      padObj->setProperty("sampleName", drumSound->getSampleName());
      padObj->setProperty("filePath", drumSound->getFilePath());

      if (includeAudio) {
        padObj->setProperty("base64Data", drumSound->getSourceBase64());
      }
      padObj->setProperty("offsetMs", drumSound->getOffset() * 1000.0 /
                                          drumSound->getSampleRate());
      padObj->setProperty("endMs", drumSound->getEnd() >= 0
                                       ? drumSound->getEnd() * 1000.0 /
                                             drumSound->getSampleRate()
                                       : -1.0);
      padObj->setProperty("loopEnabled", drumSound->isLoopEnabled());
      padObj->setProperty("loopStartMs", drumSound->getLoopStart() * 1000.0 /
                                             drumSound->getSampleRate());
      padObj->setProperty("loopEndMs", drumSound->getLoopEnd() * 1000.0 /
                                           drumSound->getSampleRate());
      padObj->setProperty("reverse", drumSound->isReverse());
      padObj->setProperty("gainLevel", drumSound->getGainLevel());

      auto adsr = drumSound->getAdsrParams();
      padObj->setProperty("attackMs", adsr.attack * 1000.0f);
      padObj->setProperty("decayMs", adsr.decay * 1000.0f);
      padObj->setProperty("sustainLevel", adsr.sustain);
      padObj->setProperty("releaseMs", adsr.release * 1000.0f);
      padObj->setProperty("attackCurve", drumSound->getAttackCurve());
      padObj->setProperty("decayCurve", drumSound->getDecayCurve());
      padObj->setProperty("releaseCurve", drumSound->getReleaseCurve());

      padsArray.add(juce::var(padObj.get()));
    }
  }

  juce::DynamicObject::Ptr stateObj = new juce::DynamicObject();
  stateObj->setProperty("pads", juce::var(padsArray));

  juce::String jsonString = juce::JSON::toString(juce::var(stateObj.get()));
  processor.pluginStateJson = jsonString;
  return jsonString;
}

void StateManager::setStateInformation(WebPluginAudioProcessor &processor,
                                       const void *data, int sizeInBytes) {
  juce::String stateString =
      juce::String::createStringFromData(data, sizeInBytes);
  if (stateString.isNotEmpty()) {
    processor.pluginStateJson = stateString;

    auto parsedVar = juce::JSON::parse(stateString);
    if (auto *obj = parsedVar.getDynamicObject()) {
      if (obj->hasProperty("pads")) {
        auto padsArray = obj->getProperty("pads").getArray();
        if (padsArray != nullptr) {
          for (auto &padVar : *padsArray) {
            auto *padObj = padVar.getDynamicObject();
            if (padObj) {
              int midiNote = padObj->getProperty("midiNote");
              int padIndex = midiNote - 20;
              juce::String sampleName = padObj->getProperty("sampleName");
              juce::String base64Data = padObj->getProperty("base64Data");
              juce::String filePath = padObj->getProperty("filePath");
              double offsetMs = padObj->getProperty("offsetMs");
              double endMs = padObj->getProperty("endMs");
              bool loopEnabled = padObj->getProperty("loopEnabled");
              double loopStartMs = padObj->getProperty("loopStartMs");
              double loopEndMs = padObj->getProperty("loopEndMs");
              bool reverse = padObj->getProperty("reverse");
              float gainLevel = padObj->getProperty("gainLevel");

              float attackMs = padObj->getProperty("attackMs");
              float decayMs = padObj->getProperty("decayMs");
              float sustainLevel = padObj->getProperty("sustainLevel");
              float releaseMs = padObj->getProperty("releaseMs");
              float attackCurve = padObj->getProperty("attackCurve");
              float decayCurve = padObj->getProperty("decayCurve");
              float releaseCurve = padObj->getProperty("releaseCurve");

              processor.loadDrumSample(
                  padIndex, sampleName, base64Data, filePath, offsetMs, endMs,
                  loopEnabled, loopStartMs, loopEndMs, reverse, attackMs,
                  decayMs, sustainLevel, releaseMs, attackCurve, decayCurve,
                  releaseCurve, gainLevel);
            }
          }
        }
      }
    }
  }
}
