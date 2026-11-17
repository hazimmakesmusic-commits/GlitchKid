#include "BeatLockEditor.h"
#include "../UI/Brand.h"

using namespace glitchkid::beatlock;
namespace gui = glitchkid::ui;

namespace
{
constexpr int designW = 1120, designH = 680;
const juce::Colour warning (0xffffb54c); // meaning, not brand (the GlitchKid warning colour)
juce::Colour vocalColour() { return gui::textDim; }

void styleLabel (juce::Label& l, float size, bool bold, juce::Colour c, juce::Justification j = juce::Justification::centredLeft)
{
    l.setFont (gui::fontOf (size, bold));
    l.setColour (juce::Label::textColourId, c);
    l.setJustificationType (j);
    l.setInterceptsMouseClicks (false, false);
}

juce::String hzText (float hz) { return hz >= 1000.0f ? juce::String (hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz"; }
}

namespace glitchkid::beatlock::ui
{
// ================================================================== Knob
Knob::Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, const juce::String& name, const juce::String& tooltip)
    : attachment (apvts, paramId, slider)
{
    addAndMakeVisible (slider);
    addAndMakeVisible (label);
    label.setText (name, juce::dontSendNotification);
    styleLabel (label, 11.5f, true, gui::textDim, juce::Justification::centred);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 86, 18);
    slider.setColour (juce::Slider::textBoxTextColourId, gui::textBright);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setTooltip (tooltip);
    slider.setDoubleClickReturnValue (true, (double) apvts.getParameterRange (paramId).convertFrom0to1 (apvts.getParameter (paramId)->getDefaultValue()));
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (18));
    slider.setBounds (r);
}

// ================================================================== Graph
Graph::Graph (BeatLockProcessor& p) : proc (p)
{
    for (auto* s : { &specIn, &specOut, &specVocal })
        s->setAveragingSeconds (0.3f);
    setTooltip ("The instrumental's real spectrum in and out, the vocal sidechain's spectrum, and the cut Beat Lock is making right now. "
                "Drag a band marker to move it; scroll over it to change its width (Q); double-click to switch it on or off.");
}

juce::Rectangle<float> Graph::plotArea() const { return getLocalBounds().toFloat().reduced (44.0f, 26.0f).withTrimmedBottom (0.0f); }
float Graph::xOf (float hz) const
{
    const auto p = plotArea();
    return p.getX() + p.getWidth() * std::log (juce::jlimit (20.0f, 20000.0f, hz) / 20.0f) / std::log (1000.0f);
}
float Graph::hzOf (float x) const
{
    const auto p = plotArea();
    return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, (x - p.getX()) / p.getWidth()));
}
float Graph::cutY (float db) const
{
    const auto p = plotArea();
    return p.getY() + 14.0f + juce::jlimit (0.0f, 1.0f, -db / 12.0f) * (p.getHeight() * 0.55f);
}
float Graph::specY (float db) const
{
    const auto p = plotArea();
    return p.getY() + p.getHeight() * juce::jlimit (0.0f, 1.0f, -db / 96.0f);
}

void Graph::tick (float dt)
{
    const double sr = proc.getSampleRateNow();
    bool changed = specIn.update (proc.getInstrumentalInTap(), sr, dt);
    changed = specOut.update (proc.getInstrumentalOutTap(), sr, dt) || changed;
    changed = specVocal.update (proc.getVocalTap(), sr, dt) || changed;
    const auto& e = proc.getEngine();
    for (int b = 0; b < numBands; ++b)
    {
        const float c = e.cutDb (b);
        changed = changed || std::abs (c - cut[(size_t) b]) > 0.01f;
        cut[(size_t) b] = c;
    }
    const float g = e.gate();
    const auto st = proc.getSidechainStatus();
    changed = changed || std::abs (g - gate) > 0.01f || st != status;
    gate = g;
    status = st;
    if (changed)
        repaint();
}

int Graph::bandAt (juce::Point<float> pt) const
{
    int best = -1;
    float bestD = 16.0f;
    for (int b = 0; b < numBands; ++b)
    {
        const auto& id = bandIds()[(size_t) b];
        const float x = xOf (param (id.freq)->convertFrom0to1 (param (id.freq)->getValue()));
        const float d = pt.getDistanceFrom ({ x, cutY (cut[(size_t) b]) });
        if (d < bestD) { bestD = d; best = b; }
    }
    return best;
}

void Graph::mouseDown (const juce::MouseEvent& e)
{
    dragBand = bandAt (e.position);
    if (dragBand >= 0)
        param (bandIds()[(size_t) dragBand].freq)->beginChangeGesture();
}

void Graph::mouseDrag (const juce::MouseEvent& e)
{
    if (dragBand < 0)
        return;
    auto* p = param (bandIds()[(size_t) dragBand].freq);
    p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (100.0f, 12000.0f, hzOf (e.position.x))));
    repaint();
}

void Graph::mouseUp (const juce::MouseEvent&)
{
    if (dragBand >= 0)
        param (bandIds()[(size_t) dragBand].freq)->endChangeGesture();
    dragBand = -1;
}

