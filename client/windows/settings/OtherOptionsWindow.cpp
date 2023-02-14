/*
 * VcmiSettingsWindow.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "OtherOptionsWindow.h"

#include "../../../lib/filesystem/ResourceID.h"
#include "../../gui/CGuiHandler.h"
#include "../../widgets/Buttons.h"
#include "CConfigHandler.h"

static void setBoolSetting(std::string group, std::string field, bool value)
{
	Settings fullscreen = settings.write[group][field];
	fullscreen->Bool() = value;
}

OtherOptionsWindow::OtherOptionsWindow() : InterfaceObjectConfigurable()
{
	OBJ_CONSTRUCTION_CAPTURING_ALL_NO_DISPOSE;

	const JsonNode config(ResourceID("config/widgets/settings/otherOptionsTab.json"));
	addCallback("availableCreaturesAsDwellingLabelChanged", std::bind(&setBoolSetting, "gameTweaks", "availableCreaturesAsDwellingLabel", _1));
	addCallback("compactTownCreatureInfoChanged", std::bind(&setBoolSetting, "gameTweaks", "compactTownCreatureInfo", _1));
	build(config);

	std::shared_ptr<CToggleButton> availableCreaturesAsDwellingLabelCheckbox = widget<CToggleButton>("availableCreaturesAsDwellingLabelCheckbox");
	availableCreaturesAsDwellingLabelCheckbox->setSelected((bool)settings["gameTweaks"]["availableCreaturesAsDwellingLabel"].Bool());

	std::shared_ptr<CToggleButton> compactTownCreatureInfo = widget<CToggleButton>("compactTownCreatureInfoCheckbox");
	compactTownCreatureInfo->setSelected((bool)settings["gameTweaks"]["compactTownCreatureInfo"].Bool());
}


