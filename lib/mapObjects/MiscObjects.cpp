/*
 * MiscObjects.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"
#include "MiscObjects.h"

#include "../constants/StringConstants.h"
#include "../NetPacks.h"
#include "../CGeneralTextHandler.h"
#include "../CSoundBase.h"
#include "../CSkillHandler.h"
#include "../spells/CSpellHandler.h"
#include "../IGameCallback.h"
#include "../gameState/CGameState.h"
#include "../mapping/CMap.h"
#include "../CPlayerState.h"
#include "../serializer/JsonSerializeFormat.h"
#include "../mapObjectConstructors/AObjectTypeHandler.h"
#include "../mapObjectConstructors/CObjectClassesHandler.h"
#include "../modding/ModScope.h"

VCMI_LIB_NAMESPACE_BEGIN

std::map <si32, std::vector<ObjectInstanceID> > CGMagi::eyelist;
ui8 CGObelisk::obeliskCount = 0; //how many obelisks are on map
std::map<TeamID, ui8> CGObelisk::visited; //map: team_id => how many obelisks has been visited

///helpers
static std::string visitedTxt(const bool visited)
{
	int id = visited ? 352 : 353;
	return VLC->generaltexth->allTexts[id];
}

void CTeamVisited::setPropertyDer(ui8 what, ui32 val)
{
	if(what == CTeamVisited::OBJPROP_VISITED)
		players.insert(PlayerColor(val));
}

bool CTeamVisited::wasVisited(PlayerColor player) const
{
	return wasVisited(cb->getPlayerState(player)->team);
}

bool CTeamVisited::wasVisited(const CGHeroInstance * h) const
{
	return wasVisited(h->tempOwner);
}

bool CTeamVisited::wasVisited(const TeamID & team) const
{
	for(const auto & i : players)
	{
		if(cb->getPlayerState(i)->team == team)
			return true;
	}
	return false;
}

//CGMine
void CGMine::onHeroVisit( const CGHeroInstance * h ) const
{
	auto relations = cb->gameState()->getPlayerRelations(h->tempOwner, tempOwner);

	if(relations == PlayerRelations::SAME_PLAYER) //we're visiting our mine
	{
		cb->showGarrisonDialog(id,h->id,true);
		return;
	}
	else if (relations == PlayerRelations::ALLIES)//ally
		return;

	if(stacksCount()) //Mine is guarded
	{
		BlockingDialog ynd(true,false);
		ynd.player = h->tempOwner;
		ynd.text.appendLocalString(EMetaText::ADVOB_TXT, subID == 7 ? 84 : 187);
		cb->showBlockingDialog(&ynd);
		return;
	}

	flagMine(h->tempOwner);

}

void CGMine::newTurn(CRandomGenerator & rand) const
{
	if(cb->getDate() == 1)
		return;

	if (tempOwner == PlayerColor::NEUTRAL)
		return;

	cb->giveResource(tempOwner, producedResource, producedQuantity);
}

void CGMine::initObj(CRandomGenerator & rand)
{
	if(isAbandoned())
	{
		//set guardians
		int howManyTroglodytes = rand.nextInt(100, 199);
		auto * troglodytes = new CStackInstance(CreatureID::TROGLODYTES, howManyTroglodytes);
		putStack(SlotID(0), troglodytes);

		assert(!abandonedMineResources.empty());
		producedResource = *RandomGeneratorUtil::nextItem(abandonedMineResources, rand);
	}
	else
	{
		producedResource = GameResID(subID);
	}
	producedQuantity = defaultResProduction();
}

bool CGMine::isAbandoned() const
{
	return (subID >= 7);
}

std::string CGMine::getObjectName() const
{
	return VLC->generaltexth->translate("core.minename", subID);
}

std::string CGMine::getHoverText(PlayerColor player) const
{
	std::string hoverName = CArmedInstance::getHoverText(player);

	if (tempOwner != PlayerColor::NEUTRAL)
		hoverName += "\n(" + VLC->generaltexth->restypes[producedResource] + ")";

	if(stacksCount())
	{
		hoverName += "\n";
		hoverName += VLC->generaltexth->allTexts[202]; //Guarded by
		hoverName += " ";
		hoverName += getArmyDescription();
	}
	return hoverName;
}

void CGMine::flagMine(const PlayerColor & player) const
{
	assert(tempOwner != player);
	cb->setOwner(this, player); //not ours? flag it!

	InfoWindow iw;
	iw.type = EInfoWindowMode::AUTO;
	iw.soundID = soundBase::FLAGMINE;
	iw.text.appendLocalString(EMetaText::MINE_EVNTS, producedResource); //not use subID, abandoned mines uses default mine texts
	iw.player = player;
	iw.components.emplace_back(Component::EComponentType::RESOURCE, producedResource, producedQuantity, -1);
	cb->showInfoDialog(&iw);
}

ui32 CGMine::defaultResProduction() const
{
	switch(producedResource.toEnum())
	{
	case EGameResID::WOOD:
	case EGameResID::ORE:
		return 2;
	case EGameResID::GOLD:
		return 1000;
	default:
		return 1;
	}
}

void CGMine::battleFinished(const CGHeroInstance *hero, const BattleResult &result) const
{
	if(result.winner == 0) //attacker won
	{
		if(isAbandoned())
		{
			hero->showInfoDialog(85);
		}
		flagMine(hero->tempOwner);
	}
}

void CGMine::blockingDialogAnswered(const CGHeroInstance *hero, ui32 answer) const
{
	if(answer)
		cb->startBattleI(hero, this);
}

void CGMine::serializeJsonOptions(JsonSerializeFormat & handler)
{
	CArmedInstance::serializeJsonOptions(handler);

	if(isAbandoned())
	{
		if(handler.saving)
		{
			JsonNode node(JsonNode::JsonType::DATA_VECTOR);
			for(auto const & resID : abandonedMineResources)
			{
				JsonNode one(JsonNode::JsonType::DATA_STRING);
				one.String() = GameConstants::RESOURCE_NAMES[resID];
				node.Vector().push_back(one);
			}
			handler.serializeRaw("possibleResources", node, std::nullopt);
		}
		else
		{
			auto guard = handler.enterArray("possibleResources");
			const JsonNode & node = handler.getCurrent();

			auto names = node.convertTo<std::vector<std::string>>();

			for(const std::string & s : names)
			{
				int raw_res = vstd::find_pos(GameConstants::RESOURCE_NAMES, s);
				if(raw_res < 0)
					logGlobal->error("Invalid resource name: %s", s);
				else
					abandonedMineResources.emplace(raw_res);
			}
		}
	}
	else
	{
		serializeJsonOwner(handler);
	}
}

std::string CGResource::getHoverText(PlayerColor player) const
{
	return VLC->generaltexth->restypes[subID];
}

void CGResource::initObj(CRandomGenerator & rand)
{
	blockVisit = true;

	if(amount == CGResource::RANDOM_AMOUNT)
	{
		switch(static_cast<EGameResID>(subID))
		{
		case EGameResID::GOLD:
			amount = rand.nextInt(5, 10) * 100;
			break;
		case EGameResID::WOOD: case EGameResID::ORE:
			amount = rand.nextInt(6, 10);
			break;
		default:
			amount = rand.nextInt(3, 5);
			break;
		}
	}
}

void CGResource::onHeroVisit( const CGHeroInstance * h ) const
{
	if(stacksCount())
	{
		if(!message.empty())
		{
			BlockingDialog ynd(true,false);
			ynd.player = h->getOwner();
			ynd.text = message;
			cb->showBlockingDialog(&ynd);
		}
		else
		{
			blockingDialogAnswered(h, true); //behave as if player accepted battle
		}
	}
	else
		collectRes(h->getOwner());
}

void CGResource::collectRes(const PlayerColor & player) const
{
	cb->giveResource(player, static_cast<EGameResID>(subID), amount);
	InfoWindow sii;
	sii.player = player;
	if(!message.empty())
	{
		sii.type = EInfoWindowMode::AUTO;
		sii.text = message;
	}
	else
	{
		sii.type = EInfoWindowMode::INFO;
		sii.text.appendLocalString(EMetaText::ADVOB_TXT,113);
		sii.text.replaceLocalString(EMetaText::RES_NAMES, subID);
	}
	sii.components.emplace_back(Component::EComponentType::RESOURCE,subID,amount,0);
	sii.soundID = soundBase::pickup01 + CRandomGenerator::getDefault().nextInt(6);
	cb->showInfoDialog(&sii);
	cb->removeObject(this, player);
}

void CGResource::battleFinished(const CGHeroInstance *hero, const BattleResult &result) const
{
	if(result.winner == 0) //attacker won
		collectRes(hero->getOwner());
}

void CGResource::blockingDialogAnswered(const CGHeroInstance *hero, ui32 answer) const
{
	if(answer)
		cb->startBattleI(hero, this);
}

void CGResource::serializeJsonOptions(JsonSerializeFormat & handler)
{
	CArmedInstance::serializeJsonOptions(handler);
	if(!handler.saving && !handler.getCurrent()["guards"].Vector().empty())
		CCreatureSet::serializeJson(handler, "guards", 7);
	handler.serializeInt("amount", amount, 0);
	handler.serializeStruct("guardMessage", message);
}

bool CGTeleport::isEntrance() const
{
	return type == BOTH || type == ENTRANCE;
}

bool CGTeleport::isExit() const
{
	return type == BOTH || type == EXIT;
}

bool CGTeleport::isChannelEntrance(const ObjectInstanceID & id) const
{
	return vstd::contains(getAllEntrances(), id);
}

bool CGTeleport::isChannelExit(const ObjectInstanceID & id) const
{
	return vstd::contains(getAllExits(), id);
}

std::vector<ObjectInstanceID> CGTeleport::getAllEntrances(bool excludeCurrent) const
{
	auto ret = cb->getTeleportChannelEntraces(channel);
	if(excludeCurrent)
		vstd::erase_if_present(ret, id);

	return ret;
}

std::vector<ObjectInstanceID> CGTeleport::getAllExits(bool excludeCurrent) const
{
	auto ret = cb->getTeleportChannelExits(channel);
	if(excludeCurrent)
		vstd::erase_if_present(ret, id);

	return ret;
}

ObjectInstanceID CGTeleport::getRandomExit(const CGHeroInstance * h) const
{
	auto passableExits = getPassableExits(cb->gameState(), h, getAllExits(true));
	if(!passableExits.empty())
		return *RandomGeneratorUtil::nextItem(passableExits, CRandomGenerator::getDefault());

	return ObjectInstanceID();
}

bool CGTeleport::isTeleport(const CGObjectInstance * obj)
{
	return dynamic_cast<const CGTeleport *>(obj) != nullptr;
}

bool CGTeleport::isConnected(const CGTeleport * src, const CGTeleport * dst)
{
	return src && dst && src->isChannelExit(dst->id);
}

bool CGTeleport::isConnected(const CGObjectInstance * src, const CGObjectInstance * dst)
{
	const auto * srcObj = dynamic_cast<const CGTeleport *>(src);
	const auto * dstObj = dynamic_cast<const CGTeleport *>(dst);
	return isConnected(srcObj, dstObj);
}

bool CGTeleport::isExitPassable(CGameState * gs, const CGHeroInstance * h, const CGObjectInstance * obj)
{
	auto * objTopVisObj = gs->map->getTile(obj->visitablePos()).topVisitableObj();
	if(objTopVisObj->ID == Obj::HERO)
	{
		if(h->id == objTopVisObj->id) // Just to be sure it's won't happen.
			return false;

		// Check if it's friendly hero or not
		if(gs->getPlayerRelations(h->tempOwner, objTopVisObj->tempOwner) != PlayerRelations::ENEMIES)
		{
			// Exchange between heroes only possible via subterranean gates
			if(!dynamic_cast<const CGSubterraneanGate *>(obj))
				return false;
		}
	}
	return true;
}

std::vector<ObjectInstanceID> CGTeleport::getPassableExits(CGameState * gs, const CGHeroInstance * h, std::vector<ObjectInstanceID> exits)
{
	vstd::erase_if(exits, [&](const ObjectInstanceID & exit) -> bool 
	{
		return !isExitPassable(gs, h, gs->getObj(exit));
	});
	return exits;
}

void CGTeleport::addToChannel(std::map<TeleportChannelID, std::shared_ptr<TeleportChannel> > &channelsList, const CGTeleport * obj)
{
	std::shared_ptr<TeleportChannel> tc;
	if(channelsList.find(obj->channel) == channelsList.end())
	{
		tc = std::make_shared<TeleportChannel>();
		channelsList.insert(std::make_pair(obj->channel, tc));
	}
	else
		tc = channelsList[obj->channel];

	if(obj->isEntrance() && !vstd::contains(tc->entrances, obj->id))
		tc->entrances.push_back(obj->id);

	if(obj->isExit() && !vstd::contains(tc->exits, obj->id))
		tc->exits.push_back(obj->id);

	if(!tc->entrances.empty() && !tc->exits.empty()
		&& (tc->entrances.size() != 1 || tc->entrances != tc->exits))
	{
		tc->passability = TeleportChannel::PASSABLE;
	}
}

TeleportChannelID CGMonolith::findMeChannel(const std::vector<Obj> & IDs, int SubID) const
{
	for(auto obj : cb->gameState()->map->objects)
	{
		if(!obj)
			continue;

		const auto * teleportObj = dynamic_cast<const CGTeleport *>(cb->getObj(obj->id));
		if(teleportObj && vstd::contains(IDs, teleportObj->ID) && teleportObj->subID == SubID)
			return teleportObj->channel;
	}
	return TeleportChannelID();
}

void CGMonolith::onHeroVisit( const CGHeroInstance * h ) const
{
	TeleportDialog td(h->id, channel);
	if(isEntrance())
	{
		if(cb->isTeleportChannelBidirectional(channel) && 1 < cb->getTeleportChannelExits(channel).size())
		{
			auto exits = cb->getTeleportChannelExits(channel);
			for(const auto & exit : exits)
			{
				td.exits.push_back(std::make_pair(exit, h->convertFromVisitablePos(cb->getObj(exit)->visitablePos())));
			}
		}

		if(cb->isTeleportChannelImpassable(channel))
		{
			logGlobal->debug("Cannot find corresponding exit monolith for %d at %s", id.getNum(), pos.toString());
			td.impassable = true;
		}
		else if(getRandomExit(h) == ObjectInstanceID())
			logGlobal->debug("All exits blocked for monolith %d at %s", id.getNum(), pos.toString());
	}
	else
		h->showInfoDialog(70);

	cb->showTeleportDialog(&td);
}

void CGMonolith::teleportDialogAnswered(const CGHeroInstance *hero, ui32 answer, TTeleportExitsList exits) const
{
	int3 dPos;
	auto randomExit = getRandomExit(hero);
	auto realExits = getAllExits(true);
	if(!isEntrance() // Do nothing if hero visited exit only object
		|| (exits.empty() && realExits.empty()) // Do nothing if there no exits on this channel
		|| ObjectInstanceID() == randomExit) // Do nothing if all exits are blocked by friendly hero and it's not subterranean gate
	{
		return;
	}
	else if(vstd::isValidIndex(exits, answer))
		dPos = exits[answer].second;
	else
		dPos = hero->convertFromVisitablePos(cb->getObj(randomExit)->visitablePos());

	cb->moveHero(hero->id, dPos, true);
}

void CGMonolith::initObj(CRandomGenerator & rand)
{
	std::vector<Obj> IDs;
	IDs.push_back(ID);
	switch(ID)
	{
	case Obj::MONOLITH_ONE_WAY_ENTRANCE:
		type = ENTRANCE;
		IDs.emplace_back(Obj::MONOLITH_ONE_WAY_EXIT);
		break;
	case Obj::MONOLITH_ONE_WAY_EXIT:
		type = EXIT;
		IDs.emplace_back(Obj::MONOLITH_ONE_WAY_ENTRANCE);
		break;
	case Obj::MONOLITH_TWO_WAY:
	default:
		type = BOTH;
		break;
	}

	channel = findMeChannel(IDs, subID);
	if(channel == TeleportChannelID())
		channel = TeleportChannelID(static_cast<si32>(cb->gameState()->map->teleportChannels.size()));

	addToChannel(cb->gameState()->map->teleportChannels, this);
}

void CGSubterraneanGate::onHeroVisit( const CGHeroInstance * h ) const
{
	TeleportDialog td(h->id, channel);
	if(cb->isTeleportChannelImpassable(channel))
	{
		h->showInfoDialog(153);//Just inside the entrance you find a large pile of rubble blocking the tunnel. You leave discouraged.
		logGlobal->debug("Cannot find exit subterranean gate for  %d at %s", id.getNum(), pos.toString());
		td.impassable = true;
	}
	else
	{
		auto exit = getRandomExit(h);
		td.exits.push_back(std::make_pair(exit, h->convertFromVisitablePos(cb->getObj(exit)->visitablePos())));
	}

	cb->showTeleportDialog(&td);
}

void CGSubterraneanGate::initObj(CRandomGenerator & rand)
{
	type = BOTH;
}

void CGSubterraneanGate::postInit() //matches subterranean gates into pairs
{
	//split on underground and surface gates
	std::vector<CGSubterraneanGate *> gatesSplit[2]; //surface and underground gates
	for(auto & obj : cb->gameState()->map->objects)
	{
		if(!obj) // FIXME: Find out why there are nullptr objects right after initialization
			continue;

		auto * hlp = dynamic_cast<CGSubterraneanGate *>(cb->gameState()->getObjInstance(obj->id));
		if(hlp)
			gatesSplit[hlp->pos.z].push_back(hlp);
	}

	//sort by position
	std::sort(gatesSplit[0].begin(), gatesSplit[0].end(), [](const CGObjectInstance * a, const CGObjectInstance * b)
	{
		return a->pos < b->pos;
	});

	auto assignToChannel = [&](CGSubterraneanGate * obj)
	{
		if(obj->channel == TeleportChannelID())
		{ // if object not linked to channel then create new channel
			obj->channel = TeleportChannelID(static_cast<si32>(cb->gameState()->map->teleportChannels.size()));
			addToChannel(cb->gameState()->map->teleportChannels, obj);
		}
	};

	for(size_t i = 0; i < gatesSplit[0].size(); i++)
	{
		CGSubterraneanGate * objCurrent = gatesSplit[0][i];

		//find nearest underground exit
		std::pair<int, si32> best(-1, std::numeric_limits<si32>::max()); //pair<pos_in_vector, distance^2>
		for(int j = 0; j < gatesSplit[1].size(); j++)
		{
			CGSubterraneanGate *checked = gatesSplit[1][j];
			if(checked->channel != TeleportChannelID())
				continue;
			si32 hlp = checked->pos.dist2dSQ(objCurrent->pos);
			if(hlp < best.second)
			{
				best.first = j;
				best.second = hlp;
			}
		}

		assignToChannel(objCurrent);
		if(best.first >= 0) //found pair
		{
			gatesSplit[1][best.first]->channel = objCurrent->channel;
			addToChannel(cb->gameState()->map->teleportChannels, gatesSplit[1][best.first]);
		}
	}

	// we should assign empty channels to underground gates if they don't have matching overground gates
	for(auto & i : gatesSplit[1])
		assignToChannel(i);
}

void CGWhirlpool::onHeroVisit( const CGHeroInstance * h ) const
{
	TeleportDialog td(h->id, channel);
	if(cb->isTeleportChannelImpassable(channel))
	{
		logGlobal->debug("Cannot find exit whirlpool for %d at %s", id.getNum(), pos.toString());
		td.impassable = true;
	}
	else if(getRandomExit(h) == ObjectInstanceID())
		logGlobal->debug("All exits are blocked for whirlpool  %d at %s", id.getNum(), pos.toString());

	if(!isProtected(h))
	{
		SlotID targetstack = h->Slots().begin()->first; //slot numbers may vary
		for(auto i = h->Slots().rbegin(); i != h->Slots().rend(); i++)
		{
			if(h->getPower(targetstack) > h->getPower(i->first))
				targetstack = (i->first);
		}

		auto countToTake = static_cast<TQuantity>(h->getStackCount(targetstack) * 0.5);
		vstd::amax(countToTake, 1);

		InfoWindow iw;
		iw.type = EInfoWindowMode::AUTO;
		iw.player = h->tempOwner;
		iw.text.appendLocalString(EMetaText::ADVOB_TXT, 168);
		iw.components.emplace_back(CStackBasicDescriptor(h->getCreature(targetstack), -countToTake));
		cb->showInfoDialog(&iw);
		cb->changeStackCount(StackLocation(h, targetstack), -countToTake);
	}
	else
	{
		auto exits = getAllExits();
		for(const auto & exit : exits)
		{
			auto blockedPosList = cb->getObj(exit)->getBlockedPos();
			for(const auto & bPos : blockedPosList)
				td.exits.push_back(std::make_pair(exit, h->convertFromVisitablePos(bPos)));
		}
	}

	cb->showTeleportDialog(&td);
}

void CGWhirlpool::teleportDialogAnswered(const CGHeroInstance *hero, ui32 answer, TTeleportExitsList exits) const
{
	int3 dPos;
	auto realExits = getAllExits();
	if(exits.empty() && realExits.empty())
		return;
	else if(vstd::isValidIndex(exits, answer))
		dPos = exits[answer].second;
	else
	{
		auto exit = getRandomExit(hero);

		if(exit == ObjectInstanceID())
			return;

		const auto * obj = cb->getObj(exit);
		std::set<int3> tiles = obj->getBlockedPos();
		dPos = hero->convertFromVisitablePos(*RandomGeneratorUtil::nextItem(tiles, CRandomGenerator::getDefault()));
	}

	cb->moveHero(hero->id, dPos, true);
}

bool CGWhirlpool::isProtected(const CGHeroInstance * h)
{
	return h->hasBonusOfType(BonusType::WHIRLPOOL_PROTECTION)
	|| (h->stacksCount() == 1 && h->Slots().begin()->second->count == 1);
}

void CGArtifact::initObj(CRandomGenerator & rand)
{
	blockVisit = true;
	if(ID == Obj::ARTIFACT)
	{
		if (!storedArtifact)
		{
			auto * a = new CArtifactInstance();
			cb->gameState()->map->addNewArtifactInstance(a);
			storedArtifact = a;
		}
		if(!storedArtifact->artType)
			storedArtifact->setType(VLC->arth->objects[subID]);
	}
	if(ID == Obj::SPELL_SCROLL)
		subID = 1;

	assert(storedArtifact->artType);
	assert(storedArtifact->getParentNodes().size());

	//assert(storedArtifact->artType->id == subID); //this does not stop desync
}

std::string CGArtifact::getObjectName() const
{
	return VLC->artifacts()->getByIndex(subID)->getNameTranslated();
}

void CGArtifact::onHeroVisit(const CGHeroInstance * h) const
{
	if(!stacksCount())
	{
		InfoWindow iw;
		iw.type = EInfoWindowMode::AUTO;
		iw.player = h->tempOwner;

		if(storedArtifact->artType->canBePutAt(h))
		{
			switch (ID)
			{
			case Obj::ARTIFACT:
			{
				iw.components.emplace_back(Component::EComponentType::ARTIFACT, subID, 0, 0);
				if(!message.empty())
					iw.text = message;
				else
					iw.text.appendLocalString(EMetaText::ART_EVNTS, subID);
			}
			break;
			case Obj::SPELL_SCROLL:
			{
				int spellID = storedArtifact->getScrollSpellID();
				iw.components.emplace_back(Component::EComponentType::SPELL, spellID, 0, 0);
				if(!message.empty())
					iw.text = message;
				else
				{
					iw.text.appendLocalString(EMetaText::ADVOB_TXT,135);
					iw.text.replaceLocalString(EMetaText::SPELL_NAME, spellID);
				}
			}
			break;
			}
		}
		else
		{
			iw.text.appendLocalString(EMetaText::ADVOB_TXT, 2);
		}
		cb->showInfoDialog(&iw);
		pick(h);
	}
	else
	{
		switch(ID)
		{
		case Obj::ARTIFACT:
			{
				BlockingDialog ynd(true,false);
				ynd.player = h->getOwner();
				if(!message.empty())
					ynd.text = message;
				else
				{
					// TODO: Guard text is more complex in H3, see mantis issue 2325 for details
					ynd.text.appendLocalString(EMetaText::GENERAL_TXT, 420);
					ynd.text.replaceRawString("");
					ynd.text.replaceRawString(getArmyDescription());
					ynd.text.replaceLocalString(EMetaText::GENERAL_TXT, 43); // creatures
				}
				cb->showBlockingDialog(&ynd);
			}
			break;
		case Obj::SPELL_SCROLL:
			{
				if(!message.empty())
				{
					BlockingDialog ynd(true,false);
					ynd.player = h->getOwner();
					ynd.text = message;
					cb->showBlockingDialog(&ynd);
				}
				else
					blockingDialogAnswered(h, true);
			}
			break;
		}
	}
}

void CGArtifact::pick(const CGHeroInstance * h) const
{
	if(cb->giveHeroArtifact(h, storedArtifact, ArtifactPosition::FIRST_AVAILABLE))
		cb->removeObject(this, h->getOwner());
}

BattleField CGArtifact::getBattlefield() const
{
	return BattleField::NONE;
}

void CGArtifact::battleFinished(const CGHeroInstance *hero, const BattleResult &result) const
{
	if(result.winner == 0) //attacker won
		pick(hero);
}

void CGArtifact::blockingDialogAnswered(const CGHeroInstance *hero, ui32 answer) const
{
	if(answer)
		cb->startBattleI(hero, this);
}

void CGArtifact::afterAddToMap(CMap * map)
{
	//Artifacts from map objects are never removed
	//FIXME: This should be revertible in map editor

	if(ID == Obj::SPELL_SCROLL && storedArtifact && storedArtifact->getId().getNum() < 0)
        map->addNewArtifactInstance(storedArtifact);
}

void CGArtifact::serializeJsonOptions(JsonSerializeFormat& handler)
{
	handler.serializeStruct("guardMessage", message);
	CArmedInstance::serializeJsonOptions(handler);
	if(!handler.saving && !handler.getCurrent()["guards"].Vector().empty())
		CCreatureSet::serializeJson(handler, "guards", 7);

	if(handler.saving && ID == Obj::SPELL_SCROLL)
	{
		const std::shared_ptr<Bonus> b = storedArtifact->getBonusLocalFirst(Selector::type()(BonusType::SPELL));
		SpellID spellId(b->subtype);

		handler.serializeId("spell", spellId, SpellID::NONE);
	}
}

void CGShrine::onHeroVisit( const CGHeroInstance * h ) const
{
	if(spell == SpellID::NONE)
	{
		logGlobal->error("Not initialized shrine visited!");
		return;
	}

	if(!wasVisited(h->tempOwner))
		cb->setObjProperty(id, CGShrine::OBJPROP_VISITED, h->tempOwner.getNum());

	InfoWindow iw;
	iw.type = EInfoWindowMode::AUTO;
	iw.player = h->getOwner();
	iw.text = visitText;
	iw.text.appendLocalString(EMetaText::SPELL_NAME,spell);
	iw.text.appendRawString(".");

	if(!h->getArt(ArtifactPosition::SPELLBOOK))
	{
		iw.text.appendLocalString(EMetaText::ADVOB_TXT,131);
	}
	else if(h->spellbookContainsSpell(spell))//hero already knows the spell
	{
		iw.text.appendLocalString(EMetaText::ADVOB_TXT,174);
	}
	else if(spell.toSpell()->getLevel() > h->maxSpellLevel()) //it's third level spell and hero doesn't have wisdom
	{
		iw.text.appendLocalString(EMetaText::ADVOB_TXT,130);
	}
	else //give spell
	{
		std::set<SpellID> spells;
		spells.insert(spell);
		cb->changeSpells(h, true, spells);

		iw.components.emplace_back(Component::EComponentType::SPELL, spell, 0, 0);
	}

	cb->showInfoDialog(&iw);
}

void CGShrine::initObj(CRandomGenerator & rand)
{
	VLC->objtypeh->getHandlerFor(ID, subID)->configureObject(this, rand);
}

std::string CGShrine::getHoverText(PlayerColor player) const
{
	std::string hoverName = getObjectName();
	if(wasVisited(player))
	{
		hoverName += "\n" + VLC->generaltexth->allTexts[355]; // + (learn %s)
		boost::algorithm::replace_first(hoverName,"%s", spell.toSpell()->getNameTranslated());
	}
	return hoverName;
}

std::string CGShrine::getHoverText(const CGHeroInstance * hero) const
{
	std::string hoverName = getHoverText(hero->tempOwner);
	if(wasVisited(hero->tempOwner) && hero->spellbookContainsSpell(spell)) //know what spell there is and hero knows that spell
		hoverName += "\n\n" + VLC->generaltexth->allTexts[354]; // (Already learned)
	return hoverName;
}

void CGShrine::serializeJsonOptions(JsonSerializeFormat & handler)
{
	handler.serializeId("spell", spell, SpellID::NONE);
}

void CGSignBottle::initObj(CRandomGenerator & rand)
{
	//if no text is set than we pick random from the predefined ones
	if(message.empty())
	{
		auto vector = VLC->generaltexth->findStringsWithPrefix("core.randsign");
		std::string messageIdentifier = *RandomGeneratorUtil::nextItem(vector, rand);
		message.appendTextID(TextIdentifier("core", "randsign", messageIdentifier).get());
	}

	if(ID == Obj::OCEAN_BOTTLE)
	{
		blockVisit = true;
	}
}

void CGSignBottle::onHeroVisit( const CGHeroInstance * h ) const
{
	InfoWindow iw;
	iw.player = h->getOwner();
	iw.text = message;
	cb->showInfoDialog(&iw);

	if(ID == Obj::OCEAN_BOTTLE)
		cb->removeObject(this, h->getOwner());
}

void CGSignBottle::serializeJsonOptions(JsonSerializeFormat& handler)
{
	handler.serializeStruct("text", message);
}

void CGScholar::onHeroVisit( const CGHeroInstance * h ) const
{
	EBonusType type = bonusType;
	int bid = bonusID;
	//check if the bonus if applicable, if not - give primary skill (always possible)
	int ssl = h->getSecSkillLevel(SecondarySkill(bid)); //current sec skill level, used if bonusType == 1
	if((type == SECONDARY_SKILL	&& ((ssl == 3)  ||  (!ssl  &&  !h->canLearnSkill()))) ////hero already has expert level in the skill or (don't know skill and doesn't have free slot)
		|| (type == SPELL && !h->canLearnSpell(SpellID(bid).toSpell())))
	{
		type = PRIM_SKILL;
		bid = CRandomGenerator::getDefault().nextInt(GameConstants::PRIMARY_SKILLS - 1);
	}

	InfoWindow iw;
	iw.type = EInfoWindowMode::AUTO;
	iw.player = h->getOwner();
	iw.text.appendLocalString(EMetaText::ADVOB_TXT,115);

	switch (type)
	{
	case PRIM_SKILL:
		cb->changePrimSkill(h,static_cast<PrimarySkill>(bid),+1);
		iw.components.emplace_back(Component::EComponentType::PRIM_SKILL, bid, +1, 0);
		break;
	case SECONDARY_SKILL:
		cb->changeSecSkill(h,SecondarySkill(bid),+1);
		iw.components.emplace_back(Component::EComponentType::SEC_SKILL, bid, ssl + 1, 0);
		break;
	case SPELL:
		{
			std::set<SpellID> hlp;
			hlp.insert(SpellID(bid));
			cb->changeSpells(h,true,hlp);
			iw.components.emplace_back(Component::EComponentType::SPELL, bid, 0, 0);
		}
		break;
	default:
		logGlobal->error("Error: wrong bonus type (%d) for Scholar!\n", static_cast<int>(type));
		return;
	}

	cb->showInfoDialog(&iw);
	cb->removeObject(this, h->getOwner());
}

void CGScholar::initObj(CRandomGenerator & rand)
{
	blockVisit = true;
	if(bonusType == RANDOM)
	{
		bonusType = static_cast<EBonusType>(rand.nextInt(2));
		switch(bonusType)
		{
		case PRIM_SKILL:
			bonusID = rand.nextInt(GameConstants::PRIMARY_SKILLS -1);
			break;
		case SECONDARY_SKILL:
			bonusID = rand.nextInt(static_cast<int>(VLC->skillh->size()) - 1);
			break;
		case SPELL:
			std::vector<SpellID> possibilities;
			cb->getAllowedSpells (possibilities);
			bonusID = *RandomGeneratorUtil::nextItem(possibilities, rand);
			break;
		}
	}
}

void CGScholar::serializeJsonOptions(JsonSerializeFormat & handler)
{
	if(handler.saving)
	{
		std::string value;
		switch(bonusType)
		{
		case PRIM_SKILL:
			value = NPrimarySkill::names[bonusID];
			handler.serializeString("rewardPrimSkill", value);
			break;
		case SECONDARY_SKILL:
			value = CSkillHandler::encodeSkill(bonusID);
			handler.serializeString("rewardSkill", value);
			break;
		case SPELL:
			value = SpellID::encode(bonusID);
			handler.serializeString("rewardSpell", value);
			break;
		case RANDOM:
			break;
		}
	}
	else
	{
		//TODO: unify
		const JsonNode & json = handler.getCurrent();
		bonusType = RANDOM;
		if(!json["rewardPrimSkill"].String().empty())
		{
			auto raw = VLC->identifiers()->getIdentifier(ModScope::scopeBuiltin(), "primSkill", json["rewardPrimSkill"].String());
			if(raw)
			{
				bonusType = PRIM_SKILL;
				bonusID = raw.value();
			}
		}
		else if(!json["rewardSkill"].String().empty())
		{
			auto raw = VLC->identifiers()->getIdentifier(ModScope::scopeBuiltin(), "skill", json["rewardSkill"].String());
			if(raw)
			{
				bonusType = SECONDARY_SKILL;
				bonusID = raw.value();
			}
		}
		else if(!json["rewardSpell"].String().empty())
		{
			auto raw = VLC->identifiers()->getIdentifier(ModScope::scopeBuiltin(), "spell", json["rewardSpell"].String());
			if(raw)
			{
				bonusType = SPELL;
				bonusID = raw.value();
			}
		}
	}
}

void CGGarrison::onHeroVisit (const CGHeroInstance *h) const
{
	auto relations = cb->gameState()->getPlayerRelations(h->tempOwner, tempOwner);
	if (relations == PlayerRelations::ENEMIES && stacksCount() > 0) {
		//TODO: Find a way to apply magic garrison effects in battle.
		cb->startBattleI(h, this);
		return;
	}

	//New owner.
	if (relations == PlayerRelations::ENEMIES)
		cb->setOwner(this, h->tempOwner);

	cb->showGarrisonDialog(id, h->id, removableUnits);
}

bool CGGarrison::passableFor(PlayerColor player) const
{
	//FIXME: identical to same method in CGTownInstance

	if ( !stacksCount() )//empty - anyone can visit
		return true;
	if ( tempOwner == PlayerColor::NEUTRAL )//neutral guarded - no one can visit
		return false;

	if (cb->getPlayerRelations(tempOwner, player) != PlayerRelations::ENEMIES)
		return true;
	return false;
}

void CGGarrison::battleFinished(const CGHeroInstance *hero, const BattleResult &result) const
{
	if (result.winner == 0)
		onHeroVisit(hero);
}

void CGGarrison::serializeJsonOptions(JsonSerializeFormat& handler)
{
	handler.serializeBool("removableUnits", removableUnits);
	serializeJsonOwner(handler);
	CArmedInstance::serializeJsonOptions(handler);
}

void CGMagi::reset()
{
	eyelist.clear();
}

void CGMagi::initObj(CRandomGenerator & rand)
{
	if (ID == Obj::EYE_OF_MAGI)
	{
		blockVisit = true;
		eyelist[subID].push_back(id);
	}
}
void CGMagi::onHeroVisit(const CGHeroInstance * h) const
{
	if (ID == Obj::HUT_OF_MAGI)
	{
		h->showInfoDialog(61);

		if (!eyelist[subID].empty())
		{
			CenterView cv;
			cv.player = h->tempOwner;
			cv.focusTime = 2000;

			FoWChange fw;
			fw.player = h->tempOwner;
			fw.mode = FoWChange::Mode::REVEAL;
			fw.waitForDialogs = true;

			for(const auto & it : eyelist[subID])
			{
				const CGObjectInstance *eye = cb->getObj(it);

				cb->getTilesInRange (fw.tiles, eye->pos, 10, h->tempOwner, 1);
				cb->sendAndApply(&fw);
				cv.pos = eye->pos;

				cb->sendAndApply(&cv);
			}
			cv.pos = h->visitablePos();
			cv.focusTime = 0;
			cb->sendAndApply(&cv);
		}
	}
	else if (ID == Obj::EYE_OF_MAGI)
	{
		h->showInfoDialog(48);
	}
}

CGBoat::CGBoat()
{
	hero = nullptr;
	direction = 4;
	layer = EPathfindingLayer::SAIL;
}

bool CGBoat::isCoastVisitable() const
{
	return true;
}

void CGSirens::initObj(CRandomGenerator & rand)
{
	blockVisit = true;
}

std::string CGSirens::getHoverText(const CGHeroInstance * hero) const
{
	return getObjectName() + " " + visitedTxt(hero->hasBonusFrom(BonusSource::OBJECT,ID));
}

void CGSirens::onHeroVisit( const CGHeroInstance * h ) const
{
	InfoWindow iw;
	iw.player = h->tempOwner;
	if(h->hasBonusFrom(BonusSource::OBJECT,ID)) //has already visited Sirens
	{
		iw.type = EInfoWindowMode::AUTO;
		iw.text.appendLocalString(EMetaText::ADVOB_TXT,133);
	}
	else
	{
		giveDummyBonus(h->id, BonusDuration::ONE_BATTLE);
		TExpType xp = 0;

		for (auto i = h->Slots().begin(); i != h->Slots().end(); i++)
		{
			// 1-sized stacks are not affected by sirens
			if (i->second->count == 1)
				continue;

			// tested H3 behavior: 30% (rounded up) of stack drowns
			TQuantity drown = std::ceil(i->second->count * 0.3);

			if(drown)
			{
				cb->changeStackCount(StackLocation(h, i->first), -drown);
				xp += drown * i->second->type->getMaxHealth();
			}
		}

		if(xp)
		{
			xp = h->calculateXp(static_cast<int>(xp));
			iw.text.appendLocalString(EMetaText::ADVOB_TXT,132);
			iw.text.replaceNumber(static_cast<int>(xp));
			cb->changePrimSkill(h, PrimarySkill::EXPERIENCE, xp, false);
		}
		else
		{
			iw.text.appendLocalString(EMetaText::ADVOB_TXT,134);
		}
	}
	cb->showInfoDialog(&iw);
}

void CGShipyard::getOutOffsets( std::vector<int3> &offsets ) const
{
	// H J L K I
	// A x S x B
	// C E G F D
	offsets = {
		{-2, 0,  0}, // A
		{+2, 0,  0}, // B
		{-2, 1,  0}, // C
		{+2, 1,  0}, // D
		{-1, 1,  0}, // E
		{+1, 1,  0}, // F
		{0,  1,  0}, // G
		{-2, -1, 0}, // H
		{+2, -1, 0}, // I
		{-1, -1, 0}, // G
		{+1, -1, 0}, // K
		{0,  -1, 0}, // L
	};
}

const IObjectInterface * CGShipyard::getObject() const
{
	return this;
}

void CGShipyard::onHeroVisit( const CGHeroInstance * h ) const
{
	if(cb->gameState()->getPlayerRelations(tempOwner, h->tempOwner) == PlayerRelations::ENEMIES)
		cb->setOwner(this, h->tempOwner);

	if(shipyardStatus() != IBoatGenerator::GOOD)
	{
		InfoWindow iw;
		iw.type = EInfoWindowMode::AUTO;
		iw.player = tempOwner;
		getProblemText(iw.text, h);
		cb->showInfoDialog(&iw);
	}
	else
	{
		cb->showObjectWindow(this, EOpenWindowMode::SHIPYARD_WINDOW, h, false);
	}
}

void CGShipyard::serializeJsonOptions(JsonSerializeFormat& handler)
{
	serializeJsonOwner(handler);
}

BoatId CGShipyard::getBoatType() const
{
	return createdBoat;
}

void CGDenOfthieves::onHeroVisit (const CGHeroInstance * h) const
{
	cb->showObjectWindow(this, EOpenWindowMode::THIEVES_GUILD, h, false);
}

void CGObelisk::onHeroVisit( const CGHeroInstance * h ) const
{
	InfoWindow iw;
	iw.type = EInfoWindowMode::AUTO;
	iw.player = h->tempOwner;
	TeamState *ts = cb->gameState()->getPlayerTeam(h->tempOwner);
	assert(ts);
	TeamID team = ts->id;

	if(!wasVisited(team))
	{
		iw.text.appendLocalString(EMetaText::ADVOB_TXT, 96);
		cb->sendAndApply(&iw);

		// increment general visited obelisks counter
		cb->setObjProperty(id, CGObelisk::OBJPROP_INC, team.getNum());
		cb->showObjectWindow(this, EOpenWindowMode::PUZZLE_MAP, h, false);

		// mark that particular obelisk as visited for all players in the team
		for(const auto & color : ts->players)
		{
			cb->setObjProperty(id, CGObelisk::OBJPROP_VISITED, color.getNum());
		}
	}
	else
	{
		iw.text.appendLocalString(EMetaText::ADVOB_TXT, 97);
		cb->sendAndApply(&iw);
	}

}

void CGObelisk::initObj(CRandomGenerator & rand)
{
	obeliskCount++;
}

void CGObelisk::reset()
{
	obeliskCount = 0;
	visited.clear();
}

std::string CGObelisk::getHoverText(PlayerColor player) const
{
	return getObjectName() + " " + visitedTxt(wasVisited(player));
}

void CGObelisk::setPropertyDer( ui8 what, ui32 val )
{
	switch(what)
	{
		case CGObelisk::OBJPROP_INC:
			{
				auto progress = ++visited[TeamID(val)];
				logGlobal->debug("Player %d: obelisk progress %d / %d", val, static_cast<int>(progress) , static_cast<int>(obeliskCount));

				if(progress > obeliskCount)
				{
					logGlobal->error("Visited %d of %d", static_cast<int>(progress), obeliskCount);
					throw std::runtime_error("internal error");
				}

				break;
			}
		default:
			CTeamVisited::setPropertyDer(what, val);
			break;
	}
}

void CGLighthouse::onHeroVisit( const CGHeroInstance * h ) const
{
	if(h->tempOwner != tempOwner)
	{
		PlayerColor oldOwner = tempOwner;
		cb->setOwner(this,h->tempOwner); //not ours? flag it!
		h->showInfoDialog(69);
		giveBonusTo(h->tempOwner);

		if(oldOwner.isValidPlayer()) //remove bonus from old owner
		{
			RemoveBonus rb(GiveBonus::ETarget::PLAYER);
			rb.whoID = oldOwner.getNum();
			rb.source = vstd::to_underlying(BonusSource::OBJECT);
			rb.id = id.getNum();
			cb->sendAndApply(&rb);
		}
	}
}

void CGLighthouse::initObj(CRandomGenerator & rand)
{
	if(tempOwner.isValidPlayer())
	{
		// FIXME: This is dirty hack
		giveBonusTo(tempOwner, true);
	}
}

void CGLighthouse::giveBonusTo(const PlayerColor & player, bool onInit) const
{
	GiveBonus gb(GiveBonus::ETarget::PLAYER);
	gb.bonus.type = BonusType::MOVEMENT;
	gb.bonus.val = 500;
	gb.id = player.getNum();
	gb.bonus.duration = BonusDuration::PERMANENT;
	gb.bonus.source = BonusSource::OBJECT;
	gb.bonus.sid = id.getNum();
	gb.bonus.subtype = 0;

	// FIXME: This is really dirty hack
	// Proper fix would be to make CGLighthouse into bonus system node
	// Unfortunately this will cause saves breakage
	if(onInit)
		gb.applyGs(cb->gameState());
	else
		cb->sendAndApply(&gb);
}

void CGLighthouse::serializeJsonOptions(JsonSerializeFormat& handler)
{
	serializeJsonOwner(handler);
}

void HillFort::onHeroVisit(const CGHeroInstance * h) const
{
	cb->showObjectWindow(this, EOpenWindowMode::HILL_FORT_WINDOW, h, false);
}

void HillFort::fillUpgradeInfo(UpgradeInfo & info, const CStackInstance &stack) const
{
	int32_t level = stack.type->getLevel();
	int32_t index = std::clamp<int32_t>(level - 1, 0, upgradeCostPercentage.size() - 1);

	int costModifier = upgradeCostPercentage[index];

	if (costModifier < 0)
		return; // upgrade not allowed

	for(const auto & nid : stack.type->upgrades)
	{
		info.newID.push_back(nid);
		info.cost.push_back((nid.toCreature()->getFullRecruitCost() - stack.type->getFullRecruitCost()) * costModifier / 100);
	}
}

VCMI_LIB_NAMESPACE_END
