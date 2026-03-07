#include "AudioProcessor.h"
#include "AudioEditor.h"
#include "DrumSynth.h"
#include "StateManager.h"

WebPluginAudioProcessor::WebPluginAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(
          BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
      )
#endif
{
  formatManager.registerBasicFormats();
  for (int i = 0; i < 16; ++i) {
    sampler.addVoice(new DrumVoice());
  }
}

WebPluginAudioProcessor::~WebPluginAudioProcessor() {}

const juce::String WebPluginAudioProcessor::getName() const {
  return JucePlugin_Name;
}
juce::String WebPluginAudioProcessor::getFilePathForPad(int padIndex) {
  int midiNote = 20 + padIndex;
  std::lock_guard<std::mutex> lock(audioMutex);
  for (int i = 0; i < sampler.getNumSounds(); ++i) {
    if (auto *drumSound =
            dynamic_cast<DrumSound *>(sampler.getSound(i).get())) {
      if (drumSound->getMidiNote() == midiNote) {
        return drumSound->getFilePath();
      }
    }
  }
  return "";
}
bool WebPluginAudioProcessor::acceptsMidi() const { return true; }
bool WebPluginAudioProcessor::producesMidi() const { return true; }
bool WebPluginAudioProcessor::isMidiEffect() const { return false; }
double WebPluginAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int WebPluginAudioProcessor::getNumPrograms() { return 1; }
int WebPluginAudioProcessor::getCurrentProgram() { return 0; }
void WebPluginAudioProcessor::setCurrentProgram(int index) {}
const juce::String WebPluginAudioProcessor::getProgramName(int index) {
  return {};
}
void WebPluginAudioProcessor::changeProgramName(int index,
                                                const juce::String &newName) {}

void WebPluginAudioProcessor::prepareToPlay(double sampleRate,
                                            int samplesPerBlock) {
  hostSampleRate = sampleRate;
  sampler.setCurrentPlaybackSampleRate(sampleRate);
}

void WebPluginAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool WebPluginAudioProcessor::isBusesLayoutSupported(
    const BusesLayout &layouts) const {
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
      layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;
#if !JucePlugin_IsSynth
  if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
    return false;
#endif
  return true;
}
#endif

void WebPluginAudioProcessor::loadDrumSample(
    int padIndex, const juce::String &sampleName,
    const juce::String &base64Data, const juce::String &filePath,
    double offsetMs, double endMs, bool loopEnabled, double loopStartMs,
    double loopEndMs, bool reverse, float attackMs, float decayMs,
    float sustainLevel, float releaseMs, float attackCurve, float decayCurve,
    float releaseCurve, float gainLevel) {

  std::unique_ptr<juce::AudioFormatReader> reader;
  juce::MemoryBlock memoryBlock;

  // 1. Fullモード(Base64)の場合、Base64からデコードして読み込む
  if (base64Data.isNotEmpty()) {
    juce::MemoryOutputStream outStream;
    if (juce::Base64::convertFromBase64(outStream, base64Data)) {
      memoryBlock = outStream.getMemoryBlock();
      auto inputStream =
          std::make_unique<juce::MemoryInputStream>(memoryBlock, true);
      reader.reset(formatManager.createReaderFor(std::move(inputStream)));
    }
  }
  // 2. Lightモード(Base64なし)の場合、ファイルパスから直接読み込む
  else if (filePath.isNotEmpty()) {
    juce::File audioFile(filePath);
    if (audioFile.existsAsFile()) {
      reader.reset(formatManager.createReaderFor(audioFile));
    } else {
      // ファイルが移動・削除されていて見つからない場合は安全にスキップ
      return;
    }
  }

  // リーダーの生成に成功した場合のみバッファに書き込む
  if (reader != nullptr) {
    auto newBuffer = std::make_shared<juce::AudioBuffer<float>>(
        static_cast<int>(reader->numChannels),
        static_cast<int>(reader->lengthInSamples));
    reader->read(newBuffer.get(), 0, static_cast<int>(reader->lengthInSamples),
                 0, true, true);

    int midiNote = 20 + padIndex;

    std::lock_guard<std::mutex> lock(audioMutex);

    // 既存の同じノートのサウンドを削除
    for (int i = sampler.getNumSounds(); --i >= 0;) {
      if (auto *drumSound =
              dynamic_cast<DrumSound *>(sampler.getSound(i).get())) {
        if (drumSound->getMidiNote() == midiNote) {
          sampler.removeSound(i);
        }
      }
    }

    // 新しいサウンドを登録
    auto *newSound = new DrumSound(midiNote, newBuffer, reader->sampleRate,
                                   sampleName, base64Data, filePath);
    newSound->setOffset(offsetMs);
    newSound->setEnd(endMs);
    newSound->setLoop(loopEnabled, loopStartMs, loopEndMs);
    newSound->setReverse(reverse);
    newSound->setAdsrParams({attackMs / 1000.0f, decayMs / 1000.0f,
                             sustainLevel, releaseMs / 1000.0f},
                            attackCurve, decayCurve, releaseCurve);
    newSound->setGainLevel(gainLevel);
    sampler.addSound(newSound);
  }
}

