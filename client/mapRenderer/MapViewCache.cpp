/*
 * MapViewCache.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"
#include "MapViewCache.h"

#include "IMapRendererContext.h"
#include "MapRenderer.h"
#include "MapViewModel.h"

#include "../render/CAnimation.h"
#include "../render/Canvas.h"
#include "../render/IImage.h"

#include "../../lib/mapObjects/CObjectHandler.h"

MapViewCache::~MapViewCache() = default;

MapViewCache::MapViewCache(const std::shared_ptr<MapViewModel> & model)
	: model(model)
	, cachedLevel(0)
	, mapRenderer(new MapRenderer())
	, iconsStorage(new CAnimation("VwSymbol"))
	, intermediate(new Canvas(Point(32, 32)))
	, terrain(new Canvas(model->getCacheDimensionsPixels()))
{
	iconsStorage->preload();
	for(size_t i = 0; i < iconsStorage->size(); ++i)
		iconsStorage->getImage(i)->setBlitMode(EImageBlitMode::COLORKEY);

	Point visibleSize = model->getTilesVisibleDimensions();
	terrainChecksum.resize(boost::extents[visibleSize.x][visibleSize.y]);
	tilesUpToDate.resize(boost::extents[visibleSize.x][visibleSize.y]);
}

Canvas MapViewCache::getTile(const int3 & coordinates)
{
	return Canvas(*terrain, model->getCacheTileArea(coordinates));
}

std::shared_ptr<IImage> MapViewCache::getOverlayImageForTile(const std::shared_ptr<IMapRendererContext> & context, const int3 & coordinates)
{
	size_t imageIndex = context->overlayImageIndex(coordinates);

	if(imageIndex < iconsStorage->size())
		return iconsStorage->getImage(imageIndex);
	return nullptr;
}

void MapViewCache::updateTile(const std::shared_ptr<IMapRendererContext> & context, const int3 & coordinates)
{
	int cacheX = (terrainChecksum.shape()[0] + coordinates.x) % terrainChecksum.shape()[0];
	int cacheY = (terrainChecksum.shape()[1] + coordinates.y) % terrainChecksum.shape()[1];

	auto & oldCacheEntry = terrainChecksum[cacheX][cacheY];
	TileChecksum newCacheEntry;

	newCacheEntry.tileX = coordinates.x;
	newCacheEntry.tileY = coordinates.y;
	newCacheEntry.checksum = mapRenderer->getTileChecksum(*context, coordinates);

	if(cachedLevel == coordinates.z && oldCacheEntry == newCacheEntry && !context->tileAnimated(coordinates))
		return;

	Canvas target = getTile(coordinates);

	if(model->getSingleTileSize() == Point(32, 32))
	{
		mapRenderer->renderTile(*context, target, coordinates);
	}
	else
	{
		mapRenderer->renderTile(*context, *intermediate, coordinates);
		target.drawScaled(*intermediate, Point(0, 0), model->getSingleTileSize());
	}

	if (context->filterGrayscale())
		target.applyGrayscale();

	oldCacheEntry = newCacheEntry;
	tilesUpToDate[cacheX][cacheY] = false;
}

void MapViewCache::update(const std::shared_ptr<IMapRendererContext> & context)
{
	Rect dimensions = model->getTilesTotalRect();

	if(dimensions.w != terrainChecksum.shape()[0] || dimensions.h != terrainChecksum.shape()[1])
	{
		boost::multi_array<TileChecksum, 2> newCache;
		newCache.resize(boost::extents[dimensions.w][dimensions.h]);
		terrainChecksum.resize(boost::extents[dimensions.w][dimensions.h]);
		terrainChecksum = newCache;
	}

	if(dimensions.w != tilesUpToDate.shape()[0] || dimensions.h != tilesUpToDate.shape()[1])
	{
		boost::multi_array<bool, 2> newCache;
		newCache.resize(boost::extents[dimensions.w][dimensions.h]);
		tilesUpToDate.resize(boost::extents[dimensions.w][dimensions.h]);
		tilesUpToDate = newCache;
	}

	for(int y = dimensions.top(); y < dimensions.bottom(); ++y)
		for(int x = dimensions.left(); x < dimensions.right(); ++x)
			updateTile(context, {x, y, model->getLevel()});

	cachedLevel = model->getLevel();
}

void MapViewCache::render(const std::shared_ptr<IMapRendererContext> & context, Canvas & target, bool fullRedraw)
{
	bool mapMoved = (cachedPosition != model->getMapViewCenter());
	bool lazyUpdate = !mapMoved && !fullRedraw;

	Rect dimensions = model->getTilesTotalRect();

	for(int y = dimensions.top(); y < dimensions.bottom(); ++y)
	{
		for(int x = dimensions.left(); x < dimensions.right(); ++x)
		{
			int cacheX = (terrainChecksum.shape()[0] + x) % terrainChecksum.shape()[0];
			int cacheY = (terrainChecksum.shape()[1] + y) % terrainChecksum.shape()[1];
			int3 tile(x, y, model->getLevel());

			if(lazyUpdate && tilesUpToDate[cacheX][cacheY])
				continue;

			Canvas source = getTile(tile);
			Rect targetRect = model->getTargetTileArea(tile);
			target.draw(source, targetRect.topLeft());

			tilesUpToDate[cacheX][cacheY] = true;
		}
	}

	if(context->showOverlay())
	{
		for(int y = dimensions.top(); y < dimensions.bottom(); ++y)
		{
			for(int x = dimensions.left(); x < dimensions.right(); ++x)
			{
				int3 tile(x, y, model->getLevel());
				Rect targetRect = model->getTargetTileArea(tile);
				auto overlay = getOverlayImageForTile(context, tile);

				if(overlay)
				{
					Point position = targetRect.center() - overlay->dimensions() / 2;
					target.draw(overlay, position);
				}
			}
		}
	}

	cachedPosition = model->getMapViewCenter();
}
