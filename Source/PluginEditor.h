#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Custom LookAndFeel for FilmStrip Knobs
class FilmStripLookAndFeel : public juce::LookAndFeel_V4 {
public:
    FilmStripLookAndFeel() {
        knobImage = juce::ImageCache::getFromMemory(UTALIAssets::hise_knob_small_png, UTALIAssets::hise_knob_small_pngSize);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
        float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override {
        
        if (knobImage.isValid()) {
            const int numFrames = 128;
            const int frameIndex = (int)(sliderPos * (numFrames - 1));
            const int frameHeight = knobImage.getHeight() / numFrames;
            const int frameWidth = knobImage.getWidth();

            // Center the knob in the allotted rectangle
             // The simple drawImage fits the dest rect. We might want to preserve aspect ratio or just fill.
             // Usually for knobs we want to draw it centered.
             // But the user didn't specify complex placement, just "use BinaryData...".
             // Assuming the slider bounds are roughly square or the knob fits.
             // Let's draw it to fill the bounds but respecting the frame aspect logic if needed. 
             // Simplest: g.drawImage (..., 0, frameIndex * frameHeight, frameWidth, frameHeight);
            
             g.drawImage(knobImage, x, y, width, height, 0, frameIndex * frameHeight, frameWidth, frameHeight);
        } else {
             // Fallback if image fails (shouldn't happen with BinaryData)
             juce::LookAndFeel_V4::drawRotarySlider(g, x, y, width, height, sliderPos, rotaryStartAngle, rotaryEndAngle, slider);
        }
    }

private:
    juce::Image knobImage;
};

class UTALISYNTHAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    UTALISYNTHAudioProcessorEditor(UTALISYNTHAudioProcessor&);
    ~UTALISYNTHAudioProcessorEditor() override;
    
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    // Row 1 Controls
    juce::Slider waveSlider, toneSlider, ageSlider, subSlider, driveSlider, mixSlider;
    juce::Label waveLabel, toneLabel, ageLabel, subLabel, driveLabel, mixLabel;
    
    // Row 2 Controls (ADSR + Effects)
    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider, lagSlider, wobbleSlider;
    juce::Label attackLabel, decayLabel, sustainLabel, releaseLabel, lagLabel, wobbleLabel;

    // Parameter attachments
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> toneAttach, ageAttach, waveAttach, lagAttach, wobbleAttach, mixAttach;
    std::unique_ptr<Attachment> attackAttach, decayAttach, sustainAttach, releaseAttach, subAttach, driveAttach;

    // Audio processor reference
    UTALISYNTHAudioProcessor& audioProcessor;
    
    // GUI Assets & Components
    FilmStripLookAndFeel filmStripLookAndFeel;
    juce::MidiKeyboardComponent keyboardComponent;
    juce::Image background;

    /** Helper method to layout slider controls in a row */
    void layoutRow(juce::Rectangle<int> row, juce::Slider** sliders, 
                   juce::Label** labels, int count, int colWidth, int knobHeight);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UTALISYNTHAudioProcessorEditor)
};