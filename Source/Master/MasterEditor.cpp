#include "MasterEditor.h"
#include "../UI/Brand.h"

using namespace glitchkid::masterplugin;
namespace gui = glitchkid::ui;
namespace gm = glitchkid::master;

namespace
{
constexpr int designW = 1120, designH = 680;
const juce::Colour warning (0xffffb54c); // meaning, not brand (the GlitchKid warning colour)
constexpr float unavailable = -99.0f;    // at or below: not measured yet (shown as "-")

juce::String lufs (float v) { return v <= unavailable ? juce::String ("-") : juce::String (v, 1) + " LUFS"; }
juce::String dbtp (float v) { return v <= unavailable ? juce::String ("-") : juce::String (v, 1) + " dBTP"; }
juce::String dbfs (float v) { return v <= unavailable ? juce::String ("-") : juce::String (v, 1) + " dBFS"; }
}

// ================================================================== Knob
MasterEditor::Knob::Knob (MasterProcessor& p, const char* id, const juce::String& n, const juce::String& tip)
    : name (n), attachment (p.apvts, id, slider)
{
    addAndMakeVisible (slider);
    addAndMakeVisible (label);
    label.setText (n, juce::dontSendNotification);
    label.setFont (gui::fontOf (11.5f, true));
    label.setColour (juce::Label::textColourId, gui::textDim);
    label.setJustificationType (juce::Justification::centred);
    label.setInterceptsMouseClicks (false, false);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 104, 18);
    slider.setColour (juce::Slider::textBoxTextColourId, gui::textBright);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setTooltip (tip);
    slider.setTitle (n);
    if (auto* prm = p.apvts.getParameter (id))
        slider.setDoubleClickReturnValue (true, (double) p.apvts.getParameterRange (id).convertFrom0to1 (prm->getDefaultValue()));
}

void MasterEditor::Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (18));
    slider.setBounds (r);
}

// ================================================================== the export panel
void MasterEditor::ExportHelp::paint (juce::Graphics& g)
{
    gui::drawPanel (g, getLocalBounds().toFloat());
    g.setColour (gui::obsidian.withAlpha (0.94f));
    g.fillRect (getLocalBounds().reduced (1));
    auto r = getLocalBounds().reduced (24, 18);
    g.setColour (gui::textBright);
    g.setFont (gui::fontOf (16.0f, true));
    g.drawText ("EXPORTING THE MASTER", r.removeFromTop (26), juce::Justification::centredLeft, false);
    g.setFont (gui::fontOf (13.0f));
    g.setColour (gui::textDim);
    g.drawFittedText (
        "GLITCHKID MASTER processes the audio your DAW plays through it, so the master is exported by your DAW, with the plug-in on the master bus:\n\n"
        "1.  Put GLITCHKID MASTER last on your master bus (nothing after it that changes the level).\n"
        "2.  Set it up while the whole song plays. Use ORIGINAL / PROCESSED to compare at matched loudness.\n"
        "3.  Make sure BYPASS and ORIGINAL are off: ORIGINAL is for listening only, and it is never saved with the session.\n"
        "4.  Export the whole song from your DAW (FL Studio: File > Export > WAV file). Choose 24-bit or 32-bit float WAV; dither only for a 16-bit file.\n"
        "5.  Press RESET METERS, play the song from the start to the end, and read INTEGRATED: that is the loudness of the whole song.\n\n"
        "The plug-in's latency is reported to your DAW, which compensates for it in playback and in the export.",
        r.withTrimmedBottom (40), juce::Justification::topLeft, 14, 1.0f);
}

// ================================================================== Content
void MasterEditor::Content::paint (juce::Graphics& g) { editor.paintContent (g); }

