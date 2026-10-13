#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::String pctText (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }

    juce::String dbText (float v, int)
    {
        if (v <= -59.9f) return "-inf dB";
        return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB";
    }

    juce::String hzText (float hz)
    {
        return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz";
    }

    float dbToGain (float db) { return db <= -59.9f ? 0.0f : juce::Decibels::decibelsToGain (db); }

    // A preset sets the sound and leaves IN, OUT, MIX, HOLD, OPEN, DUB and KICK alone.
    struct Preset
    {
        const char* name;
        bool sync;
        float time, feedback, tone, thin, wobble, spread, spring, decay, tension, drip, feed, shake, drive, noise, filter;
    };

    constexpr float div (int i) { return (float) i / (float) (tby::kDivs - 1); }

    const Preset presets[] = {
        //  name          sync  time     fb     tone   thin   wob    sprd   sprg   dcy    tens   drip   feed   shk    drv    nse    filt
        { "KING",         true, div (5), 0.68f, 0.30f, 0.35f, 0.30f, 0.70f, 0.35f, 0.55f, 0.50f, 0.60f, 0.60f, 0.00f, 0.35f, 0.25f, 0.00f },
        { "SCIENTIST",    true, div (6), 0.75f, 0.20f, 0.30f, 0.45f, 0.85f, 0.50f, 0.65f, 0.40f, 0.80f, 0.70f, 0.20f, 0.30f, 0.30f, 0.25f },
        { "BLACK ARK",    true, div (3), 0.74f, 0.25f, 0.25f, 0.60f, 0.60f, 0.45f, 0.60f, 0.60f, 0.70f, 0.50f, 0.30f, 0.60f, 0.50f, 0.00f },
        { "LANKY",        true, div (10), 0.82f, 0.15f, 0.30f, 0.70f, 1.00f, 0.40f, 0.80f, 0.70f, 0.55f, 0.80f, 0.00f, 0.25f, 0.30f, -0.15f },
        { "SIREN",        true, div (0), 1.10f, 0.45f, 0.50f, 0.25f, 0.50f, 0.20f, 0.50f, 0.50f, 0.50f, 0.80f, 0.00f, 0.50f, 0.20f, 0.10f },
        { "DRIP",         true, div (8), 0.30f, 0.30f, 0.40f, 0.20f, 0.60f, 0.90f, 0.70f, 0.55f, 0.90f, 0.10f, 0.60f, 0.20f, 0.20f, 0.00f },
        { "HI-FI",        true, div (5), 0.55f, 0.75f, 0.15f, 0.05f, 0.80f, 0.20f, 0.45f, 0.50f, 0.40f, 0.60f, 0.00f, 0.05f, 0.00f, 0.00f },
        { "CHANNEL ONE",  false, 0.62f, 0.72f, 0.28f, 0.45f, 0.35f, 0.50f, 0.30f, 0.50f, 0.45f, 0.60f, 0.50f, 0.10f, 0.45f, 0.35f, 0.40f },
    };
    constexpr int kNumPresets = (int) (sizeof (presets) / sizeof (presets[0]));
}

