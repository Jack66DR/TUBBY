#include "PluginEditor.h"
#include "BinaryData.h"

using namespace tby::ui;

//==============================================================================
juce::Font tby::ui::mono (float height)
{
    static juce::Typeface::Ptr stm = juce::Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf,
                                                                             BinaryData::ShareTechMonoRegular_ttfSize);
    // JUCE font height includes the descender, so it reads smaller than a CSS px size.
    return juce::Font (juce::FontOptions (stm).withHeight (height * kFontScale));
}

static float textWidth (const juce::Font& f, const juce::String& s)
{
    return juce::GlyphArrangement::getStringWidth (f, s);
}

void tby::ui::glowText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, juce::Justification j,
                         const juce::Font& f, juce::Colour c, float alpha)
{
    g.setFont (f);
    static const float offs[8][2] = { { -1.6f, 0.0f }, { 1.6f, 0.0f }, { 0.0f, -1.6f }, { 0.0f, 1.6f },
                                      { -1.1f, -1.1f }, { 1.1f, 1.1f }, { -1.1f, 1.1f }, { 1.1f, -1.1f } };
    g.setColour (c.withAlpha (0.10f * alpha));
    for (auto& o : offs)
        g.drawText (text, r.translated (o[0], o[1]), j, false);
    g.setColour (c.withAlpha (alpha));
    g.drawText (text, r, j, false);
}

void tby::ui::glowStroke (juce::Graphics& g, const juce::Path& p, float width, juce::Colour c, float alpha)
{
    const auto js = juce::PathStrokeType::curved;
    g.setColour (c.withAlpha (0.07f * alpha));
    g.strokePath (p, juce::PathStrokeType (width * 5.0f, js, juce::PathStrokeType::rounded));
    g.setColour (c.withAlpha (0.16f * alpha));
    g.strokePath (p, juce::PathStrokeType (width * 2.6f, js, juce::PathStrokeType::rounded));
    g.setColour (c.withAlpha (alpha));
    g.strokePath (p, juce::PathStrokeType (width, js, juce::PathStrokeType::rounded));
}

static void glowBox (juce::Graphics& g, juce::Rectangle<float> r, float width, float alpha)
{
    juce::Path p;
    p.addRectangle (r);
    glowStroke (g, p, width, yel, alpha);
}

