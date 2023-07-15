/*
 * HeroPoolProcessor.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "HeroPoolProcessor.h"

#include "CGameHandler.h"

#include "../lib/CHeroHandler.h"
#include "../lib/CPlayerState.h"
#include "../lib/GameSettings.h"
#include "../lib/NetPacks.h"
#include "../lib/StartInfo.h"
#include "../lib/mapObjects/CGTownInstance.h"
#include "../lib/gameState/CGameState.h"
#include "../lib/gameState/TavernHeroesPool.h"

HeroPoolProcessor::HeroPoolProcessor()
	: gameHandler(nullptr)
{
}

HeroPoolProcessor::HeroPoolProcessor(CGameHandler * gameHandler)
	: gameHandler(gameHandler)
{
}

void HeroPoolProcessor::onHeroSurrendered(const PlayerColor & color, const CGHeroInstance * hero)
{
	SetAvailableHero sah;
	sah.slotID = 0;
	sah.player = color;
	sah.hid = hero->subID;
	sah.army.clear();
	sah.army.setCreature(SlotID(0), hero->type->initialArmy.at(0).creature, 1);
	gameHandler->sendAndApply(&sah);
}

void HeroPoolProcessor::onHeroEscaped(const PlayerColor & color, const CGHeroInstance * hero)
{
	SetAvailableHero sah;
	sah.slotID = 0;
	sah.player = color;
	sah.hid = hero->subID;

	gameHandler->sendAndApply(&sah);
}

void HeroPoolProcessor::clearHeroFromSlot(const PlayerColor & color, TavernHeroSlot slot)
{
	SetAvailableHero sah;
	sah.player = color;
	sah.slotID = static_cast<int>(slot);
	sah.hid = HeroTypeID::NONE;
	gameHandler->sendAndApply(&sah);
}

void HeroPoolProcessor::selectNewHeroForSlot(const PlayerColor & color, TavernHeroSlot slot, bool needNativeHero, bool giveArmy)
{
	SetAvailableHero sah;
	sah.player = color;
	sah.slotID = static_cast<int>(slot);

	//first hero - native if possible, second hero -> any other class
	CGHeroInstance *h = pickHeroFor(needNativeHero, color, gameHandler->getPlayerSettings(color)->castle, gameHandler->getRandomGenerator(), nullptr);

	if (h)
	{
		sah.hid = h->subID;

		if (giveArmy)
		{
			h->initArmy(gameHandler->getRandomGenerator(), &sah.army);
		}
		else
		{
			sah.army.clear();
			sah.army.setCreature(SlotID(0), h->type->initialArmy[0].creature, 1);
		}
	}
	else
	{
		sah.hid = -1;
	}
	gameHandler->sendAndApply(&sah);
}

void HeroPoolProcessor::onNewWeek(const PlayerColor & color)
{
	clearHeroFromSlot(color, TavernHeroSlot::NATIVE);
	clearHeroFromSlot(color, TavernHeroSlot::RANDOM);
	selectNewHeroForSlot(color, TavernHeroSlot::NATIVE, true, true);
	selectNewHeroForSlot(color, TavernHeroSlot::RANDOM, false, true);
}

bool HeroPoolProcessor::hireHero(const CGObjectInstance *obj, const HeroTypeID & heroToRecruit, const PlayerColor & player)
{
	const PlayerState * playerState = gameHandler->getPlayerState(player);
	const CGTownInstance * town = gameHandler->getTown(obj->id);

	if (playerState->resources[EGameResID::GOLD] < GameConstants::HERO_GOLD_COST && gameHandler->complain("Not enough gold for buying hero!"))
		return false;

	if (gameHandler->getHeroCount(player, false) >= VLC->settings()->getInteger(EGameSettings::HEROES_PER_PLAYER_ON_MAP_CAP) && gameHandler->complain("Cannot hire hero, too many wandering heroes already!"))
		return false;

	if (gameHandler->getHeroCount(player, true) >= VLC->settings()->getInteger(EGameSettings::HEROES_PER_PLAYER_TOTAL_CAP) && gameHandler->complain("Cannot hire hero, too many heroes garrizoned and wandering already!"))
		return false;

	if(town) //tavern in town
	{
		if(!town->hasBuilt(BuildingID::TAVERN) && gameHandler->complain("No tavern!"))
			return false;

		if(town->visitingHero && gameHandler->complain("There is visiting hero - no place!"))
			return false;
	}

	if(obj->ID == Obj::TAVERN)
	{
		if(gameHandler->getTile(obj->visitablePos())->visitableObjects.back() != obj && gameHandler->complain("Tavern entry must be unoccupied!"))
			return false;
	}

	auto recruitableHeroes = gameHandler->gameState()->hpool->getHeroesFor(player);

	const CGHeroInstance * recruitedHero = nullptr;

	for(const auto & hero : recruitableHeroes)
	{
		if(hero->subID == heroToRecruit)
			recruitedHero = hero;
	}

	if(!recruitedHero)
	{
		gameHandler->complain("Hero is not available for hiring!");
		return false;
	}

	HeroRecruited hr;
	hr.tid = obj->id;
	hr.hid = recruitedHero->subID;
	hr.player = player;
	hr.tile = recruitedHero->convertFromVisitablePos(obj->visitablePos());
	if(gameHandler->getTile(hr.tile)->isWater())
	{
		//Create a new boat for hero
		gameHandler->createObject(obj->visitablePos(), Obj::BOAT, recruitedHero->getBoatType().getNum());

		hr.boatId = gameHandler->getTopObj(hr.tile)->id;
	}

	// apply netpack -> this will remove hired hero from tavern slot
	gameHandler->sendAndApply(&hr);

	if(recruitableHeroes[0] == recruitedHero)
		selectNewHeroForSlot(player, TavernHeroSlot::NATIVE, false, false);
	else
		selectNewHeroForSlot(player, TavernHeroSlot::RANDOM, false, false);

	gameHandler->giveResource(player, EGameResID::GOLD, -GameConstants::HERO_GOLD_COST);

	if(town)
	{
		gameHandler->visitCastleObjects(town, recruitedHero);
		gameHandler->giveSpells(town, recruitedHero);
	}
	return true;
}

std::set<const CHeroClass *> HeroPoolProcessor::findAvailableClassesFor(const PlayerColor & player) const
{
	std::set<const CHeroClass *> result;

	const auto & hpool = gameHandler->gameState()->hpool;
	FactionID factionID = gameHandler->getPlayerSettings(player)->castle;

	for(auto & elem : hpool->unusedHeroesFromPool())
	{
		bool heroAvailable = hpool->isHeroAvailableFor(elem.first, player);
		bool heroClassBanned = elem.second->type->heroClass->selectionProbability[factionID] == 0;

		if(heroAvailable && !heroClassBanned)
			result.insert(elem.second->type->heroClass);
	}

	return result;
}

std::set<CGHeroInstance *> HeroPoolProcessor::findAvailableHeroesFor(const PlayerColor & player, const CHeroClass * heroClass) const
{
	std::set<CGHeroInstance *> result;

	const auto & hpool = gameHandler->gameState()->hpool;

	for(auto & elem : hpool->unusedHeroesFromPool())
	{
		bool heroAvailable = hpool->isHeroAvailableFor(elem.first, player);
		bool heroClassMatches = elem.second->type->heroClass == heroClass;

		if(heroAvailable && heroClassMatches)
			result.insert(elem.second);
	}

	return result;
}

const CHeroClass * HeroPoolProcessor::pickClassFor(bool isNative, const PlayerColor & player, const FactionID & factionID, CRandomGenerator & rand) const
{
	if(player >= PlayerColor::PLAYER_LIMIT)
	{
		logGlobal->error("Cannot pick hero for player %d. Wrong owner!", player.getStr());
		return nullptr;
	}

	const auto & hpool = gameHandler->gameState()->hpool;
	const auto & currentTavern = hpool->getHeroesFor(player);

	std::set<const CHeroClass *> potentialClasses = findAvailableClassesFor(player);
	std::set<const CHeroClass *> possibleClasses;

	if(potentialClasses.empty())
	{
		logGlobal->error("There are no heroes available for player %s!", player.getStr());
		return nullptr;
	}

	for(const auto & heroClass : potentialClasses)
	{
		if (isNative && heroClass->faction != factionID)
			continue;

		bool hasSameClass = vstd::contains_if(currentTavern, [&](const CGHeroInstance * hero){
			return hero->type->heroClass == heroClass;
		});

		if (hasSameClass)
			continue;

		possibleClasses.insert(heroClass);
	}

	if (possibleClasses.empty())
	{
		logGlobal->error("Cannot pick native hero for %s. Picking any...", player.getStr());
		possibleClasses = potentialClasses;
	}

	int totalWeight = 0;
	for(const auto & heroClass : possibleClasses)
		totalWeight += heroClass->selectionProbability.at(factionID);

	int roll = rand.nextInt(totalWeight - 1);
	for(const auto & heroClass : possibleClasses)
	{
		roll -= heroClass->selectionProbability.at(factionID);
		if(roll < 0)
			return heroClass;
	}

	return *possibleClasses.rbegin();
}

CGHeroInstance * HeroPoolProcessor::pickHeroFor(bool isNative,
													 const PlayerColor & player,
													 const FactionID & factionID,
													 CRandomGenerator & rand,
													 const CHeroClass * bannedClass) const
{
	const CHeroClass * heroClass = pickClassFor(isNative, player, factionID, rand);

	if(!heroClass)
		return nullptr;

	std::set<CGHeroInstance *> possibleHeroes = findAvailableHeroesFor(player, heroClass);

	assert(!possibleHeroes.empty());
	if(possibleHeroes.empty())
		return nullptr;

	return *RandomGeneratorUtil::nextItem(possibleHeroes, rand);
}
