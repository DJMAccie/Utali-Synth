#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
    struct KnobConfig {
        const char* paramId;
        const char* displayName;
    };

    constexpr std::array<KnobConfig, 12> knobConfigs = {{
        { "WAVE", "Wave" },
        { "TONE", "Tone" },
        { "AGE", "Age" },
        { "SUB", "Sub" },
        { "DRIVE", "Drive" },
        { "MIX", "Mix" },
        { "LAG", "Lag" },
        { "WOBBLE", "Wobble" },
        { "ATTACK", "Attack" },
        { "DECAY", "Decay" },
        { "SUSTAIN", "Sustain" },
        { "RELEASE", "Release" }
    }};

    // Vintage palette sampled from the illustrated backpanel
    const juce::Colour vintageBeigeCream (0xffEFE7D5);   // Crisp cream beige
    const juce::Colour vintageBeigeBorder(0x55B4AAA0);   // Subtle badge border
    const juce::Colour vintageDarkPill   (0xd81A1816);   // Dark vintage tag backing
    const juce::Colour vintageCharcoal   (0xff141210);   // Deep chassis charcoal
}

UTALISYNTHAudioProcessorEditor::UTALISYNTHAudioProcessorEditor(UTALISYNTHAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      keyboardComponent(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard) {
    
    for (size_t i = 0; i < knobs.size(); ++i) {
        auto& k = knobs[i];
        const auto& cfg = knobConfigs[i];

        addAndMakeVisible(k.slider);
        k.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        k.slider.setLookAndFeel(&filmStripLookAndFeel);

        addAndMakeVisible(k.label);
        k.label.setText(cfg.displayName, juce::dontSendNotification);
        k.label.setJustificationType(juce::Justification::centred);
        
        // Vintage styled nameplate badge
        k.label.setColour(juce::Label::textColourId, vintageBeigeCream);
        k.label.setColour(juce::Label::backgroundColourId, vintageDarkPill);
        k.label.setColour(juce::Label::outlineColourId, vintageBeigeBorder);
        k.label.setFont(juce::FontOptions(10.0f).withStyle("Bold"));

        using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
        k.attachment = std::make_unique<Attachment>(audioProcessor.apvts, cfg.paramId, k.slider);
    }

    // Vintage styled keyboard: ivory white keys, matte ebony black keys
    addAndMakeVisible(keyboardComponent);
    keyboardComponent.setKeyWidth(34);
    keyboardComponent.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xffF2ECE0));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xff1C1A18));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour(0x5522201D));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour(0x33B4AAA0));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour(0x66B4AAA0));

    // Load illustrated vintage backpanel (1024 x 231 native)
    background = juce::ImageCache::getFromMemory(
        BinaryData::vintage_illustrated_panel_png,
        BinaryData::vintage_illustrated_panel_pngSize
    );

    // Exact 1:1 pixel dimensions: 1024 width, 231 panel + 75 keyboard = 306 height
    setSize(1024, 306);
}

UTALISYNTHAudioProcessorEditor::~UTALISYNTHAudioProcessorEditor() {
    for (auto& k : knobs)
        k.slider.setLookAndFeel(nullptr);
}

void UTALISYNTHAudioProcessorEditor::paint(juce::Graphics& g) {
    // 1. Draw illustrated vintage backpanel at exact 1:1 native resolution (NO stretching or warping)
    if (background.isValid()) {
        g.drawImage(background, 0, 0, 1024, 231, 0, 0, 1024, 231);
    } else {
        g.fillAll(juce::Colour(0xffB4AAA0)); // Fallback warm beige
    }

    // 2. Draw dark vintage keyboard bed
    auto keyboardArea = juce::Rectangle<int>(0, 231, 1024, 75);
    g.setColour(vintageCharcoal);
    g.fillRect(keyboardArea);
    g.setColour(juce::Colour(0xff38332C));
    g.drawHorizontalLine(231, 0.0f, 1024.0f);

    // 3. Draw vintage stamped title on top metal rim
    auto titleArea = juce::Rectangle<int>(0, 3, 1024, 16);
    g.setFont(juce::FontOptions(13.0f).withStyle("Bold"));

    // Subtle dark stamped shadow
    g.setColour(juce::Colour(0xbb141210));
    g.drawText("Utalisynth", titleArea.translated(1, 1), juce::Justification::centred, true);

    // Vintage cream beige lettering
    g.setColour(vintageBeigeCream);
    g.drawText("Utalisynth", titleArea, juce::Justification::centred, true);
}

void UTALISYNTHAudioProcessorEditor::resized() {
    keyboardComponent.setBounds(0, 231, 1024, 75);

    // Active rack faceplate bounds between ears (x=38 to x=986)
    const int startX = 38;
    const int totalWidth = 1024 - (startX * 2); // 948 px
    const int colWidth = totalWidth / 6;        // 158 px

    constexpr int kSize = 58;
    constexpr int lW = 60;
    constexpr int lH = 17;

    auto layoutRow = [startX, colWidth](int rowY, auto begin, auto end) {
        int idx = 0;
        for (auto it = begin; it != end; ++it, ++idx) {
            const int cellX = startX + idx * colWidth;
            const int knobCenterX = cellX + colWidth / 2;

            // Center knob horizontally in cell
            it->slider.setBounds(knobCenterX - kSize / 2, rowY, kSize, kSize);

            // Center label tag under knob
            it->label.setBounds(knobCenterX - lW / 2, rowY + kSize + 2, lW, lH);
        }
    };

    // Row 1: y = 25 (comfortably below top rim)
    layoutRow(25, knobs.begin(), knobs.begin() + 6);

    // Row 2: y = 122 (comfortably above bottom rim)
    layoutRow(122, knobs.begin() + 6, knobs.end());
}
