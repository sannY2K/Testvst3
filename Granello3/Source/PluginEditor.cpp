#include "PluginEditor.h"

using namespace juce;

static Font F (float h, bool bold = false) { return Font (FontOptions (h, bold ? Font::bold : Font::plain)); }

// ------------------------------------------------------------- LookAndFeel

void GranelloLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                            float a0, float a1, Slider&)
{
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (1.f);
    const float r = jmin (b.getWidth(), b.getHeight()) / 2.f;
    const auto c = b.getCentre();
    const float ang = a0 + pos * (a1 - a0);

    g.setColour (Theme::knob);
    g.fillEllipse (b.withSizeKeepingCentre (r * 2.f, r * 2.f));

    Path p;
    p.addRoundedRectangle (-r * 0.035f, -r * 0.92f, r * 0.07f, r * 0.34f, r * 0.03f);
    g.setColour (Theme::cream);
    g.fillPath (p, AffineTransform::rotation (ang).translated (c.x, c.y));
}

void GranelloLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    const auto style = b.getProperties()["style"].toString();
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const float rad = jmin (r.getHeight() * 0.3f, 8.f);

    if (style == "step")
    {
        const bool sel = b.getToggleState();
        const bool on = (bool) b.getProperties()["on"];
        const bool playing = (bool) b.getProperties()["playing"];
        g.setColour (sel ? Theme::stepSel : (on ? Theme::stepBtn : Theme::stepBtn.withAlpha (0.35f)));
        g.fillRoundedRectangle (r, rad);
        if (playing)
        {
            g.setColour (Theme::orange);
            g.drawRoundedRectangle (r, rad, 2.f);
        }
    }
    else if (style == "seg")
    {
        if (b.getToggleState())
        {
            g.setColour (Theme::ink.withAlpha (0.22f));
            g.fillRoundedRectangle (r.reduced (1.5f), rad);
        }
    }
    else if (style == "link")
    {
        const auto c = r.getCentre();
        const float rr = r.getHeight() * 0.28f;
        g.setColour (b.getToggleState() ? Theme::ink.darker (0.3f) : Theme::line.withAlpha (0.55f));
        g.drawRoundedRectangle (Rectangle<float> (rr * 2.3f, rr * 1.5f).withCentre (c.translated (-rr * 0.75f, 0.f)), rr * 0.7f, 1.4f);
        g.drawRoundedRectangle (Rectangle<float> (rr * 2.3f, rr * 1.5f).withCentre (c.translated (rr * 0.75f, 0.f)), rr * 0.7f, 1.4f);
    }
    else if (style == "arrow")
    {
        if (down) { g.setColour (Theme::ink.withAlpha (0.2f)); g.fillRoundedRectangle (r, rad); }
    }
    else
    {
        g.setColour (down ? Theme::ink.withAlpha (0.25f) : (over ? Theme::ink.withAlpha (0.1f) : Colours::transparentBlack));
        g.fillRoundedRectangle (r, rad);
        g.setColour (Theme::line);
        g.drawRoundedRectangle (r, rad, 1.f);
    }
}

void GranelloLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    const auto style = b.getProperties()["style"].toString();
    if (style == "link") return;

    const bool step = style == "step";
    g.setColour (step ? Theme::cream : (b.getToggleState() ? Theme::ink.darker (0.4f) : Theme::ink));
    g.setFont (F ((float) b.getHeight() * (step ? 0.5f : 0.62f)));
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred, false);
}

// ------------------------------------------------------------- ScreenView

static bool isAudioFile (const String& f)
{
    return f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aif") || f.endsWithIgnoreCase (".aiff")
        || f.endsWithIgnoreCase (".flac") || f.endsWithIgnoreCase (".mp3") || f.endsWithIgnoreCase (".ogg");
}

bool ScreenView::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files) if (isAudioFile (f)) return true;
    return false;
}

void ScreenView::filesDropped (const StringArray& files, int, int)
{
    dragging = false;
    for (auto& f : files)
        if (isAudioFile (f)) { if (onFile) onFile (File (f)); break; }
    repaint();
}

void ScreenView::mouseDown (const MouseEvent& e)
{
    const float sc = (float) getWidth() / 652.f;
    const float inner = 4.f * sc;
    const float topH = ((float) getHeight() - 2.f * inner) * 0.64f;
    if ((float) e.y < inner + topH) return;
    const int n = proc.getNumActiveSteps();
    const float cw = ((float) getWidth() - 2.f * inner) / (float) n;
    const int idx = jlimit (0, n - 1, (int) (((float) e.x - inner) / cw));
    if (onSelectStep) onSelectStep (idx);
}