// ================================================================== MasterEditor
MasterEditor::MasterEditor (MasterProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    const struct { const char* id; const char* name; const char* tip; } k[] = {
        { ids::input, "INPUT", "The level going into the chain. The INPUT meters and loudness read after it." },
        { ids::intensity, "INTENSITY", "How much of the style is used: its EQ, glue compression, saturation and width. 0 % = only the limiter and your tone and width." },
        { ids::loudness, "LOUDNESS", "How hard the mix is driven into the true-peak limiter. More = louder and more limited. Watch GAIN REDUCTION and judge by ear." },
        { ids::tone, "TONE", "Darker to brighter: a gentle tilt around 1 kHz (low and high shelves)." },
        { ids::width, "WIDTH", "Stereo width on top of the style. The mid (and so the mono sum) is never changed, and nothing below about 120 Hz is widened." },
        { ids::ceiling, "CEILING", "The highest true peak the limiter lets through. -1 dBTP leaves room for streaming encoders." },
        { ids::output, "OUTPUT", "Level after the limiter. It only lowers, so the ceiling always holds." } };
    for (const auto& kk : k)
    {
        knobs.push_back (std::make_unique<Knob> (proc, kk.id, kk.name, kk.tip));
        content.addAndMakeVisible (*knobs.back());
    }

    for (int s = 0; s < gm::numStyles; ++s)
    {
        auto& t = styleTiles[(size_t) s];
        t.setButtonText (gm::styleName ((gm::Style) s));
        t.setTooltip (juce::String (gm::styleName ((gm::Style) s)) + ": " + gm::styleDescription ((gm::Style) s));
        t.setClickingTogglesState (false);
        t.onClick = [this, s]
        {
            if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (ids::style)))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) s));
                prm->endChangeGesture();
            }
            timerCallback();
        };
        content.addAndMakeVisible (t);
    }

    for (auto* b : { &bypass, &reset, &setA, &setB, &recallA, &recallB, &original, &processed, &resetMeters, &howExport })
        content.addAndMakeVisible (b);
    bypass.setClickingTogglesState (true);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::bypass, bypass);
    bypass.setTooltip ("Bypass: the untouched mix, time-aligned (delayed by the plug-in's latency) and crossfaded over 20 ms. The host's bypass does the same.");
    reset.setTooltip ("Every control back to its default (A and B are kept).");
    setA.setTooltip ("Store the current settings in A.");
    setB.setTooltip ("Store the current settings in B.");
    recallA.setTooltip ("Switch to the settings stored in A.");
    recallB.setTooltip ("Switch to the settings stored in B.");
    original.setTooltip ("Hear the untouched mix, time-aligned and turned to the master's loudness (so louder never wins), never above the ceiling. For listening only: not saved.");
    processed.setTooltip ("Hear the master.");
    resetMeters.setTooltip ("Start the loudness, peak and clipping measurements again (play the whole song after this to read its INTEGRATED loudness).");
    howExport.setTooltip ("How to export the master from your DAW.");
    reset.onClick = [this] { proc.resetToDefaults(); };
    setA.onClick = [this] { proc.storeSlot (0); timerCallback(); };
    setB.onClick = [this] { proc.storeSlot (1); timerCallback(); };
    recallA.onClick = [this] { proc.recallSlot (0); timerCallback(); };
    recallB.onClick = [this] { proc.recallSlot (1); timerCallback(); };
    original.onClick = [this] { proc.setCompareOriginal (true); timerCallback(); };
    processed.onClick = [this] { proc.setCompareOriginal (false); timerCallback(); };
    resetMeters.onClick = [this] { proc.requestMeterReset(); };
    howExport.onClick = [this] { exportHelp.setVisible (! exportHelp.isVisible()); };
    exportHelp.close.onClick = [this] { exportHelp.setVisible (false); };
    content.addChildComponent (exportHelp);

    setResizable (true, true);
    setResizeLimits (designW * 7 / 10, designH * 7 / 10, designW * 3 / 2, designH * 3 / 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) designW / designH);
    setSize (designW, designH);
    timerCallback();
    startTimerHz (30);
}

