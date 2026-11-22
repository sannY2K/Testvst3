#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <climits>

using namespace juce;

const StepParamInfo kStepInfo[kNumStepParams] = {
    { "space",    0.0,   1.0,   0.0,       1.0 },
    { "chaos",    0.0,   1.0,   0.2,       1.0 },
    { "length",   5.0,   500.0, 134.0,     0.4 },
    { "grains",   0.0,   1.0,   4.0 / 9.0, 1.0 },
    { "position", 0.0,   1.0,   0.0,       1.0 },
    { "tune",    -24.0,  24.0,  0.0,       1.0 },
    { "output",  -24.0,  12.0,  0.0,       1.0 }
};

static const char* kG[] = { "sync", "mode", "steps", "rate", "attack", "decay", "sustain", "release", "hp", "lp", "reso" };
static const float kRateBeats[6] = { 4.f, 2.f, 1.f, 0.5f, 0.25f, 0.125f };

struct PresetDef
{
    const char* name;
    float chaos, space, len, grains, tune;
    int pattern, rate;
    float atk, dec, sus, rel;
    int steps16;
};
static const PresetDef kPresets[kNumPresets] = {
    { "lantern", 0.40f, 0.25f, 134.f, 4 / 9.f,   0.f, 0, 3,   5.f, 300.f, 0.8f,  200.f, 1 },
    { "dust",    0.70f, 0.45f,  60.f, 6 / 9.f,   0.f, 2, 3,  20.f, 200.f, 0.5f,  400.f, 1 },
    { "pulse",   0.10f, 0.05f,  90.f, 3 / 9.f,   0.f, 3, 2,   2.f, 120.f, 0.7f,   80.f, 1 },
    { "drift",   0.55f, 0.70f, 300.f, 5 / 9.f, -12.f, 1, 1, 400.f, 800.f, 0.8f, 1500.f, 1 },
    { "shatter", 0.90f, 0.20f,  25.f, 7 / 9.f,   7.f, 2, 4,   1.f,  80.f, 0.3f,   60.f, 1 },
    { "bloom",   0.30f, 0.85f, 420.f, 2 / 9.f,  12.f, 0, 1, 900.f, 500.f, 0.9f, 2500.f, 0 },
    { "tide",    0.25f, 0.50f, 200.f, 4 / 9.f,  -7.f, 1, 2, 150.f, 400.f, 0.7f,  900.f, 1 },
    { "ember",   0.60f, 0.35f,  40.f, 8 / 9.f,   5.f, 4, 3,   3.f, 150.f, 0.6f,  150.f, 1 }
};

String GranelloProcessor::presetName (int i) { return kPresets[jlimit (0, kNumPresets - 1, i)].name; }

AudioProcessorValueTreeState::ParameterLayout GranelloProcessor::createLayout()
{
    using NR = NormalisableRange<float>;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "sync", 1 }, "Sync", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", StringArray { "hold", "step", "keys" }, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "steps", 1 }, "Steps", StringArray { "8", "16" }, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "rate", 1 }, "Rate", StringArray { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" }, 3));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "attack", 1 },  "Attack",  NR (1.f, 5000.f, 0.1f, 0.4f), 5.f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "decay", 1 },   "Decay",   NR (1.f, 5000.f, 0.1f, 0.4f), 300.f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "sustain", 1 }, "Sustain", NR (0.f, 1.f, 0.001f), 0.8f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "release", 1 }, "Release", NR (1.f, 10000.f, 0.1f, 0.4f), 200.f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "hp", 1 },      "High-pass", NR (20.f, 2000.f, 1.f, 0.4f), 20.f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "lp", 1 },      "Low-pass",  NR (200.f, 20000.f, 1.f, 0.3f), 20000.f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "reso", 1 },    "Resonance", NR (0.5f, 6.f, 0.01f), 0.7f));
    for (int i = 0; i < kNumStepParams; ++i)
        l.add (std::make_unique<AudioParameterBool> (ParameterID { "link" + String (i), 1 },
                                                     String ("Link ") + kStepInfo[i].name, true));
    return l;
}

