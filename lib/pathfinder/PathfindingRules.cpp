/*
 * PathfindingRules.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "PathfindingRules.h"

#include "CGPathNode.h"
#include "CPathfinder.h"
#include "INodeStorage.h"
#include "PathfinderOptions.h"

#include "../mapObjects/CGHeroInstance.h"
#include "../mapObjects/MiscObjects.h"
#include "../mapping/CMapDefines.h"

VCMI_LIB_NAMESPACE_BEGIN

void MovementCostRule::process(
	const PathNodeInfo & source,
	CDestinationNodeInfo & destination,
	const PathfinderConfig * pathfinderConfig,
	CPathfinderHelper * pathfinderHelper) const
{
	float costAtNextTile = destination.cost;
	int turnAtNextTile = destination.turn;
	int moveAtNextTile = destination.movementLeft;
	int cost = pathfinderHelper->getMovementCost(source, destination, moveAtNextTile);
	int remains = moveAtNextTile - cost;
	int sourceLayerMaxMovePoints = pathfinderHelper->getMaxMovePoints(source.node->layer);

	if(remains < 0)
	{
		//occurs rarely, when hero with low movepoints tries to leave the road
		costAtNextTile += static_cast<float>(moveAtNextTile) / sourceLayerMaxMovePoints;//we spent all points of current turn
		pathfinderHelper->updateTurnInfo(++turnAtNextTile);

		int destinationLayerMaxMovePoints = pathfinderHelper->getMaxMovePoints(destination.node->layer);

		moveAtNextTile = destinationLayerMaxMovePoints;

		cost = pathfinderHelper->getMovementCost(source, destination, moveAtNextTile); //cost must be updated, movement points changed :(
		remains = moveAtNextTile - cost;
	}

	if(destination.action == CGPathNode::EMBARK || destination.action == CGPathNode::DISEMBARK)
	{
		/// FREE_SHIP_BOARDING bonus only remove additional penalty
		/// land <-> sail transition still cost movement points as normal movement
		remains = pathfinderHelper->movementPointsAfterEmbark(moveAtNextTile, cost, (destination.action == CGPathNode::DISEMBARK));
		cost = moveAtNextTile - remains;
	}

	costAtNextTile += static_cast<float>(cost) / sourceLayerMaxMovePoints;

	destination.cost = costAtNextTile;
	destination.turn = turnAtNextTile;
	destination.movementLeft = remains;

	if(destination.isBetterWay() &&
		((source.node->turns == turnAtNextTile && remains) || pathfinderHelper->passOneTurnLimitCheck(source)))
	{
		pathfinderConfig->nodeStorage->commit(destination, source);

		return;
	}

	destination.blocked = true;
}

void PathfinderBlockingRule::process(
	const PathNodeInfo & source,
	CDestinationNodeInfo & destination,
	const PathfinderConfig * pathfinderConfig,
	CPathfinderHelper * pathfinderHelper) const
{
	auto blockingReason = getBlockingReason(source, destination, pathfinderConfig, pathfinderHelper);

	destination.blocked = blockingReason != BlockingReason::NONE;
}

void DestinationActionRule::process(
	const PathNodeInfo & source,
	CDestinationNodeInfo & destination,
	const PathfinderConfig * pathfinderConfig,
	CPathfinderHelper * pathfinderHelper) const
{
	if(destination.action != CGPathNode::ENodeAction::UNKNOWN)
	{
#ifdef VCMI_TRACE_PATHFINDER
		logAi->trace("Accepted precalculated action at %s", destination.coord.toString());
#endif
		return;
	}

	CGPathNode::ENodeAction action = CGPathNode::NORMAL;
	const auto * hero = pathfinderHelper->hero;

	switch(destination.node->layer)
	{
	case EPathfindingLayer::LAND:
		if(source.node->layer == EPathfindingLayer::SAIL)
		{
			// TODO: Handle dismebark into guarded areaa
			action = CGPathNode::DISEMBARK;
			break;
		}

		/// don't break - next case shared for both land and sail layers
		[[fallthrough]];

	case EPathfindingLayer::SAIL:
		if(destination.isNodeObjectVisitable())
		{
			auto objRel = destination.objectRelations;

			if(destination.nodeObject->ID == Obj::BOAT)
				action = CGPathNode::EMBARK;
			else if(destination.nodeHero)
			{
				if(destination.heroRelations == PlayerRelations::ENEMIES)
					action = CGPathNode::BATTLE;
				else
					action = CGPathNode::BLOCKING_VISIT;
			}
			else if(destination.nodeObject->ID == Obj::TOWN)
			{
				if(destination.nodeObject->passableFor(hero->tempOwner))
					action = CGPathNode::VISIT;
				else if(objRel == PlayerRelations::ENEMIES)
					action = CGPathNode::BATTLE;
			}
			else if(destination.nodeObject->ID == Obj::GARRISON || destination.nodeObject->ID == Obj::GARRISON2)
			{
				if(destination.nodeObject->passableFor(hero->tempOwner))
				{
					if(destination.guarded)
						action = CGPathNode::BATTLE;
				}
				else if(objRel == PlayerRelations::ENEMIES)
					action = CGPathNode::BATTLE;
			}
			else if(destination.nodeObject->ID == Obj::BORDER_GATE)
			{
				if(destination.nodeObject->passableFor(hero->tempOwner))
				{
					if(destination.guarded)
						action = CGPathNode::BATTLE;
				}
				else
					action = CGPathNode::BLOCKING_VISIT;
			}
			else if(destination.isGuardianTile)
				action = CGPathNode::BATTLE;
			else if(destination.nodeObject->blockVisit && !(pathfinderConfig->options.useCastleGate && destination.nodeObject->ID == Obj::TOWN))
				action = CGPathNode::BLOCKING_VISIT;

			if(action == CGPathNode::NORMAL)
			{
				if(destination.guarded)
					action = CGPathNode::BATTLE;
				else
					action = CGPathNode::VISIT;
			}
		}
		else if(destination.guarded)
			action = CGPathNode::BATTLE;

		break;
	}

	destination.action = action;
}

void MovementAfterDestinationRule::process(
	const PathNodeInfo & source,
	CDestinationNodeInfo & destination,
	const PathfinderConfig * config,
	CPathfinderHelper * pathfinderHelper) const
{
	auto blocker = getBlockingReason(source, destination, config, pathfinderHelper);

	if(blocker == BlockingReason::DESTINATION_GUARDED && destination.action == CGPathNode::ENodeAction::BATTLE)
	{
		return; // allow bypass guarded tile but only in direction of guard, a bit UI related thing
	}

	destination.blocked = blocker != BlockingReason::NONE;
}


PathfinderBlockingRule::BlockingReason MovementAfterDestinationRule::getBlockingReason(
	const PathNodeInfo & source,
	const CDestinationNodeInfo & destination,
	const PathfinderConfig * config,
	const CPathfinderHelper * pathfinderHelper) const
{
	switch(destination.action)
	{
	/// TODO: Investigate what kind of limitation is possible to apply on movement from visitable tiles
	/// Likely in many cases we don't need to add visitable tile to queue when hero doesn't fly
	case CGPathNode::VISIT:
	{
		/// For now we only add visitable tile into queue when it's teleporter that allow transit
		/// Movement from visitable tile when hero is standing on it is possible into any layer
		const auto * objTeleport = dynamic_cast<const CGTeleport *>(destination.nodeObject);
		if(pathfinderHelper->isAllowedTeleportEntrance(objTeleport))
		{
			/// For now we'll always allow transit over teleporters
			/// Transit over whirlpools only allowed when hero is protected
			return BlockingReason::NONE;
		}
		else if(destination.nodeObject->ID == Obj::GARRISON
			|| destination.nodeObject->ID == Obj::GARRISON2
			|| destination.nodeObject->ID == Obj::BORDER_GATE)
		{
			/// Transit via unguarded garrisons is always possible
			return BlockingReason::NONE;
		}

		return BlockingReason::DESTINATION_VISIT;
	}

	case CGPathNode::BLOCKING_VISIT:
		return destination.guarded
			? BlockingReason::DESTINATION_GUARDED
			: BlockingReason::DESTINATION_BLOCKVIS;

	case CGPathNode::NORMAL:
		return BlockingReason::NONE;

	case CGPathNode::EMBARK:
		if(pathfinderHelper->options.useEmbarkAndDisembark)
			return BlockingReason::NONE;

		return BlockingReason::DESTINATION_BLOCKED;

	case CGPathNode::DISEMBARK:
		if(pathfinderHelper->options.useEmbarkAndDisembark)
			return destination.guarded ? BlockingReason::DESTINATION_GUARDED : BlockingReason::NONE;

		return BlockingReason::DESTINATION_BLOCKED;

	case CGPathNode::BATTLE:
		/// Movement after BATTLE action only possible from guarded tile to guardian tile
		if(destination.guarded)
			return BlockingReason::DESTINATION_GUARDED;

		break;
	}

	return BlockingReason::DESTINATION_BLOCKED;
}


PathfinderBlockingRule::BlockingReason MovementToDestinationRule::getBlockingReason(
	const PathNodeInfo & source,
	const CDestinationNodeInfo & destination,
	const PathfinderConfig * pathfinderConfig,
	const CPathfinderHelper * pathfinderHelper) const
{

	if(destination.node->accessible == CGPathNode::BLOCKED)
		return BlockingReason::DESTINATION_BLOCKED;

	switch(destination.node->layer)
	{
	case EPathfindingLayer::LAND:
		if(!pathfinderHelper->canMoveBetween(source.coord, destination.coord))
			return BlockingReason::DESTINATION_BLOCKED;

		if(source.guarded)
		{
			if(!(pathfinderConfig->options.originalMovementRules && source.node->layer == EPathfindingLayer::AIR) &&
				!destination.isGuardianTile) // Can step into tile of guard
			{
				return BlockingReason::SOURCE_GUARDED;
			}
		}

		break;

	case EPathfindingLayer::SAIL:
		if(!pathfinderHelper->canMoveBetween(source.coord, destination.coord))
			return BlockingReason::DESTINATION_BLOCKED;

		if(source.guarded)
		{
			// Hero embarked a boat standing on a guarded tile -> we must allow to move away from that tile
			if(source.node->action != CGPathNode::EMBARK && !destination.isGuardianTile)
				return BlockingReason::SOURCE_GUARDED;
		}

		if(source.node->layer == EPathfindingLayer::LAND)
		{
			if(!destination.isNodeObjectVisitable())
				return BlockingReason::DESTINATION_BLOCKED;

			if(destination.nodeObject->ID != Obj::BOAT && !destination.nodeHero)
				return BlockingReason::DESTINATION_BLOCKED;
		}
		else if(destination.isNodeObjectVisitable() && destination.nodeObject->ID == Obj::BOAT)
		{
			/// Hero in boat can't visit empty boats
			return BlockingReason::DESTINATION_BLOCKED;
		}

		break;

	case EPathfindingLayer::WATER:
		if(!pathfinderHelper->canMoveBetween(source.coord, destination.coord)
			|| destination.node->accessible != CGPathNode::ACCESSIBLE)
		{
			return BlockingReason::DESTINATION_BLOCKED;
		}

		if(destination.guarded)
			return BlockingReason::DESTINATION_BLOCKED;

		break;
	}

	return BlockingReason::NONE;
}

void LayerTransitionRule::process(
	const PathNodeInfo & source,
	CDestinationNodeInfo & destination,
	const PathfinderConfig * pathfinderConfig,
	CPathfinderHelper * pathfinderHelper) const
{
	if(source.node->layer == destination.node->layer)
		return;

	switch(source.node->layer)
	{
	case EPathfindingLayer::LAND:
		if(destination.node->layer == EPathfindingLayer::SAIL)
		{
			/// Cannot enter empty water tile from land -> it has to be visitable
			if(destination.node->accessible == CGPathNode::ACCESSIBLE)
				destination.blocked = true;
		}

		break;

	case EPathfindingLayer::SAIL:
		//tile must be accessible -> exception: unblocked blockvis tiles -> clear but guarded by nearby monster coast
		if((destination.node->accessible != CGPathNode::ACCESSIBLE && (destination.node->accessible != CGPathNode::BLOCKVIS || destination.tile->blocked))
			|| destination.tile->visitable)  //TODO: passableness problem -> town says it's passable (thus accessible) but we obviously can't disembark onto town gate
		{
			destination.blocked = true;
		}

		break;

	case EPathfindingLayer::AIR:
		if(pathfinderConfig->options.originalMovementRules)
		{
			if((source.node->accessible != CGPathNode::ACCESSIBLE &&
				source.node->accessible != CGPathNode::VISITABLE) &&
				(destination.node->accessible != CGPathNode::VISITABLE &&
				 destination.node->accessible != CGPathNode::ACCESSIBLE))
			{
				destination.blocked = true;
			}
		}
		else if(destination.node->accessible != CGPathNode::ACCESSIBLE)
		{
			/// Hero that fly can only land on accessible tiles
			if(destination.nodeObject)
				destination.blocked = true;
		}

		break;

	case EPathfindingLayer::WATER:
		if(destination.node->accessible != CGPathNode::ACCESSIBLE && destination.node->accessible != CGPathNode::VISITABLE)
		{
			/// Hero that walking on water can transit to accessible and visitable tiles
			/// Though hero can't interact with blocking visit objects while standing on water
			destination.blocked = true;
		}

		break;
	}
}

VCMI_LIB_NAMESPACE_END