MasterEditor::~MasterEditor()
{
    proc.setCompareOriginal (false); // ORIGINAL is a listening switch: closing the window never leaves the master on the original
    setLookAndFeel (nullptr);
}

void MasterEditor::pressForTest (const juce::String& text)
{
    for (auto* c : content.getChildren())
        if (auto* b = dynamic_cast<juce::Button*> (c); b != nullptr && b->getButtonText() == text)
        {
            if (b->getClickingTogglesState())
                b->setToggleState (! b->getToggleState(), juce::sendNotificationSync);
            else if (b->onClick)
                b->onClick();
            return;
        }
}

void MasterEditor::resized()
{
    content.setBounds (0, 0, designW, designH);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / designW, (float) getHeight() / designH));
    layout();
}

void MasterEditor::layout()
{
    // top bar: A/B, bypass, reset
    setA.setBounds (612, 18, 60, 30);
    setB.setBounds (676, 18, 60, 30);
    recallA.setBounds (744, 18, 40, 30);
    recallB.setBounds (788, 18, 40, 30);
    bypass.setBounds (848, 18, 112, 30);
    reset.setBounds (968, 18, 128, 30);

    inPanel = { 24, 84, 236, 440 };
    outPanel = { designW - 24 - 236, 84, 236, 440 };
    const int cx = inPanel.getRight() + 16, cw = outPanel.getX() - 16 - cx;
    stylesBox = { cx, 84, cw, 92 };
    const int tw = (cw - 3 * 8) / gm::numStyles;
    for (int s = 0; s < gm::numStyles; ++s)
        styleTiles[(size_t) s].setBounds (cx + s * (tw + 8), stylesBox.getY() + 26, tw, 34);
    historyBox = { cx, 188, cw, 156 };
    dialsBox = { cx, 356, cw, 168 };
    const int kw = cw / (int) knobs.size();
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i]->setBounds (dialsBox.getX() + (int) i * kw, dialsBox.getY() + 34, kw, 120);

    compareBox = { 24, 540, designW - 48, 116 };
    original.setBounds (compareBox.getX() + 20, compareBox.getY() + 40, 140, 34);
    processed.setBounds (compareBox.getX() + 168, compareBox.getY() + 40, 140, 34);
    resetMeters.setBounds (compareBox.getRight() - 20 - 140 - 8 - 140, compareBox.getY() + 40, 140, 34);
    howExport.setBounds (compareBox.getRight() - 20 - 140, compareBox.getY() + 40, 140, 34);
    exportHelp.setBounds (180, 96, designW - 360, 440);
}

