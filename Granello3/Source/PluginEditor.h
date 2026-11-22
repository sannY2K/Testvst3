#pragma once
#include "PluginProcessor.h"

// Paleta única: ajusta aquí los colores de toda la interfaz.
namespace Theme
{
    const juce::Colour body         { 0xffd6d2c5 };
    const juce::Colour line         { 0xff8f8c82 };
    const juce::Colour ink          { 0xff6a675e };
    const juce::Colour knob         { 0xff1e1b18 };
    const juce::Colour cream        { 0xffe9e5d9 };
    const juce::Colour screen       { 0xff0b0b0d };
    const juce::Colour screenBorder { 0xff1a1a1d };
    const juce::Colour stepBtn      { 0xff1d1b19 };
    const juce::Colour stepSel      { 0xff8f8d87 };
    const juce::Colour orange       { 0xffff9a52 };
    const juce::Colour blue         { 0xff3b5bff };
    const juce::Colour salmon       { 0xffd9826a };
    const juce::Colour green        { 0xff6cc070 };
}

class GranelloLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GranelloLookAndFeel() = default;
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

class StepButton : public juce::TextButton
{
public:
    std::function<void()> onRight;
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onRight) onRight(); return; }
        juce::TextButton::mouseDown (e);
    }
};

class ScreenView : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    explicit ScreenView (GranelloProcessor& p) : proc (p) {}
    std::function<void (int)> onSelectStep;
    std::function<void (const juce::File&)> onFile;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragging = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragging = false; repaint(); }

private:
    GranelloProcessor& proc;
    std::shared_ptr<SampleData> cached;
    int cachedW = 0;
    std::vector<float> peaks;
    bool dragging = false;
};

class ParamView : public juce::Component
{
protected:
    explicit ParamView (GranelloProcessor& p) : proc (p) {}
    float get (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void set (const char* id, float v)
    {
        auto* p = proc.apvts.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (v));
    }
    void gesture (const char* id, bool begin)
    {
        auto* p = proc.apvts.getParameter (id);
        if (begin) p->beginChangeGesture(); else p->endChangeGesture();
    }
    GranelloProcessor& proc;
};

class EnvView : public ParamView
{
public:
    explicit EnvView (GranelloProcessor& p) : ParamView (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    juce::Rectangle<float> area() const;
    std::array<juce::Point<float>, 5> points() const;
    int drag = -1;
};

class FilterView : public ParamView
{
public:
    explicit FilterView (GranelloProcessor& p) : ParamView (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    juce::Rectangle<float> area() const;
    float freqToX (float f) const;
    float xToFreq (float x) const;
    int drag = -1;
};

class GranelloEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GranelloEditor (GranelloProcessor&);
    ~GranelloEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label name, value;
        juce::TextButton link;
    };

    void timerCallback() override;
    void loadFile (const juce::File& f);
    void chooseFile();
    void showLibraryMenu();
    void stepMenu (int step);
    void setChoice (const char* id, int idx);
    void setBool (const char* id, bool v);
    void refreshKnobs();
    juce::Rectangle<float> D (float x, float y, float w, float h) const;

    GranelloProcessor& proc;
    GranelloLookAndFeel laf;
    ScreenView screenView;
    EnvView envView;
    FilterView filterView;

    juce::TextButton libraryBtn { "library" }, presetBtn, presetPrev { "<" }, presetNext { ">" }, zoomBtn { "zoom" };
    std::array<juce::TextButton, 3> modeBtn;
    std::array<juce::TextButton, 2> stepsBtn;
    juce::TextButton ratePrev { "<" }, rateNext { ">" }, resetBtn { "reset" }, undoBtn { "undo" }, redoBtn { "redo" };
    juce::TextButton syncBtn { "sync" }, freeBtn { "free" };
    juce::Label rateLabel;
    std::array<StepButton, kMaxSteps> stepBtn;
    std::array<Knob, kNumStepParams> knobs;
    std::unique_ptr<juce::FileChooser> chooser;

    float sc = 1.f, meter = 0.f, clipboard[kNumStepParams] {};
    bool hasClip = false;
    int zoomIdx = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranelloEditor)
};