//==============================================================================
LookAndFeel::LookAndFeel (const Skin& s) : skin (s)
{
    setColour (juce::ResizableWindow::backgroundColourId, tube);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float startAngle, float endAngle, juce::Slider& s)
{
    const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);

    // a terminal field: [ LABEL      VALUE ]
    if ((bool) s.getProperties().getWithDefault ("field", false))
    {
        const auto r = area.reduced (2.5f);
        const bool over = s.isMouseOverOrDragging();
        if (over)
        {
            g.setColour (yel.withAlpha (0.10f));
            g.fillRect (r);
        }
        glowBox (g, r, 1.4f, over ? 1.0f : 0.8f);
        const auto label = s.getProperties().getWithDefault ("label", "").toString();
        const auto f = mono (13.0f).withExtraKerningFactor (0.1f);
        glowText (g, label, r.reduced (10.0f, 0.0f), juce::Justification::centredLeft, f, yel, 0.65f);
        glowText (g, s.getTextFromValue (s.getValue()).toUpperCase(), r.reduced (10.0f, 0.0f),
                  juce::Justification::centredRight, mono (15.0f).withExtraKerningFactor (0.08f), yel);
        return;
    }

    // CRT knob: a ring of tick marks, lit up to the value, around a dim bezel and a glowing needle
    const float size = (float) juce::jmin (w, h) - 16.0f;
    const float radius = size * 0.5f - 3.0f;
    const auto centre = area.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;

    constexpr int ticks = 31;
    const float r0 = radius + 2.0f, r1 = radius + 8.0f;
    for (int i = 0; i < ticks; ++i)
    {
        const float a = startAngle + (float) i / (ticks - 1) * (endAngle - startAngle);
        const bool lit = (a >= juce::jmin (from, angle) - 0.001f && a <= juce::jmax (from, angle) + 0.001f);
        juce::Path t;
        t.startNewSubPath (centre.x + r0 * std::sin (a), centre.y - r0 * std::cos (a));
        t.lineTo (centre.x + r1 * std::sin (a), centre.y - r1 * std::cos (a));
        if (lit)
            glowStroke (g, t, 2.0f, yel, 0.95f);
        else
        {
            g.setColour (yel.withAlpha (0.22f));
            g.strokePath (t, juce::PathStrokeType (1.6f));
        }
    }

    juce::Path bezel;
    bezel.addEllipse (juce::Rectangle<float> (radius * 1.6f, radius * 1.6f).withCentre (centre));
    g.setColour (yel.withAlpha (0.45f));
    g.strokePath (bezel, juce::PathStrokeType (1.5f));

    juce::Path needle;
    needle.startNewSubPath (centre.x + radius * 0.18f * std::sin (angle), centre.y - radius * 0.18f * std::cos (angle));
    needle.lineTo (centre.x + radius * 0.78f * std::sin (angle), centre.y - radius * 0.78f * std::cos (angle));
    glowStroke (g, needle, 2.6f, yel);

    g.setColour (yel);
    g.fillRect (juce::Rectangle<float> (3.0f, 3.0f).withCentre (centre));
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.5f);
    const bool on = b.getToggleState() || down;
    if (on)
    {
        g.setColour (yel.withAlpha (0.18f));
        g.fillRect (r.expanded (2.0f));
        g.setColour (yel);
        g.fillRect (r);
    }
    else
    {
        if (over)
        {
            g.setColour (yel.withAlpha (0.10f));
            g.fillRect (r);
        }
        glowBox (g, r, 1.4f, 0.9f);
    }
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    const bool on = b.getToggleState() || down;
    juce::String text = b.getButtonText();
    // a lit switch blinks a block in front of its name, like a status lamp
    if (b.getToggleState() && (bool) b.getProperties().getWithDefault ("lamp", false))
        text = (skin.blink() ? juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xa0 ")) : "  ") + text;
    const auto f = mono (16.0f).withExtraKerningFactor (0.12f);
    if (on)
    {
        g.setColour (tube);
        g.setFont (f);
        g.drawText (text, b.getLocalBounds(), juce::Justification::centred);
    }
    else
        glowText (g, text, b.getLocalBounds().toFloat(), juce::Justification::centred, f, yel);
}

//==============================================================================
static constexpr float kLabelSize = 17.0f, kValueSize = 15.0f, kKnobSize = 64.0f;

Knob::Knob (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& text, bool bipolar)
    : label (text)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (220);
    slider.setTitle (text);
    slider.getProperties().set ("bipolar", bipolar);
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    param = state.getParameter (paramId);
    if (param != nullptr)
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
}

void Knob::resized()
{
    const float top = kLabelSize * kFontScale + 12.0f;
    slider.setBounds (juce::Rectangle<float> (kKnobSize + 16.0f, kKnobSize + 16.0f)
                          .withCentre ({ getWidth() * 0.5f, top + kKnobSize * 0.5f + 4.0f })
                          .toNearestInt());
}

void Knob::paint (juce::Graphics& g)
{
    const float w = (float) getWidth();
    const float labelH = kLabelSize * kFontScale;
    const float vy = labelH + 12.0f + kKnobSize + 18.0f;
    const juce::String value = param != nullptr ? param->getCurrentValueAsText() : juce::String();
    glowText (g, label, { 0.0f, 0.0f, w, labelH + 4.0f }, juce::Justification::centred,
              mono (kLabelSize).withExtraKerningFactor (0.14f), yel);
    glowText (g, value.toUpperCase(), { 0.0f, vy, w, kValueSize * kFontScale + 6.0f }, juce::Justification::centred,
              mono (kValueSize), yel, 0.8f);
}