void ScreenView::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float sc = b.getWidth() / 652.f;

    Path outer;
    outer.addRoundedRectangle (b, 24.f * sc);
    g.setColour (Theme::screenBorder);
    g.fillPath (outer);

    auto in = b.reduced (4.f * sc);
    Path ip;
    ip.addRoundedRectangle (in, 20.f * sc);
    g.setColour (Theme::screen);
    g.fillPath (ip);

    g.saveState();
    g.reduceClipRegion (ip);

    const float topH = in.getHeight() * 0.64f;
    const int nS = proc.getNumActiveSteps();
    const int sel = proc.getSelectedStep(), playing = proc.getPlayingStep();

    // nube de chops
    for (int s = 0; s < nS; ++s)
    {
        if (! proc.isStepOn (s)) continue;
        const float px = in.getX() + proc.getStep (s, pPosition) * in.getWidth();
        const float fy = std::fmod ((float) s * 0.381f, 1.f);
        const float py = in.getY() + topH * (0.25f + 0.5f * fy);
        const float rad = (34.f + proc.getStep (s, pSpace) * 46.f) * sc;
        const auto col = (s % 2 == 0) ? Theme::orange : Theme::blue;
        const float a = s == playing ? 0.75f : (s == sel ? 0.5f : 0.26f);
        g.setGradientFill (ColourGradient (col.withAlpha (a), px, py, col.withAlpha (0.f), px + rad, py, true));
        g.fillEllipse (px - rad, py - rad, rad * 2.f, rad * 2.f);
        if (s == sel)
        {
            g.setColour (Colours::white.withAlpha (0.45f));
            g.drawRect (Rectangle<float> (56.f * sc, 56.f * sc).withCentre ({ px, py }), 1.f);
            g.setFont (F (10.f * sc));
            g.drawText ("id:" + String (s + 1), Rectangle<float> (px - 28.f * sc, py + 29.f * sc, 60.f * sc, 12.f * sc),
                        Justification::centredLeft, false);
        }
    }

    // tira de forma de onda con los cortes
    const float stripTop = in.getY() + topH;
    const float stripH = in.getHeight() - topH;
    g.setColour (Colour (0xff131316));
    g.fillRect (in.getX(), stripTop, in.getWidth(), stripH);

    const float numH = 14.f * sc;
    const float cw = in.getWidth() / (float) nS;
    for (int i = 0; i < nS; ++i)
    {
        const float x0 = in.getX() + (float) i * cw;
        if (i == sel) { g.setColour (Colours::white.withAlpha (0.10f)); g.fillRect (x0, stripTop, cw, stripH); }
        if (i == playing) { g.setColour (Colours::white.withAlpha (0.20f)); g.fillRect (x0, stripTop, cw, stripH); }
        g.setColour (Colours::white.withAlpha (0.12f));
        g.drawVerticalLine ((int) x0, stripTop, stripTop + stripH);
        g.setColour (Colours::white.withAlpha (0.55f));
        g.setFont (F (9.f * sc));
        g.drawText (String (i + 1), Rectangle<float> (x0 + 4.f * sc, stripTop + 1.f, cw, numH), Justification::centredLeft, false);
    }

    auto s = proc.getSample();
    const int N = s != nullptr ? s->buffer.getNumSamples() : 0;
    if (N > 0)
    {
        const int w = jmax (1, (int) in.getWidth());
        if (s != cached || w != cachedW)
        {
            cached = s; cachedW = w;
            peaks.assign ((size_t) w, 0.f);
            for (int x = 0; x < w; ++x)
            {
                const int a = (int) ((int64) x * N / w), e = jmax (a + 1, (int) ((int64) (x + 1) * N / w));
                const int step = jmax (1, (e - a) / 64);
                float m = 0.f;
                for (int ch = 0; ch < s->buffer.getNumChannels(); ++ch)
                {
                    auto* d = s->buffer.getReadPointer (ch);
                    for (int i = a; i < e && i < N; i += step) m = jmax (m, std::abs (d[i]));
                }
                peaks[(size_t) x] = jmin (1.f, m);
            }
        }
        const float mid = stripTop + numH + (stripH - numH) * 0.5f;
        const float half = (stripH - numH) * 0.40f;
        Path top, bot;
        for (int x = 0; x < w; ++x)
        {
            const float px = in.getX() + (float) x;
            const float h = peaks[(size_t) x] * half;
            if (x == 0) { top.startNewSubPath (px, mid - h); bot.startNewSubPath (px, mid + h); }
            else { top.lineTo (px, mid - h); bot.lineTo (px, mid + h); }
        }
        g.setColour (Colours::white.withAlpha (0.85f));
        g.strokePath (top, PathStrokeType (1.3f * sc));
        g.strokePath (bot, PathStrokeType (1.3f * sc));
    }

    const float phx = in.getX() + proc.getStep (sel, pPosition) * in.getWidth();
    g.setColour (Colours::white);
    g.fillRect (phx - 0.75f, stripTop, 1.5f, stripH);
    Path tri;
    tri.addTriangle (phx - 4.f * sc, stripTop, phx + 4.f * sc, stripTop, phx, stripTop + 6.f * sc);
    g.fillPath (tri);

    g.setColour (Theme::cream.withAlpha (0.55f));
    g.setFont (F (11.f * sc));
    g.drawText (proc.getSampleName() + (s != nullptr ? "  |  " + String (s->sampleRate / 1000.0, 1) + " kHz" : String()),
                Rectangle<float> (in.getX() + 16.f * sc, in.getY() + 8.f * sc, in.getWidth() - 32.f * sc, 16.f * sc),
                Justification::centredLeft, true);

    g.restoreState();
    if (dragging)
    {
        g.setColour (Theme::orange);
        g.drawRoundedRectangle (in, 20.f * sc, 2.f);
    }
}

