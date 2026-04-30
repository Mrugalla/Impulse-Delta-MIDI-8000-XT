#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr auto asciiRamp = " .`'^\",:;Il!i~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";
    constexpr int asciiRampLength = static_cast<int>(sizeof(asciiRamp) - 1);

    static float luma709(const juce::Colour& c)
    {
        return 0.2126f * c.getFloatRed() + 0.7152f * c.getFloatGreen() + 0.0722f * c.getFloatBlue();
    }
}


ImpulseDeltaMIDI8000XTAudioProcessorEditor::ImpulseDeltaMIDI8000XTAudioProcessorEditor(ImpulseDeltaMIDI8000XTAudioProcessor& p) :
    AudioProcessorEditor(&p),
    audioProcessor(p),
    featherHandCursor(),
    startedExternalDrag(false)
{
    auto cursorImage = juce::ImageCache::getFromMemory(BinaryData::Cursor_PNG, BinaryData::Cursor_PNGSize);
    if (cursorImage.isNull())
    {
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }
    else
    {
        constexpr auto targetSize = 48;
        if (cursorImage.getWidth() != targetSize || cursorImage.getHeight() != targetSize)
            cursorImage = cursorImage.rescaled(targetSize, targetSize, juce::Graphics::lowResamplingQuality);
        const int hotspotX = juce::jlimit(0, cursorImage.getWidth() - 1, 22);
        const int hotspotY = juce::jlimit(0, cursorImage.getHeight() - 1, 28);
        featherHandCursor = juce::MouseCursor(cursorImage, hotspotX, hotspotY);
        setMouseCursor(featherHandCursor);
    }
    
    swipeTypeface = juce::Typeface::createSystemTypefaceFor(BinaryData::SwipeRaceDemo_ttf, BinaryData::SwipeRaceDemo_ttfSize);
    if (swipeTypeface != nullptr)
    {
        swipeFont = juce::Font(swipeTypeface);
        swipeFont.setHeight(titleHeightPx);
    }

    lastPaintTimeMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz(60);

    setOpaque(true);
    setSize(400, 400);
}

ImpulseDeltaMIDI8000XTAudioProcessorEditor::~ImpulseDeltaMIDI8000XTAudioProcessorEditor()
{
    stopTimer();
}

