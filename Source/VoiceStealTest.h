#pragma once
#include <JuceHeader.h>
#include "SynthVoice.h"

/**
 * Unit Test: Voice Steal Discontinuity Detection
 * 
 * This test verifies that:
 * 1. Voice stealing does not cause audio discontinuities (pops/clicks)
 * 2. The fast fade-out mechanism properly smooths the transition
 * 
 * Pass criteria: No sample-to-sample step change > 0.5 at the steal point
 */
class VoiceStealTest : public juce::UnitTest {
public:
    VoiceStealTest() : juce::UnitTest("Voice Steal Discontinuity Test", "Audio") {}

    void runTest() override {
        beginTest("Voice stealing produces no discontinuity");

        // Setup
        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 256;
        constexpr int numChannels = 2;

        SynthVoice voice;
        voice.prepareToPlay(sampleRate, blockSize, numChannels);
        voice.setVoiceIndex(0);

        // Create a simple sine wave sound
        SynthSound sound;

        // Allocate output buffer
        juce::AudioBuffer<float> buffer(numChannels, blockSize * 3);
        buffer.clear();

        // Phase 1: Start note and render some audio
        voice.startNote(60, 0.8f, &sound, 0);  // C4 at velocity 0.8
        voice.renderNextBlock(buffer, 0, blockSize);

        // Phase 2: Trigger voice steal (allowTailOff = false)
        voice.stopNote(0.0f, false);  // Voice steal!

        // Phase 3: Continue rendering - this is where the fast fade should happen
        voice.renderNextBlock(buffer, blockSize, blockSize);
        voice.renderNextBlock(buffer, blockSize * 2, blockSize);

        // Analysis: Check for discontinuities
        bool hasDiscontinuity = false;
        float maxStep = 0.0f;
        int discontinuityPosition = -1;

        for (int ch = 0; ch < numChannels; ++ch) {
            const float* data = buffer.getReadPointer(ch);
            for (int s = 1; s < blockSize * 3; ++s) {
                float step = std::abs(data[s] - data[s - 1]);
                if (step > maxStep) {
                    maxStep = step;
                    if (step > 0.5f) {
                        hasDiscontinuity = true;
                        discontinuityPosition = s;
                    }
                }
            }
        }

        // Report findings
        logMessage("Max sample-to-sample step: " + juce::String(maxStep, 4));
        if (discontinuityPosition >= 0) {
            logMessage("Discontinuity at sample: " + juce::String(discontinuityPosition));
        }

        expect(!hasDiscontinuity, 
               "Voice steal should not produce discontinuity > 0.5. Max step was: " 
               + juce::String(maxStep, 4));
    }
};

// Register the test
static VoiceStealTest voiceStealTestInstance;
