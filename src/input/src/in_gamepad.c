/*
 * Copyright (C) 1996-1997 Id Software, Inc.
 * Copyright (C) Henrique Barateli, <henriquejb194@gmail.com>, et al.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
// in_gamepad.c -- gamepad code
//
// Two backends share the button/axis state and movement math in this file:
//
//   * Desktop  -- SDL_GameController events (IN_GamepadEvent, pumped by
//                 Sys_SendKeyEvents' SDL_PollEvent loop).
//   * PS3      -- PSL1GHT io/pad.h polled every frame (IN_PollGamepad).
//                 We bypass SDL entirely on PS3 because the PSL1GHT SDL2
//                 joystick driver ships with no SDL_GameController mapping
//                 and the resulting pad-rumble/Bluetooth glue added latency.
//                 PSL1GHT's padData already exposes decoded bitfields
//                 (BTN_CROSS, BTN_TRIANGLE, ANA_L_H, PRE_L2, ...) so the
//                 direct path is both simpler and lower-latency.


#include "in_gamepad.h"
#include "console.h"
#include "keys.h"
#include "sys.h"

#ifndef CHOCOLATE_QUAKE_PS3
#include <SDL.h>
#else
#include <io/pad.h>
#endif


static cvar_t joy_deadzone_look = {"joy_deadzone_look", "0.175", true};
static cvar_t joy_deadzone_move = {"joy_deadzone_move", "0.175", true};
static cvar_t joy_outer_threshold_look = {"joy_outer_threshold_look", "0.02", true};
static cvar_t joy_outer_threshold_move = {"joy_outer_threshold_move", "0.02", true};
static cvar_t joy_deadzone_trigger = {"joy_deadzone_trigger", "0.2", true};
static cvar_t joy_sensitivity_yaw = {"joy_sensitivity_yaw", "240", true};
static cvar_t joy_sensitivity_pitch = {"joy_sensitivity_pitch", "130", true};
static cvar_t joy_invert = {"joy_invert", "0", true};
static cvar_t joy_exponent = {"joy_exponent", "2", true};
static cvar_t joy_exponent_move = {"joy_exponent_move", "2", true};
static cvar_t joy_swapmovelook = {"joy_swapmovelook", "0", true};
static cvar_t joy_enable = {"joy_enable", "1", true};

// ---------------------------------------------------------------------------
// Internal gamepad button / axis identifiers.
//
// Numeric values intentionally match SDL_GameControllerButton /
// SDL_GameControllerAxis so the desktop SDL path is unchanged, but the
// enum decouples the rest of the file from SDL's headers. The PS3 backend
// indexes the same arrays via these IDs.
// ---------------------------------------------------------------------------
typedef enum {
    PAD_BTN_A,             // Cross on PS3
    PAD_BTN_B,             // Circle on PS3
    PAD_BTN_X,             // Square on PS3
    PAD_BTN_Y,             // Triangle on PS3
    PAD_BTN_BACK,          // Select on PS3
    PAD_BTN_START,
    PAD_BTN_LEFTSTICK,     // L3
    PAD_BTN_RIGHTSTICK,    // R3
    PAD_BTN_LEFTSHOULDER,  // L1
    PAD_BTN_RIGHTSHOULDER, // R1
    PAD_BTN_DPAD_UP,
    PAD_BTN_DPAD_DOWN,
    PAD_BTN_DPAD_LEFT,
    PAD_BTN_DPAD_RIGHT,
    PAD_BTN_MAX,
} pad_button_t;

typedef enum {
    PAD_AXIS_LEFTX,
    PAD_AXIS_LEFTY,
    PAD_AXIS_RIGHTX,
    PAD_AXIS_RIGHTY,
    PAD_AXIS_TRIGGERLEFT,
    PAD_AXIS_TRIGGERRIGHT,
    PAD_AXIS_MAX,
} pad_axis_t;

typedef struct {
    qboolean was_pressed; // The previous state of the button.
    qboolean pressed;     // The current state of the button.
    i32 key;              // The key code associated with this button.
    double timer;         // Timer to manage repeated button presses.
} button_t;

typedef struct {
    float previous_value;
    float value;
} axis_t;

typedef struct {
    float x; // Horizontal axis value
    float y; // Vertical axis value
} analog_stick_t;

typedef enum {
    EMULATE_NONE,
    EMULATE_TRIGGER,
} emulate_axis_t;

typedef struct {
    const emulate_axis_t type; // Type of emulation.
    const float* threshold;    // Threshold value to trigger button press.
    button_t* pos_button;      // Button for positive direction.
    button_t* neg_button;      // Button for negative direction.
} axis_button_mapping_t;

static button_t buttons[PAD_BTN_MAX] = {
    [PAD_BTN_A]             = {.key = K_ABUTTON},
    [PAD_BTN_B]             = {.key = K_BBUTTON},
    [PAD_BTN_X]             = {.key = K_XBUTTON},
    [PAD_BTN_Y]             = {.key = K_YBUTTON},
    [PAD_BTN_BACK]          = {.key = K_TAB},
    [PAD_BTN_START]         = {.key = K_ESCAPE},
    [PAD_BTN_LEFTSTICK]     = {.key = K_LTHUMB},
    [PAD_BTN_RIGHTSTICK]    = {.key = K_RTHUMB},
    [PAD_BTN_LEFTSHOULDER]  = {.key = K_LSHOULDER},
    [PAD_BTN_RIGHTSHOULDER] = {.key = K_RSHOULDER},
    [PAD_BTN_DPAD_UP]       = {.key = K_UPARROW},
    [PAD_BTN_DPAD_DOWN]     = {.key = K_DOWNARROW},
    [PAD_BTN_DPAD_LEFT]     = {.key = K_LEFTARROW},
    [PAD_BTN_DPAD_RIGHT]    = {.key = K_RIGHTARROW},
};
static button_t left_trigger_button = {.key = K_LTRIGGER};
static button_t right_trigger_button = {.key = K_RTRIGGER};

static axis_t axes[PAD_AXIS_MAX] = {0};

#ifndef CHOCOLATE_QUAKE_PS3
static SDL_GameController* gamepad = NULL;
static SDL_JoystickID gamepad_id = -1;
#else
static qboolean gamepad_connected = false;
// PSL1GHT padInfo2 port_status is only updated by ioPadGetInfo2; we cache
// it here so IN_PollGamepad can detect (dis)connect transitions and feed
// them into the same was_pressed/pressed machinery SDL events would have.
static u32 ps3_port_status = 0;
#endif


/*
================================================================================

BUTTON EMULATION FOR AXES

================================================================================
*/

