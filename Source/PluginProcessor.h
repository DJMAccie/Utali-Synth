#pragma once
#include <JuceHeader.h>
#include "SynthVoice.h"
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
    juce::MidiKeyboardState keyboardState;

private:
    static constexpr int MAX_VOICES = 8;
    static constexpr int CURRENT_STATE_VERSION = 1;

    juce::AudioProcessorValueTreeState::ParameterLayout createParams();

    juce::Synthesiser mySynth;
    std::array<SynthVoice*, MAX_VOICES> voices{};
    juce::dsp::Chorus<float> chorus;

    struct ParameterPointers {
        std::atomic<float>* tone = nullptr;
        std::atomic<float>* reso = nullptr;
        std::atomic<float>* age = nullptr;
        std::atomic<float>* wave = nullptr;
        std::atomic<float>* lag = nullptr;
        std::atomic<float>* wobble = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* attack = nullptr;
        std::atomic<float>* decay = nullptr;
        std::atomic<float>* sustain = nullptr;
        std::atomic<float>* release = nullptr;
        std::atomic<float>* sub = nullptr;
        std::atomic<float>* drive = nullptr;
        std::atomic<float>* unison = nullptr;
    } params;

    juce::SmoothedValue<float> smoothedDrive;
    juce::dsp::FirstOrderTPTFilter<float> dcBlocker;
    std::array<ADAAProcessor, 2> adaaProcessors;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UTALISYNTHAudioProcessor)
};
