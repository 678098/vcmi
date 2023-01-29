/*
 * SystemOptionsWindow.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../lib/CConfigHandler.h"
#include "gui/InterfaceObjectConfigurable.h"

class SystemOptionsWindow : public InterfaceObjectConfigurable
{
private:
	SettingsListener onFullscreenChanged;

	//functions bound to buttons
	void loadGameButtonCallback(); //load game
	void saveGameButtonCallback(); //save game
	void quitGameButtonCallback(); //quit game
	void backButtonCallback(); //return to game
	void restartGameButtonCallback(); //restart game
	void mainMenuButtonCallback(); //return to main menu

	void close(); //TODO: copypaste of WindowBase::close(), consider changing Windowbase to IWindowbase with default close() implementation and changing WindowBase inheritance to CIntObject + IWindowBase
	void selectGameResolution();
	void setGameResolution(int index);
	void closeAndPushEvent(int eventType, int code = 0);

public:
	SystemOptionsWindow();
};