#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class FilmStripLookAndFeel : public juce::LookAndFeel_V4 {
public:
    FilmStripLookAndFeel() {
        knobImage = juce::ImageCache::getFromMemory(BinaryData::hise_knob_small_png, BinaryData::hise_knob_small_pngSize);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                          float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override {
        if (knobImage.isValid()) {
            const int numFrames = 128;
            const int frameIndex = juce::jlimit(0, numFrames - 1, static_cast<int>(sliderPos * (numFrames - 1)));
            const int frameHeight = knobImage.getHeight() / numFrames;
            const int frameWidth = knobImage.getWidth();

            g.drawImage(knobImage, x, y, width, height, 0, frameIndex * frameHeight, frameWidth, frameHeight);
        } else {
            juce::LookAndFeel_V4::drawRotarySlider(g, x, y, width, height, sliderPos, rotaryStartAngle, rotaryEndAngle, slider);
        }
    }

private:
    juce::Image knobImage;
};

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
    FilmStripLookAndFeel filmStripLookAndFeel;
    juce::MidiKeyboardComponent keyboardComponent;
    juce::Image background;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UTALISYNTHAudioProcessorEditor)
};