void WebPluginAudioProcessor::updatePadConfig(
    int padIndex, double offsetMs, double endMs, bool loopEnabled,
    double loopStartMs, double loopEndMs, bool reverse, float attackMs,
    float decayMs, float sustainLevel, float releaseMs, float attackCurve,
    float decayCurve, float releaseCurve, float gainLevel) {
  int midiNote = 20 + padIndex;
  std::lock_guard<std::mutex> lock(audioMutex);
  for (int i = 0; i < sampler.getNumSounds(); ++i) {
    if (auto *drumSound =
            dynamic_cast<DrumSound *>(sampler.getSound(i).get())) {
      if (drumSound->getMidiNote() == midiNote) {
        drumSound->setOffset(offsetMs);
        drumSound->setEnd(endMs);
        drumSound->setLoop(loopEnabled, loopStartMs, loopEndMs);
        drumSound->setReverse(reverse);
        drumSound->setAdsrParams({attackMs / 1000.0f, decayMs / 1000.0f,
                                  sustainLevel, releaseMs / 1000.0f},
                                 attackCurve, decayCurve, releaseCurve);
        drumSound->setGainLevel(gainLevel);
        break;
      }
    }
  }
}

juce::String WebPluginAudioProcessor::getPadAudioBase64(int padIndex) {
  int midiNote = 20 + padIndex;
  std::lock_guard<std::mutex> lock(audioMutex);
  for (int i = 0; i < sampler.getNumSounds(); ++i) {
    if (auto *drumSound =
            dynamic_cast<DrumSound *>(sampler.getSound(i).get())) {
      if (drumSound->getMidiNote() == midiNote) {
        return drumSound->getSourceBase64();
      }
    }
  }
  return "";
}

void WebPluginAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                           juce::MidiBuffer &midiMessages) {
  buffer.clear();

  bool hostPlaying = false;
  if (auto *playHead = getPlayHead()) {
    if (auto positionInfo = playHead->getPosition()) {
      hostPlaying = positionInfo->getIsPlaying();
      isHostPlaying.store(hostPlaying);
      if (hostPlaying)
        isStandalonePlaying.store(false);

      if (auto ppq = positionInfo->getPpqPosition()) {
        if (hostPlaying)
          currentPpqPosition.store(*ppq);
      }
      if (auto bpm = positionInfo->getBpm())
        currentBpm.store(*bpm);
      if (auto timeSig = positionInfo->getTimeSignature()) {
        timeSigNumerator.store(timeSig->numerator);
        timeSigDenominator.store(timeSig->denominator);
      }
    }
  }

  if (!hostPlaying && isStandalonePlaying.load()) {
    double currentPos = currentPpqPosition.load();
    double currentBpmVal = currentBpm.load() > 0.0 ? currentBpm.load() : 120.0;
    double timeInSeconds = (double)buffer.getNumSamples() / getSampleRate();
    double ppqAdvance = timeInSeconds * (currentBpmVal / 60.0);
    currentPpqPosition.store(currentPos + ppqAdvance);
  }

  std::lock_guard<std::mutex> lock(audioMutex);

  // UIからのMIDIイベントをマージ
  {
    std::lock_guard<std::mutex> uiMidiLock(uiMidiMutex);
    if (!uiMidiBuffer.isEmpty()) {
      midiMessages.addEvents(uiMidiBuffer, 0, buffer.getNumSamples(), 0);
      uiMidiBuffer.clear();
    }
  }

  sampler.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
}

bool WebPluginAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor *WebPluginAudioProcessor::createEditor() {
  return new WebPluginAudioProcessorEditor(*this);
}

juce::String WebPluginAudioProcessor::getStateAsJsonString(bool includeAudio) {
  return StateManager::getStateAsJsonString(*this, includeAudio);
}

// DAWのプロジェクト保存時に呼ばれる
void WebPluginAudioProcessor::getStateInformation(juce::MemoryBlock &destData) {
  juce::String jsonString = getStateAsJsonString(true); // Full export for DAW
  juce::MemoryOutputStream stream(destData, true);
  stream.writeString(jsonString);
}

// DAWのプロジェクト読み込み時に呼ばれる
void WebPluginAudioProcessor::setStateInformation(const void *data,
                                                  int sizeInBytes) {
  StateManager::setStateInformation(*this, data, sizeInBytes);
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new WebPluginAudioProcessor();
}