// ----------------------------------------------------------------- EnvView

static float normT (float t, float mx) { return jlimit (0.f, 1.f, std::log (jmax (1.f, t)) / std::log (mx)); }
static float denormT (float n, float mx) { return std::exp (jlimit (0.f, 1.f, n) * std::log (mx)); }

Rectangle<float> EnvView::area() const { return getLocalBounds().toFloat().reduced (12.f, 8.f).withTrimmedTop (12.f); }

std::array<Point<float>, 5> EnvView::points() const
{
    const auto r = area();
    const float seg = r.getWidth() * 0.26f;
    const float a = normT (get ("attack"), 5000.f), d = normT (get ("decay"), 5000.f), rl = normT (get ("release"), 10000.f);
    const float s = get ("sustain");
    std::array<Point<float>, 5> p;
    p[0] = { r.getX(), r.getBottom() };
    p[1] = { r.getX() + seg * a, r.getY() };
    p[2] = { p[1].x + seg * d, r.getBottom() - s * r.getHeight() };
    p[3] = { p[2].x + seg * 0.6f, p[2].y };
    p[4] = { p[3].x + seg * rl, r.getBottom() };
    return p;
}

void EnvView::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Theme::line);
    g.drawRoundedRectangle (b, 8.f, 1.f);
    g.setColour (Theme::ink);
    g.setFont (F (11.f));
    g.drawText ("env", 10, 4, 40, 14, Justification::centredLeft, false);

    const auto p = points();
    Path path;
    path.startNewSubPath (p[0]);
    for (size_t i = 1; i < p.size(); ++i) path.lineTo (p[i]);
    g.setColour (Theme::ink);
    g.strokePath (path, PathStrokeType (1.5f, PathStrokeType::curved));
    g.setColour (Theme::ink.darker (0.3f));
    for (int i : { 1, 2, 4 })
        g.fillEllipse (p[(size_t) i].x - 3.5f, p[(size_t) i].y - 3.5f, 7.f, 7.f);
}

void EnvView::mouseDown (const MouseEvent& e)
{
    const auto p = points();
    float best = 16.f;
    drag = -1;
    for (int i : { 1, 2, 4 })
    {
        const float d = p[(size_t) i].getDistanceFrom (e.position);
        if (d < best) { best = d; drag = i; }
    }
    if (drag == 1) gesture ("attack", true);
    if (drag == 2) { gesture ("decay", true); gesture ("sustain", true); }
    if (drag == 4) gesture ("release", true);
}

void EnvView::mouseDrag (const MouseEvent& e)
{
    if (drag < 0) return;
    const auto r = area();
    const auto p = points();
    const float seg = r.getWidth() * 0.26f;
    if (drag == 1) set ("attack", denormT ((e.position.x - r.getX()) / seg, 5000.f));
    if (drag == 2)
    {
        set ("decay", denormT ((e.position.x - p[1].x) / seg, 5000.f));
        set ("sustain", jlimit (0.f, 1.f, (r.getBottom() - e.position.y) / r.getHeight()));
    }
    if (drag == 4) set ("release", denormT ((e.position.x - p[3].x) / seg, 10000.f));
    repaint();
}

