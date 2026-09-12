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
    const juce::Colour vintageCharcoal(0xff141210);   // Deep chassis charcoal
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
        k.slider.setLookAndFeel(&utaliLookAndFeel);

        addAndMakeVisible(k.label);
        k.label.setText(cfg.displayName, juce::dontSendNotification);
        k.label.setJustificationType(juce::Justification::centred);
        k.label.setLookAndFeel(&utaliLookAndFeel);
        k.label.setFont(juce::FontOptions(10.0f).withStyle("Bold"));

        using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
        k.attachment = std::make_unique<Attachment>(audioProcessor.apvts, cfg.paramId, k.slider);
    }

    // Vintage styled keyboard: ivory white keys, matte ebony black keys
    addAndMakeVisible(keyboardComponent);
    keyboardComponent.setKeyWidth(34.0f);
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

    // Aspect ratio locking: 1024 width / 306 height (231 panel + 75 keyboard)
    setResizable(true, true);
    getConstrainer()->setFixedAspectRatio(1024.0 / 306.0);
    setResizeLimits(512, 153, 2048, 612);

    setSize(1024, 306);
}

UTALISYNTHAudioProcessorEditor::~UTALISYNTHAudioProcessorEditor() {
    for (auto& k : knobs) {
        k.slider.setLookAndFeel(nullptr);
        k.label.setLookAndFeel(nullptr);
    }
}

void UTALISYNTHAudioProcessorEditor::paint(juce::Graphics& g) {
    const float scale = static_cast<float>(getWidth()) / 1024.0f;
    const int panelH = juce::roundToInt(231.0f * scale);
    const auto cream = utali::UtaliLookAndFeel::getCreamColour();

    // 1. Draw illustrated vintage backpanel with high resampling quality
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

    if (background.isValid()) {
        g.drawImage(background, 0, 0, getWidth(), panelH, 0, 0, background.getWidth(), background.getHeight());
    } else {
        g.fillAll(juce::Colour(0xffB4AAA0)); // Fallback warm beige
    }

    // 2. Draw dark vintage keyboard bed
    auto keyboardArea = juce::Rectangle<int>(0, panelH, getWidth(), getHeight() - panelH);
    g.setColour(vintageCharcoal);
    g.fillRect(keyboardArea);
    g.setColour(juce::Colour(0xff38332C));
    g.drawHorizontalLine(panelH, 0.0f, static_cast<float>(getWidth()));

    // 3. Draw vintage stamped title on top metal rim
    auto titleArea = juce::Rectangle<int>(0, juce::roundToInt(3.0f * scale), getWidth(), juce::roundToInt(16.0f * scale));
    g.setFont(juce::FontOptions(13.0f * scale).withStyle("Bold"));

    // Subtle dark stamped shadow
    g.setColour(juce::Colour(0xbb141210));
    g.drawText("Utali Synth", titleArea.translated(1, 1), juce::Justification::centred, true);

    // Warm cream lettering
    g.setColour(cream);
    g.drawText("Utali Synth", titleArea, juce::Justification::centred, true);
}

void UTALISYNTHAudioProcessorEditor::resized() {
    const float scale = static_cast<float>(getWidth()) / 1024.0f;
    const int panelH = juce::roundToInt(231.0f * scale);

    keyboardComponent.setBounds(0, panelH, getWidth(), getHeight() - panelH);
    keyboardComponent.setKeyWidth(static_cast<float>(juce::jmax(16, juce::roundToInt(34.0f * scale))));

    // Active rack faceplate bounds between ears (x=38 to x=986 in 1024 canvas)
    const int startX = juce::roundToInt(38.0f * scale);
    const int totalWidth = getWidth() - (startX * 2);
    const int colWidth = totalWidth / 6;

    const int kSize = juce::roundToInt(58.0f * scale);
    const int lW = juce::roundToInt(60.0f * scale);
    const int lH = juce::roundToInt(17.0f * scale);

    auto layoutRow = [startX, colWidth, kSize, lW, lH, scale](int baseRowY, auto begin, auto end) {
        const int rowY = juce::roundToInt(static_cast<float>(baseRowY) * scale);
        int idx = 0;
        for (auto it = begin; it != end; ++it, ++idx) {
            const int cellX = startX + idx * colWidth;
            const int knobCenterX = cellX + colWidth / 2;

            it->slider.setBounds(knobCenterX - kSize / 2, rowY, kSize, kSize);
            it->label.setBounds(knobCenterX - lW / 2, rowY + kSize + juce::roundToInt(2.0f * scale), lW, lH);
            it->label.setFont(juce::FontOptions(juce::jmax(8.0f, 10.0f * scale)).withStyle("Bold"));
        }
    };

    // Row 1: y = 25 (comfortably below top rim)
    layoutRow(25, knobs.begin(), knobs.begin() + 6);

    // Row 2: y = 122 (comfortably above bottom rim)
    layoutRow(122, knobs.begin() + 6, knobs.end());
}