void ImpulseDeltaMIDI8000XTAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    const int w = bounds.getWidth();
    const int h = bounds.getHeight();

    if (w <= 0 || h <= 0)
        return;

    if (sceneImage.isNull() || sceneImage.getWidth() != w || sceneImage.getHeight() != h)
        sceneImage = juce::Image(juce::Image::ARGB, w, h, true);

    // Render the scene to an offscreen buffer, then apply a glitchy post-process.
    {
        juce::Graphics sg(sceneImage);
        sg.fillAll(juce::Colours::black);

        for (const auto& p : particles)
        {
            const auto t = juce::jlimit(0.0f, 1.0f, p.ageSeconds / juce::jmax(0.001f, p.lifeSeconds));
            const auto fadeIn = juce::jlimit(0.0f, 1.0f, t / 0.12f);
            const auto fadeOut = 1.0f - t;
            const auto alpha = juce::jlimit(0.0f, 1.0f, fadeIn * fadeOut);
            if (alpha <= 0.0f)
                continue;

            const float size = p.size * (p.isSmoke ? (1.0f + 1.6f * t) : (0.85f + 0.9f * t));
            const auto r = juce::Rectangle<float>(size, size).withCentre(p.position);

            if (p.isSmoke)
            {
                auto c0 = p.colour.withAlpha(0.20f * alpha);
                auto c1 = p.colour.withAlpha(0.0f);
                juce::ColourGradient grad(c0, r.getCentreX(), r.getCentreY(), c1, r.getX(), r.getBottom(), true);
                sg.setGradientFill(grad);
                sg.fillEllipse(r);
            }
            else
            {
                auto core = p.colour.withAlpha(0.75f * alpha);
                auto outer = p.colour.darker(0.6f).withAlpha(0.0f);
                juce::ColourGradient grad(core, r.getCentreX(), r.getCentreY(), outer, r.getCentreX(), r.getBottom(), true);
                sg.setGradientFill(grad);
                sg.fillEllipse(r);
            }

        // Title flash (only sometimes, brutalist skew)
        if (titleTimeRemainingSeconds > 0.0f && swipeTypeface != nullptr && titleText.isNotEmpty())
        {
            const auto a = juce::jlimit(0.0f, 1.0f, titleTimeRemainingSeconds / 0.30f);
            juce::Graphics::ScopedSaveState ss(sg);
            sg.addTransform(titleTransform);
            sg.setFont(swipeFont);
            sg.setColour(titleColour.withAlpha(0.9f * a));

            auto area = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h) * 0.35f)
                .reduced(16.0f, 8.0f);

            if (area.getWidth() > 0 && area.getHeight() > 0)
            {
                sg.drawFittedText(titleText, area.toNearestInt(), juce::Justification::topLeft, 2, 1.0f);

                // Harsh underline accent
                sg.setColour(juce::Colour(0xffffffff).withAlpha(0.20f * a));
                sg.fillRect(area.getX(), area.getY() + titleHeightPx * 0.95f, juce::jmin(area.getWidth(), 320.0f), 3.0f);
            }
        }
        }

        // Electricity (only during glitch)
        if (glitchTimeRemainingSeconds > 0.0f && lightningPoints.size() >= 2)
        {
            juce::Path bolt;
            bolt.startNewSubPath(lightningPoints.front());
            for (size_t i = 1; i < lightningPoints.size(); ++i)
                bolt.lineTo(lightningPoints[i]);

            auto a = juce::jlimit(0.0f, 1.0f, glitchTimeRemainingSeconds / 0.12f);
            sg.setColour(juce::Colour(0xffa6e7ff).withAlpha(0.85f * a));
            sg.strokePath(bolt, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            sg.setColour(juce::Colour(0xffffffff).withAlpha(0.65f * a));
            sg.strokePath(bolt, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // Post-process: occasional glitch strips (offset rows/columns)
    if (glitchTimeRemainingSeconds > 0.0f && !glitchStrips.empty())
    {
        juce::Image glitched(sceneImage.createCopy());
        {
            // IMPORTANT: ensure this Graphics is destroyed before calling paintAscii(),
            // which opens an Image::BitmapData on the same Image (JUCE asserts otherwise).
            juce::Graphics gg(glitched);

            for (const auto& s : glitchStrips)
            {
                if (s.thickness <= 0)
                    continue;

                if (s.vertical)
                {
                    const auto src = juce::Rectangle<int>(juce::jlimit(0, w - 1, s.start), 0, juce::jlimit(1, w, s.thickness), h)
                        .getIntersection(juce::Rectangle<int>(0, 0, w, h));
                    if (!src.isEmpty())
                        gg.drawImage(sceneImage, src.getX() + s.offset, 0, src.getWidth(), h,
                            src.getX(), 0, src.getWidth(), h);
                }
                else
                {
                    const auto src = juce::Rectangle<int>(0, juce::jlimit(0, h - 1, s.start), w, juce::jlimit(1, h, s.thickness))
                        .getIntersection(juce::Rectangle<int>(0, 0, w, h));
                    if (!src.isEmpty())
                        gg.drawImage(sceneImage, 0, src.getY() + s.offset, w, src.getHeight(),
                            0, src.getY(), w, src.getHeight());
                }
            }

            // Subtle scanline tint
            gg.setColour(juce::Colour(0xff5bd0ff).withAlpha(0.06f));
            for (int y = 0; y < h; y += 3)
                gg.fillRect(0, y, w, 1);
        }

        paintAscii(g, glitched);
        return;
    }

    paintAscii(g, sceneImage);
}

void ImpulseDeltaMIDI8000XTAudioProcessorEditor::paintAscii(juce::Graphics& g, const juce::Image& srcImage)
{
    const auto bounds = getLocalBounds();
    const int w = bounds.getWidth();
    const int h = bounds.getHeight();

    if (w <= 0 || h <= 0 || srcImage.isNull())
        return;

    // Target a fixed-width grid so the effect is stable across resize.
    constexpr int targetCols = 120;
    const int cols = juce::jlimit(16, 240, targetCols);

    // Assume typical monospace aspect ratio: character height ~ 2x width.
    const float cellW = static_cast<float>(w) / static_cast<float>(cols);
    const float cellH = cellW * 2.0f;
    const int rows = juce::jlimit(8, 200, static_cast<int>(std::floor(static_cast<float>(h) / juce::jmax(1.0f, cellH))));

    if (rows <= 0)
        return;

    const float usedH = static_cast<float>(rows) * cellH;
    const float yOffset = 0.5f * (static_cast<float>(h) - usedH);

    const float sampleW = static_cast<float>(srcImage.getWidth()) / static_cast<float>(cols);
    const float sampleH = static_cast<float>(srcImage.getHeight()) / static_cast<float>(rows);

    juce::Font font;
    font = font.withHeight(cellH * 0.95f);
    font.setTypefaceName("Consolas");
    font.setExtraKerningFactor(0.0f);
    g.setFont(font);

    // Background black so any gaps between glyphs don't show through.
    g.fillAll(juce::Colours::black);

    juce::Image::BitmapData bd(srcImage, juce::Image::BitmapData::readOnly);

    for (int row = 0; row < rows; ++row)
    {
        const int sy = juce::jlimit(0, bd.height - 1, static_cast<int>((row + 0.5f) * sampleH));
        const float y = yOffset + static_cast<float>(row) * cellH;

        for (int col = 0; col < cols; ++col)
        {
            const int sx = juce::jlimit(0, bd.width - 1, static_cast<int>((col + 0.5f) * sampleW));
            const auto c = bd.getPixelColour(sx, sy);
            const float lum = juce::jlimit(0.0f, 1.0f, luma709(c));

            // Choose glyph by luminance (dark -> dense glyph).
            const int idx = juce::jlimit(0, asciiRampLength - 1,
                static_cast<int>(std::round((1.0f - lum) * static_cast<float>(asciiRampLength - 1))));

            const char ch = asciiRamp[idx];
            if (ch == ' ')
                continue;

            // Monochrome ASCII for a cleaner "shader" look.
            g.setColour(juce::Colours::white);
            g.drawSingleLineText(juce::String::charToString(static_cast<juce::juce_wchar>(ch)),
                static_cast<int>(std::floor(static_cast<float>(col) * cellW)),
                static_cast<int>(std::floor(y + cellH * 0.85f)));
        }
    }
}

void ImpulseDeltaMIDI8000XTAudioProcessorEditor::mouseDown(const juce::MouseEvent&)
{
    startedExternalDrag = false;
}

void ImpulseDeltaMIDI8000XTAudioProcessorEditor::mouseUp(const juce::MouseEvent&)
{
    startedExternalDrag = false;
}

void ImpulseDeltaMIDI8000XTAudioProcessorEditor::mouseMove(const juce::MouseEvent& e)
{
    lastMousePos = e.position;
    hasMousePos = true;
}

void ImpulseDeltaMIDI8000XTAudioProcessorEditor::mouseDrag(const juce::MouseEvent& e)
{
    lastMousePos = e.position;
    hasMousePos = true;

    if (startedExternalDrag)
        return;

    if (!e.mouseWasDraggedSinceMouseDown())
        return;

    startedExternalDrag = true;

    auto f = createImpulseFile();
    if (!f.existsAsFile())
        return;

    juce::StringArray files;
    files.add(f.getFullPathName());

    juce::DragAndDropContainer::performExternalDragDropOfFiles(files, false, this);
}

void ImpulseDeltaMIDI8000XTAudioProcessorEditor::timerCallback()
{
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();
    const auto dt = static_cast<float>((nowMs - lastPaintTimeMs) / 1000.0);
    lastPaintTimeMs = nowMs;

    // Clamp to keep things stable if the UI thread stalls.
    const float dtClamped = juce::jlimit(0.0f, 0.05f, dt);

    stepParticles(dtClamped);
    emitFlames(10);
    if (hasMousePos)
        emitSmoke(3);

    if (glitchTimeRemainingSeconds > 0.0f)
        glitchTimeRemainingSeconds = juce::jmax(0.0f, glitchTimeRemainingSeconds - dtClamped);
    else if (rng.nextFloat() < 0.045f)
        triggerGlitch();

    if (titleTimeRemainingSeconds > 0.0f)
        titleTimeRemainingSeconds = juce::jmax(0.0f, titleTimeRemainingSeconds - dtClamped);

    repaint();
}

void ImpulseDeltaMIDI8000XTAudioProcessorEditor::stepParticles(float dtSeconds)
{
    for (auto& p : particles)
    {
        p.ageSeconds += dtSeconds;
        if (p.isSmoke)
        {
            // Slowly drift upwards and spread.
            p.velocity.x += (rng.nextFloat() - 0.5f) * 14.0f * dtSeconds;
            p.velocity.y -= 10.0f * dtSeconds;
            p.velocity *= (1.0f - 0.55f * dtSeconds);
        }
        else
        {
            // Flame buoyancy + jitter.
            p.velocity.x += (rng.nextFloat() - 0.5f) * 60.0f * dtSeconds;
            p.velocity.y -= 50.0f * dtSeconds;
            p.velocity *= (1.0f - 0.25f * dtSeconds);
        }

        p.position += p.velocity * dtSeconds;
    }

    particles.erase(std::remove_if(particles.begin(), particles.end(), [](const Particle& p)
    {
        return p.ageSeconds >= p.lifeSeconds;
    }), particles.end());
}

void ImpulseDeltaMIDI8000XTAudioProcessorEditor::emitFlames(int count)
{
    const auto b = getLocalBounds().toFloat();
    const auto originY = b.getBottom() - 8.0f;

    for (int i = 0; i < count; ++i)
    {
        Particle p;
        p.isSmoke = false;
        const float x = b.getX() + rng.nextFloat() * b.getWidth();
        p.position = juce::Point<float>(x, originY) + juce::Point<float>((rng.nextFloat() - 0.5f) * 10.0f, (rng.nextFloat() - 0.5f) * 4.0f);
        p.velocity = juce::Point<float>((rng.nextFloat() - 0.5f) * 40.0f, -(80.0f + rng.nextFloat() * 120.0f));
        p.lifeSeconds = 0.65f + rng.nextFloat() * 0.55f;
        p.ageSeconds = 0.0f;
        p.size = 18.0f + rng.nextFloat() * 22.0f;

        // Warm palette.
        const auto mix = rng.nextFloat();
        const auto cA = juce::Colour(0xffffd37a);
        const auto cB = juce::Colour(0xffff6a2b);
        p.colour = cA.interpolatedWith(cB, mix);
        particles.push_back(p);
    }

    constexpr size_t maxParticles = 1200;
    if (particles.size() > maxParticles)
        particles.erase(particles.begin(), particles.begin() + static_cast<std::ptrdiff_t>(particles.size() - maxParticles));
}

void ImpulseDeltaMIDI8000XTAudioProcessorEditor::triggerGlitch()
{
    // Short burst.
    glitchTimeRemainingSeconds = 0.08f + rng.nextFloat() * 0.10f;
    glitchStrips.clear();

    const auto b = getLocalBounds();
    const int w = b.getWidth();
    const int h = b.getHeight();
    if (w <= 0 || h <= 0)
        return;

    const int numStrips = 6 + rng.nextInt(8);
    for (int i = 0; i < numStrips; ++i)
    {
        GlitchStrip s;
        s.vertical = rng.nextBool();
        s.offset = (rng.nextInt(9) - 4) * (rng.nextBool() ? 1 : 2);
        if (s.vertical)
        {
            s.thickness = 3 + rng.nextInt(18);
            s.start = rng.nextInt(juce::jmax(1, w - 1));
        }
        else
        {
            s.thickness = 2 + rng.nextInt(14);
            s.start = rng.nextInt(juce::jmax(1, h - 1));
        }
        glitchStrips.push_back(s);
    }

    // New lightning bolt path.
    lightningPoints.clear();
    const float x0 = rng.nextFloat() * static_cast<float>(w);
    float x = x0;
    float y = 0.0f;
    lightningPoints.emplace_back(x, y);
    const int steps = 10 + rng.nextInt(8);
    for (int i = 0; i < steps; ++i)
    {
        y += (static_cast<float>(h) / static_cast<float>(steps)) * (0.9f + rng.nextFloat() * 0.25f);
        x += (rng.nextFloat() - 0.5f) * 40.0f;
        x = juce::jlimit(0.0f, static_cast<float>(w), x);
        lightningPoints.emplace_back(x, juce::jmin(static_cast<float>(h), y));
    }

    // Sometimes flash the title along with the glitch.
    if (rng.nextFloat() < 0.65f)
        triggerTitleFlash();
}

void ImpulseDeltaMIDI8000XTAudioProcessorEditor::triggerTitleFlash()
{
    if (swipeTypeface == nullptr)
        return;

    const juce::String fullName(JucePlugin_Name);
    juce::StringArray parts;
    parts.addTokens(fullName, " _-", "\"'");
    parts.removeEmptyStrings();

    juce::String candidate;
    if (parts.size() >= 2 && rng.nextFloat() < 0.70f)
    {
        const int start = rng.nextInt(parts.size());
        const int len = 1 + rng.nextInt(juce::jmin(3, parts.size()));
        for (int i = 0; i < len; ++i)
        {
            candidate << (i == 0 ? "" : " ") << parts[(start + i) % parts.size()];
        }
    }
    else
    {
        const int n = juce::jmin(juce::jmax(4, fullName.length() / 2), fullName.length());
        const int start = rng.nextInt(juce::jmax(1, fullName.length() - n + 1));
        candidate = fullName.substring(start, start + n);
    }

    // Brutalist styling: random casing + separators.
    if (rng.nextBool())
        candidate = candidate.toUpperCase();
    if (rng.nextFloat() < 0.35f)
        candidate = candidate.replaceCharacter(' ', rng.nextBool() ? '_' : '-');

    titleText = candidate;
    titleTimeRemainingSeconds = 0.20f + rng.nextFloat() * 0.35f;

    // Random transform: skew + slight rotation + jitter translation.
    const auto b = getLocalBounds().toFloat();
    const float tx = 10.0f + rng.nextFloat() * 20.0f;
    const float ty = 8.0f + rng.nextFloat() * 18.0f;
    const float angle = juce::degreesToRadians((rng.nextFloat() - 0.5f) * 10.0f);
    const float sx = 1.0f + (rng.nextFloat() - 0.5f) * 0.10f;
    const float sy = 1.0f + (rng.nextFloat() - 0.5f) * 0.12f;
    const float shx = (rng.nextFloat() - 0.5f) * 0.55f;
    const float shy = (rng.nextFloat() - 0.5f) * 0.08f;

    titleTransform = juce::AffineTransform::translation(tx, ty)
        .followedBy(juce::AffineTransform::rotation(angle, b.getCentreX() * 0.25f, b.getCentreY() * 0.10f))
        .followedBy(juce::AffineTransform::scale(sx, sy))
        .followedBy(juce::AffineTransform(1.0f, shx, 0.0f, shy, 1.0f, 0.0f));

    // High-contrast color pop.
    const juce::Colour palette[] =
    {
        juce::Colour(0xffffffff),
        juce::Colour(0xff5bd0ff),
        juce::Colour(0xffffd37a),
        juce::Colour(0xffff4fd8)
    };
    titleColour = palette[rng.nextInt(static_cast<int>(std::size(palette)))];

    swipeFont = juce::Font(swipeTypeface);
    const float h = titleHeightPx * (0.85f + rng.nextFloat() * 0.45f);
    swipeFont.setHeight(h);
    swipeFont.setHorizontalScale(0.86f + rng.nextFloat() * 0.14f);
}

void ImpulseDeltaMIDI8000XTAudioProcessorEditor::emitSmoke(int count)
{
    for (int i = 0; i < count; ++i)
    {
        Particle p;
        p.isSmoke = true;
        p.position = lastMousePos + juce::Point<float>((rng.nextFloat() - 0.5f) * 4.0f, (rng.nextFloat() - 0.5f) * 4.0f);
        p.velocity = juce::Point<float>((rng.nextFloat() - 0.5f) * 30.0f, -(10.0f + rng.nextFloat() * 35.0f));
        p.lifeSeconds = 1.2f + rng.nextFloat() * 0.9f;
        p.ageSeconds = 0.0f;
        p.size = 10.0f + rng.nextFloat() * 12.0f;
        p.colour = juce::Colour(0xffc6c6c6).interpolatedWith(juce::Colour(0xff6f6f6f), rng.nextFloat());
        particles.push_back(p);
    }
}

juce::File ImpulseDeltaMIDI8000XTAudioProcessorEditor::createImpulseFile() const
{
    constexpr auto lengthSeconds = 10.;
    constexpr auto sampleRate = 44100.;
    const auto numChannels = 2;
    const auto numSamples = static_cast<int>(lengthSeconds * sampleRate);
    juce::File folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile(ProjectInfo::projectName)
        .getChildFile("Exports");
    folder.createDirectory();
    const auto fileName = juce::String(JucePlugin_Name);
    juce::File outFile = folder.getNonexistentChildFile(fileName, ".flac", false);
    juce::FlacAudioFormat format;
    std::unique_ptr<juce::FileOutputStream> fileStream(outFile.createOutputStream());
    if (!fileStream)
        return {};
    if(!fileStream->openedOk())
		return {};
    auto stream = std::unique_ptr<juce::OutputStream>(fileStream.release());
    const auto options = juce::AudioFormatWriterOptions{}
        .withSampleRate(sampleRate)
        .withNumChannels(numChannels)
        .withBitsPerSample(16);
    auto writer = format.createWriterFor(stream, options);
    if (!writer)
        return {};
    juce::AudioBuffer<float> buffer(numChannels, numSamples);
    buffer.clear();
    for (int ch = 0; ch < numChannels; ++ch)
        buffer.setSample(ch, 0, 1.f);
    if (!writer->writeFromAudioSampleBuffer(buffer, 0, numSamples))
        return {};
    writer.reset();
    return outFile;
}