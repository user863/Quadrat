// one-component editor
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Panel.h"
#include "Tour.h"
#include <set>
#include <map>

namespace layout
{
    constexpr float W = 1120.0f, H = 660.0f, M = 24.0f;
    constexpr float headerH = 64.0f;
    constexpr float paletteW = 210.0f;
    constexpr float cell = 78.0f;        // square @ zoom 1
}

class QuadratEditor : public juce::AudioProcessorEditor,
                      public juce::FileDragAndDropTarget,
                      private juce::Timer
{
public:
    explicit QuadratEditor (QuadratProcessor&);
    ~QuadratEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float) override;
    bool keyPressed (const juce::KeyPress&) override;

    // .mid files: drop onto a midi block, or anywhere for a new one
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int x, int y) override { fileHover = { (float) x, (float) y }; repaint(); }
    void fileDragMove (const juce::StringArray&, int x, int y) override { fileHover = { (float) x, (float) y }; repaint(); }
    void fileDragExit (const juce::StringArray&) override { fileHover = { -1.0f, -1.0f }; repaint(); }
    void filesDropped (const juce::StringArray&, int x, int y) override;
    bool loadMidiFile (const juce::File&, int targetId, juce::Point<float> at);   // public for tests

    // test hooks
    bool tourRunning() const { return touring; }
    int  tourAt() const { return guide.at(); }
    bool tourCardUp() const { return tourCard != TourCard::step; }
    bool pressTourButton (const juce::String& label);
    juce::Rectangle<float> tourLink() const;     // on the about page
    juce::Rectangle<float> githubLink() const;   // ditto
    void openAbout() { showAbout = true; dimChildren(); repaint(); }
    bool welcomeShowing() const { return welcoming; }

    // first-run flag
    static juce::File settingsOverride;          // tests only
    static bool welcomeSeen();
    static void markWelcomeSeen();