void EnvView::mouseUp (const MouseEvent&)
{
    if (drag == 1) gesture ("attack", false);
    if (drag == 2) { gesture ("decay", false); gesture ("sustain", false); }
    if (drag == 4) gesture ("release", false);
    drag = -1;
}

// -------------------------------------------------------------- FilterView

Rectangle<float> FilterView::area() const { return getLocalBounds().toFloat().reduced (12.f, 8.f).withTrimmedTop (12.f); }
float FilterView::freqToX (float f) const
{
    const auto r = area();
    return r.getX() + r.getWidth() * std::log (jmax (20.f, f) / 20.f) / std::log (1000.f);
}
float FilterView::xToFreq (float x) const
{
    const auto r = area();
    return 20.f * std::exp (jlimit (0.f, 1.f, (x - r.getX()) / r.getWidth()) * std::log (1000.f));
}

void FilterView::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Theme::line);
    g.drawRoundedRectangle (b, 8.f, 1.f);
    g.setColour (Theme::ink);
    g.setFont (F (11.f));
    g.drawText ("filter", 10, 4, 50, 14, Justification::centredLeft, false);

    const auto r = area();
    const float hpf = get ("hp"), lpf = get ("lp"), q = get ("reso");
    Path path;
    for (int i = 0; i <= (int) r.getWidth(); ++i)
    {
        const float f = xToFreq (r.getX() + (float) i);
        float m = 1.f / std::sqrt (1.f + std::pow (hpf / f, 4.f)) / std::sqrt (1.f + std::pow (f / lpf, 4.f));
        m *= 1.f + (q - 0.7f) * 0.12f * (std::exp (-std::pow (std::log (f / hpf) * 3.f, 2.f))
                                         + std::exp (-std::pow (std::log (f / lpf) * 3.f, 2.f)));
        const float y = r.getBottom() - 8.f - jlimit (0.f, 1.2f, m) * (r.getHeight() - 14.f);
        if (i == 0) path.startNewSubPath (r.getX(), y); else path.lineTo (r.getX() + (float) i, y);
    }
    g.setColour (Theme::ink);
    g.strokePath (path, PathStrokeType (1.5f, PathStrokeType::curved));

    g.setColour (Theme::ink.darker (0.3f));
    for (float x : { freqToX (hpf), freqToX (lpf) })
    {
        Path t;
        t.addTriangle (x - 4.f, r.getBottom() + 1.f, x + 4.f, r.getBottom() + 1.f, x, r.getBottom() - 6.f);
        g.fillPath (t);
    }
}

void FilterView::mouseDown (const MouseEvent& e)
{
    const float xh = freqToX (get ("hp")), xl = freqToX (get ("lp"));
    drag = std::abs (e.position.x - xh) <= std::abs (e.position.x - xl) ? 0 : 1;
    gesture (drag == 0 ? "hp" : "lp", true);
    gesture ("reso", true);
    mouseDrag (e);
}

void FilterView::mouseDrag (const MouseEvent& e)
{
    if (drag < 0) return;
    const float f = xToFreq (e.position.x);
    if (drag == 0) set ("hp", jlimit (20.f, 2000.f, f));
    else set ("lp", jlimit (200.f, 20000.f, f));
    // arrastrar en vertical cambia la resonancia común
    const auto r = area();
    set ("reso", jmap (jlimit (0.f, 1.f, (r.getBottom() - e.position.y) / r.getHeight()), 0.5f, 6.f));
    repaint();
}

void FilterView::mouseUp (const MouseEvent&)
{
    if (drag >= 0) gesture (drag == 0 ? "hp" : "lp", false);
    gesture ("reso", false);
    drag = -1;
}

// ------------------------------------------------------------------ Editor

static const char* kGrainNames[10] = { "1/1", "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T", "1/64" };
static const char* kRateNames[6] = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" };