juce::AudioProcessorValueTreeState::ParameterLayout TubbyProcessor::createLayout (std::shared_ptr<SyncLink> link)
{
    using namespace juce;
    using F = AudioParameterFloat;
    using B = AudioParameterBool;
    using A = AudioParameterFloatAttributes;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto pct = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<F> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def,
                                    A().withStringFromValueFunction (pctText));
    };
    auto gain = [] (const char* id, const char* name)
    {
        return std::make_unique<F> (ParameterID { id, 1 }, name, NormalisableRange<float> (-60.0f, 12.0f, 0.0f, 2.0f), 0.0f,
                                    A().withStringFromValueFunction (dbText));
    };

    // ECHO
    l.add (std::make_unique<F> (ParameterID { ParamIDs::time, 1 }, "Time", NormalisableRange<float> (0.0f, 1.0f), div (5),
                                A().withStringFromValueFunction ([link] (float v, int)
                                {
                                    auto* raw = link->raw.load();
                                    const bool sync = raw == nullptr || raw->load() > 0.5f;
                                    if (sync)
                                        return juce::String (tby::kDivName[tby::divIndex (v)]);
                                    const double ms = tby::freeMs (v);
                                    return ms >= 1000.0 ? juce::String (ms / 1000.0, 2) + " s"
                                                        : juce::String (juce::roundToInt (ms)) + " ms";
                                })));
    l.add (std::make_unique<F> (ParameterID { ParamIDs::feedback, 1 }, "Feedback", NormalisableRange<float> (0.0f, 1.25f), 0.68f,
                                A().withStringFromValueFunction (pctText)));
    l.add (std::make_unique<F> (ParameterID { ParamIDs::tone, 1 }, "Tone", NormalisableRange<float> (0.0f, 1.0f), 0.3f,
                                A().withStringFromValueFunction ([] (float v, int) { return hzText (600.0f * std::pow (15.0f, v)); })));
    l.add (std::make_unique<F> (ParameterID { ParamIDs::thin, 1 }, "Thin", NormalisableRange<float> (0.0f, 1.0f), 0.35f,
                                A().withStringFromValueFunction ([] (float v, int) { return hzText (20.0f * std::pow (45.0f, v)); })));
    l.add (pct (ParamIDs::wobble, "Wobble", 0.3f));
    l.add (pct (ParamIDs::spread, "Spread", 0.7f));

    // SPRING
    l.add (pct (ParamIDs::spring, "Spring", 0.35f));
    l.add (pct (ParamIDs::decay, "Spring Decay", 0.55f));
    l.add (pct (ParamIDs::tension, "Tension", 0.5f));
    l.add (pct (ParamIDs::drip, "Drip", 0.6f));
    l.add (std::make_unique<F> (ParameterID { ParamIDs::feed, 1 }, "Spring Feed", NormalisableRange<float> (0.0f, 1.0f), 0.6f,
                                A().withStringFromValueFunction ([] (float v, int)
                                {
                                    if (v < 0.03f) return juce::String ("dry");
                                    if (v > 0.97f) return juce::String ("echo");
                                    return "echo " + juce::String (juce::roundToInt (v * 100.0f));
                                })));
    l.add (pct (ParamIDs::shake, "Shake", 0.0f));

    // DESK
    l.add (pct (ParamIDs::drive, "Drive", 0.35f));
    l.add (pct (ParamIDs::noise, "Noise", 0.25f));
    l.add (std::make_unique<F> (ParameterID { ParamIDs::filter, 1 }, "Filter", NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
                                A().withStringFromValueFunction ([] (float v, int)
                                {
                                    if (std::abs (v) <= 0.015f) return juce::String ("flat");
                                    if (v > 0.0f) return "HP " + hzText (40.0f * std::pow (100.0f, v));
                                    return "LP " + hzText (14000.0f * std::pow (350.0f / 14000.0f, -v));
                                })));

    // OUTPUT
    l.add (gain (ParamIDs::in, "In"));
    l.add (gain (ParamIDs::out, "Out"));
    l.add (pct (ParamIDs::mix, "Mix", 1.0f));   // a send effect: all wet by default

    // switches
    l.add (std::make_unique<B> (ParameterID { ParamIDs::sync, 1 }, "Sync", true));
    l.add (std::make_unique<B> (ParameterID { ParamIDs::hold, 1 }, "Hold", false));
    l.add (std::make_unique<B> (ParameterID { ParamIDs::altec, 1 }, "Altec", false));
    l.add (std::make_unique<B> (ParameterID { ParamIDs::open, 1 }, "Open", true));
    l.add (std::make_unique<B> (ParameterID { ParamIDs::dub, 1 }, "Dub", false));
    l.add (std::make_unique<B> (ParameterID { ParamIDs::kick, 1 }, "Kick", false));

    return l;
}

TubbyProcessor::TubbyProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout (syncLink))
{
    const char* ids[kNumP] = { ParamIDs::time, ParamIDs::feedback, ParamIDs::tone, ParamIDs::thin, ParamIDs::wobble,
                               ParamIDs::spread, ParamIDs::spring, ParamIDs::decay, ParamIDs::tension, ParamIDs::drip,
                               ParamIDs::feed, ParamIDs::shake, ParamIDs::drive, ParamIDs::noise, ParamIDs::filter,
                               ParamIDs::in, ParamIDs::out, ParamIDs::mix, ParamIDs::sync, ParamIDs::hold, ParamIDs::altec,
                               ParamIDs::open, ParamIDs::dub, ParamIDs::kick };
    for (int i = 0; i < kNumP; ++i)
        p[i] = apvts.getRawParameterValue (ids[i]);
    syncLink->raw.store (p[pSync]);
}

