/*
 * NetPacksServer.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "ServerNetPackVisitors.h"

#include "CGameHandler.h"
#include "battles/BattleProcessor.h"
#include "processors/HeroPoolProcessor.h"
#include "processors/PlayerMessageProcessor.h"
#include "processors/TurnOrderProcessor.h"
#include "queries/QueriesProcessor.h"

#include "../lib/IGameCallback.h"
#include "../lib/mapObjects/CGTownInstance.h"
#include "../lib/gameState/CGameState.h"
#include "../lib/battle/BattleInfo.h"
#include "../lib/battle/BattleAction.h"
#include "../lib/battle/Unit.h"
#include "../lib/serializer/Connection.h"
#include "../lib/spells/CSpellHandler.h"
#include "../lib/spells/ISpellMechanics.h"
#include "../lib/serializer/Cast.h"

void ApplyGhNetPackVisitor::visitSaveGame(SaveGame & pack)
{
	gh.save(pack.fname);
	logGlobal->info("Game has been saved as %s", pack.fname);
	result = true;
}

void ApplyGhNetPackVisitor::visitEndTurn(EndTurn & pack)
{
	gh.throwIfWrongPlayer(&pack);
	result = gh.turnOrder->onPlayerEndsTurn(pack.player);
}

void ApplyGhNetPackVisitor::visitDismissHero(DismissHero & pack)
{
	gh.throwIfWrongOwner(&pack, pack.hid);
	result = gh.removeObject(gh.getObj(pack.hid));
}

void ApplyGhNetPackVisitor::visitMoveHero(MoveHero & pack)
{
	gh.throwIfWrongOwner(&pack, pack.hid);
	result = gh.moveHero(pack.hid, pack.dest, 0, pack.transit, pack.player);
}

void ApplyGhNetPackVisitor::visitCastleTeleportHero(CastleTeleportHero & pack)
{
	gh.throwIfWrongOwner(&pack, pack.hid);

	result = gh.teleportHero(pack.hid, pack.dest, pack.source, pack.player);
}

void ApplyGhNetPackVisitor::visitArrangeStacks(ArrangeStacks & pack)
{
	gh.throwIfWrongPlayer(&pack);
	result = gh.arrangeStacks(pack.id1, pack.id2, pack.what, pack.p1, pack.p2, pack.val, pack.player);
}

void ApplyGhNetPackVisitor::visitBulkMoveArmy(BulkMoveArmy & pack)
{
	gh.throwIfWrongPlayer(&pack);
	result = gh.bulkMoveArmy(pack.srcArmy, pack.destArmy, pack.srcSlot);
}

void ApplyGhNetPackVisitor::visitBulkSplitStack(BulkSplitStack & pack)
{
	gh.throwIfWrongPlayer(&pack);
	result = gh.bulkSplitStack(pack.src, pack.srcOwner, pack.amount);
}

void ApplyGhNetPackVisitor::visitBulkMergeStacks(BulkMergeStacks & pack)
{
	gh.throwIfWrongPlayer(&pack);
	result = gh.bulkMergeStacks(pack.src, pack.srcOwner);
}

void ApplyGhNetPackVisitor::visitBulkSmartSplitStack(BulkSmartSplitStack & pack)
{
	gh.throwIfWrongPlayer(&pack);
	result = gh.bulkSmartSplitStack(pack.src, pack.srcOwner);
}

void ApplyGhNetPackVisitor::visitDisbandCreature(DisbandCreature & pack)
{
	gh.throwIfWrongOwner(&pack, pack.id);
	result = gh.disbandCreature(pack.id, pack.pos);
}

void ApplyGhNetPackVisitor::visitBuildStructure(BuildStructure & pack)
{
	gh.throwIfWrongOwner(&pack, pack.tid);
	result = gh.buildStructure(pack.tid, pack.bid);
}

void ApplyGhNetPackVisitor::visitRecruitCreatures(RecruitCreatures & pack)
{
	gh.throwIfWrongPlayer(&pack);
	// ownership checks are inside recruitCreatures
	result = gh.recruitCreatures(pack.tid, pack.dst, pack.crid, pack.amount, pack.level, pack.player);
}

void ApplyGhNetPackVisitor::visitUpgradeCreature(UpgradeCreature & pack)
{
	gh.throwIfWrongOwner(&pack, pack.id);
	result = gh.upgradeCreature(pack.id, pack.pos, pack.cid);
}

void ApplyGhNetPackVisitor::visitGarrisonHeroSwap(GarrisonHeroSwap & pack)
{
	const CGTownInstance * town = gh.getTown(pack.tid);
	if(!gh.isPlayerOwns(&pack, pack.tid) && !(town->garrisonHero && gh.isPlayerOwns(&pack, town->garrisonHero->id)))
		gh.throwNotAllowedAction(&pack); //neither town nor garrisoned hero (if present) is ours
	result = gh.garrisonSwap(pack.tid);
}

void ApplyGhNetPackVisitor::visitExchangeArtifacts(ExchangeArtifacts & pack)
{
	gh.throwIfWrongPlayer(&pack, pack.src.owningPlayer()); //second hero can be ally
	result = gh.moveArtifact(pack.src, pack.dst);
}

void ApplyGhNetPackVisitor::visitBulkExchangeArtifacts(BulkExchangeArtifacts & pack)
{
	gh.throwIfWrongOwner(&pack, pack.srcHero);
	result = gh.bulkMoveArtifacts(pack.srcHero, pack.dstHero, pack.swap);
}

void ApplyGhNetPackVisitor::visitAssembleArtifacts(AssembleArtifacts & pack)
{
	gh.throwIfWrongOwner(&pack, pack.heroID);
	result = gh.assembleArtifacts(pack.heroID, pack.artifactSlot, pack.assemble, pack.assembleTo);
}

void ApplyGhNetPackVisitor::visitEraseArtifactByClient(EraseArtifactByClient & pack)
{
	gh.throwIfWrongPlayer(&pack, pack.al.owningPlayer());
	result = gh.eraseArtifactByClient(pack.al);
}

void ApplyGhNetPackVisitor::visitBuyArtifact(BuyArtifact & pack)
{
	gh.throwIfWrongOwner(&pack, pack.hid);
	result = gh.buyArtifact(pack.hid, pack.aid);
}

void ApplyGhNetPackVisitor::visitTradeOnMarketplace(TradeOnMarketplace & pack)
{
	gh.throwIfWrongPlayer(&pack);

	const CGObjectInstance * market = gh.getObj(pack.marketId);
	if(!market)
		gh.throwAndComplain(&pack, "Invalid market object");
	const CGHeroInstance * hero = gh.getHero(pack.heroId);

	//market must be owned or visited
	const IMarket * m = IMarket::castFrom(market);

	if(!m)
		gh.throwAndComplain(&pack, "market is not-a-market! :/");

	PlayerColor player = market->tempOwner;

	if(!player.isValidPlayer())
		player = gh.getTile(market->visitablePos())->visitableObjects.back()->tempOwner;

	if(!player.isValidPlayer())
		gh.throwAndComplain(&pack, "No player can use this market!");

	bool allyTownSkillTrade = (pack.mode == EMarketMode::RESOURCE_SKILL && gh.getPlayerRelations(player, hero->tempOwner) == PlayerRelations::ALLIES);

	if(hero && (!(player == hero->tempOwner || allyTownSkillTrade)
		|| hero->visitablePos() != market->visitablePos()))
		gh.throwAndComplain(&pack, "This hero can't use this marketplace!");

	if(!allyTownSkillTrade)
		gh.throwIfWrongPlayer(&pack, player);

	result = true;

	switch(pack.mode)
	{
	case EMarketMode::RESOURCE_RESOURCE:
		for(int i = 0; i < pack.r1.size(); ++i)
			result &= gh.tradeResources(m, pack.val[i], player, pack.r1[i], pack.r2[i]);
		break;
	case EMarketMode::RESOURCE_PLAYER:
		for(int i = 0; i < pack.r1.size(); ++i)
			result &= gh.sendResources(pack.val[i], player, GameResID(pack.r1[i]), PlayerColor(pack.r2[i]));
		break;
	case EMarketMode::CREATURE_RESOURCE:
		for(int i = 0; i < pack.r1.size(); ++i)
			result &= gh.sellCreatures(pack.val[i], m, hero, SlotID(pack.r1[i]), GameResID(pack.r2[i]));
		break;
	case EMarketMode::RESOURCE_ARTIFACT:
		for(int i = 0; i < pack.r1.size(); ++i)
			result &= gh.buyArtifact(m, hero, GameResID(pack.r1[i]), ArtifactID(pack.r2[i]));
		break;
	case EMarketMode::ARTIFACT_RESOURCE:
		for(int i = 0; i < pack.r1.size(); ++i)
			result &= gh.sellArtifact(m, hero, ArtifactInstanceID(pack.r1[i]), GameResID(pack.r2[i]));
		break;
	case EMarketMode::CREATURE_UNDEAD:
		for(int i = 0; i < pack.r1.size(); ++i)
			result &= gh.transformInUndead(m, hero, SlotID(pack.r1[i]));
		break;
	case EMarketMode::RESOURCE_SKILL:
		for(int i = 0; i < pack.r2.size(); ++i)
			result &= gh.buySecSkill(m, hero, SecondarySkill(pack.r2[i]));
		break;
	case EMarketMode::CREATURE_EXP:
	{
		std::vector<SlotID> slotIDs(pack.r1.begin(), pack.r1.end());
		std::vector<ui32> count(pack.val.begin(), pack.val.end());
		result = gh.sacrificeCreatures(m, hero, slotIDs, count);
		return;
	}
	case EMarketMode::ARTIFACT_EXP:
	{
		std::vector<ArtifactPosition> positions(pack.r1.begin(), pack.r1.end());
		result = gh.sacrificeArtifact(m, hero, positions);
		return;
	}
	default:
		gh.throwAndComplain(&pack, "Unknown exchange pack.mode!");
	}
}

void ApplyGhNetPackVisitor::visitSetFormation(SetFormation & pack)
{
	gh.throwIfWrongOwner(&pack, pack.hid);
	result = gh.setFormation(pack.hid, pack.formation);
}

void ApplyGhNetPackVisitor::visitHireHero(HireHero & pack)
{
	gh.throwIfWrongPlayer(&pack);

	result = gh.heroPool->hireHero(pack.tid, pack.hid, pack.player);
}

void ApplyGhNetPackVisitor::visitBuildBoat(BuildBoat & pack)
{
	gh.throwIfWrongPlayer(&pack);

	if(gh.getPlayerRelations(gh.getOwner(pack.objid), pack.player) == PlayerRelations::ENEMIES)
		gh.throwAndComplain(&pack, "Can't build boat at enemy shipyard");

	result = gh.buildBoat(pack.objid, pack.player);
}

void ApplyGhNetPackVisitor::visitQueryReply(QueryReply & pack)
{
	gh.throwIfWrongPlayer(&pack);

	auto playerToConnection = gh.connections.find(pack.player);
	if(playerToConnection == gh.connections.end())
		gh.throwAndComplain(&pack, "No such pack.player!");
	if(!vstd::contains(playerToConnection->second, pack.c))
		gh.throwAndComplain(&pack, "Message came from wrong connection!");
	if(pack.qid == QueryID(-1))
		gh.throwAndComplain(&pack, "Cannot answer the query with pack.id -1!");

	result = gh.queryReply(pack.qid, pack.reply, pack.player);
}

void ApplyGhNetPackVisitor::visitMakeAction(MakeAction & pack)
{
	gh.throwIfWrongPlayer(&pack);

	result = gh.battles->makePlayerBattleAction(pack.player, pack.ba);
}

void ApplyGhNetPackVisitor::visitDigWithHero(DigWithHero & pack)
{
	gh.throwIfWrongOwner(&pack, pack.id);
	result = gh.dig(gh.getHero(pack.id));
}

void ApplyGhNetPackVisitor::visitCastAdvSpell(CastAdvSpell & pack)
{
	gh.throwIfWrongOwner(&pack, pack.hid);

	const CSpell * s = pack.sid.toSpell();
	if(!s)
		gh.throwNotAllowedAction(&pack);
	const CGHeroInstance * h = gh.getHero(pack.hid);
	if(!h)
		gh.throwNotAllowedAction(&pack);

	AdventureSpellCastParameters p;
	p.caster = h;
	p.pos = pack.pos;

	result = s->adventureCast(gh.spellEnv, p);
}

void ApplyGhNetPackVisitor::visitPlayerMessage(PlayerMessage & pack)
{
	if(!pack.player.isSpectator()) // TODO: clearly not a great way to verify permissions
		gh.throwIfWrongPlayer(&pack, pack.player);
	
	gh.playerMessages->playerMessage(pack.player, pack.text, pack.currObj);
	result = true;
}