GranelloProcessor::GranelloProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GRANELLO3", createLayout())
{
    for (int i = 0; i < gNum; ++i)
        raw[(size_t) i] = apvts.getRawParameterValue (kG[i]);

    for (size_t i = 0; i < window.size(); ++i)
        window[i] = 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) (window.size() - 1));

    resetSteps();

    // audio inicial para que suene sin cargar nada
    auto d = std::make_shared<SampleData>();
    d->sampleRate = 44100.0;
    const int n = 44100 * 4;
    d->buffer.setSize (2, n);
    Random r (1234);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* x = d->buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            const double t = i / 44100.0;
            double v = 0.0;
            for (int h = 1; h <= 10; ++h)
                v += std::sin (MathConstants<double>::twoPi * 110.0 * h * t * (1.0 + 0.0015 * ch * h) + h) / h
                     * (0.6 + 0.4 * std::sin (MathConstants<double>::twoPi * (0.2 + 0.07 * h) * t));
            v += (r.nextFloat() - 0.5f) * 0.05;
            x[i] = (float) (v * 0.25);
        }
    }
    sample = d;
    sampleName = "init pad";
}

void GranelloProcessor::resetSteps()
{
    for (int s = 0; s < kMaxSteps; ++s)
    {
        for (int p = 0; p < kNumStepParams; ++p)
            setStep (s, p, (float) kStepInfo[p].def);
        setStep (s, pPosition, (float) s / 16.f);
        setStepOn (s, true);
    }
}

