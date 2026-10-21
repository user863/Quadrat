// panel toolkit (Grainfield fork)
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <string>

namespace paint
{
    const juce::Colour plate     { 0xffd2d3d0 };
    const juce::Colour plateHi   { 0xffdddedb };
    const juce::Colour plateLo   { 0xffc6c7c4 };
    const juce::Colour engrave   { 0xff8d8f8b };
    const juce::Colour relief    { 0xffeaebe8 };
    const juce::Colour shelf     { 0xffe9eae7 };   // palette backdrop

    const juce::Colour knobTop   { 0xff3d3f40 };
    const juce::Colour knobBot   { 0xff2a2c2d };
    const juce::Colour knobRim   { 0xff1d1f20 };
    const juce::Colour index     { 0xfff4f4f1 };

    const juce::Colour capTop    { 0xffedeee9 };
    const juce::Colour capBot    { 0xffd2d3ce };
    const juce::Colour capDown   { 0xffcfd0cb };

    const juce::Colour chrome    { 0xffe8e9e6 };
    const juce::Colour chromeLo  { 0xffa9aba7 };

    const juce::Colour field     { 0xffc3c4c1 };   // field well
    const juce::Colour screen    { 0xff141416 };   // screen face
    const juce::Colour screenLn  { 0xff2b2b2e };   // screen lines
    const juce::Colour grid      { 0xffb7b8b5 };
    const juce::Colour tile      { 0xffdadbd8 };   // a block's own face
    const juce::Colour tileEdge  { 0xff8d8f8b };

    const juce::Colour ink       { 0xff2c2e2c };
    const juce::Colour inkSoft   { 0xff6d6f6b };
    const juce::Colour cable     { 0xff4a4c49 };
    const juce::Colour red       { 0xffc8362b };
    const juce::Colour green     { 0xff2f7d4f };

    // signal colours
    const juce::Colour sigAudio  { 0xff2f7d4f };   // green
    const juce::Colour sigPitch  { 0xff2d5d9e };   // blue
    const juce::Colour sigGate   { 0xffc8362b };   // red
    const juce::Colour sigCv     { 0xffc79a1e };   // yellow
}

void drawLabel (juce::Graphics&, const juce::String&, float x, float y,
                  float size, float tracking, juce::Justification, float width = 0.0f, bool bold = false);

// char* is UTF-8
void drawLabel (juce::Graphics&, const char* text, float x, float y,
                  float size, float tracking, juce::Justification, float width = 0.0f, bool bold = false);

inline void drawLabel (juce::Graphics& g, const std::string& text, float x, float y,
                         float size, float tracking = 0.0f,
                         juce::Justification just = juce::Justification::left,
                         float width = 0.0f, bool bold = false)
{
    drawLabel (g, text.c_str(), x, y, size, tracking, just, width, bold);
}

// knob, no Slider
// pointer tint = mod source
void drawKnob (juce::Graphics&, float cx, float cy, float r, float value01, bool withScale,
               juce::Colour indexColour = paint::index);

// engraved line
void engraveLine (juce::Graphics&, float x, float y, float w, float h);

class PanelLNF : public juce::LookAndFeel_V4
{
public:
    PanelLNF();
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    // rounded text fields
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
private:
    juce::Typeface::Ptr face;
};

class CapButton : public juce::Button
{
public:
    CapButton (const juce::String& label, bool latching, bool isRed = false);
    void paintButton (juce::Graphics&, bool over, bool down) override;
    void setCapRadius (float r) { capR = r; }
    void setCapColour (juce::Colour c) { cap = c; tinted = true; }
private:
    float capR = 11.0f;
    bool latch, tinted = false;
    juce::Colour cap;
};
