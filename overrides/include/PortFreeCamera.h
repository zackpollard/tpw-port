// The launcher's "Free camera" (the port's fix freecam): a camera that tilts freely, zooms further, flies, and takes a
// trackpad's swipes and pinches. Only in the port, and only with gPortFreeCamera (include/TPCamera.h) on; off, every line
// of the original camera runs unchanged. The input side is here, header only, so the host test (tests/host_free_camera.cpp)
// reaches it; the camera side is in HandleCamera (src/game/TPCamera.cpp), the pointer's side in TPMain.cpp and the run
// loop's in platform/gamework.cpp.
#pragma once
#include <cmath>
#include <cstdint>

#include "TPCamera.h"
#include "TPMain.h"
#include "keyboard.h"

// TPW_PORT_FIX freecam settings | Mac: (none) | Port: the launcher's Free camera sub-settings, set before the game starts (launcher/main.cpp); the defaults are the launcher's | Why: the Free camera's options, as SPEC asks
inline bool gPortFreeSwipeZooms = false;          // Two-finger swipe: false "Moves the camera", true "Zooms and rotates"
inline bool gPortFreeInvertLook = false;          // Invert vertical look
inline float gPortFreeLookSensitivity = 1.0f;     // Look sensitivity, 0.25 to 4
inline float gPortFreeFlySpeed = 1.0f;            // Fly speed, 0.25 to 4 (right button + wheel changes it until the game quits)
inline bool gPortFreeSmoothing = true;            // Smoothing: the tilt, the turn and the flight ease
inline bool gPortFreeThroughGround = false;       // Fly through the ground
// The modifiers held now, as the game's modifier byte (3 shift, 0xc command or control, 0x30 option); the launcher sets it
// (the platform's key state). None: no modifier.
inline int32_t (*gPortFreeModifiers)() = nullptr;

// The limits. The tilt (gCameraPitch, the angle down from the horizon) goes from just above the horizon to straight down;
// the zoom (gCameraF68, the distance from the target) from 25 to 300.
inline constexpr float kPortFreePitchLow = 0.05f;      // about 3 degrees
inline constexpr float kPortFreePitchHigh = 1.5533f;   // about 89 degrees
inline constexpr float kPortFreeZoomIn = 25.0f;
inline constexpr float kPortFreeZoomOut = 300.0f;
inline constexpr float kPortFreeGroundMargin = 4.0f;   // the eye stays this far above the ground under it
inline constexpr float kPortFreeTiltRate = 0.0012f;    // Z and X: radians a millisecond
inline constexpr float kPortFreeDragTurn = 0.005f;     // a mouse drag: radians a pixel at sensitivity 1
inline constexpr float kPortFreeSwipeTurn = 0.03f;     // a swipe: radians a unit of SDL's precise delta
inline constexpr float kPortFreeSwipeTilt = 0.02f;
inline constexpr float kPortFreeSwipeMove = 0.02f;     // a swipe: the zoom's share a unit
inline constexpr float kPortFreeSwipeZoom = 2.0f;      // a swipe in "Zooms and rotates": zoom units a unit
inline constexpr float kPortFreePinchZoom = 0.85f;     // Ctrl + wheel: the zoom times this a unit up
inline constexpr uint64_t kPortFreeFineMs = 300;       // a wheel event this soon after a fractional one is a trackpad's too

// Which key of the free camera's a key of the key tables is: the letters by their place on the keyboard (as W A S D,
// include/port_wasd.h), Tab, Page Up and Page Down by their codes (the original's key table, platform/key_tables.inc, gives the
// Mac's Page Up key Windows' VK_NEXT, 0x2200 here, and its Page Down VK_PRIOR, 0x2100).
enum PortFreeKeyId : int {
    kPortFreeQ, kPortFreeE, kPortFreeZ, kPortFreeX, kPortFreeW, kPortFreeA, kPortFreeS, kPortFreeD,
    kPortFreeTab, kPortFreePgUp, kPortFreePgDn, kPortFreeKeyCount
};
inline constexpr uint8_t kPortFreePlaces[kPortFreeKeyCount] = {0x0c, 0x0e, 0x06, 0x07, 0x0d, 0x00, 0x01, 0x02, 0x30, 0x74, 0x79};