void GranelloProcessor::applyPreset (int i)
{
    i = jlimit (0, kNumPresets - 1, i);
    const auto& d = kPresets[i];
    currentPreset = i;

    int perm[16];
    for (int k = 0; k < 16; ++k) perm[k] = k;
    Random pr (i * 17 + 3);
    for (int k = 15; k > 0; --k) std::swap (perm[k], perm[pr.nextInt (k + 1)]);

    for (int s = 0; s < kMaxSteps; ++s)
    {
        float pos = (float) s / 16.f;
        switch (d.pattern)
        {
            case 1: pos = (float) (15 - s) / 16.f; break;
            case 2: pos = (float) perm[s] / 16.f; break;
            case 3: pos = ((s % 2) ? 0.62f : 0.08f) + 0.01f * (float) s; break;
            case 4: pos = 0.3f + 0.045f * (float) s; break;
            default: break;
        }
        setStep (s, pSpace, d.space);
        setStep (s, pChaos, d.chaos);
        setStep (s, pLength, d.len);
        setStep (s, pGrains, d.grains);
        setStep (s, pPosition, jlimit (0.f, 1.f, pos));
        setStep (s, pTune, d.tune);
        setStep (s, pOutput, 0.f);
        setStepOn (s, d.pattern == 3 ? (s % 2 == 0) : true);
    }

    auto setP = [this] (const char* id, float v)
    {
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    setP ("rate", (float) d.rate);
    setP ("steps", (float) d.steps16);
    setP ("mode", 1.f);
    setP ("attack", d.atk);
    setP ("decay", d.dec);
    setP ("sustain", d.sus);
    setP ("release", d.rel);
    setP ("sync", 1.f);
}

void GranelloProcessor::editStep (int p, float v, bool linked)
{
    const int sel = getSelectedStep();
    const float lo = (float) kStepInfo[p].lo, hi = (float) kStepInfo[p].hi;
    const float delta = v - getStep (sel, p);
    if (linked)
        for (int s = 0; s < kMaxSteps; ++s)
            setStep (s, p, jlimit (lo, hi, getStep (s, p) + delta));
    else
        setStep (sel, p, jlimit (lo, hi, v));
}

GranelloProcessor::Snapshot GranelloProcessor::takeSnapshot() const
{
    Snapshot sn {};
    size_t k = 0;
    for (int s = 0; s < kMaxSteps; ++s)
    {
        for (int p = 0; p < kNumStepParams; ++p) sn[k++] = getStep (s, p);
        sn[k++] = isStepOn (s) ? 1.f : 0.f;
    }
    return sn;
}

void GranelloProcessor::restoreSnapshot (const Snapshot& sn)
{
    size_t k = 0;
    for (int s = 0; s < kMaxSteps; ++s)
    {
        for (int p = 0; p < kNumStepParams; ++p) setStep (s, p, sn[k++]);
        setStepOn (s, sn[k++] > 0.5f);
    }
}

void GranelloProcessor::pushUndo()
{
    undoStack.push_back (takeSnapshot());
    if (undoStack.size() > 64) undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void GranelloProcessor::undo()
{
    if (undoStack.empty()) return;
    redoStack.push_back (takeSnapshot());
    restoreSnapshot (undoStack.back());
    undoStack.pop_back();
}

void GranelloProcessor::redo()
{
    if (redoStack.empty()) return;
    undoStack.push_back (takeSnapshot());
    restoreSnapshot (redoStack.back());
    redoStack.pop_back();
}

bool GranelloProcessor::loadSample (const File& f)
{
    AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0)
        return false;

    const int len = (int) jmin<int64> (reader->lengthInSamples, (int64) (reader->sampleRate * 600.0));
    auto d = std::make_shared<SampleData>();
    d->buffer.setSize (jmin (2, (int) reader->numChannels), len);
    reader->read (&d->buffer, 0, len, 0, true, true);
    d->sampleRate = reader->sampleRate;

    retired = std::atomic_exchange (&sample, d);
    sampleName = f.getFileName();
    samplePath = f.getFullPathName();
    return true;
}

void GranelloProcessor::prepareToPlay (double sr, int block)
{
    hostSR = sr;
    dsp::ProcessSpec spec { sr, (uint32) jmax (1, block), 2 };
    hp.prepare (spec); lp.prepare (spec);
    hp.setType (dsp::StateVariableTPTFilterType::highpass);
    lp.setType (dsp::StateVariableTPTFilterType::lowpass);
    hp.reset(); lp.reset();
    reverb.setSampleRate (sr);
    reverb.reset();
    for (auto& g : grains) g = Grain();
    stage = 0; env = 0.f; prevGate = false; keyGate = false; lastSC = LLONG_MIN; timer = 0.0;
}

bool GranelloProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

double GranelloProcessor::spawnGrain (const SampleData& s, int cur, const Ctx& c, bool sync)
{
    const float chaos = getStep (cur, pChaos);
    const float lenMs = getStep (cur, pLength);
    const float gr = getStep (cur, pGrains);
    const float position = getStep (cur, pPosition);
    const float tune = getStep (cur, pTune);

    const double bpm = c.bpm > 20.0 ? c.bpm : 120.0;
    double iv;
    if (sync)
    {
        static const float beats[10] = { 4.f, 2.f, 1.f, 0.5f, 1.f / 3.f, 0.25f, 1.f / 6.f, 0.125f, 1.f / 12.f, 1.f / 16.f };
        const int di = jlimit (0, 9, (int) std::lround (gr * 9.f));
        iv = (double) beats[di] * 60.0 / bpm * hostSR;
    }
    else
        iv = hostSR / std::pow (200.0, (double) gr);
    iv = jmax (16.0, iv);

    const int N = s.buffer.getNumSamples();
    for (auto& g : grains)
    {
        if (g.active) continue;
        float rp = position + (rng.nextFloat() - 0.5f) * chaos * 0.2f;
        rp -= std::floor (rp);
        const float semis = tune + (rng.nextFloat() * 2.f - 1.f) * chaos * 3.f;
        const double ratio = std::pow (2.0, semis / 12.0) * s.sampleRate / hostSR;
        const bool rev = rng.nextFloat() < chaos * 0.3f;

        g.len = jmax (16, (int) (lenMs * 0.001 * hostSR));
        g.inc = rev ? -ratio : ratio;
        g.pos = (double) rp * N;
        g.age = 0;
        g.active = true;

        const float pan = 0.5f + (rng.nextFloat() - 0.5f) * (0.2f + 0.8f * chaos);
        const float overlap = (float) ((double) g.len / iv);
        const float norm = 1.f / std::sqrt (jmax (1.f, overlap));
        g.gl = std::cos (pan * MathConstants<float>::halfPi) * norm;
        g.gr = std::sin (pan * MathConstants<float>::halfPi) * norm;
        break;
    }
    return jmax (16.0, iv * (1.0 + (double) (rng.nextFloat() - 0.5f) * chaos));
}

void GranelloProcessor::renderRange (AudioBuffer<float>& out, int start, int num, const SampleData* s,
                                     const Ctx& c, const BlockP& p)
{
    if (num <= 0) return;

    const int N = s != nullptr ? s->buffer.getNumSamples() : 0;
    const float* L = N > 0 ? s->buffer.getReadPointer (0) : nullptr;
    const float* R = N > 0 ? (s->buffer.getNumChannels() > 1 ? s->buffer.getReadPointer (1) : L) : nullptr;

    const double bpm = c.bpm > 20.0 ? c.bpm : 120.0;
    const double ppqInc = bpm / 60.0 / hostSR;
    auto* oL = out.getWritePointer (0);
    auto* oR = out.getWritePointer (1);

    for (int i = 0; i < num; ++i)
    {
        const int idx = start + i;
        bool gate = false, newStep = false;
        int cur = curStep;

        if (p.mode == 2)
        {
            gate = keyGate;
            cur = keyStep;
            if (keyRetrig) { newStep = true; keyRetrig = false; }
        }
        else if (c.playing)
        {
            const double ppqNow = c.ppq + (double) idx * ppqInc;
            const long long sc = (long long) std::floor (ppqNow / p.stepLen);
            if (sc != lastSC) { lastSC = sc; newStep = true; }
            cur = p.mode == 0 ? getSelectedStep()
                              : (int) (((sc % p.nSteps) + p.nSteps) % p.nSteps);
            gate = isStepOn (cur);
        }
        else
            lastSC = LLONG_MIN;
        curStep = cur;

        if (gate && (! prevGate || newStep))
        {
            stage = 1;
            timer = 0.0;
        }
        else if (! gate && prevGate)
        {
            if (env > 0.f) { stage = 4; relRate = env / p.relS; }
            else stage = 0;
        }
        prevGate = gate;

        switch (stage)
        {
            case 1: env += 1.f / p.atkS; if (env >= 1.f) { env = 1.f; stage = 2; } break;
            case 2: env -= (1.f - p.sus) / p.decS; if (env <= p.sus) { env = p.sus; stage = 3; } break;
            case 3: env = p.sus; break;
            case 4: env -= relRate; if (env <= 0.f) { env = 0.f; stage = 0; } break;
            default: break;
        }

        float l = 0.f, r = 0.f;
        if (N > 0)
        {
            if (stage != 0 && stage != 4)
            {
                timer -= 1.0;
                if (timer <= 0.0) timer += spawnGrain (*s, cur, c, p.sync);
            }

            for (auto& g : grains)
            {
                if (! g.active) continue;
                int i0 = (int) std::floor (g.pos);
                const float fr = (float) (g.pos - (double) i0);
                i0 = ((i0 % N) + N) % N;
                const int i1 = (i0 + 1) % N;
                const float w = window[(size_t) ((g.age * 511) / g.len)];
                l += (L[i0] + fr * (L[i1] - L[i0])) * w * g.gl;
                r += (R[i0] + fr * (R[i1] - R[i0])) * w * g.gr;
                g.pos += g.inc;
                if (++g.age >= g.len) g.active = false;
            }
        }

        outGain += (outT[(size_t) cur] - outGain) * 0.002f;
        const float e = env * env * outGain;
        oL[idx] = std::tanh (l * e);
        oR[idx] = std::tanh (r * e);
    }
}

void GranelloProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    buffer.clear();
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2) return;

    auto smp = std::atomic_load (&sample);

    BlockP p;
    p.sync = raw[gSync]->load() > 0.5f;
    p.mode = (int) raw[gMode]->load();
    p.nSteps = getNumActiveSteps();
    p.stepLen = (double) kRateBeats[jlimit (0, 5, (int) raw[gRate]->load())];
    p.atkS = jmax (1.f, raw[gAttack]->load() * 0.001f * (float) hostSR);
    p.decS = jmax (1.f, raw[gDecay]->load() * 0.001f * (float) hostSR);
    p.sus = raw[gSustain]->load();
    p.relS = jmax (1.f, raw[gRelease]->load() * 0.001f * (float) hostSR);
    if (p.mode != 2) keyGate = false;

    Ctx c;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) c.bpm = *b;
            if (auto q = pos->getPpqPosition()) { c.ppq = *q; c.playing = pos->getIsPlaying(); }
        }
    hostPlaying.store (c.playing);

    for (int i = 0; i < kMaxSteps; ++i)
        outT[(size_t) i] = Decibels::decibelsToGain (getStep (i, pOutput));

    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = jlimit (0, n, meta.samplePosition);
        renderRange (buffer, pos, t - pos, smp.get(), c, p);
        pos = t;
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            if (p.mode == 2)
            {
                keyStep = ((m.getNoteNumber() - 60) % 16 + 16) % 16;
                keyNote = m.getNoteNumber();
                keyGate = true;
                keyRetrig = true;
            }
        }
        else if (m.isNoteOff())
        {
            if (m.getNoteNumber() == keyNote) { keyGate = false; keyNote = -1; }
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            keyGate = false; keyNote = -1;
        }
    }
    renderRange (buffer, pos, n - pos, smp.get(), c, p);

    // filtro (paso alto + paso bajo con resonancia común)
    const float hpf = raw[gHP]->load();
    const float lpf = jmin (raw[gLP]->load(), (float) hostSR * 0.45f);
    const float q = raw[gReso]->load();
    const bool useHP = hpf > 25.f, useLP = lpf < 19000.f;
    if (useHP || useLP)
    {
        hp.setCutoffFrequency (hpf); lp.setCutoffFrequency (lpf);
        hp.setResonance (q); lp.setResonance (q);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
            {
                float x = d[i];
                if (useHP) x = hp.processSample (ch, x);
                if (useLP) x = lp.processSample (ch, x);
                d[i] = x;
            }
        }
    }

    // space (reverb) según el paso actual
    const float space = getStep (curStep, pSpace);
    Reverb::Parameters rp;
    rp.roomSize = 0.75f; rp.damping = 0.45f; rp.width = 1.f; rp.freezeMode = 0.f;
    rp.wetLevel = space * 0.55f;
    rp.dryLevel = 1.f - space * 0.35f;
    reverb.setParameters (rp);
    reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), n);

    level.store (buffer.getMagnitude (0, n));
    playingStep.store ((stage != 0 && (c.playing || p.mode == 2)) ? curStep : -1);
}

void GranelloProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("samplePath", samplePath, nullptr);
    state.setProperty ("preset", currentPreset, nullptr);

    StringArray rows;
    for (int s = 0; s < kMaxSteps; ++s)
    {
        StringArray f;
        for (int p = 0; p < kNumStepParams; ++p) f.add (String (getStep (s, p), 5));
        f.add (isStepOn (s) ? "1" : "0");
        rows.add (f.joinIntoString (","));
    }
    state.setProperty ("steps", rows.joinIntoString (";"), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void GranelloProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    auto vt = ValueTree::fromXml (*xml);
    apvts.replaceState (vt);
    currentPreset = jlimit (0, kNumPresets - 1, (int) vt.getProperty ("preset", 0));

    const auto rows = StringArray::fromTokens (vt.getProperty ("steps").toString(), ";", "");
    for (int s = 0; s < jmin (kMaxSteps, rows.size()); ++s)
    {
        const auto f = StringArray::fromTokens (rows[s], ",", "");
        if (f.size() >= kNumStepParams + 1)
        {
            for (int p = 0; p < kNumStepParams; ++p) setStep (s, p, f[p].getFloatValue());
            setStepOn (s, f[kNumStepParams].getIntValue() != 0);
        }
    }

    const auto path = vt.getProperty ("samplePath").toString();
    if (path.isNotEmpty() && File (path).existsAsFile())
        loadSample (File (path));
}

AudioProcessorEditor* GranelloProcessor::createEditor() { return new GranelloEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GranelloProcessor(); }
