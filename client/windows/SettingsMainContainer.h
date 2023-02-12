/*
 * SettingsMainContainer.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "../widgets/ObjectLists.h"
#include "../widgets/Buttons.h"
#include "gui/InterfaceObjectConfigurable.h"
#include "gui/CGuiHandler.h"
#include "battle/BattleInterface.h"

class SettingsMainContainer : public InterfaceObjectConfigurable
{
private:
	BattleInterface * parentBattleInterface;
	std::shared_ptr<CTabbedInt> tabContentArea;

	std::shared_ptr<CIntObject> createTab(size_t index);
	void openTab(size_t index);

	void close(); //TODO: copypaste of WindowBase::close(), consider changing Windowbase to IWindowbase with default close() implementation and changing WindowBase inheritance to CIntObject + IWindowBase
	void closeAndPushEvent(EUserEvent code);

	void loadGameButtonCallback();
	void saveGameButtonCallback();
	void quitGameButtonCallback();
	void backButtonCallback();
	void restartGameButtonCallback();
	void mainMenuButtonCallback();
public:
	SettingsMainContainer(BattleInterface * parentBattleInterface = nullptr);
};

