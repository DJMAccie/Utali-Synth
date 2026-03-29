#pragma once
#include <JuceHeader.h>

struct SynthSound : public juce::SynthesiserSound {
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

class SynthVoice : public juce::SynthesiserVoice {
public:
    bool canPlaySound(juce::SynthesiserSound* sound) override { return dynamic_cast<SynthSound*>(sound) != nullptr; }
    
    void setVoiceIndex(int index) { 
        vIndex = index; 
        float spread = (index / 7.0f) - 0.5f; 
        setPan(spread * 0.6f); 
    }

    SynthVoice() {
        filter.setMode(juce::dsp::LadderFilterMode::LPF12);
        for (auto& o : unisonOscs) o.initialise([](float x) { return x / juce::MathConstants<float>::pi; });
        subOsc.initialise([](float x) { return x < 0 ? -1.0f : 1.0f; });
        driftRate = 0.00005f;
    }

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override {
        currentFreq = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
        level = velocity * 0.5f; // Reduced base gain to prevent internal clipping

        // Reset ADSR and Oscillators to prevent "pops" from previous notes
        adsr.reset();
        for (auto& o : unisonOscs) { 
            o.setFrequency(currentFreq); 
            o.reset(); 
        }
        subOsc.setFrequency(currentFreq * 0.5f);
        subOsc.reset();

        adsr.noteOn();
        noteStartRamp = 0.0f;
        
        float randomPan = (juce::Random::getSystemRandom().nextFloat() - 0.5f) * 0.2f;
        setPan(basePan + randomPan);
    }

    void stopNote(float, bool allowTailOff) override {
        adsr.noteOff();
        if (!allowTailOff) {
            // Voice stealing: trigger 5ms fast fade instead of instant cut
            isFastFading = true;
            fastFadeGain = 1.0f;
        }
        // Do NOT call clearCurrentNote() here for voice stealing - handled in renderNextBlock
    }

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void updateParams(float cutoff, float ageAmount, int waveType, float a, float d, float s, float r, float subLevel, float resonance, int unisonCount) {
        smoothedCutoff.setTargetValue(cutoff);
        smoothedResonance.setTargetValue(resonance);
        driftIntensity = ageAmount;
        smoothedSubMix.setTargetValue(subLevel);
        currentUnison = juce::jlimit(1, maxUnison, unisonCount);

        if (waveType != lastWaveType && !isVoiceActive()) {
            updateWaveform(waveType);
            lastWaveType = waveType;
        }

        adsrParams.attack = std::max(a, 0.002f); // Tiny minimum to prevent instant-on click
        adsrParams.decay = d;
        adsrParams.sustain = s;
        adsrParams.release = std::max(r, 0.01f);
        adsr.setParameters(adsrParams);
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock, int) {
        juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, 1 };
        for (auto& o : unisonOscs) o.prepare(spec);
        subOsc.prepare(spec);
        filter.prepare(spec);
        adsr.setSampleRate(sampleRate);
        
        smoothedCutoff.reset(sampleRate, 0.02);
        smoothedSubMix.reset(sampleRate, 0.05);
        smoothedResonance.reset(sampleRate, 0.02);
        smoothedPanLeft.reset(sampleRate, 0.02);
        smoothedPanRight.reset(sampleRate, 0.02);
        