private:
    enum class Drag { none, fromPalette, moveBlock, cable, pan, knob, step, box, note, depth };

    void timerCallback() override;

    // ── geometry ────────────────────────────────────────────────────────────
    juce::Rectangle<float> fieldArea() const;
    juce::Rectangle<float> paletteArea() const;
    float cellSize() const;
    float subSize() const;
    juce::Point<float> cellToScreen (int cx, int cy) const;
    juce::Point<int>   screenToCell (juce::Point<float>) const;
    juce::Rectangle<float> tileRect (const qd::Block&) const;
    juce::Point<float> portPos (const qd::Block&, bool isInput, int index) const;
    int controlAt (const qd::Block&, juce::Rectangle<float>, juce::Point<float>) const;
    juce::Rectangle<float> controlRect (const qd::Block&, juce::Rectangle<float>, int index) const;
    juce::Rectangle<float> seqArea (juce::Rectangle<float> tile, const qd::Block&) const;
    juce::Rectangle<float> seqCell (juce::Rectangle<float> tile, const qd::Block&,
                                    int step, int lane) const;       // lane 2 = switch
    int stepAt (const qd::Block&, juce::Point<float>, int& lane) const;
    int playingStep (int id) const;
    void setStep (qd::Block&, int step, int lane, float y);
    juce::Rectangle<float> optionRect (const qd::Block&) const;

    // knob-input drawers
    juce::Rectangle<float> tabRect (juce::Rectangle<float>, const qd::Block&) const;
    float tabRotation (const qd::Block&, bool wired) const;
    juce::Rectangle<float> drawerTab (const qd::Block&) const;
    juce::Rectangle<float> drawerRect (const qd::Block&) const;
    juce::Rectangle<float> drawerCell (const qd::Block&, int knob) const;
    juce::Point<float> knobTarget (const qd::Block&, int knob) const;
    juce::Point<float> jackAt (const qd::Block&, int knob, float& radius) const;
    juce::Point<float> dragOffsetFor (const qd::Block&) const;
    int drawerJackAt (const qd::Block&, juce::Point<float>) const;
    int cableToKnob (int blockId, int knob) const;      // cable idx or -1
    void drawDrawer (juce::Graphics&, const qd::Block&);
    void setDrawer (int id, bool open);
    std::map<int, float> tabAnim;        // tab angle, animated
    juce::Rectangle<float> optionStrip (juce::Rectangle<float>, const qd::Block&) const;
    void drawOptionStrip (juce::Graphics&, juce::Rectangle<float>, const qd::Block&);
    static juce::Colour colourFor (qd::Sig);
    static bool hasScope (qd::Kind);
    void drawScope (juce::Graphics&, juce::Rectangle<float>, const float* data, juce::Colour);
    void drawModelName (juce::Graphics&, juce::Rectangle<float>, const qd::Block&);
    void drawArena (juce::Graphics&, juce::Rectangle<float>, const qd::Block&);

    // midi block face
    void drawPhrase (juce::Graphics&, juce::Rectangle<float>, const qd::Block&);
    bool pasteNotes (int targetId);          // false: no notes
    bool placePhrase (std::vector<qd::MidiNote> phrase, int loop, int targetId, juce::Point<float> at);
    void chooseMidiFile (int targetId);
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Point<float> fileHover { -1.0f, -1.0f };     // a .mid being dragged over us
    juce::Rectangle<float> faceArea (juce::Rectangle<float> tile, const qd::Block&) const;

    // header preset menu (FL et al. don't list ours)
    juce::Rectangle<float> presetsRect() const;
    void showPresetMenu();

    // auto-zoom per block
    void drawInput (juce::Graphics&, juce::Rectangle<float>, const qd::Block&, float level);
    std::map<int, float> inputZoom;
    bool inputLive = false;                  // sidechain live?

    // ball + trail
    struct BallView
    {
        float x = 0.0f, y = 0.0f, floorLit = 0.0f, wallLit = 0.0f;
        std::vector<juce::Point<float>> trail;
    };

    std::map<int, BallView> balls;
    const BallView& ballState (int id) const;
    void drawMeterFace (juce::Graphics&, juce::Rectangle<float>, float value);
    void drawReduction (juce::Graphics&, juce::Rectangle<float>, const float* data, float now);
    void drawClockFace (juce::Graphics&, juce::Rectangle<float>, float phase01, float width01,
                        int marks, float tempo);
    // notes on the field
    int noteAt (juce::Point<float>) const;
    juce::Rectangle<float> noteRect (const qd::Note&) const;
    void drawNotes (juce::Graphics&);
    void commitNote();
    int editingNote = -1;
    int draggingNote = -1;

    void say (const juce::String&);      // header flash
    juce::String notice;
    double noticeUntil = 0.0;

    std::vector<double> taps;            // tap tempo
    double lastTap = 0.0;
    void drawResponse (juce::Graphics&, juce::Rectangle<float>, const qd::Block&);
    float clockPhase (int id) const;
    float faceReading (int id) const;
    void zoomToFit();
    void zoomToFitBounds (int x0, int y0, int x1, int y1);   // in sub-cells
    int paletteIndexOf (qd::Kind) const;
    int paletteItemAt (juce::Point<float>) const;
    juce::Rectangle<float> paletteItemRect (int index) const;
    void filterPalette();
    const qd::Block* viewBlock (int id) const;
    const float* scopeFor (int id) const;
    float paletteContentHeight() const;
    juce::Rectangle<float> scrollTrack() const;
    juce::Rectangle<float> scrollThumb() const;
    void scrollPalette (float delta);
    void drawTooltip (juce::Graphics&);
    void drawAbout (juce::Graphics&);
    juce::Rectangle<float> wordmarkRect() const;
    bool showAbout = false;

    // ── drawing ─────────────────────────────────────────────────────────────
    void drawPalette (juce::Graphics&);
    void drawField (juce::Graphics&);
    void drawTile (juce::Graphics&, const qd::Block&, juce::Rectangle<float>, float meterValue, bool selected);
    void drawCables (juce::Graphics&);

    QuadratProcessor& proc;
    PanelLNF lnf;

    CapButton clearCap { "clear", false }, rollCap { "roll", false },
              startCap { "start", false, true }, stopCap { "stop", true, true };
    juce::TextEditor search;

    // view copy, brief lock
    std::vector<qd::Block> viewBlocks;
    std::vector<qd::Cable> viewCables;
    std::vector<qd::Cable> viewTouching;  // adjacency wires
    std::vector<qd::Note> viewNotes;
    std::vector<float> viewMeters;
    std::vector<std::array<float, qd::Patch::kScope>> viewScopes;
    std::vector<int> visibleItems;
    std::vector<int> viewSteps;          // seq steps
    std::vector<float> viewPhase;        // clock phases
    std::vector<float> faceReadings;         // face readings
    int framedProgram = -1;               // last framed program

    // undo = full snapshots
    struct Snapshot
    {
        std::vector<qd::Block> blocks;
        std::vector<qd::Cable> cables;
        std::vector<qd::Note> notes;
        juce::String title;              // roll names undo too
    };
    std::vector<Snapshot> undoStack, redoStack;

    // block clipboard
    Snapshot clipboard;
    void copySelection (bool cut);
    void pasteAt (juce::Point<int> cell);
    void pushUndo();
    void restore (const Snapshot&);
    void undo();
    void redo();

    float palScroll = 0.0f;
    bool draggingScroll = false;
    float scrollGrab = 0.0f;

    // hover tooltip
    int hoverKind = -1;
    juce::Point<float> hoverAt;

    float zoom = 1.0f;
    float panX = 0.0f, panY = 0.0f;

    Drag drag = Drag::none;
    qd::Kind dragKind = qd::Kind::osc;
    int dragBlock = -1, dragKnob = -1, dragStep = -1, dragLane = 0;
    int cableFrom = -1, cableFromPort = 0;
    juce::Point<float> mousePos, dragOffset;
    float dragStartValue = 0.0f;
    int hoverBlock = -1;

    // multi-select
    std::set<int> selection;
    juce::Point<float> boxFrom;
    bool isSelected (int id) const { return selection.count (id) > 0; }

    // tour overlay (see Tour.h)
    enum class TourCard { step, chapter, leave, finished, welcome };

    struct Hints
    {
        enum class Move { none, drag, cable, updown, up, click, doubleClick };

        std::vector<juce::Rectangle<float>> spots;    // lit + ringed
        juce::Rectangle<float> outline;               // where something should go
        juce::String outlineText;
        Move move = Move::none;
        juce::Point<float> from, to, also;            // 2nd click target
        juce::Rectangle<float> carry;                 // drag ghost size
        juce::Colour colour { paint::ink };
        bool lens = false;                            // magnify tiny targets
    };

    struct CardButton { juce::Rectangle<float> r; juce::String label; int action; bool primary; bool checkbox = false; };

    struct CardLayout
    {
        juce::Rectangle<float> card;
        std::vector<CardButton> buttons;
        juce::String head, counter, main, why;
        float headY = 0.0f, dotsY = 0.0f, textY = 0.0f, whyY = 0.0f, mainSize = 12.0f;
    };

    template <typename Fn> auto withTour (Fn&& fn)
    {
        const juce::ScopedLock sl (proc.patchLock);
        const tour::Context c { proc.patch, proc.isMuted(), proc.patchTitle().toStdString(), editingNote >= 0 };
        return fn (c);
    }

    void startTour();
    void finishTour (bool keep);
    void updateTour();
    void onStepStart();
    void advanceTour();
    void onTourButton (int action);
    void frameTour();
    void showPaletteItem (qd::Kind);
    bool handleTourClick (juce::Point<float>);
    bool handleTourKey (const juce::KeyPress&);
    juce::Point<int> snapToTarget (qd::Kind, juce::Point<int> cell) const;
    const qd::Block* firstOfKind (qd::Kind) const;
    Hints hints() const;
    CardLayout cardLayout (const Hints&) const;
    juce::String tourLabel() const;
    void toggleTour();
    void drawAboutLinks (juce::Graphics&);
    void dimChildren();                          // fade components under overlays
    juce::Rectangle<float> aboutPanel() const;
    void drawHints (juce::Graphics&);
    void drawGesture (juce::Graphics&, const Hints&, double t, float magnified);
    void drawCard (juce::Graphics&);
    void drawConfetti (juce::Graphics&, juce::Rectangle<float> card);
    void celebrate();

    bool touring = false;
    bool welcoming = false;                      // first-run card up
    bool dontShowAgain = false;                  // welcome checkbox
    TourCard tourCard = TourCard::step;
    tour::Runner guide;
    Snapshot savedPatch;                          // user's patch
    std::vector<Snapshot> savedUndo, savedRedo;    // stashed history
    double stepDoneAt = 0.0;                     // step done at
    double stepStartedAt = 0.0;                     // step started at
    double confettiAt = 0.0;                    // confetti start

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuadratEditor)
};