//==============================================================================
void PresetSelector::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (2.5f);
    glowBox (g, r, 1.4f, 0.85f);
    const float w = r.getWidth();
    const auto mouse = getMouseXYRelative().toFloat();
    const bool over = isMouseOver();
    const bool overLeft = over && mouse.x < 52.0f, overRight = over && mouse.x > getWidth() - 52.0f;

    auto arrow = [&] (juce::Rectangle<float> a, bool left, bool hot)
    {
        if (hot)
        {
            g.setColour (yel.withAlpha (0.14f));
            g.fillRect (a);
        }
        glowText (g, left ? "<" : ">", a, juce::Justification::centred, mono (20.0f), yel, hot ? 1.0f : 0.75f);
    };
    arrow ({ r.getX(), r.getY(), 48.0f, r.getHeight() }, true, overLeft);
    arrow ({ r.getRight() - 48.0f, r.getY(), 48.0f, r.getHeight() }, false, overRight);

    glowText (g, "SET", { r.getX() + 56.0f, r.getY(), 60.0f, r.getHeight() }, juce::Justification::centredLeft,
              mono (13.0f).withExtraKerningFactor (0.2f), yel, 0.6f);
    const int index = proc.getCurrentProgram();
    glowText (g, juce::String (index + 1).paddedLeft ('0', 2) + "  " + proc.getProgramName (index),
              { r.getX() + 96.0f, r.getY(), w - 150.0f, r.getHeight() }, juce::Justification::centredLeft,
              mono (17.0f).withExtraKerningFactor (0.16f), yel);
}

void PresetSelector::mouseDown (const juce::MouseEvent& e)
{
    const int count = proc.getNumPrograms();
    const int step = e.x < 52 ? -1 : 1;
    proc.setCurrentProgram ((proc.getCurrentProgram() + step + count) % count);
    proc.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withProgramChanged (true));
    repaint();
}

void PresetSelector::tick()
{
    const int now = proc.getCurrentProgram();
    if (now != shown)
    {
        shown = now;
        repaint();
    }
}

//==============================================================================
Glass::Glass (const Skin& s) : skin (s)
{
    setInterceptsMouseClicks (false, false);
}

void Glass::resized()
{
    // scan lines: one dark line every 3 px, drawn once
    lines = juce::Image (juce::Image::ARGB, juce::jmax (1, getWidth()), juce::jmax (1, getHeight()), true);
    juce::Graphics g (lines);
    g.setColour (juce::Colours::black.withAlpha (0.28f));
    for (int y = 0; y < getHeight(); y += 3)
        g.fillRect (0, y, getWidth(), 1);

    // vignette: the tube darkens toward its corners
    const float w = (float) getWidth(), h = (float) getHeight();
    juce::ColourGradient v (juce::Colours::transparentBlack, w * 0.5f, h * 0.5f,
                            juce::Colours::black.withAlpha (0.55f), 0.0f, 0.0f, true);
    v.addColour (0.6, juce::Colours::transparentBlack);
    g.setGradientFill (v);
    g.fillRect (getLocalBounds());
}

void Glass::paint (juce::Graphics& g)
{
    const double age = skin.t - bootStart;
    if (age >= kBootSecs)
    {
        g.drawImageAt (lines, 0, 0);
        return;
    }

    // boot sequence: type the lines, hold, then fade the screen up
    const float fade = age < 2.4 ? 1.0f : (float) (1.0 - (age - 2.4) / (kBootSecs - 2.4));
    g.setColour (tube.withAlpha (juce::jlimit (0.0f, 1.0f, fade)));
    g.fillRect (getLocalBounds());

    static const char* boot[] = {
        "MANNERS! DSP  INTERFACE SYSTEM",
        "",
        "TUBBY  DUB DELAY",
        "TAPE ..................... 5.0 S, TWO HEADS",
        "SPRINGS .................. 2, SLIGHTLY BENT",
        "BASS ..................... HEAVY",
        "STUDIO ................... DROMILLY AVENUE",
        "",
        "> READY" };
    const int totalChars = (int) (age * 95.0);
    const auto f = mono (20.0f).withExtraKerningFactor (0.08f);
    int used = 0;
    float y = 230.0f, curX = 230.0f, curY = 230.0f;
    for (auto* line : boot)
    {
        const juce::String s (line);
        const int show = juce::jlimit (0, s.length(), totalChars - used);
        used += s.length() + 4;   // a little pause at the end of each line
        if (show > 0)
            glowText (g, s.substring (0, show), { 230.0f, y, 860.0f, 30.0f }, juce::Justification::centredLeft, f, yel, fade);
        curX = 230.0f + textWidth (f, s.substring (0, show)) + 4.0f;
        curY = y;
        if (show < s.length() || totalChars - used < 0)
            break;
        y += 34.0f;
    }
    if (std::fmod (age, 0.4) < 0.22)
    {
        g.setColour (yel.withAlpha (fade));
        g.fillRect (curX, curY + 5.0f, 12.0f, 21.0f);
    }
    g.drawImageAt (lines, 0, 0);
}

