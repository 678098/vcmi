/*
 * CCampaignHandler.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../../lib/GameConstants.h"

VCMI_LIB_NAMESPACE_BEGIN

struct StartInfo;
class CGHeroInstance;
class CBinaryReader;
class CInputStream;
class CMap;
class CMapHeader;
class CMapInfo;
class JsonNode;

namespace CampaignVersion
{
	enum Version
	{
		RoE = 4,
		AB = 5,
		SoD = 6,
		WoG = 6,
		VCMI = 1
	};

	const int VCMI_MIN = 1;
	const int VCMI_MAX = 1;
}

struct DLL_LINKAGE CampaignRegions
{
	std::string campPrefix;
	int colorSuffixLength;

	struct DLL_LINKAGE RegionDescription
	{
		std::string infix;
		int xpos, ypos;
		
		template <typename Handler> void serialize(Handler &h, const int formatVersion)
		{
			h & infix;
			h & xpos;
			h & ypos;
		}
		
		static CampaignRegions::RegionDescription fromJson(const JsonNode & node);
	};

	std::vector<RegionDescription> regions;
	
	template <typename Handler> void serialize(Handler &h, const int formatVersion)
	{
		h & campPrefix;
		h & colorSuffixLength;
		h & regions;
	}
	
	static CampaignRegions fromJson(const JsonNode & node);
	static CampaignRegions getLegacy(int campId);
};

class DLL_LINKAGE CCampaignHeader
{
public:
	si32 version = 0; //4 - RoE, 5 - AB, 6 - SoD and WoG
	CampaignRegions campaignRegions;
	int numberOfScenarios = 0;
	std::string name, description;
	ui8 difficultyChoosenByPlayer = 0;
	ui8 music = 0; //CmpMusic.txt, start from 0, field is unused in vcmi
	bool valid = false;

	std::string filename;
	std::string modName;
	std::string encoding;
	
	void loadLegacyData(ui8 campId);

	template <typename Handler> void serialize(Handler &h, const int formatVersion)
	{
		h & version;
		if(!h.saving && formatVersion < 821)
		{
			ui8 campId = 0; //legacy field
			h & campId;
			loadLegacyData(campId);
		}
		else
		{
			h & campaignRegions;
			h & numberOfScenarios;
		}
		h & name;
		h & description;
		h & difficultyChoosenByPlayer;
		if(formatVersion < 821)
			h & music; //deprecated
		h & filename;
		h & modName;
		h & encoding;
	}
};

class DLL_LINKAGE CScenarioTravel
{
public:
	ui8 whatHeroKeeps = 0; //bitfield [0] - experience, [1] - prim skills, [2] - sec skills, [3] - spells, [4] - artifacts
	std::array<ui8, 19> monstersKeptByHero;
	std::array<ui8, 18> artifsKeptByHero;

	ui8 startOptions = 0; //1 - start bonus, 2 - traveling hero, 3 - hero options

	ui8 playerColor = 0; //only for startOptions == 1

	struct DLL_LINKAGE STravelBonus
	{
		enum EBonusType {SPELL, MONSTER, BUILDING, ARTIFACT, SPELL_SCROLL, PRIMARY_SKILL, SECONDARY_SKILL, RESOURCE,
			HEROES_FROM_PREVIOUS_SCENARIO, HERO};
		EBonusType type = EBonusType::SPELL; //uses EBonusType
		si32 info1 = 0, info2 = 0, info3 = 0; //purpose depends on type

		bool isBonusForHero() const;

		template <typename Handler> void serialize(Handler &h, const int formatVersion)
		{
			h & type;
			h & info1;
			h & info2;
			h & info3;
		}
	};

	std::vector<STravelBonus> bonusesToChoose;

	template <typename Handler> void serialize(Handler &h, const int formatVersion)
	{
		h & whatHeroKeeps;
		h & monstersKeptByHero;
		h & artifsKeptByHero;
		h & startOptions;
		h & playerColor;
		h & bonusesToChoose;
	}

};

class DLL_LINKAGE CCampaignScenario
{
public:
	struct DLL_LINKAGE SScenarioPrologEpilog
	{
		bool hasPrologEpilog = false;
		ui8 prologVideo = 0; // from CmpMovie.txt
		ui8 prologMusic = 0; // from CmpMusic.txt
		std::string prologText;

		template <typename Handler> void serialize(Handler &h, const int formatVersion)
		{
			h & hasPrologEpilog;
			h & prologVideo;
			h & prologMusic;
			h & prologText;
		}
	};

	std::string mapName; //*.h3m
	std::string scenarioName; //from header. human-readble
	ui32 packedMapSize = 0; //generally not used
	std::set<ui8> preconditionRegions; //what we need to conquer to conquer this one (stored as bitfield in h3c)
	ui8 regionColor = 0;
	ui8 difficulty = 0;
	bool conquered = false;

	std::string regionText;
	SScenarioPrologEpilog prolog, epilog;

	CScenarioTravel travelOptions;
	std::vector<HeroTypeID> keepHeroes; // contains list of heroes which should be kept for next scenario (doesn't matter if they lost)
	std::vector<JsonNode> crossoverHeroes; // contains all heroes with the same state when the campaign scenario was finished
	std::vector<JsonNode> placedCrossoverHeroes; // contains all placed crossover heroes defined by hero placeholders when the scenario was started

	void loadPreconditionRegions(ui32 regions);
	bool isNotVoid() const;
	// FIXME: due to usage of JsonNode I can't make these methods const
	const CGHeroInstance * strongestHero(const PlayerColor & owner);
	std::vector<CGHeroInstance *> getLostCrossoverHeroes(); /// returns a list of crossover heroes which started the scenario, but didn't complete it

	template <typename Handler> void serialize(Handler &h, const int formatVersion)
	{
		h & mapName;
		h & scenarioName;
		h & packedMapSize;
		h & preconditionRegions;
		h & regionColor;
		h & difficulty;
		h & conquered;
		h & regionText;
		h & prolog;
		h & epilog;
		h & travelOptions;
		h & crossoverHeroes;
		h & placedCrossoverHeroes;
		h & keepHeroes;
	}
};

class DLL_LINKAGE CCampaign
{
public:
	CCampaignHeader header;
	std::vector<CCampaignScenario> scenarios;
	std::map<int, std::string > mapPieces; //binary h3ms, scenario number -> map data

	template <typename Handler> void serialize(Handler &h, const int formatVersion)
	{
		h & header;
		h & scenarios;
		h & mapPieces;
	}

	bool conquerable(int whichScenario) const;
};

class DLL_LINKAGE CCampaignState
{
public:
	std::unique_ptr<CCampaign> camp;
	std::string fileEncoding;
	std::vector<ui8> mapsConquered, mapsRemaining;
	boost::optional<si32> currentMap;

	std::map<ui8, ui8> chosenCampaignBonuses;

	void setCurrentMapAsConquered(const std::vector<CGHeroInstance*> & heroes);
	boost::optional<CScenarioTravel::STravelBonus> getBonusForCurrentMap() const;
	const CCampaignScenario & getCurrentScenario() const;
	CCampaignScenario & getCurrentScenario();
	ui8 currentBonusID() const;

	CMap * getMap(int scenarioId = -1) const;
	std::unique_ptr<CMapHeader> getHeader(int scenarioId = -1) const;
	std::shared_ptr<CMapInfo> getMapInfo(int scenarioId = -1) const;

	static JsonNode crossoverSerialize(CGHeroInstance * hero);
	static CGHeroInstance * crossoverDeserialize(JsonNode & node);

	CCampaignState() = default;
	CCampaignState(std::unique_ptr<CCampaign> _camp);

	template <typename Handler> void serialize(Handler &h, const int version)
	{
		h & camp;
		h & mapsRemaining;
		h & mapsConquered;
		h & currentMap;
		h & chosenCampaignBonuses;
	}
};

class DLL_LINKAGE CCampaignHandler
{
	static std::string readLocalizedString(CBinaryReader & reader, std::string filename, std::string modName, std::string encoding, std::string identifier);
	
	//parsers for VCMI campaigns (*.vcmp)
	static CCampaignHeader readHeaderFromJson(JsonNode & reader, std::string filename, std::string modName, std::string encoding);
	static CCampaignScenario readScenarioFromJson(JsonNode & reader);
	static CScenarioTravel readScenarioTravelFromJson(JsonNode & reader);

	//parsers for original H3C campaigns
	static CCampaignHeader readHeaderFromMemory(CBinaryReader & reader, std::string filename, std::string modName, std::string encoding);
	static CCampaignScenario readScenarioFromMemory(CBinaryReader & reader, const CCampaignHeader & header);
	static CScenarioTravel readScenarioTravelFromMemory(CBinaryReader & reader, int version);
	/// returns h3c split in parts. 0 = h3c header, 1-end - maps (binary h3m)
	/// headerOnly - only header will be decompressed, returned vector wont have any maps
	static std::vector<std::vector<ui8>> getFile(std::unique_ptr<CInputStream> file, bool headerOnly);

public:
	static std::string prologVideoName(ui8 index);
	static std::string prologMusicName(ui8 index);
	static std::string prologVoiceName(ui8 index);

	static CCampaignHeader getHeader( const std::string & name); //name - name of appropriate file

	static std::unique_ptr<CCampaign> getCampaign(const std::string & name); //name - name of appropriate file
};

VCMI_LIB_NAMESPACE_END