static const axis_button_mapping_t axes_mapping[PAD_AXIS_MAX] = {
    [PAD_AXIS_TRIGGERLEFT] = {
        .type = EMULATE_TRIGGER,
        .threshold = &joy_deadzone_trigger.value,
        .pos_button = &left_trigger_button,
    },
    [PAD_AXIS_TRIGGERRIGHT] = {
        .type = EMULATE_TRIGGER,
        .threshold = &joy_deadzone_trigger.value,
        .pos_button = &right_trigger_button,
    },
};

static void IN_SendButtonEvent(button_t* button);

static void IN_EmulateButtonPress(const axis_t* axis,
                                  const axis_button_mapping_t* mapping) {
    if (mapping->pos_button) {
        button_t* button = mapping->pos_button;
        const float threshold = *mapping->threshold;
        button->was_pressed = (axis->previous_value > threshold);
        button->pressed = (axis->value > threshold);
        IN_SendButtonEvent(button);
    }
    if (mapping->neg_button) {
        button_t* button = mapping->neg_button;
        const float threshold = *mapping->threshold;
        button->was_pressed = (-axis->previous_value > threshold);
        button->pressed = (-axis->value > threshold);
        IN_SendButtonEvent(button);
    }
}

static void IN_EmulateButtonForAxis(const pad_axis_t axis_type) {
    const axis_t* axis = &axes[axis_type];
    const axis_button_mapping_t* mapping = &axes_mapping[axis_type];
    switch (mapping->type) {
        case EMULATE_TRIGGER:
            IN_EmulateButtonPress(axis, mapping);
            break;
        default:
            break;
    }
}