// The free camera's state. pitchTarget below 0: not yet set (HandleCamera starts it at the original's tilt for the zoom).
struct PortFreeCameraState {
    float pitchTarget = -1.0f;
    uint32_t held = 0;                       // bit (1 << PortFreeKeyId) of each key the free camera holds
    int16_t tableHeld[kPortFreeKeyCount] = {};   // the camera table's key a key pressed (Q, E, Page Up, Page Down in orbit)
    // What the wheel, the drags and the pinch asked for since the last frame; HandleCamera takes it.
    float dYaw = 0.0f;       // radians, added to the key yaw (larger turns the view right)
    float dPitch = 0.0f;     // radians, added to the tilt (larger looks further down)
    float dZoom = 0.0f;      // zoom units, added
    float zoomScale = 1.0f;  // the zoom times this
    float dForward = 0.0f;   // moves, in shares of the zoom, along the view's ground direction
    float dRight = 0.0f;
    bool haveFine = false;   // a fractional wheel event was seen, at lastFineMs
    uint64_t lastFineMs = 0;
    int dragButton = -1;     // the button of the drag the run loop holds in relative mouse mode (0 left, 1 right, 2 middle)
};
inline PortFreeCameraState gPortFree;

// The game's modifier byte held now.
inline int32_t PortFreeModifiersNow() { return gPortFreeModifiers != nullptr ? gPortFreeModifiers() : 0; }
// Shift goes three times as fast, Option a third as fast.
inline float PortFreeSpeedFactor(int32_t modifiers) {
    float factor = 1.0f;
    if ((modifiers & 3) != 0) factor *= 3.0f;
    if ((modifiers & 0x30) != 0) factor /= 3.0f;
    return factor;
}

// The original's tilt for a zoom (HandleCamera: a quarter turn's half at 70, 1.134464 at 130, within its limits), within
// the free camera's.
inline float PortFreeFormulaPitch(float zoom) {
    const float t = (zoom - 70.0f) / 60.0f;
    float pitch = 0.7853982f * (1.0f - t) + 1.134464f * t;
    if (pitch < 0.0f) pitch = 0.0f;
    if (pitch > 1.4707963f) pitch = 1.4707963f;
    return std::fmin(std::fmax(pitch, kPortFreePitchLow), kPortFreePitchHigh);
}
inline float PortFreeClampPitch(float pitch) { return std::fmin(std::fmax(pitch, kPortFreePitchLow), kPortFreePitchHigh); }

// One frame of dt ms of easing toward a target, at the rate the original eases the yaw (dt / 140 of the way, at most all of
// it); with Smoothing off, the target at once.
inline float PortFreeEase(float value, float target, float dt) {
    if (!gPortFreeSmoothing) return target;
    const float share = std::fmin(dt, 140.0f) / 140.0f;
    const float next = value * (1.0f - share) + target * share;
    return std::fabs(next - target) < 5e-4f ? target : next;
}

// The tilt that keeps the eye at least kPortFreeGroundMargin above the ground under it: the eye is the zoom away from the
// target at (tx, base, tz), tilt pitch down toward it, turned by yaw; ground(x, z) is the ground's height. Raises the tilt
// (three passes, as the eye moves when it rises) and never lowers it.
template <class Ground>
inline float PortFreeFloorPitch(float pitch, float yaw, float zoom, float tx, float base, float tz, Ground ground) {
    if (!(zoom > 0.0f)) return pitch;
    const float s = std::sin(yaw);
    const float c = std::cos(yaw);
    for (int pass = 0; pass < 3; ++pass) {
        const float across = zoom * std::cos(pitch);
        const float ex = tx - across * s;
        const float ez = tz - across * c;
        const float need = (ground(ex, ez) + kPortFreeGroundMargin - base) / zoom;
        if (zoom * std::sin(pitch) >= need * zoom) break;
        pitch = need >= 1.0f ? 1.5707963f : std::asin(need);
    }
    return pitch;
}

// ---- keys

inline int PortFreeKeyOf(int32_t key) {
    static constexpr char kUSLetters[8] = {'Q', 'E', 'Z', 'X', 'W', 'A', 'S', 'D'};
    for (int i = 0; i < 8; ++i) {
        const char letter = gPortPlainKey != nullptr ? gPortPlainKey(kPortFreePlaces[i]) : kUSLetters[i];
        if (letter != 0 && static_cast<int16_t>(key) == letter) return i;
    }
    if (key == 9) return kPortFreeTab;
    if (key == 0x2200) return kPortFreePgUp;
    if (key == 0x2100) return kPortFreePgDn;
    return -1;
}