//==============================================================================
// Monitor geometry, in its own 392 x 380 space.
static constexpr float kTapeX = 16.0f, kTapeTop = 82.0f, kTapeBottom = 178.0f;

Monitor::Monitor (TubbyProcessor& p, const Skin& s) : proc (p), skin (s)
{
    setTitle ("Tape monitor: the last four seconds of tape with the echo heads, and the two springs");
    setInterceptsMouseClicks (false, false);
    lastCrashes = proc.engine.uiCrashes.load();
}

void Monitor::tick()
{
    auto& e = proc.engine;
    const int w = e.uiColWrite.load (std::memory_order_acquire);
    for (int k = 0; k < tby::kViewCols; ++k)
        cols[(size_t) k] = e.uiCols[(size_t) ((w + k) % tby::kViewCols)].load (std::memory_order_relaxed);
    delayMs = e.uiDelayMs.load();
    targetMs = e.uiTargetMs.load();
    loop = e.uiLoop.load();
    spring = e.uiSpring.load();
    send = e.uiSend.load();
    bpm = e.uiBpm.load();
    fb = proc.apvts.getRawParameterValue (ParamIDs::feedback)->load();
    sync = proc.apvts.getRawParameterValue (ParamIDs::sync)->load() > 0.5f;
    hold = proc.apvts.getRawParameterValue (ParamIDs::hold)->load() > 0.5f;
    dub = proc.apvts.getRawParameterValue (ParamIDs::dub)->load() > 0.5f;

    const uint32_t c = e.uiCrashes.load();
    if (c != lastCrashes) crashGlow = 1.0f;
    else crashGlow = juce::jmax (0.0f, crashGlow - 0.06f);
    lastCrashes = c;
    coilPhase += 0.05f + juce::jmin (1.0f, spring) * 0.6f;
    repaint();
}

void Monitor::paint (juce::Graphics& g)
{
    g.fillAll (tube);
    if (crashGlow < 0.55f)
    {
        paintFace (g);
        return;
    }
    // a crash shakes the picture, like knocking the cabinet
    g.saveState();
    g.setOrigin (juce::Point<float> ((random.nextFloat() - 0.5f) * 8.0f * crashGlow, (random.nextFloat() - 0.5f) * 5.0f * crashGlow).toInt());
    paintFace (g);
    g.restoreState();
}

static juce::String msText (float ms)
{
    return ms >= 1000.0f ? juce::String (ms / 1000.0f, 2) + " S" : juce::String (juce::roundToInt (ms)) + " MS";
}