GranelloEditor::GranelloEditor (GranelloProcessor& p)
    : AudioProcessorEditor (&p), proc (p), screenView (p), envView (p), filterView (p)
{
    setLookAndFeel (&laf);

    auto style = [] (Button& b, const char* s) { b.getProperties().set ("style", s); b.setClickingTogglesState (false); };

    addAndMakeVisible (screenView);
    screenView.onFile = [this] (const File& f) { loadFile (f); };
    screenView.onSelectStep = [this] (int i) { proc.setSelectedStep (i); refreshKnobs(); };

    addAndMakeVisible (envView);
    addAndMakeVisible (filterView);

    style (libraryBtn, "plain"); addAndMakeVisible (libraryBtn);
    libraryBtn.onClick = [this] { showLibraryMenu(); };
    style (presetBtn, "plain"); addAndMakeVisible (presetBtn);
    presetBtn.onClick = [this] { showLibraryMenu(); };
    style (presetPrev, "arrow"); addAndMakeVisible (presetPrev);
    style (presetNext, "arrow"); addAndMakeVisible (presetNext);
    presetPrev.onClick = [this] { proc.applyPreset ((proc.currentPreset + kNumPresets - 1) % kNumPresets); refreshKnobs(); };
    presetNext.onClick = [this] { proc.applyPreset ((proc.currentPreset + 1) % kNumPresets); refreshKnobs(); };

    style (zoomBtn, "plain"); addAndMakeVisible (zoomBtn);
    zoomBtn.onClick = [this]
    {
        static const float z[3] = { 1.f, 1.25f, 0.8f };
        zoomIdx = (zoomIdx + 1) % 3;
        setSize (roundToInt (1100.f * z[zoomIdx]), roundToInt (630.f * z[zoomIdx]));
    };

    static const char* modeNames[3] = { "hold", "step", "keys" };
    for (int i = 0; i < 3; ++i)
    {
        modeBtn[(size_t) i].setButtonText (modeNames[i]);
        style (modeBtn[(size_t) i], "seg");
        addAndMakeVisible (modeBtn[(size_t) i]);
        modeBtn[(size_t) i].onClick = [this, i] { setChoice ("mode", i); };
    }
    for (int i = 0; i < 2; ++i)
    {
        stepsBtn[(size_t) i].setButtonText (i == 0 ? "8" : "16");
        style (stepsBtn[(size_t) i], "seg");
        addAndMakeVisible (stepsBtn[(size_t) i]);
        stepsBtn[(size_t) i].onClick = [this, i] { setChoice ("steps", i); };
    }

    style (ratePrev, "arrow"); style (rateNext, "arrow");
    addAndMakeVisible (ratePrev); addAndMakeVisible (rateNext);
    auto bumpRate = [this] (int d)
    {
        const int cur = (int) proc.apvts.getRawParameterValue ("rate")->load();
        setChoice ("rate", jlimit (0, 5, cur + d));
    };
    ratePrev.onClick = [bumpRate] { bumpRate (-1); };
    rateNext.onClick = [bumpRate] { bumpRate (1); };
    rateLabel.setJustificationType (Justification::centred);
    rateLabel.setColour (Label::textColourId, Theme::ink);
    addAndMakeVisible (rateLabel);

    style (resetBtn, "plain"); addAndMakeVisible (resetBtn);
    resetBtn.onClick = [this] { proc.pushUndo(); proc.resetSteps(); refreshKnobs(); };
    style (undoBtn, "plain"); addAndMakeVisible (undoBtn);
    undoBtn.onClick = [this] { proc.undo(); refreshKnobs(); };
    style (redoBtn, "plain"); addAndMakeVisible (redoBtn);
    redoBtn.onClick = [this] { proc.redo(); refreshKnobs(); };

    style (syncBtn, "seg"); style (freeBtn, "seg");
    addAndMakeVisible (syncBtn); addAndMakeVisible (freeBtn);
    syncBtn.onClick = [this] { setBool ("sync", true); };
    freeBtn.onClick = [this] { setBool ("sync", false); };

    for (int i = 0; i < kMaxSteps; ++i)
    {
        auto& b = stepBtn[(size_t) i];
        b.setButtonText (String (i + 1));
        style (b, "step");
        addAndMakeVisible (b);
        b.onClick = [this, i] { proc.setSelectedStep (i); refreshKnobs(); };
        b.onRight = [this, i] { proc.setSelectedStep (i); refreshKnobs(); stepMenu (i); };
    }

    for (int i = 0; i < kNumStepParams; ++i)
    {
        auto& k = knobs[(size_t) i];
        const auto& info = kStepInfo[i];
        k.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        k.slider.setRotaryParameters (MathConstants<float>::pi * 1.17f, MathConstants<float>::pi * 2.83f, true);
        if (i == pTune) k.slider.setNormalisableRange (NormalisableRange<double> (info.lo, info.hi, 1.0, info.skew));
        else if (i == pGrains) k.slider.setRange (0.0, 1.0, 0.001);
        else k.slider.setNormalisableRange (NormalisableRange<double> (info.lo, info.hi, 0.0001, info.skew));
        k.slider.setDoubleClickReturnValue (true, info.def);
        k.slider.setVelocityBasedMode (false);
        k.slider.setMouseDragSensitivity (220);

        k.slider.textFromValueFunction = [this, i] (double v) -> String
        {
            switch (i)
            {
                case pSpace: case pChaos: case pPosition: return String (roundToInt ((float) v * 100.f)) + " %";
                case pLength: return String (roundToInt ((float) v)) + " ms";
                case pGrains:
                    if (proc.apvts.getRawParameterValue ("sync")->load() > 0.5f)
                        return kGrainNames[jlimit (0, 9, roundToInt ((float) v * 9.f))];
                    return String (std::pow (200.0, v), 1) + " /s";
                case pTune: return String (v > 0.5 ? "+" : "") + String (roundToInt ((float) v)) + " st";
                default: return String (v > 0.05 ? "+" : "") + String (v, 1) + " dB";
            }
        };

        k.slider.onDragStart = [this] { proc.pushUndo(); };
        k.slider.onValueChange = [this, i]
        {
            const bool linked = proc.apvts.getRawParameterValue ("link" + String (i))->load() > 0.5f;
            proc.editStep (i, (float) knobs[(size_t) i].slider.getValue(), linked);
            knobs[(size_t) i].value.setText (knobs[(size_t) i].slider.getTextFromValue (knobs[(size_t) i].slider.getValue()),
                                             dontSendNotification);
        };
        addAndMakeVisible (k.slider);

        for (auto* l : { &k.name, &k.value })
        {
            l->setJustificationType (Justification::centred);
            l->setColour (Label::textColourId, Theme::ink);
            addAndMakeVisible (*l);
        }
        k.name.setText (info.name, dontSendNotification);

        style (k.link, "link");
        addAndMakeVisible (k.link);
        k.link.onClick = [this, i] { setBool (("link" + String (i)).toRawUTF8(), ! knobs[(size_t) i].link.getToggleState()); };
    }

    getConstrainer()->setFixedAspectRatio (1198.0 / 686.0);
    setResizable (true, true);
    setResizeLimits (600, 344, 1800, 1031);
    setSize (1100, 630);

    refreshKnobs();
    startTimerHz (20);
}