void MasterEditor::timerCallback()
{
    const auto& e = proc.getEngine();
    const auto pk = proc.consumePeaks();
    const float fall = juce::Decibels::decibelsToGain (-20.0f / 30.0f); // 20 dB per second at 30 Hz
    const float now[4] = { pk.inL, pk.inR, pk.outL, pk.outR };
    for (int i = 0; i < 4; ++i)
        bars[(size_t) i] = juce::jmax (now[i], bars[(size_t) i] * fall);
    grLim[(size_t) grPos] = pk.limiterGrDb;
    grGlue[(size_t) grPos] = pk.glueGrDb;
    grPos = (grPos + 1) % histLen;

    auto rows = [] (const gm::LoudnessMeter& m, std::array<juce::String, 7>& out)
    {
        out[0] = lufs (m.momentaryLufs());
        out[1] = lufs (m.shortTermLufs());
        out[2] = lufs (m.integratedLufs());
        out[3] = dbtp (m.truePeakDb());
        out[4] = dbfs (m.samplePeakDb());
        out[5] = m.shortTermLufs() <= unavailable ? juce::String ("-") : juce::String (m.correlation(), 2);
        out[6] = juce::String (m.clippedSamples());
    };
    rows (e.inputMeter(), inRows);
    rows (e.outputMeter(), outRows);
    outTp = e.outputMeter().truePeakDb();
    ceiling = proc.readControls().ceilingDbTp;
    matchDb = e.compareGainDb();

    const auto blocks = proc.blocksProcessed();
    stillTicks = blocks != lastBlocks ? 0 : juce::jmin (stillTicks + 1, 1000);
    lastBlocks = blocks;
    if (proc.nonFiniteBlocks() != lastNonFinite)
    {
        lastNonFinite = proc.nonFiniteBlocks();
        nonFiniteTicks = 90; // say so for 3 s
    }
    else if (nonFiniteTicks > 0)
        --nonFiniteTicks;

    const bool silent = e.inputMeter().momentaryLufs() <= -70.0f;
    if (stillTicks > 15)
        status = "NO AUDIO: PRESS PLAY IN YOUR DAW", statusColour = gui::textDim;
    else if (proc.isBypassed())
        status = "BYPASSED: THE UNTOUCHED MIX", statusColour = warning;
    else if (silent)
        status = "SILENCE ON THE INPUT", statusColour = gui::textDim;
    else if (proc.isCompareOriginal())
        status = "ORIGINAL, MATCHED TO THE MASTER'S LOUDNESS (" + juce::String (matchDb >= 0.0f ? "+" : "") + juce::String (matchDb, 1) + " dB)", statusColour = warning;
    else
        status = "MASTERING THE MIX", statusColour = gui::pink;
    if (nonFiniteTicks > 0)
        status += "  -  BROKEN SAMPLES (NaN / Inf) ARRIVED AND WERE SILENCED";

    const auto c = proc.readControls();
    for (int s = 0; s < gm::numStyles; ++s)
        styleTiles[(size_t) s].setToggleState ((int) c.style == s, juce::dontSendNotification);
    styleText = gm::styleDescription (c.style);
    original.setToggleState (proc.isCompareOriginal(), juce::dontSendNotification);
    processed.setToggleState (! proc.isCompareOriginal(), juce::dontSendNotification);
    recallA.setEnabled (proc.hasSlot (0));
    recallB.setEnabled (proc.hasSlot (1));
    recallA.setToggleState (proc.activeSlot() == 0, juce::dontSendNotification);
    recallB.setToggleState (proc.activeSlot() == 1, juce::dontSendNotification);
    content.repaint();
}

void MasterEditor::paintContent (juce::Graphics& g)
{
    g.fillAll (gui::obsidian);

    // the top bar: the mark, the product, the real status
    g.setColour (gui::panelBottom);
    g.fillRect (0, 0, designW, 66);
    g.setColour (gui::border);
    g.fillRect (0, 66, designW, 1);
    gui::brand::drawEmblem (g, { 24.0f, 14.0f, 44.0f, 38.0f }, juce::Justification::centredLeft);
    g.setColour (gui::textBright);
    g.setFont (gui::fontOf (21.0f, true));
    g.drawText ("GLITCHKID MASTER", 78, 12, 300, 26, juce::Justification::centredLeft, false);
    g.setColour (gui::textDim);
    g.setFont (gui::fontOf (12.0f));
    g.drawText ("Mastering for a finished stereo mix, on your master bus", 78, 38, 340, 16, juce::Justification::centredLeft, false);
    {
        const juce::Rectangle<float> pill (404.0f, 18.0f, 196.0f, 30.0f);
        g.setColour (statusColour.withAlpha (0.14f));
        g.fillRoundedRectangle (pill, 6.0f);
        g.setColour (statusColour);
        g.drawRoundedRectangle (pill.reduced (0.5f), 6.0f, 1.0f);
        g.setFont (gui::fontOf (10.5f, true));
        g.drawFittedText (status, pill.toNearestInt().reduced (8, 2), juce::Justification::centred, 2, 0.7f);
    }

    paintMeters (g, inPanel, false);
    paintMeters (g, outPanel, true);

    // style
    gui::drawPanel (g, stylesBox.toFloat());
    g.setColour (gui::textDim);
    g.setFont (gui::fontOf (11.5f, true));
    g.drawText ("STYLE", stylesBox.getX() + 14, stylesBox.getY() + 6, 200, 16, juce::Justification::centredLeft, false);
    g.setFont (gui::fontOf (11.5f));
    g.drawFittedText (styleText, stylesBox.getX() + 14, stylesBox.getBottom() - 28, stylesBox.getWidth() - 28, 24, juce::Justification::centredLeft, 2, 0.85f);

    paintHistory (g, historyBox);

    gui::drawPanel (g, dialsBox.toFloat());
    g.setColour (gui::textDim);
    g.setFont (gui::fontOf (11.5f, true));
    g.drawText ("CONTROLS", dialsBox.getX() + 14, dialsBox.getY() + 8, 200, 16, juce::Justification::centredLeft, false);

    // compare and export
    gui::drawPanel (g, compareBox.toFloat());
    g.setColour (gui::textDim);
    g.setFont (gui::fontOf (11.5f, true));
    g.drawText ("COMPARE", compareBox.getX() + 20, compareBox.getY() + 12, 200, 16, juce::Justification::centredLeft, false);
    g.setFont (gui::fontOf (11.5f));
    g.drawFittedText ("ORIGINAL is the untouched mix turned to the master's loudness (" + juce::String (matchDb >= 0.0f ? "+" : "") + juce::String (matchDb, 1)
                          + " dB now), so you compare the sound, not the level. Latency: " + juce::String (proc.getLatencySamples()) + " samples, compensated by your DAW.",
                      compareBox.getX() + 330, compareBox.getY() + 36, compareBox.getWidth() - 330 - 330, 44, juce::Justification::centredLeft, 3, 0.85f);
}