void Monitor::paintFace (juce::Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();
    const float tapeW = w - 2.0f * kTapeX;
    const float mid = (kTapeTop + kTapeBottom) * 0.5f, half = (kTapeBottom - kTapeTop) * 0.5f;
    const auto small = mono (13.0f).withExtraKerningFactor (0.16f);
    const auto readout = mono (14.0f).withExtraKerningFactor (0.14f);

    // ---- header
    glowText (g, "TAPE", { 16.0f, 10.0f, 160.0f, 22.0f }, juce::Justification::centredLeft, readout, yel, 0.8f);
    const bool gliding = std::abs (delayMs - targetMs) > 3.0f;
    glowText (g, (gliding ? juce::String ("GLIDE ") : juce::String ("ECHO ")) + msText (delayMs),
              { w - 216.0f, 10.0f, 200.0f, 22.0f }, juce::Justification::centredRight, readout, yel, gliding ? 1.0f : 0.8f);

    juce::String clock;
    if (sync)
    {
        const float t = proc.apvts.getRawParameterValue (ParamIDs::time)->load();
        const bool whole = std::abs (bpm - std::round (bpm)) < 0.05f;
        clock << "SYNC " << tby::kDivName[tby::divIndex (t)] << "  " << (whole ? juce::String (juce::roundToInt (bpm)) : juce::String (bpm, 1)) << " BPM";
    }
    else
        clock << "FREE";
    glowText (g, clock, { 16.0f, 32.0f, 260.0f, 22.0f }, juce::Justification::centredLeft, readout, yel, 0.8f);
    if (fb > 1.0f)
    {
        if (skin.blink())
            glowText (g, "RUNAWAY " + juce::String (juce::roundToInt (fb * 100.0f)) + " %", { w - 216.0f, 32.0f, 200.0f, 22.0f },
                      juce::Justification::centredRight, readout, yel);
    }
    else
        glowText (g, "FB " + juce::String (juce::roundToInt (fb * 100.0f)) + " %", { w - 216.0f, 32.0f, 200.0f, 22.0f },
                  juce::Justification::centredRight, readout, yel, 0.8f);

    // ---- graticule
    g.setColour (yel.withAlpha (0.07f));
    for (int k = 0; k <= 4; ++k)
    {
        const float x = kTapeX + tapeW * (float) k / 4.0f;
        for (float y = kTapeTop; y < kTapeBottom; y += 8.0f)
            g.fillRect (x, y, 1.0f, 3.0f);
    }
    for (float x = kTapeX; x < kTapeX + tapeW; x += 8.0f)
        g.fillRect (x, mid, 3.0f, 1.0f);

    // ---- the tape: what went onto it, newest at the right
    {
        const float colW = tapeW / (float) tby::kViewCols;
        juce::Path wave;
        for (int k = 0; k < tby::kViewCols; ++k)
        {
            const float v = std::sqrt (juce::jlimit (0.0f, 1.0f, cols[(size_t) k] * 1.3f)) * half * 0.94f;
            if (v < 0.6f) continue;
            const float x = kTapeX + ((float) k + 0.5f) * colW;
            wave.startNewSubPath (x, mid - v);
            wave.lineTo (x, mid + v);
        }
        g.setColour (yel.withAlpha (0.55f));
        g.strokePath (wave, juce::PathStrokeType (juce::jmax (1.0f, colW * 0.7f)));
    }

    // ---- the heads: where each repeat is reading from, fading with the feedback
    const float span = (float) tby::kViewSeconds * 1000.0f;
    float alpha = 0.95f;
    for (int k = 1; k <= 12; ++k)
    {
        const float age = delayMs * (float) k;
        if (age >= span) break;
        const float x = kTapeX + tapeW * (1.0f - age / span);
        juce::Path head;
        head.startNewSubPath (x, kTapeTop - 4.0f);
        head.lineTo (x, kTapeBottom + 4.0f);
        juce::Path dashed;
        const float dashes[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.4f).createDashedStroke (dashed, head, dashes, 2);
        g.setColour (yel.withAlpha (alpha));
        g.fillPath (dashed);
        if (k <= 4 && delayMs > 120.0f)
            glowText (g, juce::String (k), { x - 12.0f, kTapeTop - 21.0f, 24.0f, 16.0f }, juce::Justification::centred, small, yel, alpha);
        alpha *= juce::jlimit (0.25f, 0.92f, fb);
        if (alpha < 0.08f) break;
    }

    // ---- time ruler
    for (int k = 0; k <= 4; ++k)
    {
        const float x = kTapeX + tapeW * (float) k / 4.0f;
        g.setColour (yel.withAlpha (0.5f));
        g.fillRect (x - 0.5f, kTapeBottom + 8.0f, 1.0f, 6.0f);
        const juce::String lbl = k == 4 ? juce::String ("NOW") : "-" + juce::String (4 - k) + "S";
        const auto just = k == 0 ? juce::Justification::centredLeft : k == 4 ? juce::Justification::centredRight
                                                                             : juce::Justification::centred;
        const float bx = k == 0 ? x : k == 4 ? x - 60.0f : x - 30.0f;
        glowText (g, lbl, { bx, kTapeBottom + 14.0f, 60.0f, 18.0f }, just, small, yel, 0.6f);
    }

    // ---- the springs: two coils that shake with what's in them
    g.setColour (yel.withAlpha (0.35f));
    g.fillRect (16.0f, 222.0f, w - 32.0f, 1.0f);
    glowText (g, "SPRINGS", { 16.0f, 228.0f, 160.0f, 20.0f }, juce::Justification::centredLeft, small, yel, 0.7f);
    if (crashGlow > 0.05f)
        glowText (g, "CRASH", { w - 176.0f, 228.0f, 160.0f, 20.0f }, juce::Justification::centredRight, readout, yel, crashGlow);

    const float energy = juce::jlimit (0.0f, 1.0f, std::sqrt (spring * 3.0f));
    for (int c = 0; c < 2; ++c)
    {
        const float cy = 274.0f + (float) c * 44.0f;
        juce::Path coil;
        const float x0 = 30.0f, x1 = w - 30.0f;
        const float amp = 9.0f + energy * 7.0f;
        bool first = true;
        for (float t = 0.0f; t <= x1 - x0; t += 1.5f)
        {
            const float th = t * 0.42f;
            // a travelling ripple rides along the coil when the spring is ringing
            const float ripple = energy * 6.0f * std::sin (t * 0.05f - coilPhase * (1.0f + 0.2f * (float) c));
            const float x = x0 + t + 5.0f * std::cos (th);
            const float y = cy + amp * std::sin (th) + ripple;
            if (first) { coil.startNewSubPath (x, y); first = false; }
            else coil.lineTo (x, y);
        }
        glowStroke (g, coil, 1.1f, yel, 0.45f + 0.5f * energy);
        // the mounts at each end
        g.setColour (yel.withAlpha (0.7f));
        g.fillRect (x0 - 12.0f, cy - 10.0f, 4.0f, 20.0f);
        g.fillRect (x1 + 10.0f, cy - 10.0f, 4.0f, 20.0f);
    }

    // ---- the send, and the loop meter
    g.setColour (yel.withAlpha (0.35f));
    g.fillRect (16.0f, 340.0f, w - 32.0f, 1.0f);
    juce::String state;
    float stateA = 0.8f;
    if (hold) { state = "LOOP HELD"; stateA = skin.blink() ? 1.0f : 0.35f; }
    else if (dub && send > 0.5f) { state = "> THROW"; stateA = 1.0f; }
    else if (send > 0.5f) state = "SEND OPEN";
    else { state = "SEND SHUT"; stateA = 0.5f; }
    glowText (g, state, { 16.0f, 348.0f, 160.0f, 22.0f }, juce::Justification::centredLeft, readout, yel, stateA);

    glowText (g, "LOOP", { 190.0f, 348.0f, 50.0f, 22.0f }, juce::Justification::centredLeft, small, yel, 0.6f);
    const float lv = juce::jlimit (0.0f, 1.0f, std::sqrt (loop * 2.0f));
    for (float x = 244.0f; x < w - 18.0f; x += 7.0f)
    {
        const bool lit = (x - 244.0f) / (w - 262.0f) < lv;
        g.setColour (yel.withAlpha (lit ? 0.9f : 0.15f));
        g.fillRect (x, 353.0f, 5.0f, 12.0f);
    }

    // ---- a slow roll bar
    const float roll = (float) std::fmod (skin.t * 0.12, 1.0) * (h + 120.0f) - 60.0f;
    juce::ColourGradient cg (yel.withAlpha (0.0f), 0.0f, roll - 50.0f, yel.withAlpha (0.0f), 0.0f, roll + 50.0f, false);
    cg.addColour (0.5, yel.withAlpha (0.045f));
    g.setGradientFill (cg);
    g.fillRect (0.0f, roll - 50.0f, w, 100.0f);

    glowBox (g, getLocalBounds().toFloat().reduced (1.0f), 1.4f, 0.85f);
}

