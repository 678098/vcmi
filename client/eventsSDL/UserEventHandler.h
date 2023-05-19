/*
* EventHandlerSDLUser.h, part of VCMI engine
*
* Authors: listed in file AUTHORS in main folder
*
* License: GNU General Public License v2.0 or later
* Full text of license available in license.txt file, in main folder
*
*/

#pragma once

struct SDL_UserEvent;

/// Class for handling events of type SDL_UserEvent
class UserEventHandler
{
public:
	void handleUserEvent(const SDL_UserEvent & current);
};