void Graph::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int b = bandAt (e.position);
    if (b < 0)
        return;
    auto* p = param (bandIds()[(size_t) b].q);
    const float q = p->convertFrom0to1 (p->getValue()) * std::pow (1.15f, w.deltaY * 4.0f);
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (0.5f, 4.0f, q)));
    p->endChangeGesture();
    repaint();
}

void Graph::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int b = bandAt (e.position);
    if (b < 0)
        return;
    auto* p = param (bandIds()[(size_t) b].on);
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->getValue() >= 0.5f ? 0.0f : 1.0f);
    p->endChangeGesture();
}

void Graph::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (gui::panelBottom);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (gui::border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
    const auto p = plotArea();

    // grid: frequencies, spectrum levels (left) and the cut scale (right)
    g.setFont (gui::fontOf (10.5f));
    for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xOf (hz);
        g.setColour (gui::border.withAlpha (0.55f));
        g.drawVerticalLine ((int) x, p.getY(), p.getBottom());
        g.setColour (gui::textDim.withAlpha (0.8f));
        g.drawText (hz >= 1000.0f ? juce::String ((int) (hz / 1000.0f)) + "k" : juce::String ((int) hz), juce::Rectangle<float> (x - 20.0f, p.getBottom() + 4.0f, 40.0f, 14.0f), juce::Justification::centred);
    }
    for (float db : { 0.0f, -3.0f, -6.0f, -9.0f, -12.0f })
    {
        const float y = cutY (db);
        g.setColour (gui::border.withAlpha (db == 0.0f ? 0.9f : 0.35f));
        g.drawHorizontalLine ((int) y, p.getX(), p.getRight());
        g.setColour (gui::textDim.withAlpha (0.8f));
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (p.getRight() + 4.0f, y - 7.0f, 36.0f, 14.0f), juce::Justification::centredLeft);
    }
    g.setColour (gui::textDim.withAlpha (0.8f));
    g.drawText ("CUT dB", juce::Rectangle<float> (p.getRight() - 4.0f, p.getY() - 20.0f, 48.0f, 14.0f), juce::Justification::centredRight);
    for (float db : { -24.0f, -48.0f, -72.0f })
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (p.getX() - 40.0f, specY (db) - 7.0f, 34.0f, 14.0f), juce::Justification::centredRight);

    // spectra (real analysers of the real audio; nothing drawn when there is no signal)
    auto spectrumPath = [&] (const glitchkid::analysis::EqAnalyzer& a, bool closed) {
        juce::Path path;
        const float step = 3.0f;
        bool started = false;
        for (float x = p.getX(); x <= p.getRight(); x += step)
        {
            const float y = specY (a.levelDb (hzOf (x), hzOf (x + step)));
            if (! started) { path.startNewSubPath (x, closed ? p.getBottom() : y); if (closed) path.lineTo (x, y); started = true; }
            else path.lineTo (x, y);
        }
        if (closed) { path.lineTo (p.getRight(), p.getBottom()); path.closeSubPath(); }
        return path;
    };
    g.saveState();
    g.reduceClipRegion (p.toNearestInt());
    if (specIn.hasSignal())
    {
        g.setColour (gui::pink.withAlpha (0.13f));
        g.fillPath (spectrumPath (specIn, true));
    }
    if (specOut.hasSignal())
    {
        g.setColour (gui::textBright.withAlpha (0.55f));
        g.strokePath (spectrumPath (specOut, false), juce::PathStrokeType (1.2f));
    }
    if (specVocal.hasSignal() && status != SidechainStatus::notConnected)
    {
        g.setColour (vocalColour().withAlpha (0.9f));
        juce::Path dashed;
        const float dashes[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.4f).createDashedStroke (dashed, spectrumPath (specVocal, false), dashes, 2);
        g.fillPath (dashed);
    }

    // the real attenuation: the sum of the eight bells at the engine's live cuts (the same filter maths the audio uses)
    const double fs = proc.getSampleRateNow();
    juce::Path curve;
    bool any = false;
    for (float x = p.getX(); x <= p.getRight(); x += 2.0f)
    {
        const float hz = hzOf (x);
        double db = 0.0;
        for (int b = 0; b < numBands; ++b)
            if (cut[(size_t) b] < -0.001f)
            {
                const auto& id = bandIds()[(size_t) b];
                db += Svf::bellResponseDb (hz, param (id.freq)->convertFrom0to1 (param (id.freq)->getValue()),
                                           param (id.q)->convertFrom0to1 (param (id.q)->getValue()), cut[(size_t) b], fs);
                any = true;
            }
        const float y = cutY ((float) db);
        x == p.getX() ? curve.startNewSubPath (x, y) : curve.lineTo (x, y);
    }
    g.setColour (any ? gui::pink : gui::pink.withAlpha (0.45f));
    g.strokePath (curve, juce::PathStrokeType (any ? 2.4f : 1.4f));
    g.restoreState();

    // band markers with their live cut
    for (int b = 0; b < numBands; ++b)
    {
        const auto& id = bandIds()[(size_t) b];
        const bool on = param (id.on)->getValue() >= 0.5f;
        const float hz = param (id.freq)->convertFrom0to1 (param (id.freq)->getValue());
        const float x = xOf (hz), y = cutY (cut[(size_t) b]);
        const bool cutting = cut[(size_t) b] < -0.05f;
        auto dot = juce::Rectangle<float> (18.0f, 18.0f).withCentre ({ x, y });
        g.setColour (! on ? gui::panelTop : cutting ? gui::pink : gui::panelTop);
        g.fillEllipse (dot);
        g.setColour (on ? gui::pink : gui::disabled);
        g.drawEllipse (dot, dragBand == b ? 2.0f : 1.2f);
        g.setColour (cutting ? gui::obsidian : on ? gui::textBright : gui::disabled);
        g.setFont (gui::fontOf (10.5f, true));
        g.drawText (juce::String (b + 1), dot, juce::Justification::centred);
        if (on && cutting)
        {
            g.setColour (gui::pink);
            g.drawText (juce::String (cut[(size_t) b], 1), juce::Rectangle<float> (x - 24.0f, y + 10.0f, 48.0f, 14.0f), juce::Justification::centred);
        }
    }

    // legend and the vocal-activity lamp
    g.setFont (gui::fontOf (10.5f, true));
    float lx = p.getX() + 4.0f;
    auto legend = [&] (juce::Colour c, const juce::String& t) {
        g.setColour (c);
        g.fillRect (juce::Rectangle<float> (lx, p.getY() - 14.0f, 12.0f, 3.0f));
        g.setColour (gui::textDim);
        g.drawText (t, juce::Rectangle<float> (lx + 16.0f, p.getY() - 20.0f, 160.0f, 14.0f), juce::Justification::centredLeft);
        lx += 22.0f + juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), t) + 14.0f;
    };
    legend (gui::pink.withAlpha (0.5f), "INSTRUMENTAL IN");
    legend (gui::textBright.withAlpha (0.7f), "INSTRUMENTAL OUT");
    legend (vocalColour(), status == SidechainStatus::notConnected ? "VOCAL KEY: NOT CONNECTED" : "VOCAL (KEY ONLY)");
    legend (gui::pink, "CUT ON THE INSTRUMENTAL");
    const bool active = gate > 0.5f && status == SidechainStatus::live;
    auto lamp = juce::Rectangle<float> (p.getRight() - 140.0f, p.getY() - 18.0f, 10.0f, 10.0f); // in the legend row, clear of the band markers
    g.setColour (active ? gui::pink : gui::panelTop);
    g.fillEllipse (lamp);
    g.setColour (active ? gui::pink : gui::border);
    g.drawEllipse (lamp, 1.0f);
    g.setColour (active ? gui::textBright : gui::textDim);
    g.drawText (status == SidechainStatus::notConnected ? "NO VOCAL KEY" : active ? "VOCAL ACTIVE" : "VOCAL IDLE",
                juce::Rectangle<float> (lamp.getRight() + 6.0f, lamp.getY() - 2.0f, 130.0f, 14.0f), juce::Justification::centredLeft);
    if (status == SidechainStatus::notConnected)
    {
        const auto box = juce::Rectangle<float> (p.getCentreX() - 330.0f, p.getY() + p.getHeight() * 0.55f, 660.0f, 54.0f);
        g.setColour (gui::obsidian.withAlpha (0.92f));
        g.fillRoundedRectangle (box, 5.0f);
        g.setColour (warning.withAlpha (0.6f));
        g.drawRoundedRectangle (box, 5.0f, 1.0f);
        g.setFont (gui::fontOf (13.0f));
        g.setColour (gui::textBright);
        g.drawFittedText ("No vocal is connected to the sidechain input, so Beat Lock is passing the instrumental through unchanged.\n"
                          "Send your vocal track to this plug-in's sidechain input (see HOW TO ROUTE).",
                          box.reduced (12.0f, 6.0f).toNearestInt(), juce::Justification::centred, 3);
    }
}