//==============================================================================


/*
================================================================================

BUTTON EVENT DISPATCH

================================================================================
*/

static void IN_SendButtonEvent(button_t* button) {
    // Using `realtime` could cause issues with key repeats
    // due to non-monotonic behavior.
    const double now = Sys_FloatTime();
    if (!button->was_pressed) {
        if (button->pressed) {
            button->timer = now + 0.5;
            Key_Event(button->key, true);
        }
        return;
    }
    if (button->pressed) {
        if (now >= button->timer) {
            button->timer = now + 0.1;
            Key_Event(button->key, true);
        }
        return;
    }
    button->timer = 0;
    Key_Event(button->key, false);
}

//==============================================================================


/*
================================================================================

DESKTOP (SDL) BACKEND

================================================================================
*/
#ifndef CHOCOLATE_QUAKE_PS3

static void IN_AxisEvent(const SDL_ControllerAxisEvent* event) {
    if (event->which != gamepad_id) {
        return;
    }
    axis_t* axis = &axes[event->axis];
    axis->previous_value = axis->value;
    axis->value = (float) event->value / 32768.0f;
    IN_EmulateButtonForAxis(event->axis);
}

static void IN_ButtonEvent(const SDL_ControllerButtonEvent* event) {
    if (event->which != gamepad_id) {
        return;
    }
    button_t* button = &buttons[event->button];
    button->was_pressed = button->pressed;
    button->pressed = (event->state == SDL_PRESSED);
    IN_SendButtonEvent(button);
}

static void IN_RemovedControllerEvent(const SDL_ControllerDeviceEvent* event) {
    if (event->which != gamepad_id) {
        return;
    }
    SDL_GameControllerClose(gamepad);
    gamepad = NULL;
    gamepad_id = -1;
}

static void IN_AddedControllerEvent(const SDL_ControllerDeviceEvent* event) {
    if (gamepad) {
        // Already have one gamepad connected.
        return;
    }
    gamepad = SDL_GameControllerOpen(event->which);
    if (!gamepad) {
        Con_Printf("\ncould not open joystick: (%s)\n\n", SDL_GetError());
        return;
    }
    SDL_Joystick* joystick = SDL_GameControllerGetJoystick(gamepad);
    gamepad_id = SDL_JoystickInstanceID(joystick);
    Con_Printf("\njoystick detected\n\n");
}

void IN_GamepadEvent(const SDL_Event* event) {
    if (joy_enable.value == 0) {
        return;
    }
    switch (event->type) {
        case SDL_CONTROLLERDEVICEADDED:
            IN_AddedControllerEvent(&event->cdevice);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            IN_RemovedControllerEvent(&event->cdevice);
            break;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
            IN_ButtonEvent(&event->cbutton);
            break;
        case SDL_CONTROLLERAXISMOTION:
            IN_AxisEvent(&event->caxis);
            break;
        default:
            break;
    }
}

#endif // !CHOCOLATE_QUAKE_PS3


/*
================================================================================

PS3 (PSL1GHT) BACKEND

================================================================================
*/
#ifdef CHOCOLATE_QUAKE_PS3

// DualShock 3 reports the analog sticks as u8 fields centred at 0x80
// (0 = full left/up, 0xFF = full right/down). Triggers (L2/R2) are u8
// pressures 0 (released) .. 0xFF (fully pressed).
//
// We normalise sticks to [-1, 1] matching SDL_GameControllerAxis convention
// (right = +X, down = +Y) and triggers to [0, 1]. The shared movement math
// and axis-button emulation then work identically to the desktop path.
static float IN_PS3_NormalizeStick(u8 v) {
    return ((float) v - 128.0f) / 128.0f;
}

static float IN_PS3_NormalizeTrigger(u8 v) {
    return (float) v / 255.0f;
}

