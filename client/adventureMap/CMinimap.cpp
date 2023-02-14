/*
 * CMinimap.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"
#include "CMinimap.h"

#include "CAdvMapInt.h"
#include "CTerrainRect.h"

#include "../widgets/Images.h"
#include "../CGameInfo.h"
#include "../CPlayerInterface.h"
#include "../gui/CGuiHandler.h"
#include "../render/Colors.h"
#include "../renderSDL/SDL_PixelAccess.h"
#include "../windows/InfoWindows.h"

#include "../../CCallback.h"
#include "../../lib/CGeneralTextHandler.h"
#include "../../lib/TerrainHandler.h"
#include "../../lib/mapObjects/CGHeroInstance.h"
#include "../../lib/mapping/CMapDefines.h"

ColorRGBA CMinimapInstance::getTileColor(const int3 & pos) const
{
	const TerrainTile * tile = LOCPLINT->cb->getTile(pos, false);

	// if tile is not visible it will be black on minimap
	if(!tile)
		return CSDL_Ext::fromSDL(Colors::BLACK);

	// if object at tile is owned - it will be colored as its owner
	for (const CGObjectInstance *obj : tile->blockingObjects)
	{
		PlayerColor player = obj->getOwner();
		if(player == PlayerColor::NEUTRAL)
			return CSDL_Ext::fromSDL(*graphics->neutralColor);

		if (player < PlayerColor::PLAYER_LIMIT)
			return CSDL_Ext::fromSDL(graphics->playerColors[player.getNum()]);
	}

	if (tile->blocked && (!tile->visitable))
		return tile->terType->minimapBlocked;
	else
		return tile->terType->minimapUnblocked;
}

void CMinimapInstance::refreshTile(const int3 &tile)
{
	if (level == tile.z)
		minimap->drawPoint(Point(tile.x, tile.y), getTileColor(tile));
}

void CMinimapInstance::redrawMinimap()
{
	int3 mapSizes = LOCPLINT->cb->getMapSize();

	for (int y = 0; y < mapSizes.y; ++y)
		for (int x = 0; x < mapSizes.x; ++x)
			minimap->drawPoint(Point(x, y), getTileColor(int3(x, y, level)));
}

CMinimapInstance::CMinimapInstance(CMinimap *Parent, int Level):
	parent(Parent),
	minimap(new Canvas(Point(LOCPLINT->cb->getMapSize().x, LOCPLINT->cb->getMapSize().y))),
	level(Level)
{
	pos.w = parent->pos.w;
	pos.h = parent->pos.h;
	redrawMinimap();
}

void CMinimapInstance::showAll(SDL_Surface * to)
{
	Canvas target(to);
	target.draw(*minimap, pos.topLeft(), pos.dimensions());
}

CMinimap::CMinimap(const Rect & position)
	: CIntObject(LCLICK | RCLICK | HOVER | MOVE, position.topLeft()),
	level(0)
{
	OBJECT_CONSTRUCTION_CAPTURING(255-DISPOSE);
	pos.w = position.w;
	pos.h = position.h;

	aiShield = std::make_shared<CPicture>("AIShield");
	aiShield->disable();
}

int3 CMinimap::pixelToTile(const Point & cursorPos) const
{
	// 0 = top-left corner, 1 = bottom-right corner
	double dx = static_cast<double>(cursorPos.x) / pos.w;
	double dy = static_cast<double>(cursorPos.y) / pos.h;

	int3 mapSizes = LOCPLINT->cb->getMapSize();

	int tileX(std::round(mapSizes.x * dx));
	int tileY(std::round(mapSizes.y * dy));

	return int3(tileX, tileY, level);
}

Point CMinimap::tileToPixels(const int3 &tile) const
{
	int3 mapSizes = LOCPLINT->cb->getMapSize();

	double stepX = static_cast<double>(pos.w) / mapSizes.x;
	double stepY = static_cast<double>(pos.h) / mapSizes.y;

	int x = static_cast<int>(stepX * tile.x);
	int y = static_cast<int>(stepY * tile.y);

	return Point(x,y);
}

void CMinimap::moveAdvMapSelection()
{
	int3 newLocation = pixelToTile(GH.getCursorPosition() - pos.topLeft());
	adventureInt->centerOn(newLocation);

	if (!(adventureInt->active & GENERAL))
		GH.totalRedraw(); //redraw this as well as inactive adventure map
	else
		redraw();//redraw only this
}

void CMinimap::clickLeft(tribool down, bool previousState)
{
	if(down)
		moveAdvMapSelection();
}

void CMinimap::clickRight(tribool down, bool previousState)
{
	if (down)
		CRClickPopup::createAndPush(CGI->generaltexth->zelp[291].second);
}

void CMinimap::hover(bool on)
{
	if(on)
		GH.statusbar->write(CGI->generaltexth->zelp[291].first);
	else
		GH.statusbar->clear();
}

void CMinimap::mouseMoved(const Point & cursorPosition)
{
	if(mouseState(MouseButton::LEFT))
		moveAdvMapSelection();
}

void CMinimap::showAll(SDL_Surface * to)
{
	CIntObject::showAll(to);
	if(minimap)
	{
		Canvas target(to);

		int3 mapSizes = LOCPLINT->cb->getMapSize();
		int3 tileCountOnScreen = adventureInt->terrain->tileCountOnScreen();

		//draw radar
		Rect radar =
		{
			adventureInt->position.x * pos.w / mapSizes.x,
			adventureInt->position.y * pos.h / mapSizes.y,
			tileCountOnScreen.x * pos.w / mapSizes.x - 1,
			tileCountOnScreen.y * pos.h / mapSizes.y - 1
		};

		Canvas clippedTarget(target, pos);
		clippedTarget.drawBorderDashed(radar, CSDL_Ext::fromSDL(Colors::PURPLE));
	}
}

void CMinimap::update()
{
	if(aiShield->recActions & UPDATE) //AI turn is going on. There is no need to update minimap
		return;

	OBJECT_CONSTRUCTION_CUSTOM_CAPTURING(255-DISPOSE);
	minimap = std::make_shared<CMinimapInstance>(this, level);
	redraw();
}

void CMinimap::setLevel(int newLevel)
{
	if (level == newLevel)
		return;

	level = newLevel;
	update();
}

void CMinimap::setAIRadar(bool on)
{
	if(on)
	{
		aiShield->enable();
		minimap.reset();
	}
	else
	{
		aiShield->disable();
		update();
	}

	// this may happen during AI turn when this interface is inactive
	// force redraw in order to properly update interface
	GH.totalRedraw();
}

void CMinimap::updateTile(const int3 &pos)
{
	if(minimap)
		minimap->refreshTile(pos);
}