// ================================================================== BandEditor
BandEditor::BandEditor (BeatLockProcessor& p)
{
    for (int b = 0; b < numBands; ++b)
    {
        auto& c = cols[(size_t) b];
        const auto& id = bandIds()[(size_t) b];
        c.title.setText ("BAND " + juce::String (b + 1), juce::dontSendNotification);
        styleLabel (c.title, 11.5f, true, gui::textBright);
        styleLabel (c.cutNow, 11.5f, true, gui::pink, juce::Justification::centredRight);
        for (auto* l : { &c.freqL, &c.qL, &c.maxCutL })
            styleLabel (*l, 10.0f, false, gui::textDim);
        c.freqL.setText ("FREQ", juce::dontSendNotification);
        c.qL.setText ("Q", juce::dontSendNotification);
        c.maxCutL.setText ("MAX CUT", juce::dontSendNotification);
        for (auto* s : { &c.freq, &c.q, &c.maxCut })
        {
            s->setSliderStyle (juce::Slider::LinearHorizontal);
            s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 18);
            s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            s->setColour (juce::Slider::textBoxTextColourId, gui::textBright);
            addAndMakeVisible (*s);
        }
        c.on.setTooltip ("Band " + juce::String (b + 1) + " on or off. Off = this band never cuts.");
        c.freq.setTooltip ("Where this band cuts the instrumental.");
        c.q.setTooltip ("How wide the cut is (higher = narrower).");
        c.maxCut.setTooltip ("The deepest cut this band may make (the global MAX CUT also applies: the milder limit wins).");
        for (auto* comp : std::initializer_list<juce::Component*> { &c.title, &c.cutNow, &c.on, &c.freqL, &c.qL, &c.maxCutL })
            addAndMakeVisible (comp);
        c.onAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, id.on, c.on);
        c.freqAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id.freq, c.freq);
        c.qAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id.q, c.q);
        c.maxAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id.maxCut, c.maxCut);
    }
}

