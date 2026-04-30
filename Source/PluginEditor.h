#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <algorithm>
#include <vector>

struct ImpulseDeltaMIDI8000XTAudioProcessorEditor :
    public juce::AudioProcessorEditor,
    private juce::Timer
{
    ImpulseDeltaMIDI8000XTAudioProcessorEditor(ImpulseDeltaMIDI8000XTAudioProcessor&);
    ~ImpulseDeltaMIDI8000XTAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;

    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;

    juce::File createImpulseFile() const;

    ImpulseDeltaMIDI8000XTAudioProcessor& audioProcessor;
    juce::MouseCursor featherHandCursor;
    bool startedExternalDrag;

    struct Particle
    {
        juce::Point<float> position;
        juce::Point<float> velocity;
        float ageSeconds = 0.0f;
        float lifeSeconds = 1.0f;
        float size = 6.0f;
        juce::Colour colour;
        bool isSmoke = false;
    };

    struct GlitchStrip
    {
        bool vertical = false;
        int start = 0;
        int thickness = 0;
        int offset = 0;
    };

    void timerCallback() override;
    void stepParticles(float dtSeconds);
    void emitFlames(int count);
    void emitSmoke(int count);
    void triggerGlitch();
    void triggerTitleFlash();
    void paintAscii(juce::Graphics& g, const juce::Image& srcImage);

    std::vector<Particle> particles;
    juce::Image sceneImage;
    juce::Point<float> lastMousePos;
    bool hasMousePos = false;
    juce::Random rng;
    double lastPaintTimeMs = 0.0;
    float glitchTimeRemainingSeconds = 0.0f;
    std::vector<GlitchStrip> glitchStrips;
    std::vector<juce::Point<float>> lightningPoints;

    juce::Typeface::Ptr swipeTypeface;
    juce::Font swipeFont;
    float titleTimeRemainingSeconds = 0.0f;
    juce::String titleText;
    juce::AffineTransform titleTransform;
    float titleHeightPx = 72.0f;
    juce::Colour titleColour;
};
