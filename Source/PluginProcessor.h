#pragma once
#include <JuceHeader.h>
#include "SynthVoice.h"
#include "JuliaChorus.h"
#include "ADAAProcessor.h"

class UTALISYNTHAudioProcessor : public juce::AudioProcessor {
public:
    UTALISYNTHAudioProcessor();
    ~UTALISYNTHAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParams();
    juce::MidiKeyboardState keyboardState;

private:
    static constexpr int MAX_VOICES = 8;

    juce::Synthesiser mySynth;
    JuliaChorus juliaChorus;

    juce::SmoothedValue<float> smoothedDrive;
    juce::SmoothedValue<float> smoothedLag, smoothedWobble, smoothedMix;
    juce::dsp::FirstOrderTPTFilter<float> dcBlocker;
    std::array<ADAAProcessor, 2> adaaProcessors;  // Per-channel ADAA state

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UTALISYNTHAudioProcessor)
};