void BandEditor::refresh (const Engine& e)
{
    for (int b = 0; b < numBands; ++b)
    {
        const float c = e.cutDb (b);
        cols[(size_t) b].cutNow.setText (c < -0.05f ? juce::String (c, 1) + " dB" : "0.0 dB", juce::dontSendNotification);
    }
}

void BandEditor::paint (juce::Graphics& g)
{
    g.setColour (gui::panelBottom);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    g.setColour (gui::border);
    for (int b = 1; b < numBands; ++b)
    {
        const int x = getWidth() * b / numBands;
        g.drawVerticalLine (x, 8.0f, (float) getHeight() - 8.0f);
    }
}

void BandEditor::resized()
{
    const int w = getWidth() / numBands;
    for (int b = 0; b < numBands; ++b)
    {
        auto r = juce::Rectangle<int> (b * w, 0, w, getHeight()).reduced (8, 6);
        auto& c = cols[(size_t) b];
        auto top = r.removeFromTop (20);
        c.title.setBounds (top.removeFromLeft (60));
        c.cutNow.setBounds (top);
        c.on.setBounds (r.removeFromTop (22));
        for (auto [l, s] : { std::pair { &c.freqL, &c.freq }, std::pair { &c.qL, &c.q }, std::pair { &c.maxCutL, &c.maxCut } })
        {
            l->setBounds (r.removeFromTop (13));
            s->setBounds (r.removeFromTop (20));
            r.removeFromTop (2);
        }
    }
}

// ================================================================== AnalysisPanel
AnalysisPanel::AnalysisPanel()
{
    for (auto* b : { &apply, &discard, &close })
        addChildComponent (b);
    apply.onClick = [this] { if (onApply) onApply(); };
    discard.onClick = [this] { if (onClose) onClose(); };
    close.onClick = [this] { if (onClose) onClose(); };
    apply.setTooltip ("Set the bands and sensitivity to the proposed values (one undoable host change per parameter).");
    discard.setTooltip ("Keep your settings exactly as they are.");
}

void AnalysisPanel::resized()
{
    auto r = getLocalBounds().reduced (20).removeFromBottom (34);
    close.setBounds (r.removeFromRight (110));
    discard.setBounds (close.getBounds());
    r.removeFromRight (10);
    apply.setBounds (r.removeFromRight (130));
}

void AnalysisPanel::showRunning (ConflictAnalysis::Mode m, float p, float active, float elapsed)
{
    view = View::running;
    title = m == ConflictAnalysis::Mode::learn ? "LEARN" : "ANALYZE";
    progress = p;
    text = "Listening to the vocal sidechain and the instrumental together. Play a section with vocals.\nActive vocal heard: " + juce::String (active, 1)
           + " s  (elapsed " + juce::String (elapsed, 1) + " s)";
    apply.setVisible (false);
    discard.setVisible (false);
    close.setButtonText ("Cancel");
    close.setVisible (true);
    repaint();
}

void AnalysisPanel::showMessage (const juce::String& t, const juce::String& m)
{
    view = View::message;
    title = t;
    text = m;
    apply.setVisible (false);
    discard.setVisible (false);
    close.setButtonText ("Close");
    close.setVisible (true);
    repaint();
}

void AnalysisPanel::showAnalysis (const AnalysisResult& r)
{
    view = View::analysis;
    title = "ANALYZE: FREQUENCY CONFLICT REPORT";
    result = r;
    apply.setVisible (false);
    discard.setVisible (false);
    close.setButtonText ("Close");
    close.setVisible (true);
    repaint();
}

void AnalysisPanel::showLearn (const AnalysisResult& r, const LearnProposal& p, const std::array<float, numBands>& cur, float curSens)
{
    view = View::learn;
    title = "LEARN: PROPOSED SETTINGS";
    result = r;
    proposal = p;
    current = cur;
    currentSens = curSens;
    apply.setVisible (p.valid);
    discard.setVisible (true);
    close.setVisible (false);
    repaint();
}