//==============================================================================
juce::Font Title::font() { return mono (50.0f).withExtraKerningFactor (0.34f); }

void Title::tick (bool throwing, float loopLevel)
{
    const float target = throwing ? 1.0f : juce::jlimit (0.0f, 0.7f, std::sqrt (loopLevel * 2.0f));
    const float before = glow;
    glow += (target - glow) * 0.2f;
    const bool b = skin.blink();
    if (std::abs (glow - before) > 0.004f || b != lastBlink)
        repaint();
    lastBlink = b;
}

void Title::paint (juce::Graphics& g)
{
    const auto f = font();
    const juce::String word ("TUBBY");
    const auto area = juce::Rectangle<float> (6.0f, 0.0f, (float) getWidth() - 6.0f, (float) getHeight());

    // the name's own echoes trail off to the right, each one smaller and fainter
    float x = 6.0f + textWidth (f, word) + 18.0f;
    float size = 50.0f;
    for (int k = 1; k <= 3; ++k)
    {
        size *= 0.64f;
        const auto fe = mono (size).withExtraKerningFactor (0.34f);
        const float a = (0.16f + 0.5f * glow) * std::pow (0.6f, (float) (k - 1));
        glowText (g, word, { x, 0.0f, 400.0f, (float) getHeight() }, juce::Justification::centredLeft, fe, yel, a);
        x += textWidth (fe, word) + 14.0f;
    }
    glowText (g, word, area, juce::Justification::centredLeft, f, yel);
}

