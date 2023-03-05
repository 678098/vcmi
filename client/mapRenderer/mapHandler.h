/*
 * mapHandler.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../gui/CIntObject.h"

#include "../../lib/int3.h"
#include "../../lib/spells/ViewSpellInt.h"
#include "../../lib/Rect.h"


#ifdef IN
#undef IN
#endif

#ifdef OUT
#undef OUT
#endif

VCMI_LIB_NAMESPACE_BEGIN

class CGObjectInstance;
class CGHeroInstance;
class CGBoat;
class CMap;
struct TerrainTile;
class PlayerColor;

VCMI_LIB_NAMESPACE_END

struct SDL_Surface;
class CAnimation;
class IImage;
class CMapHandler;
class IMapObjectObserver;

class CMapHandler
{
	const CMap * map;
	std::vector<IMapObjectObserver *> observers;

public:
	explicit CMapHandler(const CMap * map);

	const CMap * getMap();

	/// returns true if tile is within map bounds
	bool isInMap(const int3 & tile);

	/// see MapObjectObserver interface
	void onObjectFadeIn(const CGObjectInstance * obj);
	void onObjectFadeOut(const CGObjectInstance * obj);
	void onObjectInstantAdd(const CGObjectInstance * obj);
	void onObjectInstantRemove(const CGObjectInstance * obj);
	void onHeroTeleported(const CGHeroInstance * obj, const int3 & from, const int3 & dest);
	void onHeroMoved(const CGHeroInstance * obj, const int3 & from, const int3 & dest);
	void onHeroRotated(const CGHeroInstance * obj, const int3 & from, const int3 & dest);

	/// Add object to receive notifications on any changes in visible map state
	void addMapObserver(IMapObjectObserver * observer);
	void removeMapObserver(IMapObjectObserver * observer);

	/// returns string description for terrain interaction
	void getTerrainDescr(const int3 & pos, std::string & out, bool isRMB) const;

	/// returns list of ambient sounds for specified tile
	std::vector<std::string> getAmbientSounds(const int3 & tile);

	/// determines if the map is ready to handle new hero movement (not available during fading animations)
	bool hasOngoingAnimations();

	/// blocking wait until all ongoing animatins are over
	void waitForOngoingAnimations();

	static bool compareObjectBlitOrder(const CGObjectInstance * a, const CGObjectInstance * b);
};