void AnalysisPanel::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (gui::obsidian.withAlpha (0.96f));
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (gui::pink.withAlpha (0.6f));
    g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
    auto r = getLocalBounds().reduced (20);
    g.setColour (gui::textBright);
    g.setFont (gui::fontOf (16.0f, true));
    g.drawText (title, r.removeFromTop (24), juce::Justification::centredLeft);
    r.removeFromBottom (44);
    g.setFont (gui::fontOf (13.0f));
    if (view == View::message || view == View::running)
    {
        g.setColour (gui::textDim);
        g.drawFittedText (text, r.removeFromTop (60), juce::Justification::topLeft, 4);
        if (view == View::running)
        {
            auto bar = r.removeFromTop (10).toFloat().withWidth ((float) r.getWidth() * 0.6f);
            g.setColour (gui::panelTop);
            g.fillRoundedRectangle (bar, 3.0f);
            g.setColour (gui::pink);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * progress), 3.0f);
        }
        return;
    }
    g.setColour (gui::textDim);
    g.drawText (result.message, r.removeFromTop (20), juce::Justification::centredLeft);
    if (view == View::analysis)
    {
        // per third-octave region: vocal and instrumental level, and how often they conflicted while the vocal was active
        auto chart = r.removeFromTop (r.getHeight() - 40).toFloat().reduced (0.0f, 6.0f);
        const int n = (int) result.regions.size();
        const float colW = chart.getWidth() / (float) juce::jmax (1, n);
        float lo = 0.0f;
        for (const auto& reg : result.regions) lo = juce::jmin (lo, juce::jmin (reg.vocalDb, reg.instDb));
        lo = juce::jmax (lo, -100.0f);
        auto yOf = [&] (float db) { return chart.getBottom() - 14.0f - (chart.getHeight() - 30.0f) * juce::jlimit (0.0f, 1.0f, (db - lo) / (0.0f - lo)); };
        for (int k = 0; k < n; ++k)
        {
            const auto& reg = result.regions[(size_t) k];
            const float x = chart.getX() + colW * (float) k;
            const float conf = reg.conflictPct / 100.0f;
            g.setColour (gui::pink.withAlpha (0.12f + 0.5f * conf));
            g.fillRect (juce::Rectangle<float> (x + 1.0f, chart.getY(), colW - 2.0f, chart.getHeight() - 14.0f));
            g.setColour (gui::textBright.withAlpha (0.8f));
            g.fillRect (juce::Rectangle<float> (x + colW * 0.2f, yOf (reg.instDb), colW * 0.25f, 2.0f));
            g.setColour (vocalColour());
            g.fillRect (juce::Rectangle<float> (x + colW * 0.55f, yOf (reg.vocalDb), colW * 0.25f, 2.0f));
            if (k % 3 == 0)
            {
                g.setColour (gui::textDim);
                g.setFont (gui::fontOf (10.0f));
                g.drawText (hzText (reg.centreHz), juce::Rectangle<float> (x - 10.0f, chart.getBottom() - 12.0f, colW + 20.0f, 12.0f), juce::Justification::centred);
            }
        }
        g.setFont (gui::fontOf (12.0f));
        juce::String s = "Bars: instrumental (white), vocal (grey). Shading: how often the instrumental was within 6 dB of the vocal there while the vocal was active.  ";
        if (result.strongest.empty())
            s << "No region conflicted.";
        else
        {
            s << "Strongest conflicts: ";
            for (size_t k = 0; k < result.strongest.size(); ++k)
            {
                const auto& reg = result.regions[(size_t) result.strongest[k]];
                s << (k > 0 ? ", " : "") << hzText (reg.centreHz) << " (" << juce::roundToInt (reg.conflictPct) << " %)";
            }
        }
        g.setColour (gui::textDim);
        g.drawFittedText (s, r, juce::Justification::topLeft, 2);
        return;
    }
    // LEARN: current -> proposed
    if (! proposal.valid)
    {
        g.drawFittedText (proposal.reasons.joinIntoString ("\n"), r, juce::Justification::topLeft, 4);
        return;
    }
    auto table = r.removeFromTop (70);
    const float w = (float) table.getWidth() / (numBands + 1);
    g.setFont (gui::fontOf (12.0f, true));
    for (int c = 0; c <= numBands; ++c)
    {
        auto col = table.toFloat().withX ((float) table.getX() + w * (float) c).withWidth (w);
        g.setColour (gui::textDim);
        g.drawText (c == 0 ? "" : "BAND " + juce::String (c), col.removeFromTop (20.0f), juce::Justification::centred);
        g.drawText (c == 0 ? "CURRENT" : hzText (current[(size_t) c - 1]), col.removeFromTop (22.0f), juce::Justification::centred);
        g.setColour (gui::pink);
        g.drawText (c == 0 ? "PROPOSED" : hzText (proposal.freqHz[(size_t) c - 1]), col.removeFromTop (22.0f), juce::Justification::centred);
    }
    g.setFont (gui::fontOf (13.0f));
    g.setColour (gui::textBright);
    g.drawText ("SENSITIVITY  " + juce::String (currentSens, 1) + " dB  ->  " + juce::String (proposal.sensitivityDb, 1) + " dB", r.removeFromTop (24), juce::Justification::centredLeft);
    g.setColour (gui::textDim);
    g.drawFittedText (proposal.reasons.joinIntoString ("\n") + "\nNothing changes until you press APPLY.", r, juce::Justification::topLeft, 4);
}