// Release every button we manage -- used when the pad disappears so a key
// doesn't get stuck "down" forever. Axes are zeroed so the player doesn't
// keep spinning after disconnect.
static void IN_PS3_ReleaseAll(void) {
    for (int i = 0; i < PAD_BTN_MAX; i++) {
        button_t* b = &buttons[i];
        if (!b->pressed) continue;
        b->was_pressed = true;
        b->pressed = false;
        IN_SendButtonEvent(b);
    }
    if (left_trigger_button.pressed) {
        left_trigger_button.was_pressed = true;
        left_trigger_button.pressed = false;
        IN_SendButtonEvent(&left_trigger_button);
    }
    if (right_trigger_button.pressed) {
        right_trigger_button.was_pressed = true;
        right_trigger_button.pressed = false;
        IN_SendButtonEvent(&right_trigger_button);
    }
    for (int i = 0; i < PAD_AXIS_MAX; i++) {
        axes[i].previous_value = axes[i].value;
        axes[i].value = 0.0f;
    }
    ps3_port_status = 0;
}

// Read one frame of pad data and feed it through the same button / axis
// state machine the SDL backend uses. Called once per frame from
// Sys_SendKeyEvents.
void IN_PollGamepad(void) {
    if (joy_enable.value == 0) {
        return;
    }

    padInfo2 info;
    ioPadGetInfo2(&info);

    // Port 0 is the only pad we care about -- single-player game, and the
    // XMB/quit plumbing already owns the PS button.
    const u32 status = info.port_status[0];
    const qboolean connected = (status & 0x01) != 0; // PORTSTATUS_CONNECTED

    if (!connected) {
        if (gamepad_connected) {
            // Pad was pulled since last frame -- release stuck keys.
            Con_DPrintf("PS3 pad disconnected\n");
            IN_PS3_ReleaseAll();
            gamepad_connected = false;
        }
        return;
    }
    if (!gamepad_connected) {
        Con_Printf("\nPS3 pad detected\n\n");
        gamepad_connected = true;
    }
    ps3_port_status = status;

    padData data;
    // Zero so stale bits from a previous pad type (BD remote etc.) don't
    // leak through if ioPadGetData only partially fills the struct.
    memset(&data, 0, sizeof(data));
    if (ioPadGetData(0, &data) != 0) {
        return;
    }

    // ---- Digital buttons --------------------------------------------------
    // padData exposes pre-decoded 1-bit fields (BTN_CROSS, BTN_LEFT, ...).
    // L2/R2 are deliberately *not* read here: the trigger axis emulation
    // below fires K_LTRIGGER/K_RTRIGGER from the analog pressure, matching
    // the desktop path.
    struct {
        unsigned int btn : 1;
        pad_button_t id;
    } const map[] = {
        {data.BTN_LEFT,         PAD_BTN_DPAD_LEFT},
        {data.BTN_DOWN,         PAD_BTN_DPAD_DOWN},
        {data.BTN_RIGHT,        PAD_BTN_DPAD_RIGHT},
        {data.BTN_UP,           PAD_BTN_DPAD_UP},
        {data.BTN_START,        PAD_BTN_START},
        {data.BTN_R3,           PAD_BTN_RIGHTSTICK},
        {data.BTN_L3,           PAD_BTN_LEFTSTICK},
        {data.BTN_SELECT,       PAD_BTN_BACK},
        {data.BTN_SQUARE,       PAD_BTN_X},
        {data.BTN_CROSS,        PAD_BTN_A},
        {data.BTN_CIRCLE,       PAD_BTN_B},
        {data.BTN_TRIANGLE,     PAD_BTN_Y},
        {data.BTN_R1,           PAD_BTN_RIGHTSHOULDER},
        {data.BTN_L1,           PAD_BTN_LEFTSHOULDER},
    };
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        button_t* b = &buttons[map[i].id];
        b->was_pressed = b->pressed;
        b->pressed = map[i].btn ? true : false;
        IN_SendButtonEvent(b);
    }

    // ---- Analog axes ------------------------------------------------------
    // Sticks first. previous_value is the last frame's value, so the
    // axis-button emulation can detect rising/falling edges.
    axis_t* ax;

    ax = &axes[PAD_AXIS_LEFTX];
    ax->previous_value = ax->value;
    ax->value = IN_PS3_NormalizeStick((u8) data.ANA_L_H);

    ax = &axes[PAD_AXIS_LEFTY];
    ax->previous_value = ax->value;
    ax->value = IN_PS3_NormalizeStick((u8) data.ANA_L_V);

    ax = &axes[PAD_AXIS_RIGHTX];
    ax->previous_value = ax->value;
    ax->value = IN_PS3_NormalizeStick((u8) data.ANA_R_H);

    ax = &axes[PAD_AXIS_RIGHTY];
    ax->previous_value = ax->value;
    ax->value = IN_PS3_NormalizeStick((u8) data.ANA_R_V);

    // Analog trigger pressures. If the pad is in digital-only mode these
    // stay 0 and the triggers won't register -- but BTN_L2/BTN_R2 would
    // still be set; we don't currently wire those up as a fallback.
    ax = &axes[PAD_AXIS_TRIGGERLEFT];
    ax->previous_value = ax->value;
    ax->value = IN_PS3_NormalizeTrigger((u8) data.PRE_L2);

    ax = &axes[PAD_AXIS_TRIGGERRIGHT];
    ax->previous_value = ax->value;
    ax->value = IN_PS3_NormalizeTrigger((u8) data.PRE_R2);

    // Run axis-to-button emulation. Only the triggers are mapped -- the
    // analog sticks no longer generate fake D-Pad presses in menus, so
    // menu navigation is D-Pad / face buttons only.
    IN_EmulateButtonForAxis(PAD_AXIS_TRIGGERLEFT);
    IN_EmulateButtonForAxis(PAD_AXIS_TRIGGERRIGHT);
}

