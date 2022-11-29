/*
 * CBattleObstacleController.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

VCMI_LIB_NAMESPACE_BEGIN

struct BattleHex;
struct CObstacleInstance;

VCMI_LIB_NAMESPACE_END

struct SDL_Surface;
class IImage;
class CCanvas;
class CAnimation;
class CBattleInterface;
struct Point;

class CBattleObstacleController
{
	std::map<std::string, std::shared_ptr<CAnimation>> animationsCache;

	CBattleInterface * owner;

	std::map<si32, std::shared_ptr<CAnimation>> obstacleAnimations;

	std::shared_ptr<IImage> getObstacleImage(const CObstacleInstance & oi);
	Point getObstaclePosition(std::shared_ptr<IImage> image, const CObstacleInstance & obstacle);

public:
	CBattleObstacleController(CBattleInterface * owner);

	void obstaclePlaced(const CObstacleInstance & oi);
	void showObstacles(SDL_Surface *to, std::vector<std::shared_ptr<const CObstacleInstance>> &obstacles);
	void showAbsoluteObstacles(std::shared_ptr<CCanvas> canvas, const Point & offset);

	void showBattlefieldObjects(std::shared_ptr<CCanvas> canvas, const BattleHex & location );
};
