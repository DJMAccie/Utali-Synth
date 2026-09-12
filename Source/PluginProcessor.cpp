#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout UTALISYNTHAudioProcessor::createParams() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<juce::AudioParameterFloat>("TONE", "Tone", juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f), 2000.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("RESO", "Resonance", 0.0f, 1.0f, 0.1f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("AGE", "Age", 0.0f, 1.0f, 0.02f));
    p.push_back(std::make_unique<juce::AudioParameterInt>("WAVE", "Waveform", 0, 3, 0));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("LAG", "Lag", 5.0f, 30.0f, 15.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("WOBBLE", "Wobble", 0.0f, 1.0f, 0.3f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("MIX", "Chorus Mix", 0.0f, 1.0f, 0.5f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("ATTACK", "Attack", 0.001f, 2.0f, 0.01f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("DECAY", "Decay", 0.0f, 2.0f, 0.1f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("SUSTAIN", "Sustain", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 0.5f), 0.7f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("RELEASE", "Release", 0.01f, 5.0f, 0.5f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("SUB", "Sub Level", 0.0f, 1.0f, 0.2f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("DRIVE", "Drive", 1.0f, 3.0f, 1.0f));
    p.push_back(std::make_unique<juce::AudioParameterInt>("UNISON", "Unison", 1, 4, 1));
    return { p.begin(), p.end() };
}

UTALISYNTHAudioProcessor::UTALISYNTHAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParams()) {
    for (int i = 0; i < MAX_VOICES; ++i) {
        auto* v = new SynthVoice();
        v->setVoiceIndex(i);
        voices[static_cast<size_t>(i)] = v;
        mySynth.addVoice(v);
    }
    mySynth.addSound(new SynthSound());

    params.tone = apvts.getRawParameterValue("TONE");
    params.reso = apvts.getRawParameterValue("RESO");
    params.age = apvts.getRawParameterValue("AGE");
    params.wave = apvts.getRawParameterValue("WAVE");
    params.lag = apvts.getRawParameterValue("LAG");
    params.wobble = apvts.getRawParameterValue("WOBBLE");
    params.mix = apvts.getRawParameterValue("MIX");
    params.attack = apvts.getRawParameterValue("ATTACK");
    params.decay = apvts.getRawParameterValue("DECAY");
    params.sustain = apvts.getRawParameterValue("SUSTAIN");
    params.release = apvts.getRawParameterValue("RELEASE");
    params.sub = apvts.getRawParameterValue("SUB");
    params.drive = apvts.getRawParameterValue("DRIVE");
    params.unison = apvts.getRawParameterValue("UNISON");
}

UTALISYNTHAudioProcessor::~UTALISYNTHAudioProcessor() {}

void UTALISYNTHAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    mySynth.setCurrentPlaybackSampleRate(sampleRate);
    juce::dsp::ProcessSpec spec{ sampleRate, static_cast<juce::uint32>(samplesPerBlock), static_cast<juce::uint32>(getTotalNumOutputChannels()) };
    
    for (auto* voice : voices)
        voice->prepareToPlay(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    
    chorus.prepare(spec);
    chorus.setRate(1.2f);

    dcBlocker.prepare(spec);
    dcBlocker.setType(juce::dsp::FirstOrderTPTFilterType::highpass);
    dcBlocker.setCutoffFrequency(10.0f);
    
    for (auto& adaa : adaaProcessors)
        adaa.reset();
    
    smoothedDrive.reset(sampleRate, 0.015);
}

void UTALISYNTHAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    // 1. Update voices with cached atomic values
    SynthVoiceParameters vParams;
    vParams.cutoff = params.tone->load();
    vParams.resonance = params.reso->load();
    vParams.age = params.age->load();
    vParams.waveType = static_cast<int>(params.wave->load());
    vParams.attack = params.attack->load();
    vParams.decay = params.decay->load();
    vParams.sustain = params.sustain->load();
    vParams.release = params.release->load();
    vParams.subLevel = params.sub->load();
    vParams.unisonCount = static_cast<int>(params.unison->load());

    for (auto* voice : voices)
        voice->updateParams(vParams);

    // 2. Render voices
    buffer.clear();
    keyboardState.processNextMidiBuffer(midiMessages, 0, numSamples, true);
    mySynth.renderNextBlock(buffer, midiMessages, 0, numSamples);

    // 3. Process chorus directly
    chorus.setCentreDelay(juce::jlimit(1.0f, 100.0f, params.lag->load()));
    chorus.setDepth(juce::jlimit(0.0f, 1.0f, params.wobble->load()));
    chorus.setMix(juce::jlimit(0.0f, 1.0f, params.mix->load()));

    juce::dsp::AudioBlock<float> chorusBlock(buffer);
    chorus.process(juce::dsp::ProcessContextReplacing<float>(chorusBlock));

    // 4. Drive & ADAA Saturation - synchronous sample advance
    smoothedDrive.setTargetValue(params.drive->load());
    const int processChannels = std::min(numChannels, 2);

    for (int s = 0; s < numSamples; ++s) {
        const float currentDrive = smoothedDrive.getNextValue();
        for (int ch = 0; ch < processChannels; ++ch) {
            auto* data = buffer.getWritePointer(ch);
            data[s] = adaaProcessors[static_cast<size_t>(ch)].processSample(data[s] * currentDrive);
        }
    }
    
    // 5. DC Blocker and safety limiter
    juce::dsp::AudioBlock<float> block(buffer);
    dcBlocker.process(juce::dsp::ProcessContextReplacing<float>(block));
    
    for (int ch = 0; ch < numChannels; ++ch) {
        auto* data = buffer.getWritePointer(ch);
        for (int s = 0; s < numSamples; ++s) {
            data[s] = juce::jlimit(-0.99f, 0.99f, data[s]);
        }
    }
}

void UTALISYNTHAudioProcessor::releaseResources() {}

bool UTALISYNTHAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() 
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

const juce::String UTALISYNTHAudioProcessor::getName() const {
#ifdef JucePlugin_Name
    return JucePlugin_Name;
#else
    return "UTALISYNTH";
#endif
}
bool UTALISYNTHAudioProcessor::acceptsMidi() const { return true; }
bool UTALISYNTHAudioProcessor::producesMidi() const { return false; }
bool UTALISYNTHAudioProcessor::isMidiEffect() const { return false; }
double UTALISYNTHAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int UTALISYNTHAudioProcessor::getNumPrograms() { return 1; }
int UTALISYNTHAudioProcessor::getCurrentProgram() { return 0; }
void UTALISYNTHAudioProcessor::setCurrentProgram(int) {}
const juce::String UTALISYNTHAudioProcessor::getProgramName(int) { return {}; }
void UTALISYNTHAudioProcessor::changeProgramName(int, const juce::String&) {}
bool UTALISYNTHAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* UTALISYNTHAudioProcessor::createEditor() { return new UTALISYNTHAudioProcessorEditor(*this); }

void UTALISYNTHAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    state.setProperty("version", CURRENT_STATE_VERSION, nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void UTALISYNTHAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType())) {
        auto tree = juce::ValueTree::fromXml(*xmlState);
        int version = tree.getProperty("version", 1);
        juce::ignoreUnused(version);
        apvts.replaceState(tree);
    }
}

#ifndef UTALI_BUILDING_TESTS
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new UTALISYNTHAudioProcessor(); }
#endif
