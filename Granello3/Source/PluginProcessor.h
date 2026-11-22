#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>
#include <vector>

struct SampleData
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;
};

// Los siete controles principales; cada paso (chop) guarda su propio valor de cada uno.
enum StepParam { pSpace, pChaos, pLength, pGrains, pPosition, pTune, pOutput, kNumStepParams };
struct StepParamInfo { const char* name; double lo, hi, def, skew; };
extern const StepParamInfo kStepInfo[kNumStepParams];
constexpr int kMaxSteps = 16;
constexpr int kNumPresets = 8;

class GranelloProcessor : public juce::AudioProcessor
{
public:
    GranelloProcessor();
    ~GranelloProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Granello 3"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return kNumPresets; }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int i) override { applyPreset (i); }
    const juce::String getProgramName (int i) override { return presetName (i); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // audio
    bool loadSample (const juce::File& file);
    std::shared_ptr<SampleData> getSample() const { return std::atomic_load (&sample); }
    juce::String getSampleName() const { return sampleName; }

    // pasos
    float getStep (int step, int p) const { return stepVals[(size_t) step][(size_t) p].load(); }
    void setStep (int step, int p, float v) { stepVals[(size_t) step][(size_t) p].store (v); }
    bool isStepOn (int step) const { return stepOn[(size_t) step].load(); }
    void setStepOn (int step, bool on) { stepOn[(size_t) step].store (on); }
    void editStep (int p, float newValue, bool linked);
    int getSelectedStep() const { return selected.load(); }
    void setSelectedStep (int s) { selected.store (juce::jlimit (0, kMaxSteps - 1, s)); }
    int getPlayingStep() const { return playingStep.load(); }
    bool isHostPlaying() const { return hostPlaying.load(); }
    float getLevel() const { return level.load(); }
    int getNumActiveSteps() const { return raw[gSteps]->load() > 0.5f ? 16 : 8; }
    void resetSteps();

    // deshacer / rehacer
    void pushUndo();
    void undo();
    void redo();

    // presets
    static juce::String presetName (int i);
    void applyPreset (int i);
    int currentPreset = 0;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    enum GParam { gSync, gMode, gSteps, gRate, gAttack, gDecay, gSustain, gRelease, gHP, gLP, gReso, gNum };

    struct Grain
    {
        bool active = false;
        double pos = 0.0, inc = 1.0;
        int age = 0, len = 1;
        float gl = 0.f, gr = 0.f;
    };
    struct Ctx { double ppq = 0, bpm = 120; bool playing = false; };
    struct BlockP
    {
        bool sync = true;
        int mode = 1, nSteps = 16;
        double stepLen = 0.5;
        float atkS = 1, decS = 1, sus = 1, relS = 1;
    };
    using Snapshot = std::array<float, kMaxSteps * (kNumStepParams + 1)>;

    Snapshot takeSnapshot() const;
    void restoreSnapshot (const Snapshot&);
    double spawnGrain (const SampleData& s, int cur, const Ctx& c, bool sync);
    void renderRange (juce::AudioBuffer<float>& out, int start, int num, const SampleData* s, const Ctx& c, const BlockP& p);

    std::shared_ptr<SampleData> sample, retired;
    juce::String sampleName, samplePath;

    std::array<std::atomic<float>*, gNum> raw {};
    std::array<std::array<std::atomic<float>, kNumStepParams>, kMaxSteps> stepVals;
    std::array<std::atomic<bool>, kMaxSteps> stepOn;
    std::atomic<int> selected { 0 }, playingStep { -1 };
    std::atomic<bool> hostPlaying { false };
    std::atomic<float> level { 0.f };

    std::vector<Snapshot> undoStack, redoStack;

    // motor
    std::array<Grain, 128> grains;
    std::array<float, 512> window {};
    std::array<float, kMaxSteps> outT {};
    long long lastSC = 0;
    bool prevGate = false, keyGate = false, keyRetrig = false;
    int keyStep = 0, keyNote = -1, curStep = 0, stage = 0;
    float env = 0.f, relRate = 0.f, outGain = 1.f;
    double timer = 0.0, hostSR = 44100.0;
    juce::Random rng;
    juce::dsp::StateVariableTPTFilter<float> hp, lp;
    juce::Reverb reverb;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranelloProcessor)
};