void MasterEditor::paintMeters (juce::Graphics& g, juce::Rectangle<int> r, bool output)
{
    gui::drawPanel (g, r.toFloat());
    g.setColour (gui::textBright);
    g.setFont (gui::fontOf (13.0f, true));
    g.drawText (output ? "OUTPUT" : "INPUT", r.getX() + 14, r.getY() + 8, 200, 18, juce::Justification::centredLeft, false);
    g.setColour (gui::textDim);
    g.setFont (gui::fontOf (10.5f));
    g.drawText (output ? "what leaves GLITCHKID MASTER" : "what enters the chain (after INPUT)", r.getX() + 14, r.getY() + 26, r.getWidth() - 28, 14, juce::Justification::centredLeft, false);

    // L / R peak bars, -48..0 dBFS, with the ceiling marked on the output
    const auto barArea = juce::Rectangle<int> (r.getX() + 14, r.getY() + 50, 40, r.getHeight() - 70);
    auto yOf = [&barArea] (float db) { return (float) barArea.getBottom() - (float) barArea.getHeight() * juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f); };
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto bar = juce::Rectangle<float> ((float) barArea.getX() + (float) ch * 22.0f, (float) barArea.getY(), 16.0f, (float) barArea.getHeight());
        g.setColour (gui::border);
        g.fillRect (bar);
        const float db = juce::Decibels::gainToDecibels (bars[(size_t) (ch + (output ? 2 : 0))], -100.0f);
        g.setColour (db > -0.1f ? warning : output ? gui::pink : gui::neon);
        g.fillRect (bar.withTop (yOf (db)));
        g.setColour (gui::textDim);
        g.setFont (gui::fontOf (10.0f, true));
        g.drawText (ch == 0 ? "L" : "R", bar.withY (bar.getBottom() + 2).withHeight (12.0f), juce::Justification::centred, false);
    }
    if (output)
    {
        const float y = yOf (ceiling);
        g.setColour (gui::textBright.withAlpha (0.8f));
        g.fillRect ((float) barArea.getX() - 3.0f, y, 44.0f, 1.0f);
    }

    static const char* names[7] = { "MOMENTARY", "SHORT-TERM", "INTEGRATED", "TRUE PEAK", "SAMPLE PEAK", "CORRELATION", "CLIPPED SAMPLES" };
    const auto& rows = output ? outRows : inRows;
    int y = r.getY() + 54;
    for (int i = 0; i < 7; ++i)
    {
        g.setColour (gui::textDim);
        g.setFont (gui::fontOf (10.5f, true));
        g.drawText (names[i], r.getX() + 72, y, r.getWidth() - 86, 14, juce::Justification::centredLeft, false);
        juce::Colour c = gui::textBright;
        if (i == 3 && output && outTp > unavailable && outTp > ceiling + 0.05f) c = warning;
        if (i == 6 && rows[6] != "0") c = warning;
        g.setColour (c);
        g.setFont (gui::fontOf (i == 2 ? 20.0f : 15.0f, i == 2));
        g.drawText (rows[(size_t) i], r.getX() + 72, y + 14, r.getWidth() - 86, i == 2 ? 26 : 20, juce::Justification::centredLeft, false);
        y += i == 2 ? 58 : 50;
    }
    if (rows[2] == "-")
    {
        g.setColour (gui::textDim);
        g.setFont (gui::fontOf (10.5f));
        g.drawText ("integrated: needs 400 ms above -70 LUFS", r.getX() + 14, r.getBottom() - 18, r.getWidth() - 28, 14, juce::Justification::centredLeft, false);
    }
}