#endif // CHOCOLATE_QUAKE_PS3


//==============================================================================


/*
================================================================================

GAMEPAD MOVE (shared)

================================================================================
*/

static vec_t IN_MoveMagnitude(const analog_stick_t* stick) {
    return sqrtf((stick->x * stick->x) + (stick->y * stick->y));
}

static void IN_ApplyEasing(analog_stick_t* stick, const float exponent) {
    const vec_t magnitude = IN_MoveMagnitude(stick);
    if (magnitude == 0) {
        return;
    }
    const vec_t eased_magnitude = powf(magnitude, exponent);
    const vec_t scale = eased_magnitude / magnitude;
    stick->x *= scale;
    stick->y *= scale;
}

static float IN_Clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void IN_ApplyDeadzone(analog_stick_t* stick, const float deadzone,
                             const float outer_threshold) {
    const vec_t magnitude = IN_MoveMagnitude(stick);
    if (magnitude <= deadzone) {
        // Within deadzone, so no move.
        stick->x = 0;
        stick->y = 0;
        return;
    }
    // Rescale the magnitude so deadzone becomes 0 and 1-outer_threshold becomes 1.
    const vec_t range = (magnitude - deadzone);
    const vec_t full_range = (1.0f - deadzone - outer_threshold);
    vec_t new_magnitude = range / full_range;
    new_magnitude = IN_Clampf(new_magnitude, 0.0f, 1.0f);
    const vec_t scale = new_magnitude / magnitude;
    stick->x *= scale;
    stick->y *= scale;
}

static void IN_ReadAnalogSticks(analog_stick_t* left, analog_stick_t* right) {
    float deadzone = joy_deadzone_move.value;
    float outer_threshold = joy_outer_threshold_move.value;
    float exponent = joy_exponent_move.value;
    left->x = axes[PAD_AXIS_LEFTX].value;
    left->y = axes[PAD_AXIS_LEFTY].value;
    IN_ApplyDeadzone(left, deadzone, outer_threshold);
    IN_ApplyEasing(left, exponent);

    deadzone = joy_deadzone_look.value;
    outer_threshold = joy_outer_threshold_look.value;
    exponent = joy_exponent.value;
    right->x = axes[PAD_AXIS_RIGHTX].value;
    right->y = axes[PAD_AXIS_RIGHTY].value;
    IN_ApplyDeadzone(right, deadzone, outer_threshold);
    IN_ApplyEasing(right, exponent);

    if (joy_swapmovelook.value != 0) {
        const analog_stick_t temp = *left;
        *left = *right;
        *right = temp;
    }
}

