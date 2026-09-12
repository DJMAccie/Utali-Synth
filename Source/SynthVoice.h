#pragma once
#include <JuceHeader.h>

struct SynthSound : public juce::SynthesiserSound {
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

struct PolyBlepOscillator {
    float phase = 0.0f;
    float phaseIncrement = 0.0f;

    void setFrequency(float freqHz, double sampleRate) noexcept {
        phaseIncrement = juce::jlimit(0.0f, 0.499f, static_cast<float>(freqHz / sampleRate));
    }

    void reset() noexcept {
        phase = 0.0f;
    }

    float renderSample(int waveType) noexcept {
        float sample = 0.0f;
        const float dt = phaseIncrement;

        switch (waveType) {
            case 0: // Sine
                sample = std::sin(phase * juce::MathConstants<float>::twoPi);
                break;
            case 1: { // Square with PolyBLEP
                sample = (phase < 0.5f) ? 1.0f : -1.0f;
                sample += polyBlep(phase, dt);
                float phaseShifted = phase + 0.5f;
                if (phaseShifted >= 1.0f) phaseShifted -= 1.0f;
                sample -= polyBlep(phaseShifted, dt);
                break;
            }
            case 2: // Triangle
                sample = 2.0f * std::abs(2.0f * phase - 1.0f) - 1.0f;
                break;
            case 3: // Saw with PolyBLEP
            default:
                sample = (2.0f * phase) - 1.0f;
                sample -= polyBlep(phase, dt);
                break;
        }

        phase += dt;
        if (phase >= 1.0f)
            phase -= 1.0f;

        return sample;
    }

private:
    static inline float polyBlep(float t, float dt) noexcept {
        if (dt <= 0.0f) return 0.0f;
        if (t < dt) {
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        if (t > 1.0f - dt) {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }
};

struct SynthVoiceParameters {
    float cutoff = 2000.0f;
    float resonance = 0.1f;
    float age = 0.02f;
    int waveType = 0;
    float attack = 0.01f;
    float decay = 0.1f;
    float sustain = 0.8f;
    float release = 0.5f;
    float subLevel = 0.2f;
    int unisonCount = 1;
};

class SynthVoice : public juce::SynthesiserVoice {
public:
    SynthVoice();

    bool canPlaySound(juce::SynthesiserSound* sound) override { 
        return dynamic_cast<SynthSound*>(sound) != nullptr; 
    }
    
    void setVoiceIndex(int index);
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override;
    void stopNote(float, bool allowTailOff) override;
    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void updateParams(const SynthVoiceParameters& params);
    void prepareToPlay(double sampleRate, int samplesPerBlock, int numOutputChannels);
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

private:
    void setPan(float panPosition);

    static constexpr int MAX_UNISON = 4;
    std::array<PolyBlepOscillator, MAX_UNISON> unisonOscs;
    PolyBlepOscillator subOsc;
    juce::dsp::LadderFilter<float> filter;
    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams;
    juce::Random random;

    juce::SmoothedValue<float> smoothedCutoff;
    juce::SmoothedValue<float> smoothedSubMix;
    juce::SmoothedValue<float> smoothedResonance;
    juce::SmoothedValue<float> smoothedPanLeft;
    juce::SmoothedValue<float> smoothedPanRight;

    juce::AudioBuffer<float> tempBuffer;

    double currentSampleRate = 44100.0;
    float currentFreq = 440.0f;
    float driftIntensity = 0.0f;
    float level = 0.0f;
    float basePan = 0.0f;
    float driftPhase = 0.0f;
    float driftRate = 0.00005f;
    int vIndex = 0;
    int currentUnison = 1;
    int currentWaveType = 0;

    // Fast fade-out for voice stealing
    bool isFastFading = false;
    float fastFadeGain = 1.0f;
    float fastFadeDecrement = 1.0f / 220.0f;
};
