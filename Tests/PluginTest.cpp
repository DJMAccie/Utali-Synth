#define UTALI_BUILDING_TESTS 1
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include <iostream>
#include <cmath>

#define UTALI_TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        std::cerr << "Assertion failed at line " << __LINE__ << ": " #cond << std::endl; \
        std::abort(); \
    } \
} while (0)

static void testAudioRenderingAndSanity() {
    std::cout << "[Test 1] Initializing UTALISYNTH processor..." << std::endl;
    UTALISYNTHAudioProcessor processor;
    
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 512;
    processor.prepareToPlay(sampleRate, blockSize);

    juce::AudioBuffer<float> buffer(2, blockSize);
    juce::MidiBuffer midi;

    // Send Note On: C4 (60), velocity 0.8
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);
    midi.clear();

    float rmsL = buffer.getRMSLevel(0, 0, blockSize);
    float rmsR = buffer.getRMSLevel(1, 0, blockSize);
    std::cout << "  Block 1 RMS L: " << rmsL << ", R: " << rmsR << std::endl;
    UTALI_TEST_ASSERT(rmsL > 0.0001f);
    UTALI_TEST_ASSERT(rmsR > 0.0001f);

    // Check for NaNs and out-of-bounds samples
    for (int ch = 0; ch < 2; ++ch) {
        const float* data = buffer.getReadPointer(ch);
        for (int i = 0; i < blockSize; ++i) {
            UTALI_TEST_ASSERT(!std::isnan(data[i]));
            UTALI_TEST_ASSERT(!std::isinf(data[i]));
            UTALI_TEST_ASSERT(std::abs(data[i]) <= 1.0f);
        }
    }

    // Render 10 blocks of audio sustained
    for (int b = 0; b < 10; ++b) {
        buffer.clear();
        processor.processBlock(buffer, midi);
        for (int ch = 0; ch < 2; ++ch) {
            const float* data = buffer.getReadPointer(ch);
            for (int i = 0; i < blockSize; ++i) {
                UTALI_TEST_ASSERT(!std::isnan(data[i]));
                UTALI_TEST_ASSERT(!std::isinf(data[i]));
            }
        }
    }

    // Send Note Off
    midi.addEvent(juce::MidiMessage::noteOff(1, 60, 0.0f), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);
    midi.clear();

    // Render until release fades out
    for (int b = 0; b < 60; ++b) {
        buffer.clear();
        processor.processBlock(buffer, midi);
    }
    float finalRms = buffer.getRMSLevel(0, 0, blockSize);
    std::cout << "  After release RMS: " << finalRms << std::endl;
    UTALI_TEST_ASSERT(finalRms < 0.001f);
    std::cout << "  -> PASSED Audio Rendering & Sanity Test" << std::endl;
}

static void testVoiceStealingDiscontinuity() {
    std::cout << "[Test 2] Testing Voice Stealing Discontinuity..." << std::endl;
    UTALISYNTHAudioProcessor processor;
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 256;
    processor.prepareToPlay(sampleRate, blockSize);

    juce::AudioBuffer<float> buffer(2, blockSize);
    juce::MidiBuffer midi;

    // Trigger all 8 voices
    for (int note = 60; note < 68; ++note) {
        midi.addEvent(juce::MidiMessage::noteOn(1, note, 0.8f), 0);
    }
    buffer.clear();
    processor.processBlock(buffer, midi);
    midi.clear();

    // Trigger 4 more notes (forces voice stealing)
    for (int note = 72; note < 76; ++note) {
        midi.addEvent(juce::MidiMessage::noteOn(1, note, 0.9f), 10);
    }
    buffer.clear();
    processor.processBlock(buffer, midi);
    midi.clear();

    // Check for sample-to-sample discontinuities > 0.4
    float maxStep = 0.0f;
    for (int ch = 0; ch < 2; ++ch) {
        const float* data = buffer.getReadPointer(ch);
        for (int s = 1; s < blockSize; ++s) {
            float step = std::abs(data[s] - data[s - 1]);
            if (step > maxStep) maxStep = step;
        }
    }
    std::cout << "  Max sample-to-sample delta during voice stealing: " << maxStep << std::endl;
    UTALI_TEST_ASSERT(maxStep < 0.5f);
    std::cout << "  -> PASSED Voice Stealing Test" << std::endl;
}

