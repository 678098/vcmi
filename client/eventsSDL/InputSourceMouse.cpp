/*
* InputSourceMouse.cpp, part of VCMI engine
*
* Authors: listed in file AUTHORS in main folder
*
* License: GNU General Public License v2.0 or later
* Full text of license available in license.txt file, in main folder
*
*/

#include "StdInc.h"
#include "InputSourceMouse.h"
#include "InputHandler.h"

#include "../../lib/Point.h"
#include "../gui/CGuiHandler.h"
#include "../gui/EventDispatcher.h"
#include "../gui/MouseButton.h"

#include <SDL_events.h>

void InputSourceMouse::handleEventMouseMotion(const SDL_MouseMotionEvent & motion)
{
	Point newPosition(motion.x, motion.y);
	Point distance(-motion.xrel, -motion.yrel);

	if (mouseButtonsMask & SDL_BUTTON(SDL_BUTTON_MIDDLE))
		GH.events().dispatchGesturePanning(middleClickPosition, newPosition, distance);
	else
		GH.input().setCursorPosition(newPosition);

	mouseButtonsMask = motion.state;
}

void InputSourceMouse::handleEventMouseButtonDown(const SDL_MouseButtonEvent & button)
{
	Point position(button.x, button.y);

	switch(button.button)
	{
		case SDL_BUTTON_LEFT:
			if(button.clicks > 1)
				GH.events().dispatchMouseDoubleClick(position);
			else
				GH.events().dispatchMouseLeftButtonPressed(position);
			break;
		case SDL_BUTTON_RIGHT:
			GH.events().dispatchShowPopup(position);
			break;
		case SDL_BUTTON_MIDDLE:
			middleClickPosition = position;
			GH.events().dispatchGesturePanningStarted(position);
			break;
	}
}

void InputSourceMouse::handleEventMouseWheel(const SDL_MouseWheelEvent & wheel)
{
	GH.events().dispatchMouseScrolled(Point(wheel.x, wheel.y), GH.getCursorPosition());
}

void InputSourceMouse::handleEventMouseButtonUp(const SDL_MouseButtonEvent & button)
{
	Point position(button.x, button.y);

	switch(button.button)
	{
		case SDL_BUTTON_LEFT:
			GH.events().dispatchMouseLeftButtonReleased(position);
			break;
		case SDL_BUTTON_RIGHT:
			GH.events().dispatchClosePopup(position);
			break;
		case SDL_BUTTON_MIDDLE:
			GH.events().dispatchGesturePanningEnded(middleClickPosition, position);
			break;
	}
}

bool InputSourceMouse::isMouseButtonPressed(MouseButton button) const
{
	static_assert(static_cast<uint32_t>(MouseButton::LEFT)   == SDL_BUTTON_LEFT,   "mismatch between VCMI and SDL enum!");
	static_assert(static_cast<uint32_t>(MouseButton::MIDDLE) == SDL_BUTTON_MIDDLE, "mismatch between VCMI and SDL enum!");
	static_assert(static_cast<uint32_t>(MouseButton::RIGHT)  == SDL_BUTTON_RIGHT,  "mismatch between VCMI and SDL enum!");
	static_assert(static_cast<uint32_t>(MouseButton::EXTRA1) == SDL_BUTTON_X1,     "mismatch between VCMI and SDL enum!");
	static_assert(static_cast<uint32_t>(MouseButton::EXTRA2) == SDL_BUTTON_X2,     "mismatch between VCMI and SDL enum!");

	uint32_t index = static_cast<uint32_t>(button);
	return mouseButtonsMask & SDL_BUTTON(index);
}
