#pragma once

#include "DrumSynth.h"
#include <atomic>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <mutex>

class WebPluginAudioProcessor : public juce::AudioProcessor {
public:
  WebPluginAudioProcessor();
  ~WebPluginAudioProcessor() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

  bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  juce::AudioProcessorEditor *createEditor() override;
  bool hasEditor() const override;

  const juce::String getName() const override;
  juce::String getFilePathForPad(int padIndex);

  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;

  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String &newName) override;

  // DAWのセッション保存・復元用
  void getStateInformation(juce::MemoryBlock &destData) override;
  void setStateInformation(const void *data, int sizeInBytes) override;
  juce::String getStateAsJsonString(bool includeAudio = true);
  juce::String getPadAudioBase64(int padIndex);

  void loadDrumSample(int padIndex, const juce::String &sampleName,
                      const juce::String &base64Data,
                      const juce::String &filePath, double offsetMs,
                      double endMs, bool loopEnabled, double loopStartMs,
                      double loopEndMs, bool reverse, float attackMs,
                      float decayMs, float sustainLevel, float releaseMs,
                      float attackCurve, float decayCurve, float releaseCurve,
                      float gainLevel);
  void updatePadConfig(int padIndex, double offsetMs, double endMs,
                       bool loopEnabled, double loopStartMs, double loopEndMs,
                       bool reverse, float attackMs, float decayMs,
                       float sustainLevel, float releaseMs, float attackCurve,
                       float decayCurve, float releaseCurve, float gainLevel);

  // 同期・状態管理用の共有変数
  std::mutex audioMutex;
  juce::Synthesiser sampler;
  juce::AudioFormatManager formatManager;
  juce::String pluginStateJson = "{}"; // Web UIの状態を保持

  std::atomic<double> currentPpqPosition{0.0};
  std::atomic<bool> isHostPlaying{false};
  std::atomic<bool> isStandalonePlaying{false};

  std::atomic<double> currentBpm{120.0};
  std::atomic<int> timeSigNumerator{4};
  std::atomic<int> timeSigDenominator{4};

  // UIからのMidiプレビュー用バッファ
  juce::MidiBuffer uiMidiBuffer;
  std::mutex uiMidiMutex;

private:
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WebPluginAudioProcessor)
  double hostSampleRate = 44100.0;
};