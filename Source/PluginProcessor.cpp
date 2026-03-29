#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout UTALISYNTHAudioProcessor::createParams() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterFloat>("TONE", "Tone", juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f), 2000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("RESO", "Resonance", 0.0f, 1.0f, 0.1f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("AGE", "Age", 0.0f, 1.0f, 0.02f));
    params.push_back(std::make_unique<juce::AudioParameterInt>("WAVE", "Waveform", 0, 3, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("LAG", "Lag", 5.0f, 30.0f, 15.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("WOBBLE", "Wobble", 0.0f, 1.0f, 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("MIX", "Chorus Mix", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("ATTACK", "Attack", 0.001f, 2.0f, 0.01f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("DECAY", "Decay", 0.0f, 2.0f, 0.1f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("SUSTAIN", "Sustain", 0.0f, 1.0f, 0.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("RELEASE", "Release", 0.01f, 5.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("SUB", "Sub Level", 0.0f, 1.0f, 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("DRIVE", "Drive", 1.0f, 3.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterInt>("UNISON", "Unison", 1, 4, 1));
    return { params.begin(), params.end() };
}

UTALISYNTHAudioProcessor::UTALISYNTHAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "Parameters", createParams()) {
    for (int i = 0; i < MAX_VOICES; i++) {
        auto* v = new SynthVoice();
        v->setVoiceIndex(i);
        mySynth.addVoice(v);
    }
    mySynth.addSound(new SynthSound());
}

UTALISYNTHAudioProcessor::~UTALISYNTHAudioProcessor() {}

void UTALISYNTHAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    mySynth.setCurrentPlaybackSampleRate(sampleRate);
    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, (juce::uint32)getTotalNumOutputChannels() };
    
    for (int i = 0; i < mySynth.getNumVoices(); i++)
        if (auto voice = dynamic_cast<SynthVoice*>(mySynth.getVoice(i)))
            voice->prepareToPlay(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    
    juliaChorus.prepare(spec);
    dcBlocker.prepare(spec);
    dcBlocker.setType(juce::dsp::FirstOrderTPTFilterType::highpass);
    dcBlocker.setCutoffFrequency(10.0f);  // 10Hz DC blocker post-saturation
    
    // Reset ADAA processors for anti-aliased saturation
    for (auto& adaa : adaaProcessors)
        adaa.reset();
    
    // Optimized smoothing times: 15ms for responsive, click-free transitions
    smoothedDrive.reset(sampleRate, 0.015);
    smoothedLag.reset(sampleRate, 0.015);
    smoothedWobble.reset(sampleRate, 0.015);
    smoothedMix.reset(sampleRate, 0.015);
}

void UTALISYNTHAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    auto numSamples = buffer.getNumSamples();

    // 1. Get and update parameters
    float tone = apvts.getRawParameterValue("TONE")->load();
    float reso = apvts.getRawParameterValue("RESO")->load();
    float age = apvts.getRawParameterValue("AGE")->load();
    int wave = (int)apvts.getRawParameterValue("WAVE")->load();
    float a = apvts.getRawParameterValue("ATTACK")->load();
    float d = apvts.getRawParameterValue("DECAY")->load();
    float s = apvts.getRawParameterValue("SUSTAIN")->load();
    float r = apvts.getRawParameterValue("RELEASE")->load();
    float sub = apvts.getRawParameterValue("SUB")->load();
    float drive = apvts.getRawParameterValue("DRIVE")->load();
    int uni = (int)apvts.getRawParameterValue("UNISON")->load();

    for (int i = 0; i < mySynth.getNumVoices(); i++)
        if (auto voice = dynamic_cast<SynthVoice*>(mySynth.getVoice(i)))
            voice->updateParams(tone, age, wave, a, d, s, r, sub, reso, uni);

    // 2. Clear and Render Synth
    buffer.clear();
    keyboardState.processNextMidiBuffer(midiMessages, 0, numSamples, true);
    mySynth.renderNextBlock(buffer, midiMessages, 0, numSamples);

    // 3. Update Chorus (Smoothed correctly for whole block)
    smoothedLag.setTargetValue(apvts.getRawParameterValue("LAG")->load());
    smoothedWobble.setTargetValue(apvts.getRawParameterValue("WOBBLE")->load());
    smoothedMix.setTargetValue(apvts.getRawParameterValue("MIX")->load());
    
    // Advance smoothers and set parameters
    smoothedLag.skip(numSamples);
    smoothedWobble.skip(numSamples);
    smoothedMix.skip(numSamples);
    
    juliaChorus.setParameters(smoothedLag.getCurrentValue(), smoothedWobble.getCurrentValue(), smoothedMix.getCurrentValue());
    juliaChorus.process(buffer);

    // 4. Drive / Saturation with ADAA (Anti-aliased)
    smoothedDrive.setTargetValue(drive);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        auto* data = buffer.getWritePointer(ch);
        auto& adaa = adaaProcessors[static_cast<size_t>(ch)];
        for (int s = 0; s < numSamples; ++s) {
            float val = data[s] * smoothedDrive.getNextValue();
            data[s] = adaa.processSample(val);  // ADAA anti-aliased tanh saturation
        }
    }
    
    // 5. DC Blocker and Soft Clip
    juce::dsp::AudioBlock<float> block(buffer);
    dcBlocker.process(juce::dsp::ProcessContextReplacing<float>(block));
    
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        auto* data = buffer.getWritePointer(ch);
        for (int s = 0; s < numSamples; ++s) {
            // Final safety catch: very soft limit
            if (std::abs(data[s]) > 0.99f) 
                data[s] = (data[s] > 0) ? 0.99f : -0.99f;
        }
    }
}

// ... (remaining methods stay as they were)
void UTALISYNTHAudioProcessor::releaseResources() {}
bool UTALISYNTHAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}
const juce::String UTALISYNTHAudioProcessor::getName() const { return JucePlugin_Name; }
bool UTALISYNTHAudioProcessor::acceptsMidi() const { return true; }
bool UTALISYNTHAudioProcessor::producesMidi() const { return false; }
bool UTALISYNTHAudioProcessor::isMidiEffect() const { return false; }
double UTALISYNTHAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int UTALISYNTHAudioProcessor::getNumPrograms() { return 1; }
int UTALISYNTHAudioProcessor::getCurrentProgram() { return 0; }
void UTALISYNTHAudioProcessor::setCurrentProgram(int index) {}
const juce::String UTALISYNTHAudioProcessor::getProgramName(int index) { return {}; }
void UTALISYNTHAudioProcessor::changeProgramName(int index, const juce::String& newName) {}
bool UTALISYNTHAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* UTALISYNTHAudioProcessor::createEditor() { return new UTALISYNTHAudioProcessorEditor(*this); }
void UTALISYNTHAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}
void UTALISYNTHAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr) if (xmlState->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new UTALISYNTHAudioProcessor(); }