#include "PluginProcessor.h"
#include "PluginEditor.h"

/** Helper struct to manage slider creation and configuration */
struct SliderSetup {
    juce::Slider& slider;
    juce::Label& label;
    juce::String name;
    const char* paramId;

    void setup(UTALISYNTHAudioProcessorEditor* editor) {
        editor->addAndMakeVisible(slider);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

        editor->addAndMakeVisible(label);
        label.setText(name, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, juce::Colours::cyan);
        label.setFont(juce::Font(14.0f));
    }
};

UTALISYNTHAudioProcessorEditor::UTALISYNTHAudioProcessorEditor(UTALISYNTHAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), keyboardComponent(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    // Define all sliders with their parameters
    SliderSetup sliders[] = {
        { waveSlider, waveLabel, "WAVE", "WAVE" },
        { toneSlider, toneLabel, "TONE", "TONE" },
        { ageSlider, ageLabel, "AGE", "AGE" },
        { subSlider, subLabel, "SUB", "SUB" },
        { driveSlider, driveLabel, "DRIVE", "DRIVE" },
        { mixSlider, mixLabel, "d-c-v", "MIX" },
        { lagSlider, lagLabel, "LAG", "LAG" },
        { wobbleSlider, wobbleLabel, "WOBBLE", "WOBBLE" },
        { attackSlider, attackLabel, "A", "ATTACK" },
        { decaySlider, decayLabel, "D", "DECAY" },
        { sustainSlider, sustainLabel, "S", "SUSTAIN" },
        { releaseSlider, releaseLabel, "R", "RELEASE" }
    };

    // Setup all sliders
    for (auto& setup : sliders) {
        setup.setup(this);
        setup.slider.setLookAndFeel(&filmStripLookAndFeel);
    }

    // Create parameter attachments with proper parameter IDs
    toneAttach = std::make_unique<Attachment>(audioProcessor.apvts, "TONE", toneSlider);
    ageAttach = std::make_unique<Attachment>(audioProcessor.apvts, "AGE", ageSlider);
    waveAttach = std::make_unique<Attachment>(audioProcessor.apvts, "WAVE", waveSlider);
    lagAttach = std::make_unique<Attachment>(audioProcessor.apvts, "LAG", lagSlider);
    wobbleAttach = std::make_unique<Attachment>(audioProcessor.apvts, "WOBBLE", wobbleSlider);
    mixAttach = std::make_unique<Attachment>(audioProcessor.apvts, "MIX", mixSlider);
    attackAttach = std::make_unique<Attachment>(audioProcessor.apvts, "ATTACK", attackSlider);
    decayAttach = std::make_unique<Attachment>(audioProcessor.apvts, "DECAY", decaySlider);
    sustainAttach = std::make_unique<Attachment>(audioProcessor.apvts, "SUSTAIN", sustainSlider);
    releaseAttach = std::make_unique<Attachment>(audioProcessor.apvts, "RELEASE", releaseSlider);
    subAttach = std::make_unique<Attachment>(audioProcessor.apvts, "SUB", subSlider);
    driveAttach = std::make_unique<Attachment>(audioProcessor.apvts, "DRIVE", driveSlider);

    // Setup Keyboard
    addAndMakeVisible(keyboardComponent);
    keyboardComponent.setKeyWidth(40);
    
    // Load Background
    background = juce::ImageCache::getFromMemory(UTALIAssets::met_zelfde_witte_achtergrond_3_png, UTALIAssets::met_zelfde_witte_achtergrond_3_pngSize);

    setSize(800, 500);
}

UTALISYNTHAudioProcessorEditor::~UTALISYNTHAudioProcessorEditor() {}

void UTALISYNTHAudioProcessorEditor::paint(juce::Graphics& g) {
    if (background.isValid())
        g.drawImage(background, getLocalBounds().toFloat(), juce::RectanglePlacement::fillDestination);
    else
        g.fillAll(juce::Colour::fromFloatRGBA(0.42f, 0.38f, 0.53f, 1.0f));

    // Draw Main Title
    g.setColour(juce::Colours::cyan);
    g.setFont(30.0f);
    g.drawText("UTALISYNTH", getLocalBounds().removeFromTop(40), juce::Justification::centred, true);
}

void UTALISYNTHAudioProcessorEditor::resized() {
    // Layout Keyboard at bottom (80px)
    auto mainArea = getLocalBounds();
    keyboardComponent.setBounds(mainArea.removeFromBottom(80));

    // Reserve top space for Title
    mainArea.removeFromTop(40);

    // Layout knob area (remaining top area)
    auto knobArea = mainArea;

    const int colWidth = knobArea.getWidth() / 6;
    const int knobHeight = 80;
    const int labelHeight = 40;
    
    // We need to vertically center the two rows in the available space
    int totalRowHeight = (knobHeight + labelHeight) * 2;
    int startY = (knobArea.getHeight() - totalRowHeight) / 2;
    
    auto controlsArea = knobArea;
    controlsArea.removeFromTop(startY);

    // Row 1: Wave, Tone, Age, Sub, Drive, Mix
    auto row1 = controlsArea.removeFromTop(knobHeight + labelHeight);
    juce::Slider* row1Sliders[] = { &waveSlider, &toneSlider, &ageSlider, &subSlider, &driveSlider, &mixSlider };
    juce::Label* row1Labels[] = { &waveLabel, &toneLabel, &ageLabel, &subLabel, &driveLabel, &mixLabel };

    layoutRow(row1, row1Sliders, row1Labels, 6, colWidth, knobHeight);

    // Row 2: Lag, Wobble, Attack, Decay, Sustain, Release
    auto row2 = controlsArea.removeFromTop(knobHeight + labelHeight);
    juce::Slider* row2Sliders[] = { &lagSlider, &wobbleSlider, &attackSlider, &decaySlider, &sustainSlider, &releaseSlider };
    juce::Label* row2Labels[] = { &lagLabel, &wobbleLabel, &attackLabel, &decayLabel, &sustainLabel, &releaseLabel };

    layoutRow(row2, row2Sliders, row2Labels, 6, colWidth, knobHeight);
}

void UTALISYNTHAudioProcessorEditor::layoutRow(juce::Rectangle<int> row, juce::Slider** sliders,
                                              juce::Label** labels, int count, int colWidth, int knobHeight) {
    for (int i = 0; i < count; ++i) {
        auto cell = row.removeFromLeft(colWidth);
        auto sliderArea = cell.removeFromTop(knobHeight);
        
        // Force square aspect ratio for the knob
        int size = juce::jmin(sliderArea.getWidth(), sliderArea.getHeight());
        sliders[i]->setBounds(sliderArea.withSizeKeepingCentre(size, size));
        
        labels[i]->setBounds(cell);
    }
}