void MasterEditor::paintHistory (juce::Graphics& g, juce::Rectangle<int> r)
{
    gui::drawPanel (g, r.toFloat());
    g.setColour (gui::textDim);
    g.setFont (gui::fontOf (11.5f, true));
    g.drawText ("GAIN REDUCTION  (the last 8 s)", r.getX() + 14, r.getY() + 6, 300, 16, juce::Justification::centredLeft, false);
    const auto plot = r.toFloat().reduced (14.0f, 0.0f).withTrimmedTop (28.0f).withTrimmedBottom (12.0f);
    const float maxDb = 12.0f;
    auto yOf = [&plot, maxDb] (float db) { return plot.getY() + plot.getHeight() * juce::jlimit (0.0f, 1.0f, db / maxDb); }; // reduction grows downward
    g.setFont (gui::fontOf (10.0f));
    for (float db : { 0.0f, 3.0f, 6.0f, 12.0f })
    {
        g.setColour (gui::border);
        g.fillRect (plot.getX(), yOf (db), plot.getWidth(), 1.0f);
        g.setColour (gui::textDim.withAlpha (0.7f));
        g.drawText ("-" + juce::String ((int) db), (int) plot.getRight() - 24, (int) yOf (db) + (db < maxDb ? 1 : -12), 24, 12, juce::Justification::centredRight, false);
    }
    auto trace = [&] (const std::array<float, histLen>& h, juce::Colour c, float thickness)
    {
        juce::Path p;
        for (int i = 0; i < histLen; ++i)
        {
            const float v = h[(size_t) ((grPos + i) % histLen)]; // oldest first
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) (histLen - 1);
            if (i == 0) p.startNewSubPath (x, yOf (v)); else p.lineTo (x, yOf (v));
        }
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (thickness));
    };
    trace (grGlue, gui::textDim, 1.2f);
    trace (grLim, gui::pink, 1.8f);
    const float limNow = grLim[(size_t) ((grPos - 1 + histLen) % histLen)], glueNow = grGlue[(size_t) ((grPos - 1 + histLen) % histLen)];
    g.setFont (gui::fontOf (11.0f, true));
    g.setColour (gui::pink);
    g.drawText ("LIMITER  -" + juce::String (limNow, 1) + " dB", r.getRight() - 300, r.getY() + 6, 140, 16, juce::Justification::centredRight, false);
    g.setColour (gui::textDim);
    g.drawText ("GLUE  -" + juce::String (glueNow, 1) + " dB", r.getRight() - 150, r.getY() + 6, 136, 16, juce::Justification::centredRight, false);
}