// ================================================================== RoutingPanel
RoutingPanel::RoutingPanel()
{
    addAndMakeVisible (close);
}

void RoutingPanel::resized()
{
    close.setBounds (getLocalBounds().reduced (20).removeFromBottom (34).removeFromRight (110));
}

void RoutingPanel::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (gui::obsidian.withAlpha (0.97f));
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (gui::pink.withAlpha (0.6f));
    g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
    auto r = getLocalBounds().reduced (20);
    g.setColour (gui::textBright);
    g.setFont (gui::fontOf (16.0f, true));
    g.drawText ("HOW TO ROUTE BEAT LOCK", r.removeFromTop (26), juce::Justification::centredLeft);
    g.setFont (gui::fontOf (13.5f));
    const juce::String steps =
        "1.  Put GLITCHKID BEAT LOCK on the INSTRUMENTAL's mixer track (or the beat's bus).\n"
        "2.  Send the VOCAL track to Beat Lock's sidechain input (\"Vocal Sidechain\"). Beat Lock only listens to it.\n"
        "3.  Keep the vocal track routed to the master as usual: Beat Lock never outputs the vocal.\n"
        "4.  Listen to the instrumental: it makes room only where and when the vocal needs it.\n\n"
        "The status at the top tells you what Beat Lock receives: NO VOCAL SIDECHAIN means the sidechain input is not enabled; "
        "VOCAL SILENT means it is enabled but nothing arrives.";
    g.setColour (gui::textBright);
    g.drawFittedText (steps, r.removeFromTop (150), juce::Justification::topLeft, 9);
    r.removeFromTop (8);
    g.setColour (warning);
    g.setFont (gui::fontOf (12.5f, true));
    g.drawText ("FL STUDIO  -  NOT YET VERIFIED", r.removeFromTop (20), juce::Justification::centredLeft);
    g.setColour (gui::textDim);
    g.setFont (gui::fontOf (12.5f));
    g.drawFittedText ("Expected route (to be confirmed by testing): insert Beat Lock on the beat's mixer track; select the VOCAL mixer track, right-click the send control "
                      "of the beat's track and choose the sidechain option; then check the plug-in wrapper's input port settings. If the status stays at "
                      "NO VOCAL SIDECHAIN or VOCAL SILENT, the route is not reaching Beat Lock yet.",
                      r.removeFromTop (70), juce::Justification::topLeft, 4);
}
}

// ================================================================== the editor
BeatLockEditor::BeatLockEditor (BeatLockProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.addAndMakeVisible (graph);
    content.addChildComponent (bands);
    content.addChildComponent (panel);
    content.addChildComponent (routing);

    const struct { const char* id; const char* name; const char* tip; } k[] = {
        { ids::amount, "AMOUNT", "How much of the computed cut is applied. 0 % = the instrumental passes through unchanged." },
        { ids::maxCut, "MAX CUT", "The deepest cut any band may make on the instrumental." },
        { ids::sensitivity, "SENSITIVITY", "Higher = cuts start sooner (the instrumental needs to cover the vocal less before a band reacts)." },
        { ids::focusLow, "LOW FOCUS", "How strongly bands below about 500 Hz may cut (body, warmth)." },
        { ids::focusMid, "MID FOCUS", "How strongly bands between about 500 Hz and 2.5 kHz may cut (words, presence)." },
        { ids::focusHigh, "HIGH FOCUS", "How strongly bands above about 2.5 kHz may cut (consonants, air)." },
        { ids::attack, "ATTACK", "How fast a cut deepens when the vocal comes in." },
        { ids::release, "RELEASE", "How fast a cut recovers when the vocal stops." } };
    for (const auto& kk : k)
    {
        knobs.push_back (std::make_unique<glitchkid::beatlock::ui::Knob> (proc.apvts, kk.id, kk.name, kk.tip));
        content.addAndMakeVisible (*knobs.back());
    }
    for (auto* b : { &bypass, &reset, &learn, &analyze, &advanced, &howTo })
        content.addAndMakeVisible (b);
    bypass.setClickingTogglesState (true);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::bypass, bypass);
    bypass.setTooltip ("Bypass: the instrumental passes through exactly as it came in (after a 20 ms fade). The host's bypass does the same.");
    reset.setTooltip ("Every Beat Lock control back to its default.");
    learn.setTooltip ("Listens to the vocal and the instrumental together and PROPOSES band positions and sensitivity. Nothing changes until you apply.");
    analyze.setTooltip ("Measures where the instrumental covers the vocal, and how often. Changes nothing.");
    advanced.setTooltip ("The eight bands: on/off, frequency, width and maximum cut, with each band's live cut.");
    howTo.setTooltip ("How to route the vocal into Beat Lock's sidechain input.");
    advanced.setClickingTogglesState (true);
    reset.onClick = [this] { proc.resetToDefaults(); };
    learn.onClick = [this] { startAnalysis (ConflictAnalysis::Mode::learn); };
    analyze.onClick = [this] { startAnalysis (ConflictAnalysis::Mode::analyze); };
    advanced.onClick = [this] { layout(); };
    howTo.onClick = [this] { routing.setVisible (! routing.isVisible()); panel.setVisible (false); };
    routing.close.onClick = [this] { routing.setVisible (false); };
    panel.onClose = [this] { closePanel(); };
    panel.onApply = [this] { applyLearn(); };

    setResizable (true, true);
    setResizeLimits (designW * 7 / 10, designH * 7 / 10, designW * 3 / 2, designH * 3 / 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) designW / designH);
    setSize (designW, designH);
    timerCallback();
    startTimerHz (30);
}

