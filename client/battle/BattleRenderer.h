/*
 * BattleFieldController.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../../lib/battle/BattleHex.h"

class Canvas;
class BattleInterface;

enum class EBattleFieldLayer {
					   // confirmed ordering requirements:
	OBSTACLES     = 0,
	CORPSES       = 0,
	WALLS         = 1,
	HEROES        = 1,
	STACKS        = 1, // after corpses, obstacles
	BATTLEMENTS   = 2, // after stacks
	STACK_AMOUNTS = 2, // after stacks, obstacles, corpses
	EFFECTS       = 3, // after obstacles, battlements
};

class BattleRenderer
{
public:
	using RendererRef = Canvas &;
	using RenderFunctor = std::function<void(RendererRef)>;

private:
	BattleInterface & owner;

	struct RenderableInstance
	{
		RenderFunctor functor;
		EBattleFieldLayer layer;
		BattleHex tile;
	};
	std::vector<RenderableInstance> objects;

	void collectObjects();
	void sortObjects();
	void renderObjects(RendererRef targetCanvas);
public:
	BattleRenderer(BattleInterface & owner);

	void insert(EBattleFieldLayer layer, BattleHex tile, RenderFunctor functor);
	void execute(RendererRef targetCanvas);
};
