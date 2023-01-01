/*
 * CGeneralTextHandler.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "JsonNode.h"

VCMI_LIB_NAMESPACE_BEGIN

/// Namespace that provides utilites for unicode support (UTF-8)
namespace Unicode
{
	/// evaluates size of UTF-8 character
	size_t DLL_LINKAGE getCharacterSize(char firstByte);

	/// test if character is a valid UTF-8 symbol
	/// maxSize - maximum number of bytes this symbol may consist from ( = remainer of string)
	bool DLL_LINKAGE isValidCharacter(const char * character, size_t maxSize);

	/// test if text contains ASCII-string (no need for unicode conversion)
	bool DLL_LINKAGE isValidASCII(const std::string & text);
	bool DLL_LINKAGE isValidASCII(const char * data, size_t size);

	/// test if text contains valid UTF-8 sequence
	bool DLL_LINKAGE isValidString(const std::string & text);
	bool DLL_LINKAGE isValidString(const char * data, size_t size);

	/// converts text to unicode from specified encoding or from one specified in settings
	std::string DLL_LINKAGE toUnicode(const std::string & text);
	std::string DLL_LINKAGE toUnicode(const std::string & text, const std::string & encoding);

	/// converts text from unicode to specified encoding or to one specified in settings
	/// NOTE: usage of these functions should be avoided if possible
	std::string DLL_LINKAGE fromUnicode(const std::string & text);
	std::string DLL_LINKAGE fromUnicode(const std::string & text, const std::string & encoding);

	///delete (amount) UTF characters from right
	DLL_LINKAGE void trimRight(std::string & text, const size_t amount = 1);
};

class CInputStream;

/// Parser for any text files from H3
class DLL_LINKAGE CLegacyConfigParser
{
	std::unique_ptr<char[]> data;
	char * curr;
	char * end;

	void init(const std::unique_ptr<CInputStream> & input);

	/// extracts part of quoted string.
	std::string extractQuotedPart();

	/// extracts quoted string. Any end of lines are ignored, double-quote is considered as "escaping"
	std::string extractQuotedString();

	/// extracts non-quoted string
	std::string extractNormalString();

	/// reads "raw" string without encoding conversion
	std::string readRawString();
public:
	/// read one entry from current line. Return ""/0 if end of line reached
	std::string readString();
	float readNumber();

	template <typename numeric>
	std::vector<numeric> readNumArray(size_t size)
	{
		std::vector<numeric> ret;
		ret.reserve(size);
		while (size--)
			ret.push_back((numeric)readNumber());
		return ret;
	}

	/// returns true if next entry is empty
	bool isNextEntryEmpty() const;

	/// end current line
	bool endLine();

	CLegacyConfigParser(std::string URI);
	CLegacyConfigParser(const std::unique_ptr<CInputStream> & input);
};

class CGeneralTextHandler;

/// Small wrapper that provides text access API compatible with old code
class DLL_LINKAGE LegacyTextContainer
{
	CGeneralTextHandler & owner;
	std::string basePath;

public:
	LegacyTextContainer(CGeneralTextHandler & owner, std::string const & basePath);
	const std::string & operator[](size_t index) const;
};

/// Small wrapper that provides help text access API compatible with old code
class DLL_LINKAGE LegacyHelpContainer
{
	CGeneralTextHandler & owner;
	std::string basePath;

public:
	LegacyHelpContainer(CGeneralTextHandler & owner, std::string const & basePath);
	std::pair<std::string, std::string> operator[](size_t index) const;
};

/// Handles all text-related data in game
class DLL_LINKAGE CGeneralTextHandler
{
	/// map identifier -> localization
	std::unordered_map<std::string, std::string> stringsLocalizations;

	/// map localization -> identifier
	std::unordered_map<std::string, std::string> stringsIdentifiers;

	/// add selected string to internal storage
	void registerString(const std::string & UID, const std::string & localized);
	void registerH3String(const std::string & file, size_t index, const std::string & localized);

	void readToVector(std::string sourceID, std::string sourceName);

public:
	/// returns translated version of a string that can be displayed to user
	const std::string & translate(const std::string & identifier) const;

	/// returns translated version of a string that can be displayed to user, H3-array compatibility version
	const std::string & translate(const std::string & identifier, size_t index) const;

	/// converts translated string into locale-independent text that can be sent to another client
	const std::string & serialize(const std::string & identifier) const;

	/// converts identifier into user-readable string, may be identical to 'translate' but reserved for serialization calls
	const std::string & deserialize(const std::string & identifier) const;

	/// Debug methods, dumps all currently known texts into console using Json-like format
	void dumpAllTexts();

	LegacyTextContainer allTexts;

	LegacyTextContainer arraytxt;
	LegacyTextContainer primarySkillNames;
	LegacyTextContainer jktexts;
	LegacyTextContainer heroscrn;
	LegacyTextContainer overview;//text for Kingdom Overview window
	LegacyTextContainer colors; //names of player colors ("red",...)
	LegacyTextContainer capColors; //names of player colors with first letter capitalized ("Red",...)
	LegacyTextContainer turnDurations; //turn durations for pregame (1 Minute ... Unlimited)

	//towns
	LegacyTextContainer tcommands, hcommands, fcommands; //texts for town screen, town hall screen and fort screen
	LegacyTextContainer tavernInfo;
	LegacyTextContainer tavernRumors;

	LegacyTextContainer qeModCommands;

	LegacyHelpContainer zelp;
	LegacyTextContainer lossCondtions;
	LegacyTextContainer victoryConditions;

	//objects
	LegacyTextContainer creGens; //names of creatures' generators
	LegacyTextContainer creGens4; //names of multiple creatures' generators
	LegacyTextContainer advobtxt;
	LegacyTextContainer xtrainfo;
	LegacyTextContainer restypes; //names of resources
	LegacyTextContainer terrainNames;
	LegacyTextContainer randsign;
	LegacyTextContainer seerEmpty;
	LegacyTextContainer seerNames;
	LegacyTextContainer tentColors;

	//sec skills
	LegacyTextContainer levels;
	std::vector<std::string> zcrexp; //more or less useful content of that file
	//commanders
	std::vector<std::string> znpc00; //more or less useful content of that file

	//campaigns
	std::vector<std::string> campaignMapNames;
	std::vector<std::vector<std::string>> campaignRegionNames;

	std::vector<std::string> findStringsWithPrefix(std::string const & prefix);

	int32_t pluralText(const int32_t textIndex, const int32_t count) const;

	CGeneralTextHandler();
	CGeneralTextHandler(const CGeneralTextHandler&) = delete;
	CGeneralTextHandler operator=(const CGeneralTextHandler&) = delete;
};

VCMI_LIB_NAMESPACE_END