GranelloEditor::~GranelloEditor() { stopTimer(); setLookAndFeel (nullptr); }

Rectangle<float> GranelloEditor::D (float x, float y, float w, float h) const
{
    return { x * sc, y * sc, w * sc, h * sc };
}

void GranelloEditor::setChoice (const char* id, int idx)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) idx));
        p->endChangeGesture();
    }
}

void GranelloEditor::setBool (const char* id, bool v) { setChoice (id, v ? 1 : 0); }

void GranelloEditor::loadFile (const File& f)
{
    if (proc.loadSample (f)) screenView.repaint();
}

void GranelloEditor::chooseFile()
{
    chooser = std::make_unique<FileChooser> ("Elige un audio", File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [this] (const FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile()) loadFile (f);
                          });
}

void GranelloEditor::showLibraryMenu()
{
    PopupMenu m;
    m.addItem (1000, "Cargar audio...");
    m.addSeparator();
    for (int i = 0; i < kNumPresets; ++i)
        m.addItem (i + 1, GranelloProcessor::presetName (i), true, i == proc.currentPreset);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&libraryBtn), [this] (int r)
    {
        if (r == 1000) chooseFile();
        else if (r > 0) { proc.applyPreset (r - 1); refreshKnobs(); }
    });
}

void GranelloEditor::stepMenu (int step)
{
    PopupMenu m;
    m.addItem (1, "Copiar");
    m.addItem (2, "Pegar", hasClip);
    m.addItem (3, proc.isStepOn (step) ? "Desactivar paso" : "Activar paso");
    m.addItem (4, "Copiar a todos");
    m.showMenuAsync (PopupMenu::Options(), [this, step] (int r)
    {
        if (r == 1 || r == 4)
        {
            for (int p = 0; p < kNumStepParams; ++p) clipboard[p] = proc.getStep (step, p);
            hasClip = true;
        }
        if (r == 2 && hasClip)
        {
            proc.pushUndo();
            for (int p = 0; p < kNumStepParams; ++p) proc.setStep (step, p, clipboard[p]);
        }
        if (r == 4)
        {
            proc.pushUndo();
            for (int s = 0; s < kMaxSteps; ++s)
                for (int p = 0; p < kNumStepParams; ++p) proc.setStep (s, p, clipboard[p]);
        }
        if (r == 3) { proc.pushUndo(); proc.setStepOn (step, ! proc.isStepOn (step)); }
        refreshKnobs();
    });
}