// Lets go of what one of the keys holds: its bit, and the camera table's key it pressed. True when it held something.
inline bool PortFreeLetGoKey(int which) {
    if (which < 0) return false;
    bool held = (gPortFree.held & (1u << which)) != 0;
    gPortFree.held &= ~(1u << which);
    if (gPortFree.tableHeld[which] != 0) {
        gKeyTableCamera->KeyUp(gPortFree.tableHeld[which], 0);
        gPortFree.tableHeld[which] = 0;
        held = true;
    }
    return held;
}
inline void PortFreeLetGoAll() {
    for (int i = 0; i < kPortFreeKeyCount; ++i) PortFreeLetGoKey(i);
}

// TPW_PORT_FIX freecam PortFreeCameraKey | Mac: (none) | Port: with gPortFreeCamera on, over the park (not in first person): Q and E press the camera table's turn keys (the left and right arrows, so the Isometric option's quarter-turn snap holds), Page Up and Page Down its zoom keys (the up arrow, zoom in, and the down arrow, zoom out), and Z and X tilt (held bits HandleCamera reads); Shift and Option may be held (faster, slower), Command and Control leave the key to the tables; the key up lets go of what its key down took; true when the key was taken | Why: the Free camera's keys, as SPEC asks
inline bool PortFreeCameraKey(int32_t key, int32_t modifiers, bool down) {
    if (!gPortFreeCamera) return false;
    const int which = PortFreeKeyOf(key);
    if (which < 0) return false;
    if (!down) return PortFreeLetGoKey(which);
    if ((gFirstPersonFlags & kFirstPersonAny) != 0 || (modifiers & 0xc) != 0) return false;
    switch (which) {
    case kPortFreeZ:
    case kPortFreeX:
        gPortFree.held |= 1u << which;
        return true;
    case kPortFreeQ:
    case kPortFreeE:
    case kPortFreePgUp:
    case kPortFreePgDn: {
        static constexpr int16_t kCodes[4] = {0x2500, 0x2700, 0x2600, 0x2800};   // left, right, up (zoom in), down (zoom out)
        const int16_t code = kCodes[which == kPortFreeQ ? 0 : which == kPortFreeE ? 1 : which == kPortFreePgUp ? 2 : 3];
        int16_t& held = gPortFree.tableHeld[which];
        if (held != 0 && held != code) gKeyTableCamera->KeyUp(held, 0);
        if (held != code) gKeyTableCamera->KeyDown(code, 0);
        held = code;
        return true;
    }
    default:
        return false;
    }
}

// Whether a key, by its Mac key code, is one of the free camera's (the run loop keeps them down together, as W A S D).
inline bool PortFreePlace(uint8_t macKeyCode) {
    if (!gPortFreeCamera) return false;
    for (const uint8_t place : kPortFreePlaces) {
        if (macKeyCode == place) return true;
    }
    return false;
}
// A key-up the run loop could not match, by its Mac key code: lets go of what that key holds.
inline void PortFreeStrayKeyUp(uint8_t macKeyCode) {
    for (int i = 0; i < kPortFreeKeyCount; ++i) {
        if (macKeyCode == kPortFreePlaces[i]) PortFreeLetGoKey(i);
    }
}

// ---- the wheel, the drags, the pinch

