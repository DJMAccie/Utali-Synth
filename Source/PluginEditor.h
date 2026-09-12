#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "GUI/UtaliLookAndFeel.h"

class UTALISYNTHAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    explicit UTALISYNTHAudioProcessorEditor(UTALISYNTHAudioProcessor&);
    ~UTALISYNTHAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct KnobControl {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    static constexpr int NUM_KNOBS = 12;
    std::array<KnobControl, NUM_KNOBS> knobs;

    UTALISYNTHAudioProcessor& audioProcessor;
    utali::UtaliLookAndFeel utaliLookAndFeel;
    juce::MidiKeyboardComponent keyboardComponent;
    juce::Image background;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UTALISYNTHAudioProcessorEditor)
};