void GranelloEditor::refreshKnobs()
{
    const int sel = proc.getSelectedStep();
    const bool sync = proc.apvts.getRawParameterValue ("sync")->load() > 0.5f;
    for (int i = 0; i < kNumStepParams; ++i)
    {
        auto& k = knobs[(size_t) i];
        if (i == pGrains) k.slider.setRange (0.0, 1.0, sync ? 1.0 / 9.0 : 0.001);
        k.slider.setValue (proc.getStep (sel, i), dontSendNotification);
        k.value.setText (k.slider.getTextFromValue (k.slider.getValue()), dontSendNotification);
    }
}

void GranelloEditor::timerCallback()
{
    auto tog = [] (Button& b, bool v) { if (b.getToggleState() != v) b.setToggleState (v, dontSendNotification); };
    auto raw = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };

    const int mode = (int) raw ("mode");
    for (int i = 0; i < 3; ++i) tog (modeBtn[(size_t) i], i == mode);
    const int st = (int) raw ("steps");
    for (int i = 0; i < 2; ++i) tog (stepsBtn[(size_t) i], i == st);
    const bool sync = raw ("sync") > 0.5f;
    tog (syncBtn, sync); tog (freeBtn, ! sync);
    rateLabel.setText (kRateNames[jlimit (0, 5, (int) raw ("rate"))], dontSendNotification);
    presetBtn.setButtonText (GranelloProcessor::presetName (proc.currentPreset));

    const int sel = proc.getSelectedStep(), playing = proc.getPlayingStep(), nS = proc.getNumActiveSteps();
    for (int i = 0; i < kMaxSteps; ++i)
    {
        auto& b = stepBtn[(size_t) i];
        b.setVisible (i < nS);
        tog (b, i == sel);
        b.getProperties().set ("on", proc.isStepOn (i));
        b.getProperties().set ("playing", i == playing);
        b.repaint();
    }

    for (int i = 0; i < kNumStepParams; ++i)
    {
        auto& k = knobs[(size_t) i];
        tog (k.link, raw (("link" + String (i)).toRawUTF8()) > 0.5f);
        if (! k.slider.isMouseButtonDown())
        {
            const double v = proc.getStep (sel, i);
            if (std::abs (k.slider.getValue() - v) > 1e-4 || (i == pGrains && k.slider.getInterval() > 0.01 != sync))
            {
                if (i == pGrains) k.slider.setRange (0.0, 1.0, sync ? 1.0 / 9.0 : 0.001);
                k.slider.setValue (v, dontSendNotification);
            }
            k.value.setText (k.slider.getTextFromValue (k.slider.getValue()), dontSendNotification);
        }
    }

    meter = jmax (proc.getLevel(), meter * 0.85f);
    screenView.repaint();
    envView.repaint();
    filterView.repaint();
    repaint (D (737, 258, 285, 10).toNearestInt());
}