// TPW_PORT_FIX freecam PortFreeWheel | Mac: (none: a wheel step zooms by 10) | Port: with gPortFreeCamera on, over the park and not in first person: a trackpad (a fractional delta, or any wheel event within kPortFreeFineMs of one) moves the camera ("Moves the camera": up and down forward and back, sideways sideways) or zooms and turns ("Zooms and rotates"), and with Option tilts and turns; Control or Command with the wheel is a pinch (Windows' precision touchpads send their pinch so) and zooms; the deltas are SDL's as they come, which follow the system's natural scrolling; a notched wheel is not taken (the original's 10 a step); true when taken | Why: the Free camera's trackpad, as SPEC asks
inline bool PortFreeWheel(float x, float y, bool fractional, int32_t modifiers, uint64_t nowMs, bool overPark) {
    if (!gPortFreeCamera) return false;
    if (fractional) {
        gPortFree.haveFine = true;
        gPortFree.lastFineMs = nowMs;
    }
    if (!overPark || (gFirstPersonFlags & kFirstPersonAny) != 0) return false;
    const bool fine = fractional || (gPortFree.haveFine && nowMs >= gPortFree.lastFineMs && nowMs - gPortFree.lastFineMs < kPortFreeFineMs);
    if ((modifiers & 0xc) != 0) {
        gPortFree.zoomScale *= std::pow(kPortFreePinchZoom, y);
        return true;
    }
    if (!fine) return false;
    const float sense = gPortFreeLookSensitivity;
    const float invert = gPortFreeInvertLook ? -1.0f : 1.0f;
    if ((modifiers & 0x30) != 0) {
        gPortFree.dYaw += x * kPortFreeSwipeTurn * sense;
        gPortFree.dPitch -= y * kPortFreeSwipeTilt * sense * invert;
    } else if (gPortFreeSwipeZooms) {
        gPortFree.dZoom -= y * kPortFreeSwipeZoom;
        gPortFree.dYaw += x * kPortFreeSwipeTurn * sense;
    } else {
        gPortFree.dForward += y * kPortFreeSwipeMove;
        gPortFree.dRight += x * kPortFreeSwipeMove;
    }
    return true;
}

// TPW_PORT_FIX freecam PortFreeDragStart | Mac: (none: the middle button turned the view by the pointer's distance from the middle of the screen) | Port: with gPortFreeCamera on, over the park and not in first person, the middle button, or the left with Option, starts a drag the run loop holds in relative mouse mode: sideways turns, up and down tilts, by how far it moves; true when taken | Why: the Free camera's mouse, as SPEC asks
inline bool PortFreeDragStart(int button, int32_t modifiers, bool overPark) {
    if (!gPortFreeCamera || !overPark || (gFirstPersonFlags & kFirstPersonAny) != 0) return false;
    if (button == 2 || (button == 0 && (modifiers & 0x30) != 0)) {
        gPortFree.dragButton = button;
        return true;
    }
    return false;
}
inline void PortFreeDragMove(float dx, float dy) {
    if (gPortFree.dragButton < 0) return;
    const float step = kPortFreeDragTurn * gPortFreeLookSensitivity;
    gPortFree.dYaw += dx * step;
    gPortFree.dPitch += dy * step * (gPortFreeInvertLook ? -1.0f : 1.0f);
}
// Whether the drag may go on: not once first person starts or a panel opens.
inline bool PortFreeDragGoing(bool panelOpen) {
    return gPortFreeCamera && !panelOpen && (gFirstPersonFlags & kFirstPersonAny) == 0;
}
inline void PortFreeDragEnd() { gPortFree.dragButton = -1; }

// A pinch's scale change since the last (SDL 3.4's SDL_EVENT_PINCH_UPDATE): above 1 zooms in.
inline void PortFreePinch(float scale, bool overPark) {
    if (!gPortFreeCamera || !overPark || (gFirstPersonFlags & kFirstPersonAny) != 0 || !(scale > 0.0f)) return;
    gPortFree.zoomScale /= scale;
}

// Clears what the input asked for (HandleCamera took it, or first person dropped it).
inline void PortFreeClearPending() {
    gPortFree.dYaw = gPortFree.dPitch = gPortFree.dZoom = gPortFree.dForward = gPortFree.dRight = 0.0f;
    gPortFree.zoomScale = 1.0f;
}

// The Mac's modifier word (0x200 shift, 0x100 command, 0x1000 control, 0x800 option) as the game's byte, as HandleTyping
// makes it.
inline int32_t PortFreeGameModifiers(uint16_t mac) {
    int32_t mods = 0;
    if ((mac & 0x200) != 0) mods |= 3;
    if ((mac & 0x1100) != 0) mods |= 0xc;
    if ((mac & 0x800) != 0) mods |= 0x30;
    return mods;
}

// The run loop's hooks (platform/platform.h tpw::gPortWheel and the rest), which the launcher sets with the setting on;
// in src/game/TPMain.cpp, which knows whether the pointer is over the park.
bool PortFreeOverPark();
bool PortFreeHookWheel(float x, float y, bool fractional, uint16_t macModifiers, uint64_t nowMs);
int PortFreeHookDragStart(int button, uint16_t macModifiers);
void PortFreeHookDragMove(float dx, float dy);
bool PortFreeHookDragGoing();
void PortFreeHookDragEnd();
void PortFreeHookPinch(float scale);