        tempBuffer.setSize(1, samplesPerBlock);
        unisonBuffer.setSize(1, samplesPerBlock);
    }

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override {
        if (!isVoiceActive()) return;

        tempBuffer.setSize(1, numSamples, false, false, true);
        unisonBuffer.setSize(1, numSamples, false, false, true);
        tempBuffer.clear();

        driftPhase += driftRate * numSamples;
        float drift = std::sin(driftPhase) * driftIntensity * 2.0f;

        // 1. Render Unison Oscillators
        for (int i = 0; i < currentUnison; ++i) {
            float detune = 1.0f + ((i - (currentUnison - 1) / 2.0f) * 0.0015f);
            unisonOscs[i].setFrequency(currentFreq * detune + drift);

            unisonBuffer.clear();
            juce::dsp::AudioBlock<float> uniBlock(unisonBuffer);
            unisonOscs[i].process(juce::dsp::ProcessContextReplacing<float>(uniBlock));

            float unisonGain = 1.0f / static_cast<float>(currentUnison);
            tempBuffer.addFrom(0, 0, unisonBuffer, 0, 0, numSamples, unisonGain);
        }

        // 2. Mix Sub-oscillator
        auto* mainData = tempBuffer.getWritePointer(0);
        for (int s = 0; s < numSamples; ++s) {
            mainData[s] += subOsc.processSample(0) * smoothedSubMix.getNextValue();
        }

        // 3. Process filter
        juce::dsp::AudioBlock<float> filterBlock(tempBuffer);
        filter.setCutoffFrequencyHz(smoothedCutoff.getNextValue());
        filter.setResonance(smoothedResonance.getNextValue());
        filter.process(juce::dsp::ProcessContextReplacing<float>(filterBlock));

        // 4. ADSR and Master Level
        adsr.applyEnvelopeToBuffer(tempBuffer, 0, numSamples);
        tempBuffer.applyGain(level);

        // 5. Anti-click fade-in (for note starting)
        if (noteStartRamp < 1.0f) {
            for (int s = 0; s < numSamples; ++s) {
                if (noteStartRamp < 1.0f) {
                    mainData[s] *= noteStartRamp;
                    noteStartRamp += 0.002f; // Smooth 500-sample ramp
                }
            }
        }

        // 5b. Fast fade-out for voice stealing (5ms @ 44.1kHz ≈ 220 samples)
        if (isFastFading) {
            for (int s = 0; s < numSamples; ++s) {
                mainData[s] *= fastFadeGain;
                fastFadeGain -= fastFadeDecrement;
                if (fastFadeGain <= 0.0f) {
                    fastFadeGain = 0.0f;
                    // Zero remaining samples
                    for (int r = s + 1; r < numSamples; ++r)
                        mainData[r] = 0.0f;
                    break;
                }
            }
        }

        // 6. Pan and Output
        for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch) {
            auto* outData = outputBuffer.getWritePointer(ch, startSample);
            auto* tempData = tempBuffer.getReadPointer(0);
            auto& panSmooth = (ch == 0) ? smoothedPanLeft : smoothedPanRight;
            
            for (int s = 0; s < numSamples; ++s) {
                outData[s] += tempData[s] * panSmooth.getNextValue();
            }
        }

        // Clear voice when ADSR done OR fast fade complete
        if (!adsr.isActive() || (isFastFading && fastFadeGain <= 0.0f)) {
            isFastFading = false;
            clearCurrentNote();
        }
    }

private:
    void setPan(float panPosition) {
        panPosition = juce::jlimit(-1.0f, 1.0f, panPosition);
        basePan = panPosition;
        float angle = (panPosition + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        smoothedPanLeft.setTargetValue(std::cos(angle));
        smoothedPanRight.setTargetValue(std::sin(angle));
    }

    void updateWaveform(int waveType) {
        auto setW = [waveType](juce::dsp::Oscillator<float>& o) {
            if (waveType == 0) o.initialise([](float x) { return std::sin(x); });
            else if (waveType == 1) o.initialise([](float x) { return x < 0 ? -1.0f : 1.0f; });
            else if (waveType == 2) o.initialise([](float x) { return (2.0f / juce::MathConstants<float>::pi) * std::asin(std::sin(x)); });
            else o.initialise([](float x) { return x / juce::MathConstants<float>::pi; });
        };
        for (auto& o : unisonOscs) setW(o);
        setW(subOsc);
    }

    static constexpr int maxUnison = 4;
    std::array<juce::dsp::Oscillator<float>, maxUnison> unisonOscs;
    juce::dsp::Oscillator<float> subOsc;
    juce::dsp::LadderFilter<float> filter;
    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams;
    juce::SmoothedValue<float> smoothedCutoff, smoothedSubMix, smoothedResonance, smoothedPanLeft, smoothedPanRight;
    juce::AudioBuffer<float> tempBuffer, unisonBuffer;
    float currentFreq = 440.0f, driftIntensity = 0.0f, level = 0.0f, basePan = 0.0f;
    float driftPhase = 0.0f, driftRate = 0.0f, noteStartRamp = 1.0f;
    int lastWaveType = -1, vIndex = 0, currentUnison = 1;
    
    // Fast fade-out state for voice stealing (5ms @ 44.1kHz)
    bool isFastFading = false;
    float fastFadeGain = 1.0f;
    static constexpr float fastFadeDecrement = 1.0f / 220.0f;  // ~5ms fade @ 44.1kHz
};