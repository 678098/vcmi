/*
 * MapView.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"
#include "MapView.h"

#include <utility>

#include "MapRenderer.h"
#include "mapHandler.h"
#include "CAdvMapInt.h"

#include "../CGameInfo.h"
#include "../CMusicHandler.h"
#include "../CPlayerInterface.h"
#include "../gui/CGuiHandler.h"
#include "../render/CAnimation.h"
#include "../render/Canvas.h"
#include "../render/Colors.h"
#include "../render/Graphics.h"
#include "../render/IImage.h"
#include "../renderSDL/SDL_Extensions.h"

#include "../../CCallback.h"

#include "../../lib/CConfigHandler.h"
#include "../../lib/CGeneralTextHandler.h"
#include "../../lib/CRandomGenerator.h"
#include "../../lib/CStopWatch.h"
#include "../../lib/Color.h"
#include "../../lib/RiverHandler.h"
#include "../../lib/RoadHandler.h"
#include "../../lib/TerrainHandler.h"
#include "../../lib/mapObjects/CGHeroInstance.h"
#include "../../lib/mapObjects/CObjectClassesHandler.h"
#include "../../lib/mapping/CMap.h"

MapViewCache::~MapViewCache() = default;

MapViewCache::MapViewCache(const std::shared_ptr<MapViewModel> & model)
	: model(model)
	, mapRenderer(new MapRenderer())
	, terrain(new Canvas(model->getCacheDimensionsPixels()))
{
}

Canvas MapViewCache::getTile(const int3 & coordinates)
{
	return Canvas(*terrain, model->getCacheTileArea(coordinates));
}

void MapViewCache::updateTile(const std::shared_ptr<MapRendererContext> & context, const int3 & coordinates)
{
	Canvas target = getTile(coordinates);

	mapRenderer->renderTile(*context, target, coordinates);
}

void MapViewCache::update(const std::shared_ptr<MapRendererContext> & context)
{
	Rect dimensions = model->getTilesTotalRect();

	for(int y = dimensions.top(); y < dimensions.bottom(); ++y)
		for(int x = dimensions.left(); x < dimensions.right(); ++x)
			updateTile(context, {x, y, model->getLevel()});
}

void MapViewCache::render(Canvas & target)
{
	Rect dimensions = model->getTilesTotalRect();

	for(int y = dimensions.top(); y < dimensions.bottom(); ++y)
	{
		for(int x = dimensions.left(); x < dimensions.right(); ++x)
		{
			int3 tile(x, y, model->getLevel());
			Canvas source = getTile(tile);
			Rect targetRect = model->getTargetTileArea(tile);
			target.draw(source, targetRect.topLeft());
		}
	}
}

std::shared_ptr<MapViewModel> MapView::createModel(const Point & dimensions) const
{
	auto result = std::make_shared<MapViewModel>();

	result->setLevel(0);
	result->setTileSize(Point(32, 32));
	result->setViewCenter(Point(0, 0));
	result->setViewDimensions(dimensions);

	return result;
}

MapView::MapView(const Point & offset, const Point & dimensions)
	: model(createModel(dimensions))
	, context(new MapRendererContext())
	, controller(new MapViewController(context, model))
	, tilesCache(new MapViewCache(model))
{
	pos += offset;
	pos.w = dimensions.x;
	pos.h = dimensions.y;
}

void MapView::show(SDL_Surface * to)
{
	Canvas target(to);
	Canvas targetClipped(target, pos);

	CSDL_Ext::CClipRectGuard guard(to, pos);

	controller->update(GH.mainFPSmng->getElapsedMilliseconds());
	tilesCache->update(context);
	tilesCache->render(targetClipped);
}

void MapView::showAll(SDL_Surface * to)
{
	show(to);
}

int3 MapRendererContext::getMapSize() const
{
	return LOCPLINT->cb->getMapSize();
}

bool MapRendererContext::isInMap(const int3 & coordinates) const
{
	return LOCPLINT->cb->isInTheMap(coordinates);
}

const TerrainTile & MapRendererContext::getMapTile(const int3 & coordinates) const
{
	return CGI->mh->getMap()->getTile(coordinates);
}

const CGObjectInstance * MapRendererContext::getObject(ObjectInstanceID objectID) const
{
	return CGI->mh->getMap()->objects.at(objectID.getNum());
}

bool MapRendererContext::isVisible(const int3 & coordinates) const
{
	return LOCPLINT->cb->isVisible(coordinates) || settings["session"]["spectate"].Bool();
}

const CGPath * MapRendererContext::currentPath() const
{
	const auto * hero = adventureInt->curHero();

	if(!hero)
		return nullptr;

	if(!LOCPLINT->paths.hasPath(hero))
		return nullptr;

	return &LOCPLINT->paths.getPath(hero);
}

uint32_t MapRendererContext::getAnimationPeriod() const
{
	// H3 timing for adventure map objects animation is 180 ms
	// Terrain animations also use identical interval, however those are only present in HotA and/or HD Mod
	// TODO: duration of fade-in/fade-out for teleport, entering/leaving boat, removal of objects
	// TOOD: duration of hero movement animation, frame timing of hero movement animation, effect of hero speed option
	// TOOD: duration of enemy hero movement animation, frame timing of enemy hero movement animation, effect of enemy hero speed option
	return 180;
}

uint32_t MapRendererContext::getAnimationTime() const
{
	return animationTime;
}

Point MapRendererContext::getTileSize() const
{
	return Point(32, 32);
}

bool MapRendererContext::showGrid() const
{
	return true; // settings["session"]["showGrid"].Bool();
}

void MapViewController::setViewCenter(const int3 & position)
{
	model->setViewCenter(Point(position.x, position.y) * model->getSingleTileSize());
	model->setLevel(position.z);
}

void MapViewController::setViewCenter(const Point & position, int level)
{
	model->setViewCenter(position);
	model->setLevel(level);
}

void MapViewController::setTileSize(const Point & tileSize)
{
	model->setTileSize(tileSize);
}

std::shared_ptr<const MapViewModel> MapView::getModel() const
{
	return model;
}

void MapViewModel::setTileSize(const Point & newValue)
{
	tileSize = newValue;
}

void MapViewModel::setViewCenter(const Point & newValue)
{
	viewCenter = newValue;
}

void MapViewModel::setViewDimensions(const Point & newValue)
{
	viewDimensions = newValue;
}

void MapViewModel::setLevel(int newLevel)
{
	mapLevel = newLevel;
}

Point MapViewModel::getSingleTileSize() const
{
	return tileSize;
}

Point MapViewModel::getMapViewCenter() const
{
	return viewCenter;
}

Point MapViewModel::getPixelsVisibleDimensions() const
{
	return viewDimensions;
}

int MapViewModel::getLevel() const
{
	return mapLevel;
}

Point MapViewModel::getTilesVisibleDimensions() const
{
	// total number of potentially visible tiles is:
	// 1) number of completely visible tiles
	// 2) additional tile that might be partially visible from left/top size
	// 3) additional tile that might be partially visible from right/bottom size
	return {
		getPixelsVisibleDimensions().x / getSingleTileSize().x + 2,
		getPixelsVisibleDimensions().y / getSingleTileSize().y + 2,
	};
}

Rect MapViewModel::getTilesTotalRect() const
{
	return Rect(
		Point(getTileAtPoint(Point(0,0))),
		getTilesVisibleDimensions()
	);
}

int3 MapViewModel::getTileAtPoint(const Point & position) const
{
	Point topLeftOffset = getMapViewCenter() - getPixelsVisibleDimensions() / 2;

	Point absolutePosition = position + topLeftOffset;

	// NOTE: using division via double in order to use std::floor
	// which rounds to negative infinity and not towards zero (like integer division)
	return {
		static_cast<int>(std::floor(static_cast<double>(absolutePosition.x) / getSingleTileSize().x)),
		static_cast<int>(std::floor(static_cast<double>(absolutePosition.y) / getSingleTileSize().y)),
		getLevel()
	};
}

Point MapViewModel::getCacheDimensionsPixels() const
{
	return getTilesVisibleDimensions() * getSingleTileSize();
}

Rect MapViewModel::getCacheTileArea(const int3 & coordinates) const
{
	assert(mapLevel == coordinates.z);
	assert(getTilesVisibleDimensions().x + coordinates.x >= 0);
	assert(getTilesVisibleDimensions().y + coordinates.y >= 0);

	Point tileIndex{
		(getTilesVisibleDimensions().x + coordinates.x) % getTilesVisibleDimensions().x,
		(getTilesVisibleDimensions().y + coordinates.y) % getTilesVisibleDimensions().y
	};

	return Rect(tileIndex * tileSize, tileSize);
}

Rect MapViewModel::getTargetTileArea(const int3 & coordinates) const
{
	Point topLeftOffset = getMapViewCenter() - getPixelsVisibleDimensions() / 2;
	Point tilePosAbsolute = Point(coordinates) * tileSize;
	Point tilePosRelative = tilePosAbsolute - topLeftOffset;

	return Rect(tilePosRelative, tileSize);
}

MapRendererContext::MapRendererContext()
{
	auto mapSize = getMapSize();

	objects.resize(boost::extents[mapSize.z][mapSize.x][mapSize.y]);

	for(const auto & obj : CGI->mh->getMap()->objects)
		addObject(obj);
}

void MapRendererContext::addObject(const CGObjectInstance * obj)
{
	if(!obj)
		return;

	for(int fx = 0; fx < obj->getWidth(); ++fx)
	{
		for(int fy = 0; fy < obj->getHeight(); ++fy)
		{
			int3 currTile(obj->pos.x - fx, obj->pos.y - fy, obj->pos.z);

			if(isInMap(currTile) && obj->coveringAt(currTile.x, currTile.y))
			{
				auto & container = objects[currTile.z][currTile.x][currTile.y];

				container.push_back(obj->id);
				boost::range::sort(container, MapObjectsSorter(*this));
			}
		}
	}
}

void MapRendererContext::addMovingObject(const CGObjectInstance * object, const int3 & tileFrom, const int3 & tileDest)
{
	int xFrom = std::min(tileFrom.x, tileDest.x) - object->getWidth();
	int xDest = std::max(tileFrom.x, tileDest.x);
	int yFrom = std::min(tileFrom.y, tileDest.y) - object->getHeight();
	int yDest = std::max(tileFrom.y, tileDest.y);

	for(int x = xFrom; x <= xDest; ++x)
	{
		for(int y = yFrom; y <= yDest; ++y)
		{
			int3 currTile(x, y, object->pos.z);

			if(isInMap(currTile))
			{
				auto & container = objects[currTile.z][currTile.x][currTile.y];

				container.push_back(object->id);
				boost::range::sort(container, MapObjectsSorter(*this));
			}
		}
	}
}

void MapRendererContext::removeObject(const CGObjectInstance * object)
{
	for(int z = 0; z < getMapSize().z; z++)
		for(int x = 0; x < getMapSize().x; x++)
			for(int y = 0; y < getMapSize().y; y++)
				vstd::erase(objects[z][x][y], object->id);
}

const MapRendererContext::MapObjectsList & MapRendererContext::getObjects(const int3 & coordinates) const
{
	assert(isInMap(coordinates));
	return objects[coordinates.z][coordinates.x][coordinates.y];
}

size_t MapRendererContext::objectGroupIndex(ObjectInstanceID objectID) const
{
	const CGObjectInstance * obj = getObject(objectID);
	// TODO
	static const std::vector<size_t> moveGroups = {99, 10, 5, 6, 7, 8, 9, 12, 11};
	static const std::vector<size_t> idleGroups = {99, 13, 0, 1, 2, 3, 4, 15, 14};

	if(obj->ID == Obj::HERO)
	{
		const auto * hero = dynamic_cast<const CGHeroInstance *>(obj);
		if (movementAnimation && movementAnimation->target == objectID)
			return moveGroups[hero->moveDir];
		return idleGroups[hero->moveDir];
	}

	if(obj->ID == Obj::BOAT)
	{
		const auto * boat = dynamic_cast<const CGBoat *>(obj);
		if (movementAnimation && movementAnimation->target == objectID)
			return moveGroups[boat->direction];
		return idleGroups[boat->direction];
	}
	return 0;
}

Point MapRendererContext::objectImageOffset(ObjectInstanceID objectID, const int3 & coordinates) const
{
	if (movementAnimation && movementAnimation->target == objectID)
	{
		int3 offsetTilesFrom = movementAnimation->tileFrom - coordinates;
		int3 offsetTilesDest = movementAnimation->tileDest - coordinates;

		Point offsetPixelsFrom = Point(offsetTilesFrom) * Point(32,32);
		Point offsetPixelsDest = Point(offsetTilesDest) * Point(32,32);

		Point result = vstd::lerp(offsetPixelsFrom, offsetPixelsDest, movementAnimation->progress);

		return result;
	}

	const CGObjectInstance * object = getObject(objectID);
	int3 offsetTiles(object->getPosition() - coordinates);
	return Point(offsetTiles) * Point(32, 32);
}

double MapRendererContext::objectTransparency(ObjectInstanceID objectID) const
{
	if (fadeOutAnimation && objectID == fadeOutAnimation->target)
		return 1.0 - fadeOutAnimation->progress;

	if (fadeInAnimation && objectID == fadeInAnimation->target)
		return fadeInAnimation->progress;

	return 1.0;
}

MapViewController::MapViewController(std::shared_ptr<MapRendererContext> context, std::shared_ptr<MapViewModel> model)
	: context(std::move(context))
	, model(std::move(model))
{
}

void MapViewController::update(uint32_t timeDelta)
{
	static const double fadeOutDuration = 1.0;
	static const double fadeInDuration = 1.0;
	static const double heroMoveDuration = 1.0;
	static const double heroTeleportDuration = 1.0;

	//FIXME: remove code duplication?

	if (context->movementAnimation)
	{
		context->movementAnimation->progress += heroMoveDuration * timeDelta / 1000;

		Point positionFrom = Point(context->movementAnimation->tileFrom) * model->getSingleTileSize();
		Point positionDest = Point(context->movementAnimation->tileDest) * model->getSingleTileSize();

		Point positionCurr = vstd::lerp(positionFrom, positionDest, context->movementAnimation->progress);

		setViewCenter(positionCurr, context->movementAnimation->tileDest.z);

		if (context->movementAnimation->progress >= 1.0)
		{
			setViewCenter(context->movementAnimation->tileDest);

			context->removeObject(context->getObject(context->movementAnimation->target));
			context->addObject(context->getObject(context->movementAnimation->target));
			context->movementAnimation.reset();
		}
	}

	if (context->teleportAnimation)
	{
		context->teleportAnimation->progress += heroTeleportDuration * timeDelta / 1000;
		if (context->teleportAnimation->progress >= 1.0)
			context->teleportAnimation.reset();
	}

	if (context->fadeOutAnimation)
	{
		context->fadeOutAnimation->progress += fadeOutDuration * timeDelta / 1000;
		if (context->fadeOutAnimation->progress >= 1.0)
		{
			context->removeObject(context->getObject(context->fadeOutAnimation->target));
			context->fadeOutAnimation.reset();
		}
	}

	if (context->fadeInAnimation)
	{
		context->fadeInAnimation->progress += fadeInDuration * timeDelta / 1000;
		if (context->fadeInAnimation->progress >= 1.0)
			context->fadeInAnimation.reset();
	}

	context->animationTime += timeDelta;
	context->tileSize =model->getSingleTileSize();
}

void MapViewController::onObjectFadeIn(const CGObjectInstance * obj)
{
	assert(!context->fadeInAnimation);
	context->fadeInAnimation = FadingAnimationState{obj->id, 0.0};
	context->addObject(obj);
}

void MapViewController::onObjectFadeOut(const CGObjectInstance * obj)
{
	assert(!context->fadeOutAnimation);
	context->fadeOutAnimation = FadingAnimationState{obj->id, 0.0};
}

void MapViewController::onObjectInstantAdd(const CGObjectInstance * obj)
{
	context->addObject(obj);
};

void MapViewController::onObjectInstantRemove(const CGObjectInstance * obj)
{
	context->removeObject(obj);
};

void MapViewController::onHeroTeleported(const CGHeroInstance * obj, const int3 & from, const int3 & dest)
{
	assert(!context->teleportAnimation);
	context->teleportAnimation = HeroAnimationState{ obj->id, from, dest, 0.0 };
}

void MapViewController::onHeroMoved(const CGHeroInstance * obj, const int3 & from, const int3 & dest)
{
	assert(!context->movementAnimation);
	context->removeObject(obj);
	context->addMovingObject(obj, from, dest);
	context->movementAnimation = HeroAnimationState{ obj->id, from, dest, 0.0 };
}

void MapViewController::onHeroRotated(const CGHeroInstance * obj, const int3 & from, const int3 & dest)
{
	//TODO
}

std::shared_ptr<MapViewController> MapView::getController()
{
	return controller;
}

bool MapViewController::hasOngoingAnimations()
{
	if(context->movementAnimation)
		return true;

	if(context->teleportAnimation)
		return true;

	if(context->fadeOutAnimation)
		return true;

	if(context->fadeInAnimation)
		return true;

	return false;
}