bool TubbyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void TubbyProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int block = juce::jmax (1, samplesPerBlock);
    engine.prepare (sampleRate, (uint32_t) juce::Random::getSystemRandom().nextInt());
    inBuf.setSize (2, block);
    wetBuf.setSize (2, block);
    inGainS.reset (sampleRate, 0.03);  inGainS.setCurrentAndTargetValue (dbToGain (get (pIn)));
    outGainS.reset (sampleRate, 0.03); outGainS.setCurrentAndTargetValue (dbToGain (get (pOut)));
    mixS.reset (sampleRate, 0.03);     mixS.setCurrentAndTargetValue (get (pMix));
}

void TubbyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0)
        return;

    if (getTotalNumInputChannels() == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    if (inBuf.getNumSamples() < n)   // host broke its block-size promise
    {
        inBuf.setSize (2, n, false, false, true);
        wetBuf.setSize (2, n, false, false, true);
    }

    tby::Params ep;
    ep.time = get (pTime);       ep.feedback = get (pFeedback); ep.tone = get (pTone);
    ep.thin = get (pThin);       ep.wobble = get (pWobble);     ep.spread = get (pSpread);
    ep.spring = get (pSpring);   ep.decay = get (pDecay);       ep.tension = get (pTension);
    ep.drip = get (pDrip);       ep.feed = get (pFeed);         ep.shake = get (pShake);
    ep.drive = get (pDrive);     ep.noise = get (pNoise);       ep.filter = get (pFilter);
    ep.sync = get (pSync) > 0.5f;
    ep.hold = get (pHold) > 0.5f;
    ep.altec = get (pAltec) > 0.5f;
    ep.open = get (pOpen) > 0.5f;
    ep.dub = get (pDub) > 0.5f;
    ep.kick = get (pKick) > 0.5f;

    tby::Clock clock;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                clock.bpm = *bpm;

    inGainS.setTargetValue (dbToGain (get (pIn)));
    outGainS.setTargetValue (dbToGain (get (pOut)));
    mixS.setTargetValue (get (pMix));

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    auto* iL = inBuf.getWritePointer (0);  auto* iR = inBuf.getWritePointer (1);
    auto* wL = wetBuf.getWritePointer (0); auto* wR = wetBuf.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        const float g = inGainS.getNextValue();
        iL[i] = L[i] * g;
        iR[i] = R[i] * g;
    }

    engine.process (iL, iR, wL, wR, n, ep, clock);

    for (int i = 0; i < n; ++i)
    {
        const float m = mixS.getNextValue();
        const float og = outGainS.getNextValue();
        L[i] = (L[i] * (1.0f - m) + wL[i] * m) * og;
        R[i] = (R[i] * (1.0f - m) + wR[i] * m) * og;
    }

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);
}

//==============================================================================
int TubbyProcessor::getNumPrograms() { return kNumPresets; }

const juce::String TubbyProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, kNumPresets) ? juce::String (presets[index].name) : juce::String();
}

void TubbyProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, kNumPresets))
        return;
    currentProgram = index;
    const auto& k = presets[index];
    auto set = [this] (const char* id, float v)
    {
        if (auto* prm = apvts.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
            prm->endChangeGesture();
        }
    };
    set (ParamIDs::sync, k.sync ? 1.0f : 0.0f);
    set (ParamIDs::time, k.time);       set (ParamIDs::feedback, k.feedback); set (ParamIDs::tone, k.tone);
    set (ParamIDs::thin, k.thin);       set (ParamIDs::wobble, k.wobble);     set (ParamIDs::spread, k.spread);
    set (ParamIDs::spring, k.spring);   set (ParamIDs::decay, k.decay);       set (ParamIDs::tension, k.tension);
    set (ParamIDs::drip, k.drip);       set (ParamIDs::feed, k.feed);         set (ParamIDs::shake, k.shake);
    set (ParamIDs::drive, k.drive);     set (ParamIDs::noise, k.noise);       set (ParamIDs::filter, k.filter);
}

//==============================================================================
void TubbyProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TubbyProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;
    auto state = juce::ValueTree::fromXml (*xml);
    currentProgram = juce::jlimit (0, kNumPresets - 1, (int) state.getProperty ("preset", 0));
    state.removeProperty ("preset", nullptr);
    apvts.replaceState (state);
    // DUB and KICK are momentary; never come back from a saved song with them held down.
    for (auto* id : { ParamIDs::dub, ParamIDs::kick })
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (0.0f);
}

juce::AudioProcessorEditor* TubbyProcessor::createEditor()
{
    return new TubbyEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TubbyProcessor();
}