//==============================================================================
void TubbyEditor::Panel::paint (juce::Graphics& g)
{
    g.fillAll (tube);

    glowText (g, "MANNERS! DSP  //  DUB DELAY", { 60.0f, 92.0f, 640.0f, 24.0f },
              juce::Justification::centredLeft, mono (15.0f).withExtraKerningFactor (0.3f), yel, 0.8f);
    glowText (g, "DRAG KNOBS UP OR DOWN. DOUBLE-CLICK TO RESET", { 600.0f, 92.0f, 544.0f, 24.0f },
              juce::Justification::centredRight, mono (13.0f).withExtraKerningFactor (0.08f), yel, 0.6f);

    // boxes with their names set into the top edge
    auto box = [&g] (juce::Rectangle<float> r, const char* name)
    {
        const auto f = mono (14.0f).withExtraKerningFactor (0.3f);
        const juce::String label = juce::String ("[ ") + name + " ]";
        const float tw = textWidth (f, label) + 12.0f;
        const float tx = r.getX() + 18.0f;

        juce::Path p;
        p.startNewSubPath (tx, r.getY());
        p.lineTo (r.getX(), r.getY());
        p.lineTo (r.getX(), r.getBottom());
        p.lineTo (r.getRight(), r.getBottom());
        p.lineTo (r.getRight(), r.getY());
        p.lineTo (tx + tw, r.getY());
        glowStroke (g, p, 1.4f, yel, 0.85f);
        glowText (g, label, { tx, r.getY() - 11.0f, tw, 22.0f }, juce::Justification::centred, f, yel);
    };
    box ({ 40.0f, 140.0f, 346.0f, 466.0f }, "ECHO");
    box ({ 814.0f, 140.0f, 346.0f, 466.0f }, "SPRING");
    box ({ 40.0f, 626.0f, 560.0f, 166.0f }, "DESK");
    box ({ 614.0f, 626.0f, 546.0f, 166.0f }, "OUTPUT");

    g.setColour (yel.withAlpha (0.35f));
    g.fillRect (56.0f, 120.0f, 1088.0f, 1.0f);
}

