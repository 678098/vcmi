/*
 * CHeroBackpackWindow.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../widgets/CWindowWithArtifacts.h"
#include "CWindowObject.h"

class CFilledTexture;

class CHeroBackpackWindow : public CWindowObject, public CWindowWithArtifacts
{
public:
	CHeroBackpackWindow();
	CHeroBackpackWindow(const CGHeroInstance * hero);
	
protected:
	std::shared_ptr<CArtifactsOfHeroBackpack> arts;
	std::shared_ptr<CButton> quitButton;
	std::shared_ptr<CFilledTexture> stretchedBackground;
	const int windowMargin = 5;

	void init();
	void showAll(Canvas & to) override;
};

class CHeroQuickBackpackWindow : public CHeroBackpackWindow
{
public:
	CHeroQuickBackpackWindow(const CGHeroInstance * hero, ArtifactPosition targetSlot);

private:
	void showAll(Canvas & to) override;
};
