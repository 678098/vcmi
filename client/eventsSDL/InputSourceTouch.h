/*
* InputSourceTouch.h, part of VCMI engine
*
* Authors: listed in file AUTHORS in main folder
*
* License: GNU General Public License v2.0 or later
* Full text of license available in license.txt file, in main folder
*
*/

#pragma once

VCMI_LIB_NAMESPACE_BEGIN
class Point;
VCMI_LIB_NAMESPACE_END

enum class MouseButton;
struct SDL_TouchFingerEvent;

/// Enumeration that describes current state of gesture recognition
enum class TouchState
{
	// special state that allows no transitions
	// used when player selects "relative mode" in Launcher
	// in this mode touchscreen acts like touchpad, moving cursor at certains speed
	// and generates events for positions below cursor instead of positions below touch events
	RELATIVE_MODE,

	// no active touch events
	// DOWN -> transition to TAP_DOWN_SHORT
	// MOTION / UP -> not expected
	IDLE,

	// single finger is touching the screen for a short time
	// DOWN -> transition to TAP_DOWN_DOUBLE
	// MOTION -> transition to TAP_DOWN_PANNING
	// UP -> transition to IDLE, emit onLeftClickDown and onLeftClickUp
	// on timer -> transition to TAP_DOWN_LONG, emit onRightClickDown event
	TAP_DOWN_SHORT,

	// single finger is moving across screen
	// DOWN -> transition to TAP_DOWN_DOUBLE
	// MOTION -> emit panning event
	// UP -> transition to IDLE
	TAP_DOWN_PANNING,

	// two fingers are touching the screen
	// DOWN -> ??? how to handle 3rd finger? Ignore?
	// MOTION -> emit pinch event
	// UP -> transition to TAP_DOWN
	TAP_DOWN_DOUBLE,

	// single finger is down for long period of time
	// DOWN -> ignored
	// MOTION -> ignored
	// UP -> transition to IDLE, generate onRightClickUp() event
	TAP_DOWN_LONG,


	// Possible transitions:
	//                               -> DOUBLE
	//                    -> PANNING -> IDLE
	// IDLE -> DOWN_SHORT -> IDLE
	//                    -> LONG -> IDLE
	//                    -> DOUBLE -> PANNING
	//                              -> IDLE
};

struct TouchInputParameters
{
	double relativeModeSpeedFactor = 1.0;
	uint32_t longPressTimeMilliseconds = 500;

	bool useHoldGesture = true;
	bool usePanGesture = true;
	bool usePinchGesture = true;
	bool useRelativeMode = false;
};

/// Class that handles touchscreen input from SDL events
class InputSourceTouch
{
	TouchInputParameters params;
	TouchState state;
	uint32_t lastTapTimeTicks;

	Point convertTouchToMouse(const SDL_TouchFingerEvent & current);

	void emitPanningEvent();
	void emitPinchEvent();

public:
	InputSourceTouch();

	void handleEventFingerMotion(const SDL_TouchFingerEvent & current);
	void handleEventFingerDown(const SDL_TouchFingerEvent & current);
	void handleEventFingerUp(const SDL_TouchFingerEvent & current);

	void handleUpdate();

	bool isMouseButtonPressed(MouseButton button) const;
};