//==============================================================================
TubbyEditor::TubbyEditor (TubbyProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), monitor (p, skin), presets (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (panel);
    panel.setBounds (0, 0, kBaseW, kBaseH);

    auto addKnob = [this] (const char* id, const char* label, juce::Rectangle<int> bounds, bool bipolar = false)
    {
        auto k = std::make_unique<tby::ui::Knob> (proc.apvts, id, label, bipolar);
        panel.addAndMakeVisible (*k);
        k->setBounds (bounds);
        knobs.push_back (std::move (k));
        return knobs.back().get();
    };

    const int colW = 160, rowH = 146;
    auto cell = [&] (int x0, int col, int row) { return juce::Rectangle<int> (x0 + 13 + col * colW, 170 + row * rowH, colW, rowH); };

    // ECHO, left
    timeKnob = addKnob (ParamIDs::time, "TIME", cell (40, 0, 0));
    addKnob (ParamIDs::feedback, "FEEDBACK", cell (40, 1, 0));
    addKnob (ParamIDs::tone,     "TONE",     cell (40, 0, 1));
    addKnob (ParamIDs::thin,     "THIN",     cell (40, 1, 1));
    addKnob (ParamIDs::wobble,   "WOBBLE",   cell (40, 0, 2));
    addKnob (ParamIDs::spread,   "SPREAD",   cell (40, 1, 2));

    // SPRING, right
    addKnob (ParamIDs::spring,  "SPRING",  cell (814, 0, 0));
    addKnob (ParamIDs::decay,   "DECAY",   cell (814, 1, 0));
    addKnob (ParamIDs::tension, "TENSION", cell (814, 0, 1));
    addKnob (ParamIDs::drip,    "DRIP",    cell (814, 1, 1));
    addKnob (ParamIDs::feed,    "FEED",    cell (814, 0, 2));
    addKnob (ParamIDs::shake,   "SHAKE",   cell (814, 1, 2));

    // DESK and OUTPUT, bottom
    const int bw = 180;
    addKnob (ParamIDs::drive,  "DRIVE",  { 50 + 0 * bw, 648, bw, 140 });
    addKnob (ParamIDs::noise,  "NOISE",  { 50 + 1 * bw, 648, bw, 140 });
    addKnob (ParamIDs::filter, "FILTER", { 50 + 2 * bw, 648, bw, 140 }, true);
    addKnob (ParamIDs::in,     "IN",     { 618 + 0 * bw, 648, bw, 140 });
    addKnob (ParamIDs::out,    "OUT",    { 618 + 1 * bw, 648, bw, 140 });
    addKnob (ParamIDs::mix,    "MIX",    { 618 + 2 * bw, 648, bw, 140 });

    panel.addAndMakeVisible (monitor);
    monitor.setBounds (404, 130, 392, 380);

    panel.addAndMakeVisible (title);
    title.setBounds (50, 22, 640, 76);
    panel.addAndMakeVisible (presets);
    presets.setBounds (780, 36, 380, 48);

    // switches under the monitor: two rows of three
    for (auto* b : { &syncButton, &holdButton, &altecButton, &openButton })
    {
        b->setClickingTogglesState (true);
        b->getProperties().set ("lamp", true);
    }
    syncButton.setBounds (404, 520, 120, 42);
    holdButton.setBounds (540, 520, 120, 42);
    altecButton.setBounds (676, 520, 120, 42);
    openButton.setBounds (404, 568, 120, 42);
    dubButton.setBounds (540, 568, 120, 42);
    kickButton.setBounds (676, 568, 120, 42);
    altecButton.setTooltip ("The big filter clicks between ten positions, like the old stepped one");
    openButton.setTooltip ("Lit: everything goes to tape. Unlit: only what you throw with DUB");
    dubButton.setTooltip ("Hold to throw the input onto the tape");
    kickButton.setTooltip ("Crash the springs");
    for (auto* b : { &syncButton, &holdButton, &altecButton, &openButton, &dubButton, &kickButton })
        panel.addAndMakeVisible (*b);

    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    buttonAttachments.push_back (std::make_unique<BA> (proc.apvts, ParamIDs::sync, syncButton));
    buttonAttachments.push_back (std::make_unique<BA> (proc.apvts, ParamIDs::hold, holdButton));
    buttonAttachments.push_back (std::make_unique<BA> (proc.apvts, ParamIDs::altec, altecButton));
    buttonAttachments.push_back (std::make_unique<BA> (proc.apvts, ParamIDs::open, openButton));
    momentary (dubButton, ParamIDs::dub);
    momentary (kickButton, ParamIDs::kick);

    panel.addAndMakeVisible (glass);
    glass.setBounds (0, 0, kBaseW, kBaseH);

    // the boot sequence plays the first time the window opens in a session
    if (! proc.booted)
    {
        proc.booted = true;
        glass.boot (skin.t);
    }
    lastSync = syncButton.getToggleState();

    setResizable (true, true);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) kBaseW / (double) kBaseH);
    setResizeLimits (720, 480, 1500, 1000);
    setSize (1050, 700);

    startTimerHz (30);
}

// DUB and KICK are momentary: down writes 1, release writes 0.
void TubbyEditor::momentary (juce::TextButton& b, const char* paramId)
{
    b.onStateChange = [this, &b, paramId]
    {
        auto* prm = proc.apvts.getParameter (paramId);
        const float v = b.isDown() ? 1.0f : 0.0f;
        if (prm != nullptr && ! juce::exactlyEqual (prm->getValue(), v))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (v);
            prm->endChangeGesture();
        }
    };
}

TubbyEditor::~TubbyEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void TubbyEditor::paint (juce::Graphics& g)
{
    g.fillAll (tube);
}

void TubbyEditor::resized()
{
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) kBaseW));
}

void TubbyEditor::advance (double dt)
{
    skin.t += dt;
    monitor.tick();
    presets.tick();
    title.tick (proc.apvts.getRawParameterValue (ParamIDs::dub)->load() > 0.5f, proc.engine.uiLoop.load());

    const bool booting = glass.booting();
    if (booting || wasBooting)
        glass.repaint();
    wasBooting = booting;

    const bool b = skin.blink();
    if (b != lastBlink)
        for (auto* btn : { &syncButton, &holdButton, &altecButton, &openButton })
            if (btn->getToggleState()) btn->repaint();
    lastBlink = b;

    const bool s = syncButton.getToggleState();
    if (s != lastSync && timeKnob != nullptr)
        timeKnob->repaint();   // TIME reads as a division or as milliseconds
    lastSync = s;
}
