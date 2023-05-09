/*
 * GeneralOptionsTab.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "GeneralOptionsTab.h"

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
#include "render/IWindowHandler.h"


static void setIntSetting(std::string group, std::string field, int value)
{
	Settings entry = settings.write[group][field];
	entry->Float() = value;
}

static void setBoolSetting(std::string group, std::string field, bool value)
{
	Settings entry = settings.write[group][field];
	entry->Bool() = value;
}

static std::string scalingToEntryString( int scaling)
{
	return std::to_string(scaling) + '%';
}

static std::string scalingToLabelString( int scaling)
{
	std::string string = CGI->generaltexth->translate("vcmi.systemOptions.scalingButton.hover");
	boost::replace_all(string, "%p", std::to_string(scaling));

	return string;
}

static std::string resolutionToEntryString( int w, int h)
{
	std::string string = "%wx%h";

	boost::replace_all(string, "%w", std::to_string(w));
	boost::replace_all(string, "%h", std::to_string(h));

	return string;
}

static std::string resolutionToLabelString( int w, int h)
{
	std::string string = CGI->generaltexth->translate("vcmi.systemOptions.resolutionButton.hover");

	boost::replace_all(string, "%w", std::to_string(w));
	boost::replace_all(string, "%h", std::to_string(h));

	return string;
}

GeneralOptionsTab::GeneralOptionsTab()
		: InterfaceObjectConfigurable(),
		  onFullscreenChanged(settings.listen["video"]["fullscreen"])
{
	OBJ_CONSTRUCTION_CAPTURING_ALL_NO_DISPOSE;
	type |= REDRAW_PARENT;

	const JsonNode config(ResourceID("config/widgets/settings/generalOptionsTab.json"));
	addCallback("spellbookAnimationChanged", [](bool value)
	{
		setBoolSetting("video", "spellbookAnimation", value);
	});
	addCallback("setMusic", [this](int value)
	{
		setIntSetting("general", "music", value);
		widget<CSlider>("musicSlider")->redraw();

		auto targetLabel = widget<CLabel>("musicValueLabel");
		if (targetLabel)
			targetLabel->setText(std::to_string(value) + "%");
	});
	addCallback("setVolume", [this](int value)
	{
		setIntSetting("general", "sound", value);
		widget<CSlider>("soundVolumeSlider")->redraw();

		auto targetLabel = widget<CLabel>("soundValueLabel");
		if (targetLabel)
			targetLabel->setText(std::to_string(value) + "%");
	});
	//settings that do not belong to base game:
	addCallback("fullscreenChanged", [this](bool value)
	{
		setFullscreenMode(value);
	});
	addCallback("setGameResolution", [this](int dummyValue)
	{
		selectGameResolution();
	});
	addCallback("setGameScaling", [this](int dummyValue)
	{
		selectGameScaling();
	});
	addCallback("framerateChanged", [](bool value)
	{
		setBoolSetting("video", "showfps", value);
	});

	//moved from "other" tab that is disabled for now to avoid excessible tabs with barely any content
	addCallback("availableCreaturesAsDwellingChanged", [=](int value)
	{
		setBoolSetting("gameTweaks", "availableCreaturesAsDwellingLabel", value > 0);
	});

	addCallback("compactTownCreatureInfoChanged", [](bool value)
	{
		setBoolSetting("gameTweaks", "compactTownCreatureInfo", value);
	});

	build(config);

	const auto & currentResolution = settings["video"]["resolution"];

	std::shared_ptr<CLabel> resolutionLabel = widget<CLabel>("resolutionLabel");
	resolutionLabel->setText(resolutionToLabelString(currentResolution["width"].Integer(), currentResolution["height"].Integer()));

	std::shared_ptr<CLabel> scalingLabel = widget<CLabel>("scalingLabel");
	scalingLabel->setText(scalingToLabelString(currentResolution["scaling"].Integer()));

	std::shared_ptr<CToggleButton> spellbookAnimationCheckbox = widget<CToggleButton>("spellbookAnimationCheckbox");
	spellbookAnimationCheckbox->setSelected(settings["video"]["spellbookAnimation"].Bool());

	std::shared_ptr<CToggleButton> fullscreenCheckbox = widget<CToggleButton>("fullscreenCheckbox");
	fullscreenCheckbox->setSelected(settings["video"]["fullscreen"].Bool());
	onFullscreenChanged([&](const JsonNode &newState) //used when pressing F4 etc. to change fullscreen checkbox state
	{
		widget<CToggleButton>("fullscreenCheckbox")->setSelected(newState.Bool());
	});

	std::shared_ptr<CToggleButton> framerateCheckbox = widget<CToggleButton>("framerateCheckbox");
	framerateCheckbox->setSelected(settings["video"]["showfps"].Bool());

	std::shared_ptr<CSlider> musicSlider = widget<CSlider>("musicSlider");
	musicSlider->moveTo(CCS->musich->getVolume());

	std::shared_ptr<CSlider> volumeSlider = widget<CSlider>("soundVolumeSlider");
	volumeSlider->moveTo(CCS->soundh->getVolume());

	std::shared_ptr<CToggleGroup> creatureGrowthAsDwellingPicker = widget<CToggleGroup>("availableCreaturesAsDwellingPicker");
	creatureGrowthAsDwellingPicker->setSelected(settings["gameTweaks"]["availableCreaturesAsDwellingLabel"].Bool());

	std::shared_ptr<CToggleButton> compactTownCreatureInfo = widget<CToggleButton>("compactTownCreatureInfoCheckbox");
	compactTownCreatureInfo->setSelected(settings["gameTweaks"]["compactTownCreatureInfo"].Bool());

	std::shared_ptr<CLabel> musicVolumeLabel = widget<CLabel>("musicValueLabel");
	musicVolumeLabel->setText(std::to_string(CCS->musich->getVolume()) + "%");

	std::shared_ptr<CLabel> soundVolumeLabel = widget<CLabel>("soundValueLabel");
	soundVolumeLabel->setText(std::to_string(CCS->soundh->getVolume()) + "%");

#ifdef VCMI_MOBILE
	// On mobile platforms, VCMI always uses OS screen resolutions
	// Players can control UI size via "Interface Scaling" option instead
	std::shared_ptr<CButton> resolutionButton = widget<CButton>("resolutionButton");

	resolutionButton->disable();
	resolutionLabel->disable();
	fullscreenCheckbox->block(true);
#endif
}

void GeneralOptionsTab::selectGameResolution()
{
	supportedResolutions = GH.windowHandler().getSupportedResolutions();

	std::vector<std::string> items;
	size_t currentResolutionIndex = 0;
	size_t i = 0;
	for(const auto & it : supportedResolutions)
	{
		auto resolutionStr = resolutionToEntryString(it.x, it.y);
		if(widget<CLabel>("resolutionLabel")->getText() == resolutionToLabelString(it.x, it.y))
			currentResolutionIndex = i;

		items.push_back(std::move(resolutionStr));
		++i;
	}
	GH.pushIntT<CObjectListWindow>(items, nullptr,
								   CGI->generaltexth->translate("vcmi.systemOptions.resolutionMenu.hover"),
								   CGI->generaltexth->translate("vcmi.systemOptions.resolutionMenu.help"),
								   [this](int index)
								   {
									   setGameResolution(index);
								   },
								   currentResolutionIndex);
}

void GeneralOptionsTab::setGameResolution(int index)
{
	assert(index >= 0 && index < supportedResolutions.size());

	if ( index < 0 || index >= supportedResolutions.size() )
		return;

	Point resolution = supportedResolutions[index];

	Settings gameRes = settings.write["video"]["resolution"];
	gameRes["width"].Float() = resolution.x;
	gameRes["height"].Float() = resolution.y;

	widget<CLabel>("resolutionLabel")->setText(resolutionToLabelString(resolution.x, resolution.y));
}

void GeneralOptionsTab::setFullscreenMode(bool on)
{
	setBoolSetting("video", "fullscreen", on);
}

void GeneralOptionsTab::selectGameScaling()
{
	supportedScaling.clear();

	auto [minimalScaling, maximalScaling] = GH.windowHandler().getSupportedScalingRange();
	for (int i = 0; i <= static_cast<int>(maximalScaling); i += 10)
	{
		if (i >= static_cast<int>(minimalScaling))
			supportedScaling.push_back(i);
	}

	std::vector<std::string> items;
	size_t currentIndex = 0;
	size_t i = 0;
	for(const auto & it : supportedScaling)
	{
		auto resolutionStr = scalingToEntryString(it);
		if(widget<CLabel>("scalingLabel")->getText() == scalingToLabelString(it))
			currentIndex = i;

		items.push_back(std::move(resolutionStr));
		++i;
	}

	GH.pushIntT<CObjectListWindow>(
		items,
		nullptr,
		CGI->generaltexth->translate("vcmi.systemOptions.scalingMenu.hover"),
		CGI->generaltexth->translate("vcmi.systemOptions.scalingMenu.help"),
		[this](int index)
		{
			setGameScaling(index);
		},
		currentIndex
	);
}

void GeneralOptionsTab::setGameScaling(int index)
{
	assert(index >= 0 && index < supportedScaling.size());

	if ( index < 0 || index >= supportedScaling.size() )
		return;

	int scaling = supportedScaling[index];

	Settings gameRes = settings.write["video"]["resolution"];
	gameRes["scaling"].Float() = scaling;

	widget<CLabel>("scalingLabel")->setText(scalingToLabelString(scaling));
}
