#pragma once

#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>

class CustomADSR {
public:
  enum class State { Idle, Attack, Decay, Sustain, Release };

  void setSampleRate(double sr) { sampleRate = sr; }
  void setParameters(float a, float d, float s, float r, float aCurve,
                     float dCurve, float rCurve) {
    attackSec = a;
    decaySec = d;
    sustainLvl = s;
    releaseSec = r;
    attackCurve = aCurve;
    decayCurve = dCurve;
    releaseCurve = rCurve;
  }

  void noteOn() {
    state = State::Attack;
    phaseSamples = static_cast<int>(attackSec * sampleRate);
    currentSampleIndex = 0;
    if (phaseSamples <= 0) {
      currentLevel = 1.0f;
      state = State::Decay;
      phaseSamples = static_cast<int>(decaySec * sampleRate);
      if (phaseSamples <= 0) {
        currentLevel = sustainLvl;
        state = State::Sustain;
      }
    }
  }

  void noteOff() {
    if (state != State::Idle) {
      state = State::Release;
      phaseSamples = static_cast<int>(releaseSec * sampleRate);
      currentSampleIndex = 0;
      releaseStartLevel = currentLevel;
      if (phaseSamples <= 0) {
        state = State::Idle;
        currentLevel = 0.0f;
      }
    }
  }

  void reset() {
    state = State::Idle;
    currentLevel = 0.0f;
  }

  float getNextSample() {
    if (state == State::Idle)
      return 0.0f;

    if (state == State::Attack) {
      float progress = static_cast<float>(currentSampleIndex) / phaseSamples;
      currentLevel = std::pow(progress, attackCurve);
      currentSampleIndex++;
      if (currentSampleIndex >= phaseSamples) {
        currentLevel = 1.0f;
        state = State::Decay;
        phaseSamples = static_cast<int>(decaySec * sampleRate);
        currentSampleIndex = 0;
        if (phaseSamples <= 0) {
          currentLevel = sustainLvl;
          state = State::Sustain;
        }
      }
    } else if (state == State::Decay) {
      float progress = static_cast<float>(currentSampleIndex) / phaseSamples;
      currentLevel =
          1.0f - (1.0f - sustainLvl) * std::pow(progress, decayCurve);
      currentSampleIndex++;
      if (currentSampleIndex >= phaseSamples) {
        currentLevel = sustainLvl;
        state = State::Sustain;
      }
    } else if (state == State::Sustain) {
      currentLevel = sustainLvl;
    } else if (state == State::Release) {
      float progress = static_cast<float>(currentSampleIndex) / phaseSamples;
      currentLevel =
          releaseStartLevel * (1.0f - std::pow(progress, releaseCurve));
      currentSampleIndex++;
      if (currentSampleIndex >= phaseSamples) {
        currentLevel = 0.0f;
        state = State::Idle;
      }
    }
    return currentLevel;
  }

  bool isActive() const { return state != State::Idle; }

private:
  double sampleRate = 44100.0;
  State state = State::Idle;
  float currentLevel = 0.0f;
  float releaseStartLevel = 0.0f;

  float attackSec = 0.0f, decaySec = 0.0f, sustainLvl = 1.0f, releaseSec = 0.0f;
  float attackCurve = 1.0f, decayCurve = 1.0f, releaseCurve = 1.0f;

  int currentSampleIndex = 0;
  int phaseSamples = 0;
};

class DrumSound : public juce::SynthesiserSound {
public:
  DrumSound(int midiNote, std::shared_ptr<juce::AudioBuffer<float>> audioData,
            double sampleRate, const juce::String &sampleName,
            const juce::String &sourceBase64,
            const juce::String &filePath = "");

  juce::String getFilePath() const { return filePath; }

  bool appliesToNote(int midiNoteNumber) override;
  bool appliesToChannel(int midiChannel) override;

  std::shared_ptr<juce::AudioBuffer<float>> getAudioData() const {
    return audioData;
  }
  double getSampleRate() const { return sampleRate; }

  void setOffset(double offsetMs) {
    offsetSamples = static_cast<int>(offsetMs * sampleRate / 1000.0);
  }
  int getOffset() const { return offsetSamples; }

  void setReverse(bool rev) { reverse = rev; }
  bool isReverse() const { return reverse; }

  void setEnd(double eMs) {
    if (sampleRate > 0)
      endSamples = static_cast<int>(eMs * sampleRate / 1000.0);
  }
  int getEnd() const { return endSamples; }

  void setLoop(bool enabled, double startMs, double endMs) {
    loopEnabled = enabled;
    if (sampleRate > 0) {
      loopStartSamples = static_cast<int>(startMs * sampleRate / 1000.0);
      loopEndSamples = static_cast<int>(endMs * sampleRate / 1000.0);
    }
  }
  bool isLoopEnabled() const { return loopEnabled; }
  int getLoopStart() const { return loopStartSamples; }
  int getLoopEnd() const { return loopEndSamples; }

  int getMidiNote() const { return assignedNote; }

  juce::ADSR::Parameters getAdsrParams() const { return adsrParams; }
  void setAdsrParams(const juce::ADSR::Parameters &params, float aCurve = 1.0f,
                     float dCurve = 1.0f, float rCurve = 1.0f) {
    adsrParams = params;
    attackCurve = aCurve;
    decayCurve = dCurve;
    releaseCurve = rCurve;
  }
  float getAttackCurve() const { return attackCurve; }
  float getDecayCurve() const { return decayCurve; }
  float getReleaseCurve() const { return releaseCurve; }

  float getGainLevel() const { return gainLevel; }
  void setGainLevel(float gain) { gainLevel = gain; }

  juce::String getSampleName() const { return sampleName; }
  juce::String getSourceBase64() const { return sourceBase64; }

private:
  int assignedNote;
  std::shared_ptr<juce::AudioBuffer<float>> audioData;
  double sampleRate;
  int offsetSamples = 0;
  int endSamples = -1; // -1 means end of file
  bool reverse = false;
  bool loopEnabled = false;
  int loopStartSamples = 0;
  int loopEndSamples = 0;
  juce::ADSR::Parameters adsrParams{0.0f, 0.0f, 1.0f, 0.1f};
  float attackCurve = 1.0f;
  float decayCurve = 1.0f;
  float releaseCurve = 1.0f;
  float gainLevel = 1.0f;
  juce::String sampleName;
  juce::String sourceBase64;
  juce::String filePath;
};

class DrumVoice : public juce::SynthesiserVoice {
public:
  DrumVoice() = default;

  bool canPlaySound(juce::SynthesiserSound *sound) override;
  void startNote(int midiNoteNumber, float velocity,
                 juce::SynthesiserSound *sound,
                 int currentPitchWheelPosition) override;
  void stopNote(float velocity, bool allowTailOff) override;
  void pitchWheelMoved(int newPitchWheelValue) override {}
  void controllerMoved(int controllerNumber, int newControllerValue) override {}
  void renderNextBlock(juce::AudioBuffer<float> &outputBuffer, int startSample,
                       int numSamples) override;

private:
  double currentPosition = 0.0;
  bool isPlaying = false;
  CustomADSR adsr;
};