BeatLockEditor::~BeatLockEditor()
{
    proc.getAnalysisFifo().setArmed (false);
    setLookAndFeel (nullptr);
}

void BeatLockEditor::resized()
{
    content.setBounds (0, 0, designW, designH);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / designW, (float) getHeight() / designH));
    layout();
}

void BeatLockEditor::layout()
{
    bypass.setBounds (928, 18, 84, 30);
    reset.setBounds (1020, 18, 76, 30);
    graph.setBounds (24, 100, 1072, 340);
    panel.setBounds (graph.getBounds().reduced (40, 20));
    routing.setBounds (graph.getBounds().reduced (60, 16));
    const bool adv = advanced.getToggleState();
    bands.setVisible (adv);
    bands.setBounds (24, 452, 1072, 154);
    for (size_t k = 0; k < knobs.size(); ++k)
    {
        knobs[k]->setBounds (24 + (int) k * 134, 452, 128, 150);
        knobs[k]->setVisible (! adv);
    }
    int x = 24;
    for (auto* b : { &learn, &analyze, &advanced, &howTo })
    {
        b->setBounds (x, 622, 150, 36);
        x += 160;
    }
}

void BeatLockEditor::Content::paint (juce::Graphics& g)
{
    auto& e = editor;
    g.fillAll (gui::obsidian);
    // top bar: emblem, title, the real status and the sidechain level
    g.setColour (gui::panelBottom);
    g.fillRect (0, 0, designW, 64);
    g.setColour (gui::border);
    g.drawHorizontalLine (64, 0.0f, (float) designW);
    gui::brand::drawEmblem (g, { 22.0f, 14.0f, 44.0f, 36.0f });
    g.setColour (gui::textBright);
    g.setFont (gui::fontOf (19.0f, true));
    g.drawText ("GLITCHKID BEAT LOCK", juce::Rectangle<int> (76, 12, 260, 24), juce::Justification::centredLeft);
    g.setColour (gui::textDim);
    g.setFont (gui::fontOf (11.0f));
    g.drawText ("Makes room in the instrumental for the vocal", juce::Rectangle<int> (76, 36, 300, 16), juce::Justification::centredLeft);

    auto pill = juce::Rectangle<float> (380.0f, 18.0f, 330.0f, 30.0f);
    g.setColour (e.statusColour.withAlpha (0.14f));
    g.fillRoundedRectangle (pill, 5.0f);
    g.setColour (e.statusColour);
    g.drawRoundedRectangle (pill, 5.0f, 1.0f);
    g.setFont (gui::fontOf (12.0f, true));
    g.drawText (e.statusText, pill.toNearestInt(), juce::Justification::centred);

    // sidechain level meter (the vocal's real peak)
    auto meter = juce::Rectangle<float> (726.0f, 26.0f, 180.0f, 12.0f);
    g.setColour (gui::textDim);
    g.setFont (gui::fontOf (10.0f, true));
    g.drawText ("VOCAL KEY", juce::Rectangle<float> (726.0f, 8.0f, 180.0f, 14.0f), juce::Justification::centredLeft);
    g.setColour (gui::well);
    g.fillRoundedRectangle (meter, 3.0f);
    g.setColour (gui::border);
    g.drawRoundedRectangle (meter, 3.0f, 1.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, (e.meterDb + 60.0f) / 60.0f);
    g.setColour (e.meterDb > -1.0f ? gui::errorRed : gui::pink);
    g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * frac), 3.0f);
    g.setColour (gui::textDim);
    g.drawText (e.meterDb <= -89.0f ? "-inf" : juce::String (e.meterDb, 1) + " dB", juce::Rectangle<float> (726.0f, 40.0f, 180.0f, 14.0f), juce::Justification::centredRight);

    // the routing indicator: impossible to misread
    auto strip = juce::Rectangle<int> (24, 72, 1072, 22);
    g.setColour (gui::textBright);
    g.setFont (gui::fontOf (13.0f, true));
    g.drawText (juce::CharPointer_UTF8 ("INSTRUMENTAL IN  \xe2\x86\x92  BEAT LOCK  \xe2\x86\x92  INSTRUMENTAL OUT"), strip, juce::Justification::centredLeft);
    g.setColour (gui::textDim);
    g.drawText ("VOCAL = KEY ONLY  (listened to, never in the output, never changed)", strip, juce::Justification::centredRight);
}

