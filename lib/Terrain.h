/*
 * Terrain.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <vcmi/EntityService.h>
#include <vcmi/Entity.h>
#include "ConstTransitivePtr.h"
#include "GameConstants.h"
#include "JsonNode.h"
#include "IHandlerBase.h"

VCMI_LIB_NAMESPACE_BEGIN

class DLL_LINKAGE TerrainType : public EntityT<TerrainId>
{
public:
	int32_t getIndex() const override { return id.getNum(); }
	int32_t getIconIndex() const override { return 0; }
	const std::string & getName() const override { return name;}
	const std::string & getJsonKey() const override { return name;}
	void registerIcons(const IconRegistar & cb) const override {}
	TerrainId getId() const override { return id;}

	enum PassabilityType : ui8
	{
		LAND = 1,
		WATER = 2,
		SURFACE = 4,
		SUBTERRANEAN = 8,
		ROCK = 16
	};
	
	std::vector<std::string> battleFields;
	std::vector<TerrainId> prohibitTransitions;
	std::array<int, 3> minimapBlocked;
	std::array<int, 3> minimapUnblocked;
	std::string name;
	std::string musicFilename;
	std::string tilesFilename;
	std::string terrainText;
	std::string typeCode;
	std::string terrainViewPatterns;
	RiverId river;

	TerrainId id;
	TerrainId rockTerrain;
	int moveCost;
	int horseSoundId;
	ui8 passabilityType;
	bool transitionRequired;
	
	TerrainType();
	
	bool operator==(const TerrainType & other);
	bool operator!=(const TerrainType & other);
	bool operator<(const TerrainType & other);
	
	bool isLand() const;
	bool isWater() const;
	bool isPassable() const;
	bool isSurface() const;
	bool isUnderground() const;
	bool isTransitionRequired() const;
	bool isSurfaceCartographerCompatible() const;
	bool isUndergroundCartographerCompatible() const;
		
	operator std::string() const;
	
	template <typename Handler> void serialize(Handler &h, const int version)
	{
		h & battleFields;
		h & prohibitTransitions;
		h & minimapBlocked;
		h & minimapUnblocked;
		h & name;
		h & musicFilename;
		h & tilesFilename;
		h & terrainText;
		h & typeCode;
		h & terrainViewPatterns;
		h & rockTerrain;
		h & river;

		h & id;
		h & moveCost;
		h & horseSoundId;
		h & passabilityType;
		h & transitionRequired;
	}
};

class DLL_LINKAGE RiverType : public EntityT<RiverId>
{
public:
	int32_t getIndex() const override { return id.getNum(); }
	int32_t getIconIndex() const override { return 0; }
	const std::string & getName() const override { return code;}
	const std::string & getJsonKey() const override { return code;}
	void registerIcons(const IconRegistar & cb) const override {}
	RiverId getId() const override { return id;}

	std::string fileName;
	std::string code;
	std::string deltaName;
	RiverId id;

	RiverType();

	template <typename Handler> void serialize(Handler& h, const int version)
	{
		h & fileName;
		h & code;
		h & deltaName;
		h & id;
	}
};

class DLL_LINKAGE RoadType : public EntityT<RoadId>
{
public:
	int32_t getIndex() const override { return id.getNum(); }
	int32_t getIconIndex() const override { return 0; }
	const std::string & getName() const override { return code;}
	const std::string & getJsonKey() const override { return code;}
	void registerIcons(const IconRegistar & cb) const override {}
	RoadId getId() const override { return id;}

	std::string fileName;
	std::string code;
	RoadId id;
	ui8 movementCost;

	RoadType();

	template <typename Handler> void serialize(Handler& h, const int version)
	{
		h & fileName;
		h & code;
		h & id;
		h & movementCost;
	}
};

class DLL_LINKAGE TerrainTypeService : public EntityServiceT<TerrainId, TerrainType>
{
public:
};

class DLL_LINKAGE RiverTypeService : public EntityServiceT<RiverId, RiverType>
{
public:
};

class DLL_LINKAGE RoadTypeService : public EntityServiceT<RoadId, RoadType>
{
public:
};

class DLL_LINKAGE TerrainTypeHandler : public CHandlerBase<TerrainId, TerrainType, TerrainType, TerrainTypeService>
{
public:
	virtual TerrainType * loadFromJson(
		const std::string & scope,
		const JsonNode & json,
		const std::string & identifier,
		size_t index) override;

	virtual const std::vector<std::string> & getTypeNames() const override;
	virtual std::vector<JsonNode> loadLegacyData(size_t dataSize) override;
	virtual std::vector<bool> getDefaultAllowed() const override;

//	TerrainType * getInfoByCode(const std::string & identifier);

	template <typename Handler> void serialize(Handler & h, const int version)
	{
		h & objects;
	}
};

class DLL_LINKAGE RiverTypeHandler : public CHandlerBase<RiverId, RiverType, RiverType, RiverTypeService>
{
public:
	virtual RiverType * loadFromJson(
		const std::string & scope,
		const JsonNode & json,
		const std::string & identifier,
		size_t index) override;

	virtual const std::vector<std::string> & getTypeNames() const override;
	virtual std::vector<JsonNode> loadLegacyData(size_t dataSize) override;
	virtual std::vector<bool> getDefaultAllowed() const override;

//	RiverType * getInfoByCode(const std::string & identifier);

	template <typename Handler> void serialize(Handler & h, const int version)
	{
		h & objects;
	}
};

class DLL_LINKAGE RoadTypeHandler : public CHandlerBase<RoadId, RoadType, RoadType, RoadTypeService>
{
public:
	virtual RoadType * loadFromJson(
		const std::string & scope,
		const JsonNode & json,
		const std::string & identifier,
		size_t index) override;

	virtual const std::vector<std::string> & getTypeNames() const override;
	virtual std::vector<JsonNode> loadLegacyData(size_t dataSize) override;
	virtual std::vector<bool> getDefaultAllowed() const override;

//	RoadType * getInfoByCode(const std::string & identifier);

	template <typename Handler> void serialize(Handler & h, const int version)
	{
		h & objects;
	}
};

VCMI_LIB_NAMESPACE_END
