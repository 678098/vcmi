/*
* InputSourceKeyboard.cpp, part of VCMI engine
*
* Authors: listed in file AUTHORS in main folder
*
* License: GNU General Public License v2.0 or later
* Full text of license available in license.txt file, in main folder
*
*/

#include "StdInc.h"
#include "InputSourceKeyboard.h"

#include "../../lib/CConfigHandler.h"
#include "../CPlayerInterface.h"
#include "../gui/CGuiHandler.h"
#include "../gui/EventDispatcher.h"
#include "../gui/ShortcutHandler.h"

#include <SDL_events.h>

void InputSourceKeyboard::handleEventKeyDown(const SDL_KeyboardEvent & key)
{
	if(key.repeat != 0)
		return; // ignore periodic event resends

	assert(key.state == SDL_PRESSED);

	if(key.keysym.sym >= SDLK_F1 && key.keysym.sym <= SDLK_F15 && settings["session"]["spectate"].Bool())
	{
		//TODO: we need some central place for all interface-independent hotkeys
		Settings s = settings.write["session"];
		switch(key.keysym.sym)
		{
			case SDLK_F5:
				if(settings["session"]["spectate-locked-pim"].Bool())
					CPlayerInterface::pim->unlock();
				else
					CPlayerInterface::pim->lock();
				s["spectate-locked-pim"].Bool() = !settings["session"]["spectate-locked-pim"].Bool();
				break;

			case SDLK_F6:
				s["spectate-ignore-hero"].Bool() = !settings["session"]["spectate-ignore-hero"].Bool();
				break;

			case SDLK_F7:
				s["spectate-skip-battle"].Bool() = !settings["session"]["spectate-skip-battle"].Bool();
				break;

			case SDLK_F8:
				s["spectate-skip-battle-result"].Bool() = !settings["session"]["spectate-skip-battle-result"].Bool();
				break;

			default:
				break;
		}
		return;
	}

	auto shortcutsVector = GH.shortcuts().translateKeycode(key.keysym.sym);

	GH.events().dispatchShortcutPressed(shortcutsVector);
}

void InputSourceKeyboard::handleEventKeyUp(const SDL_KeyboardEvent & key)
{
	if(key.repeat != 0)
		return; // ignore periodic event resends

	assert(key.state == SDL_RELEASED);

	auto shortcutsVector = GH.shortcuts().translateKeycode(key.keysym.sym);

	GH.events().dispatchShortcutReleased(shortcutsVector);
}
