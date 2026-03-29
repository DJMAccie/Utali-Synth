#pragma once
#include <JuceHeader.h>

class JuliaChorus {
public:
    void prepare(const juce::dsp::ProcessSpec& spec) {
        chorus.prepare(spec);
    }

    // Backward-compatible overload accepting 3 args
    void setParameters(float lag, float wobble, float dcv) {
        setParameters(lag, wobble, dcv, 1.2f);
    }

    // Updated: accept lfoRate as fourth parameter and clamp inputs
    void setParameters(float lag, float wobble, float dcv, float lfoRate) {
        // Clamp ranges to sensible values
        lag = juce::jlimit(1.0f, 100.0f, lag);
        wobble = juce::jlimit(0.0f, 1.0f, wobble);
        dcv = juce::jlimit(0.0f, 1.0f, dcv);
        lfoRate = juce::jlimit(0.01f, 20.0f, lfoRate);

        chorus.setCentreDelay(lag);
        chorus.setDepth(wobble);
        chorus.setRate(lfoRate);
        chorus.setMix(dcv);
    }

    void process(juce::AudioBuffer<float>& buffer) {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        chorus.process(context);
    }

private:
    juce::dsp::Chorus<float> chorus;
};