void BeatLockEditor::timerCallback()
{
    // status: from what really arrives
    const auto st = proc.getSidechainStatus();
    const bool byp = proc.apvts.getRawParameterValue (ids::bypass)->load() >= 0.5f;
    if (byp)
    {
        statusText = juce::CharPointer_UTF8 ("BYPASSED \xe2\x80\x94 PASSING THROUGH");
        statusColour = gui::textDim;
    }
    else if (st == SidechainStatus::notConnected)
    {
        statusText = juce::CharPointer_UTF8 ("NO VOCAL SIDECHAIN \xe2\x80\x94 PASSING THROUGH");
        statusColour = warning;
    }
    else if (st == SidechainStatus::silent)
    {
        statusText = juce::CharPointer_UTF8 ("VOCAL SILENT \xe2\x80\x94 PASSING THROUGH");
        statusColour = warning;
    }
    else
    {
        statusText = "PROCESSING THE INSTRUMENTAL";
        statusColour = gui::pink;
    }
    const float pk = proc.consumeSidechainPeak();
    const float pkDb = pk > 0.0f ? juce::Decibels::gainToDecibels (pk, -90.0f) : -90.0f;
    meterDb = juce::jmax (pkDb, meterDb - 1.5f); // a falling peak meter (about 45 dB/s)
    content.repaint (0, 0, designW, 100);

    graph.tick (1.0f / 30.0f);
    if (bands.isVisible())
        bands.refresh (proc.getEngine());

    // LEARN / ANALYZE: drain the FIFO here (message thread) and analyse
    if (analysis.running())
    {
        auto& fifo = proc.getAnalysisFifo();
        if (drainV.empty()) { drainV.resize (16384); drainI.resize (16384); drainG.resize (16384); }
        for (int got = 1; got > 0 && analysis.running();)
        {
            got = fifo.read (drainV.data(), drainI.data(), drainG.data(), (int) drainV.size());
            analysis.feed (drainV.data(), drainI.data(), drainG.data(), got);
        }
        if (analysis.finished())
            finishAnalysis();
        else
            panel.showRunning (analysis.mode(), analysis.progress(), analysis.activeSeconds(), analysis.elapsedSeconds());
    }
}

void BeatLockEditor::startAnalysis (ConflictAnalysis::Mode m)
{
    routing.setVisible (false);
    panel.setVisible (true);
    const juce::String name = m == ConflictAnalysis::Mode::learn ? "LEARN" : "ANALYZE";
    const auto st = proc.getSidechainStatus();
    if (st == SidechainStatus::notConnected)
    {
        panel.showMessage (name, "No vocal sidechain is connected, so there is nothing to " + juce::String (m == ConflictAnalysis::Mode::learn ? "learn from" : "analyze")
                                     + ". Send the vocal to Beat Lock's sidechain input first (see HOW TO ROUTE). Nothing was changed.");
        return;
    }
    auto& fifo = proc.getAnalysisFifo();
    fifo.setArmed (false);
    fifo.clear();
    analysis.start (m, proc.getSampleRateNow(), m == ConflictAnalysis::Mode::learn ? 6.0f : 8.0f, 30.0f);
    fifo.setArmed (true);
    panel.showRunning (m, 0.0f, 0.0f, 0.0f);
}

void BeatLockEditor::finishAnalysis()
{
    proc.getAnalysisFifo().setArmed (false);
    const auto r = analysis.result();
    const auto mode = analysis.mode();
    analysis.cancel();
    if (r.status != AnalysisResult::Status::ready)
    {
        panel.showMessage (mode == ConflictAnalysis::Mode::learn ? "LEARN" : "ANALYZE", r.message + " Nothing was changed.");
        return;
    }
    if (mode == ConflictAnalysis::Mode::analyze)
    {
        panel.showAnalysis (r);
        return;
    }
    std::array<float, numBands> cur {};
    for (int b = 0; b < numBands; ++b)
        cur[(size_t) b] = proc.apvts.getRawParameterValue (bandIds()[(size_t) b].freq)->load();
    proposal = ConflictAnalysis::propose (r, cur);
    panel.showLearn (r, proposal, cur, proc.apvts.getRawParameterValue (ids::sensitivity)->load());
}

void BeatLockEditor::applyLearn()
{
    if (! proposal.valid)
        return;
    auto set = [this] (const juce::String& id, float value) {
        auto* p = proc.apvts.getParameter (id);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    };
    for (int b = 0; b < numBands; ++b)
        set (bandIds()[(size_t) b].freq, proposal.freqHz[(size_t) b]);
    set (ids::sensitivity, proposal.sensitivityDb);
    proposal = {};
    closePanel();
}

void BeatLockEditor::closePanel()
{
    proc.getAnalysisFifo().setArmed (false);
    analysis.cancel();
    panel.setVisible (false);
}
