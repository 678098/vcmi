/*
 * SystemOptionsWindow.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "SystemOptionsWindow.h"

#include <SDL_surface.h>
#include <SDL_rect.h>

#include "../../../lib/CGeneralTextHandler.h"
#include "../../../lib/filesystem/ResourceID.h"
#include "../../gui/CGuiHandler.h"
#include "../../widgets/Buttons.h"
#include "../../widgets/TextControls.h"
#include "../../widgets/Images.h"
#include "CGameInfo.h"
#include "CMusicHandler.h"
#include "CPlayerInterface.h"
#include "windows/GUIClasses.h"
#include "CServerHandler.h"
#include "renderSDL/SDL_Extensions.h"
#include "CMT.h"


static void setIntSetting(std::string group, std::string field, int value)
{
	Settings entry = settings.write[group][field];
	entry->Float() = value;
}

static void setBoolSetting(std::string group, std::string field, bool value)
{
	Settings fullscreen = settings.write[group][field];
	fullscreen->Bool() = value;
}

static std::string resolutionToString(int w, int h)
{
	return std::to_string(w) + 'x' + std::to_string(h);
}

SystemOptionsWindow::SystemOptionsWindow()
		: InterfaceObjectConfigurable(),
		  onFullscreenChanged(settings.listen["video"]["fullscreen"])
{
	OBJ_CONSTRUCTION_CAPTURING_ALL_NO_DISPOSE;

	const JsonNode config(ResourceID("config/widgets/optionsMenu.json"));
	addCallback("playerHeroSpeedChanged", std::bind(&setIntSetting, "adventure", "heroSpeed", _1));
	addCallback("enemyHeroSpeedChanged", std::bind(&setIntSetting, "adventure", "enemySpeed", _1));
	addCallback("mapScrollSpeedChanged", std::bind(&setIntSetting, "adventure", "scrollSpeed", _1));
	addCallback("heroReminderChanged", std::bind(&setBoolSetting, "adventure", "heroReminder", _1));
	addCallback("quickCombatChanged", std::bind(&setBoolSetting, "adventure", "quickCombat", _1));
	addCallback("spellbookAnimationChanged", std::bind(&setBoolSetting, "video", "spellbookAnimation", _1));
	addCallback("fullscreenChanged", std::bind(&SystemOptionsWindow::setFullscreenMode, this, _1));
	addCallback("setGameResolution", std::bind(&SystemOptionsWindow::selectGameResolution, this));
	addCallback("setMusic", [this](int value) { setIntSetting("general", "music", value); widget<CSlider>("musicSlider")->redraw(); });
	addCallback("setVolume", [this](int value) { setIntSetting("general", "sound", value); widget<CSlider>("soundVolumeSlider")->redraw(); });
	build(config);

	std::shared_ptr<CLabel> resolutionLabel = widget<CLabel>("resolutionLabel");
	const auto & currentResolution = settings["video"]["screenRes"];
	resolutionLabel->setText(resolutionToString(currentResolution["width"].Integer(), currentResolution["height"].Integer()));


	std::shared_ptr<CToggleGroup> playerHeroSpeedToggle = widget<CToggleGroup>("heroMovementSpeedPicker");
	playerHeroSpeedToggle->setSelected((int)settings["adventure"]["heroSpeed"].Float());

	std::shared_ptr<CToggleGroup> enemyHeroSpeedToggle = widget<CToggleGroup>("enemyMovementSpeedPicker");
	enemyHeroSpeedToggle->setSelected((int)settings["adventure"]["enemySpeed"].Float());

	std::shared_ptr<CToggleGroup> mapScrollSpeedToggle = widget<CToggleGroup>("mapScrollSpeedPicker");
	mapScrollSpeedToggle->setSelected((int)settings["adventure"]["scrollSpeed"].Float());


	std::shared_ptr<CToggleButton> heroReminderCheckbox = widget<CToggleButton>("heroReminderCheckbox");
	heroReminderCheckbox->setSelected((bool)settings["adventure"]["heroReminder"].Bool());

	std::shared_ptr<CToggleButton> quickCombatCheckbox = widget<CToggleButton>("quickCombatCheckbox");
	quickCombatCheckbox->setSelected((bool)settings["adventure"]["quickCombat"].Bool());

	std::shared_ptr<CToggleButton> spellbookAnimationCheckbox = widget<CToggleButton>("spellbookAnimationCheckbox");
	spellbookAnimationCheckbox->setSelected((bool)settings["video"]["spellbookAnimation"].Bool());

	std::shared_ptr<CToggleButton> fullscreenCheckbox = widget<CToggleButton>("fullscreenCheckbox");
	fullscreenCheckbox->setSelected((bool)settings["video"]["fullscreen"].Bool());
	onFullscreenChanged([&](const JsonNode &newState) //used when pressing F4 etc. to change fullscreen checkbox state
	{
		widget<CToggleButton>("fullscreenCheckbox")->setSelected(newState.Bool());
	});


	std::shared_ptr<CSlider> musicSlider = widget<CSlider>("musicSlider");
	musicSlider->moveTo(CCS->musich->getVolume());

	std::shared_ptr<CSlider> volumeSlider = widget<CSlider>("soundVolumeSlider");
	volumeSlider->moveTo(CCS->soundh->getVolume());
}


bool SystemOptionsWindow::isResolutionSupported(const Point & resolution)
{
	return isResolutionSupported( resolution, settings["video"]["fullscreen"].Bool());
}

bool SystemOptionsWindow::isResolutionSupported(const Point & resolution, bool fullscreen)
{
	if (!fullscreen)
		return true;

	auto supportedList = CSDL_Ext::getSupportedResolutions();

	return CSDL_Ext::isResolutionSupported(supportedList, resolution);
}

void SystemOptionsWindow::selectGameResolution()
{
	fillSelectableResolutions();

	std::vector<std::string> items;
	size_t currentResolutionIndex = 0;
	size_t i = 0;
	for(const auto & it : selectableResolutions)
	{
		auto resolutionStr = resolutionToString(it.x, it.y);
		if(widget<CLabel>("resolutionLabel")->getText() == resolutionStr)
			currentResolutionIndex = i;

		items.push_back(std::move(resolutionStr));
		++i;
	}
	GH.pushIntT<CObjectListWindow>(items, nullptr,
								   CGI->generaltexth->translate("vcmi.systemOptions.resolutionMenu.hover"),
								   CGI->generaltexth->translate("vcmi.systemOptions.resolutionMenu.help"),
								   std::bind(&SystemOptionsWindow::setGameResolution, this, _1),
								   currentResolutionIndex);
}

void SystemOptionsWindow::setGameResolution(int index)
{
	assert(index >= 0 && index < selectableResolutions.size());

	if ( index < 0 || index >= selectableResolutions.size() )
		return;

	Point resolution = selectableResolutions[index];

	Settings gameRes = settings.write["video"]["screenRes"];
	gameRes["width"].Float() = resolution.x;
	gameRes["height"].Float() = resolution.y;

	std::string resText;
	resText += std::to_string(resolution.x);
	resText += "x";
	resText += std::to_string(resolution.y);
	widget<CLabel>("resolutionLabel")->setText(resText);
}

void SystemOptionsWindow::setFullscreenMode(bool on)
{
	fillSelectableResolutions();

	const auto & screenRes = settings["video"]["screenRes"];
	const Point desiredResolution(screenRes["width"].Integer(), screenRes["height"].Integer());
	const Point currentResolution(screen->w, screen->h);

	if (!isResolutionSupported(currentResolution, on))
	{
		widget<CToggleButton>("fullscreenCheckbox")->setSelected(!on);
		LOCPLINT->showInfoDialog(CGI->generaltexth->translate("vcmi.systemOptions.fullscreenFailed"));
		return;
	}

	setBoolSetting("video", "fullscreen", on);

	if (!isResolutionSupported(desiredResolution, on))
	{
		// user changed his desired resolution and switched to fullscreen
		// however resolution he selected before is not available in fullscreen
		// so reset it back to currect resolution which is confirmed to be supported earlier
		Settings gameRes = settings.write["video"]["screenRes"];
		gameRes["width"].Float() = currentResolution.x;
		gameRes["height"].Float() = currentResolution.y;

		widget<CLabel>("resolutionLabel")->setText(resolutionToString(currentResolution.x, currentResolution.y));
	}
}

void SystemOptionsWindow::fillSelectableResolutions()
{
	selectableResolutions.clear();

	for(const auto & it : conf.guiOptions)
	{
		const Point dimensions(it.first.first, it.first.second);

		if(isResolutionSupported(dimensions))
			selectableResolutions.push_back(dimensions);
	}

	boost::range::sort(selectableResolutions, [](const auto & left, const auto & right)
	{
		return left.x * left.y < right.x * right.y;
	});
}