static void testSampleRatesAndBlockSizes() {
    std::cout << "[Test 3] Testing various sample rates and block sizes..." << std::endl;
    const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    const int sizes[] = { 32, 64, 128, 256, 512, 1024, 2048 };

    for (double sr : rates) {
        for (int bs : sizes) {
            UTALISYNTHAudioProcessor processor;
            processor.prepareToPlay(sr, bs);
            juce::AudioBuffer<float> buffer(2, bs);
            juce::MidiBuffer midi;

            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.7f), 0);
            buffer.clear();
            processor.processBlock(buffer, midi);
            midi.clear();

            for (int ch = 0; ch < 2; ++ch) {
                const float* data = buffer.getReadPointer(ch);
                for (int i = 0; i < bs; ++i) {
                    UTALI_TEST_ASSERT(!std::isnan(data[i]));
                    UTALI_TEST_ASSERT(!std::isinf(data[i]));
                }
            }
        }
    }
    std::cout << "  -> PASSED Sample Rates and Block Sizes Test" << std::endl;
}

static void testWaveformsAndADAA() {
    std::cout << "[Test 4] Testing all waveforms and ADAA drive saturation..." << std::endl;
    UTALISYNTHAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    auto* waveParam = processor.apvts.getParameter("WAVE");
    auto* driveParam = processor.apvts.getParameter("DRIVE");

    for (int w = 0; w < 4; ++w) {
        waveParam->setValueNotifyingHost(waveParam->convertTo0to1(static_cast<float>(w)));
        driveParam->setValueNotifyingHost(driveParam->convertTo0to1(3.0f)); // Max drive

        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 57, 0.9f), 0);
        buffer.clear();
        processor.processBlock(buffer, midi);

        float rms = buffer.getRMSLevel(0, 0, 512);
        UTALI_TEST_ASSERT(rms > 0.01f);
        for (int ch = 0; ch < 2; ++ch) {
            const float* data = buffer.getReadPointer(ch);
            for (int i = 0; i < 512; ++i) {
                UTALI_TEST_ASSERT(!std::isnan(data[i]));
                UTALI_TEST_ASSERT(!std::isinf(data[i]));
                UTALI_TEST_ASSERT(std::abs(data[i]) <= 1.0f);
            }
        }
    }
    std::cout << "  -> PASSED Waveforms and ADAA Saturation Test" << std::endl;
}

static void testSustainSensitivity() {
    std::cout << "[Test 5] Testing Sustain Parameter Sensitivity..." << std::endl;
    UTALISYNTHAudioProcessor processor;
    processor.prepareToPlay(44100.0, 512);

    auto* susParam = processor.apvts.getParameter("SUSTAIN");

    // Test at 1% sustain: should be very subtle, not blaring loud
    susParam->setValueNotifyingHost(0.01f);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
    buffer.clear();
    processor.processBlock(buffer, midi);
    midi.clear();

    // Render past attack & decay into sustain stage (e.g. 10 blocks)
    for (int b = 0; b < 10; ++b) {
        buffer.clear();
        processor.processBlock(buffer, midi);
    }
    float lowSustainRms = buffer.getRMSLevel(0, 0, 512);
    std::cout << "  At 1% sustain, RMS level: " << lowSustainRms << std::endl;
    UTALI_TEST_ASSERT(lowSustainRms < 0.015f);

    // Test at 100% sustain: should be full volume
    susParam->setValueNotifyingHost(1.0f);
    for (int b = 0; b < 10; ++b) {
        buffer.clear();
        processor.processBlock(buffer, midi);
    }
    float highSustainRms = buffer.getRMSLevel(0, 0, 512);
    std::cout << "  At 100% sustain, RMS level: " << highSustainRms << std::endl;
    UTALI_TEST_ASSERT(highSustainRms > 0.05f);
    std::cout << "  -> PASSED Sustain Sensitivity Test" << std::endl;
}

static void testRenderUISnapshot() {
    std::cout << "[Test 6] Rendering 1:1 pixel-perfect UI snapshot..." << std::endl;
    UTALISYNTHAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    editor->setSize(1024, 306);
    auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds());
    
    juce::File outputFile("/tmp/utalisynth_ui_preview.png");
    outputFile.deleteFile();
    juce::FileOutputStream fos(outputFile);
    juce::PNGImageFormat png;
    bool success = png.writeImageToStream(snapshot, fos);
    UTALI_TEST_ASSERT(success);
    std::cout << "  -> PASSED UI Snapshot Generated: /tmp/utalisynth_ui_preview.png" << std::endl;
}

int main() {
    juce::MessageManager::getInstance();
    std::cout << "========================================" << std::endl;
    std::cout << " RUNNING UTALISYNTH VERIFICATION TESTS  " << std::endl;
    std::cout << "========================================" << std::endl;

    testAudioRenderingAndSanity();
    testVoiceStealingDiscontinuity();
    testSampleRatesAndBlockSizes();
    testWaveformsAndADAA();
    testSustainSensitivity();
    testRenderUISnapshot();

    std::cout << "========================================" << std::endl;
    std::cout << " ALL TESTS PASSED SUCCESSFULLY!         " << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