static void IN_MoveCamera(const analog_stick_t* look) {
    const float time = (float) host_frametime;
    const float pitch_dir = (joy_invert.value != 0 ? -1.0f : 1.0f);

    float joy_sensitivity = joy_sensitivity_yaw.value;
    cl.viewangles[YAW] -= look->x * joy_sensitivity * time;

    joy_sensitivity = joy_sensitivity_pitch.value;
    cl.viewangles[PITCH] += look->y * joy_sensitivity * pitch_dir * time;
    cl.viewangles[PITCH] = IN_Clampf(cl.viewangles[PITCH], -70, 80);

    if (look->x != 0 || look->y != 0) {
        V_StopPitchDrift();
    }
}

static void IN_MovePlayer(usercmd_t* cmd, const analog_stick_t* move) {
    float speed = cl_forwardspeed.value;
    if (in_speed.state & 1) {
        speed *= cl_movespeedkey.value;
    }
    cmd->sidemove += speed * move->x;
    cmd->forwardmove -= speed * move->y;
}

void IN_JoyMove(usercmd_t* cmd) {
    if (joy_enable.value == 0) {
        return;
    }
#ifdef CHOCOLATE_QUAKE_PS3
    if (!gamepad_connected) {
        return;
    }
#else
    if (!gamepad) {
        return;
    }
#endif
    if (cl.paused || key_dest != key_game) {
        return;
    }
    analog_stick_t left;
    analog_stick_t right;
    IN_ReadAnalogSticks(&left, &right);
    IN_MovePlayer(cmd, &left);
    IN_MoveCamera(&right);
}


//==============================================================================


/*
================================================================================

INITIALIZATION AND SHUTDOWN

================================================================================
*/

static void IN_RegisterCvars(void) {
    Cvar_RegisterVariable(&joy_sensitivity_yaw);
    Cvar_RegisterVariable(&joy_sensitivity_pitch);
    Cvar_RegisterVariable(&joy_deadzone_look);
    Cvar_RegisterVariable(&joy_deadzone_move);
    Cvar_RegisterVariable(&joy_outer_threshold_look);
    Cvar_RegisterVariable(&joy_outer_threshold_move);
    Cvar_RegisterVariable(&joy_deadzone_trigger);
    Cvar_RegisterVariable(&joy_invert);
    Cvar_RegisterVariable(&joy_exponent);
    Cvar_RegisterVariable(&joy_exponent_move);
    Cvar_RegisterVariable(&joy_swapmovelook);
    Cvar_RegisterVariable(&joy_enable);
}

void IN_InitGamepad(void) {
    IN_RegisterCvars();
    if (COM_CheckParm("-nojoy")) {
        // Abort startup if user requests no joystick.
        return;
    }
#ifdef CHOCOLATE_QUAKE_PS3
    // PSL1GHT io/pad.h -- bypass SDL_GameController entirely.
    // 7 is MAX_PORT_NUM (the PS3 supports up to 7 pads over Bluetooth).
    SYS_TRACE("[pad] ioPadInit(7)\n");
    const s32 rc = ioPadInit(MAX_PORT_NUM);
    if (rc != 0) {
        Con_Printf("ioPadInit failed: 0x%08x\n", (unsigned) rc);
        return;
    }
    SYS_TRACE("[pad] ioPadInit ok\n");
#else
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0) {
        Con_Printf("Couldn't init game controller: %s\n", SDL_GetError());
        return;
    }
    SDL_GameControllerEventState(SDL_ENABLE);
#endif
}

void IN_ShutdownGamepad(void) {
#ifdef CHOCOLATE_QUAKE_PS3
    if (gamepad_connected) {
        IN_PS3_ReleaseAll();
        gamepad_connected = false;
    }
    ioPadEnd();
#else
    if (gamepad) {
        SDL_GameControllerClose(gamepad);
        gamepad = NULL;
    }
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
#endif
}

//==============================================================================
