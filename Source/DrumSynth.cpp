#include "DrumSynth.h"
#include <algorithm>

DrumSound::DrumSound(int midiNote,
                     std::shared_ptr<juce::AudioBuffer<float>> data, double sr,
                     const juce::String &name, const juce::String &b64,
                     const juce::String &path)
    : assignedNote(midiNote), audioData(std::move(data)), sampleRate(sr),
      sampleName(name), sourceBase64(b64), filePath(path) {}

bool DrumSound::appliesToNote(int midiNoteNumber) {
  return midiNoteNumber == assignedNote;
}

bool DrumSound::appliesToChannel(int) { return true; }

bool DrumVoice::canPlaySound(juce::SynthesiserSound *sound) {
  return dynamic_cast<DrumSound *>(sound) != nullptr;
}

void DrumVoice::startNote(int /*midiNoteNumber*/, float /*velocity*/,
                          juce::SynthesiserSound *sound,
                          int /*currentPitchWheelPosition*/) {
  if (auto *drumSound = dynamic_cast<DrumSound *>(sound)) {
    if (drumSound->getAudioData() == nullptr ||
        drumSound->getAudioData()->getNumSamples() == 0) {
      isPlaying = false;
      return;
    }

    int endLimit = drumSound->getEnd();
    if (endLimit < 0 || endLimit > drumSound->getAudioData()->getNumSamples()) {
      endLimit = drumSound->getAudioData()->getNumSamples();
    }

    currentPosition = drumSound->isReverse()
                          ? static_cast<double>(endLimit - 1)
                          : static_cast<double>(drumSound->getOffset());

    if (currentPosition < 0)
      currentPosition = 0;
    if (currentPosition >= drumSound->getAudioData()->getNumSamples())
      currentPosition = drumSound->getAudioData()->getNumSamples() - 1;

    adsr.setSampleRate(getSampleRate());
    auto params = drumSound->getAdsrParams();
    adsr.setParameters(params.attack, params.decay, params.sustain,
                       params.release, drumSound->getAttackCurve(),
                       drumSound->getDecayCurve(),
                       drumSound->getReleaseCurve());
    adsr.noteOn();

    isPlaying = true;
  }
}

void DrumVoice::stopNote(float /*velocity*/, bool allowTailOff) {
  if (allowTailOff) {
    adsr.noteOff();
  } else {
    isPlaying = false;
    adsr.reset();
    clearCurrentNote();
  }
}

void DrumVoice::renderNextBlock(juce::AudioBuffer<float> &outputBuffer,
                                int startSample, int numSamples) {
  if (!isPlaying)
    return;

  auto *playingSound =
      static_cast<DrumSound *>(getCurrentlyPlayingSound().get());
  if (playingSound == nullptr || playingSound->getAudioData() == nullptr) {
    isPlaying = false;
    adsr.reset();
    clearCurrentNote();
    return;
  }

  if (!adsr.isActive()) {
    isPlaying = false;
    clearCurrentNote();
    return;
  }

  auto audioData = playingSound->getAudioData();
  const int totalSamples = audioData->getNumSamples();
  const int numChannels = audioData->getNumChannels();
  const bool isReverse = playingSound->isReverse();
  const float gain = playingSound->getGainLevel();
  const bool loopEnabled = playingSound->isLoopEnabled();
  const int loopStart = playingSound->getLoopStart();
  const int loopEnd = playingSound->getLoopEnd();

  double pitchRatio = getSampleRate() / playingSound->getSampleRate();
  if (playingSound->getSampleRate() == 0.0)
    pitchRatio = 1.0;

  int endLimit = playingSound->getEnd();
  if (endLimit < 0 || endLimit > totalSamples) {
    endLimit = totalSamples;
  }
  int startLimit = playingSound->getOffset();

  for (int i = 0; i < numSamples; ++i) {
    if ((!isReverse && currentPosition >= endLimit) ||
        (isReverse && currentPosition < startLimit)) {
      isPlaying = false;
      adsr.reset();
      clearCurrentNote();
      break;
    }

    int posPos = static_cast<int>(currentPosition);
    if (posPos < 0)
      posPos = 0;
    if (posPos >= totalSamples)
      posPos = totalSamples - 1;

    float envelope = adsr.getNextSample();

    for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch) {
      int sourceCh = std::min(ch, numChannels - 1);
      outputBuffer.addSample(ch, startSample + i,
                             audioData->getSample(sourceCh, posPos) * envelope *
                                 gain);
    }

    if (isReverse) {
      currentPosition -= pitchRatio;
    } else {
      currentPosition += pitchRatio;
    }

    if (loopEnabled && loopEnd > loopStart) {
      if (!isReverse) {
        if (currentPosition >= loopEnd) {
          currentPosition = loopStart + (currentPosition - loopEnd);
        }
      } else {
        if (currentPosition < loopStart) {
          currentPosition = loopEnd - (loopStart - currentPosition);
        }
      }
    }
  }
}