void GranelloEditor::paint (Graphics& g)
{
    sc = (float) getWidth() / 1198.f;
    g.fillAll (Theme::body);
    g.setColour (Theme::line.withAlpha (0.7f));
    g.drawRoundedRectangle (D (1, 1, 1196, 684), 10.f * sc, 1.5f);

    // estrella
    Path star;
    star.addStar ({ 24.f * sc, 24.f * sc }, 4, 3.5f * sc, 13.f * sc, 0.f);
    g.setColour (Theme::salmon);
    g.fillPath (star);

    // paneles
    g.setColour (Theme::line);
    g.drawRoundedRectangle (D (737, 32, 285, 97), 12.f * sc, 1.2f);
    g.drawRoundedRectangle (D (737, 273, 412, 105), 12.f * sc, 1.2f);
    g.drawRoundedRectangle (D (1036, 32, 113, 346), 12.f * sc, 1.2f);
    g.drawRoundedRectangle (D (757, 284, 114, 17), 5.f * sc, 1.f);
    g.drawRoundedRectangle (D (880, 284, 38, 17), 5.f * sc, 1.f);
    g.drawRoundedRectangle (D (932, 284, 83, 17), 5.f * sc, 1.f);
    g.drawRoundedRectangle (D (551, 418, 96, 18), 5.f * sc, 1.f);

    g.setColour (Theme::ink);
    g.setFont (F (13.f * sc));
    g.drawText ("granello 3", D (747, 42, 200, 16), Justification::centredLeft, false);
    g.setFont (F (12.f * sc));
    g.drawText ("version 1.0.0", D (747, 58, 200, 15), Justification::centredLeft, false);
    g.drawText ("granular step sampler", D (747, 73, 200, 15), Justification::centredLeft, false);
    g.drawText ("vst3 for windows", D (747, 88, 200, 15), Justification::centredLeft, false);

    // altavoces
    auto dots = [&] (float cx, float cy)
    {
        const float R = 47.f * sc, pitch = 11.5f * sc, dr = 3.2f * sc;
        g.setColour (Theme::knob.withAlpha (0.88f));
        for (int j = -5; j <= 5; ++j)
            for (int i = -6; i <= 6; ++i)
            {
                const float x = (float) i * pitch + ((j & 1) ? pitch * 0.5f : 0.f);
                const float y = (float) j * pitch * 0.866f;
                if (std::sqrt (x * x + y * y) <= R)
                    g.fillEllipse (cx * sc + x - dr, cy * sc + y - dr, dr * 2.f, dr * 2.f);
            }
    };
    dots (1092.f, 88.f);
    dots (1092.f, 205.f);

    // medidor
    g.setColour (Theme::line.withAlpha (0.35f));
    g.fillRect (D (737, 262, 283, 2.5f));
    g.setColour (Theme::green);
    g.fillRect (D (737, 262, 283.f * jlimit (0.f, 1.f, meter), 2.5f));

    // separadores de la fila de perillas
    g.setColour (Theme::line);
    g.fillRect (D (360, 458, 1.2f, 140));
    g.fillRect (D (839, 458, 1.2f, 140));

    // marca
    Path s2;
    s2.addStar ({ 1050.f * sc, 663.f * sc }, 4, 2.f * sc, 7.f * sc, 0.f);
    g.setColour (Theme::salmon);
    g.fillPath (s2);
    g.setColour (Theme::ink.darker (0.3f));
    g.setFont (F (20.f * sc, true));
    g.drawText ("granello 3", D (1060, 650, 130, 26), Justification::centredLeft, false);
}

void GranelloEditor::resized()
{
    sc = (float) getWidth() / 1198.f;
    auto set = [this] (Component& c, float x, float y, float w, float h) { c.setBounds (D (x, y, w, h).toNearestInt()); };

    set (screenView, 38, 32, 652, 346);
    set (envView, 737, 167, 146, 86);
    set (filterView, 884, 167, 136, 86);

    set (libraryBtn, 737, 139, 136, 18);
    set (presetBtn, 884, 139, 136, 18);
    set (presetPrev, 884, 139, 22, 18);
    set (presetNext, 998, 139, 22, 18);
    set (zoomBtn, 957, 104, 54, 17);

    for (int i = 0; i < 3; ++i) set (modeBtn[(size_t) i], 757.f + 38.f * (float) i, 284, 38, 17);
    set (stepsBtn[0], 880, 284, 19, 17);
    set (stepsBtn[1], 899, 284, 19, 17);
    set (ratePrev, 932, 284, 18, 17);
    set (rateLabel, 950, 284, 47, 17);
    set (rateNext, 997, 284, 18, 17);
    set (resetBtn, 1024, 284, 54, 17);
    set (undoBtn, 1088, 284, 20, 17);
    set (redoBtn, 1108, 284, 20, 17);
    undoBtn.setButtonText ("<"); redoBtn.setButtonText (">");

    for (int i = 0; i < kMaxSteps; ++i)
        set (stepBtn[(size_t) i], 757.f + 48.f * (float) (i % 8), 313.f + 32.f * (float) (i / 8), 37, 24);

    set (syncBtn, 551, 418, 48, 18);
    set (freeBtn, 599, 418, 48, 18);

    static const float cx[7] = { 154, 275, 443, 599, 755, 923, 1044 };
    static const float dia[7] = { 72, 72, 72, 138, 72, 72, 72 };
    for (int i = 0; i < kNumStepParams; ++i)
    {
        auto& k = knobs[(size_t) i];
        const bool big = i == pGrains;
        const float d = dia[i], c = cx[i];
        const float ny = big ? 608.f : 573.f;
        set (k.slider, c - d / 2.f, 529.f - d / 2.f, d, d);
        set (k.name, c - 53.f, ny, 90, 16);
        set (k.link, c + 22.f, ny + 1.f, 14, 14);
        set (k.value, c - 50.f, ny + 20.f, 100, 16);
        k.name.setFont (F (12.5f * sc));
        k.value.setFont (F (12.5f * sc));
    }
    rateLabel.setFont (F (11.f * sc));
}
