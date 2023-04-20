/*
 * CAdvMapInt.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../gui/CIntObject.h"

#include "../../lib/int3.h"
#include "../../lib/GameConstants.h"

VCMI_LIB_NAMESPACE_BEGIN

class CGObjectInstance;
class CGHeroInstance;
class CGTownInstance;
class CArmedInstance;
class IShipyard;
struct CGPathNode;
struct ObjectPosInfo;
struct Component;

VCMI_LIB_NAMESPACE_END

class CButton;
class IImage;
class CAnimImage;
class CGStatusBar;
class CAdvMapPanel;
class CAdvMapWorldViewPanel;
class CAnimation;
class MapView;
class CResDataBar;
class CHeroList;
class CTownList;
class CInfoBar;
class CMinimap;
class MapAudioPlayer;

struct MapDrawingInfo;

enum class EAdvMapMode
{
	NORMAL,
	WORLD_VIEW
};

/// That's a huge class which handles general adventure map actions and
/// shows the right menu(questlog, spellbook, end turn,..) from where you
/// can get to the towns and heroes.
class CAdventureMapInterface : public CIntObject
{
private:
	enum EDirections {LEFT=1, RIGHT=2, UP=4, DOWN=8};
	enum class EGameStates {NA, INGAME, WAITING};

	EGameStates state;
	EAdvMapMode mode;

	/// Currently selected object, can be town, hero or null
	const CArmedInstance *selection;

	/// currently acting player
	PlayerColor player;

	bool duringAITurn;

	/// uses EDirections enum
	ui8 scrollingDir;
	bool scrollingState;

	const CSpell *spellBeingCasted; //nullptr if none

	std::vector<std::shared_ptr<CAnimImage>> gems;

	std::shared_ptr<IImage> bg;
	std::shared_ptr<IImage> bgWorldView;
	std::shared_ptr<CButton> kingOverview;
	std::shared_ptr<CButton> sleepWake;
	std::shared_ptr<CButton> underground;
	std::shared_ptr<CButton> questlog;
	std::shared_ptr<CButton> moveHero;
	std::shared_ptr<CButton> spellbook;
	std::shared_ptr<CButton> advOptions;
	std::shared_ptr<CButton> sysOptions;
	std::shared_ptr<CButton> nextHero;
	std::shared_ptr<CButton> endTurn;
	std::shared_ptr<CButton> worldViewUnderground;

	std::shared_ptr<MapView> terrain;
	std::shared_ptr<CMinimap> minimap;
	std::shared_ptr<CHeroList> heroList;
	std::shared_ptr<CTownList> townList;
	std::shared_ptr<CInfoBar> infoBar;
	std::shared_ptr<CGStatusBar> statusbar;
	std::shared_ptr<CResDataBar> resdatabar;

	std::shared_ptr<CAdvMapPanel> panelMain; // panel that holds all right-side buttons in normal view
	std::shared_ptr<CAdvMapWorldViewPanel> panelWorldView; // panel that holds all buttons and other ui in world view
	std::shared_ptr<CAdvMapPanel> activeMapPanel; // currently active panel (either main or world view, depending on current mode)

	std::shared_ptr<CAnimation> worldViewIcons;// images for world view overlay

	std::shared_ptr<MapAudioPlayer> mapAudio;

private:
	//functions bound to buttons
	void fshowOverview();
	void fworldViewBack();
	void fworldViewScale1x();
	void fworldViewScale2x();
	void fworldViewScale4x();
	void fswitchLevel();
	void fshowQuestlog();
	void fsleepWake();
	void fmoveHero();
	void fshowSpellbok();
	void fadventureOPtions();
	void fsystemOptions();
	void fnextHero();
	void fendTurn();

	void setScrollingCursor(ui8 direction) const;
	void selectionChanged();
	bool isActive();
	void adjustActiveness(bool aiTurnStart); //should be called every time at AI/human turn transition; blocks GUI during AI turn

	const IShipyard * ourInaccessibleShipyard(const CGObjectInstance *obj) const; //checks if obj is our ashipyard and cursor is 0,0 -> returns shipyard or nullptr else
	//button updates
	void updateSleepWake(const CGHeroInstance *h);
	void updateSpellbook(const CGHeroInstance *h);

	void handleMapScrollingUpdate();

	void showMoveDetailsInStatusbar(const CGHeroInstance & hero, const CGPathNode & pathNode);

	const CGObjectInstance *getActiveObject(const int3 &tile);

	std::optional<Point> keyToMoveDirection(const SDL_Keycode & key);

	bool isHeroSleeping(const CGHeroInstance *hero);
	void setHeroSleeping(const CGHeroInstance *hero, bool sleep);
	int getNextHeroIndex(int startIndex); //for Next Hero button - cycles awake heroes with movement only
	void endingTurn();

public:
	CAdventureMapInterface();

	// CIntObject interface implementation

	void activate() override;
	void deactivate() override;

	void show(SDL_Surface * to) override;
	void showAll(SDL_Surface * to) override;

	void keyPressed(const SDL_Keycode & key) override;
	void keyReleased(const SDL_Keycode & key) override;
	void mouseMoved (const Point & cursorPosition) override;

	// public interface

	void startHotSeatWait(PlayerColor Player);
	void startTurn();
	void initializeNewTurn();
	void aiTurnStarted();

	/// Called by PlayerInterface when hero is forced to wake up, e.g. on moving sleeping hero
	void onHeroWokeUp(const CGHeroInstance * hero);

	/// Called by PlayerInterface when current player changes in hotseat
	void onCurrentPlayerChanged(PlayerColor Player);

	/// Called by PlayerInterface when specific map tile changed and must be updated on minimap
	void onMapTilesChanged( boost::optional<std::unordered_set<int3> > positions);

	/// Called by PlayerInterface when hero state changed and hero list must be updated
	void onHeroChanged(const CGHeroInstance * hero);

	/// Called by PlayerInterface when town state changed and town list must be updated
	void onTownChanged(const CGTownInstance * town);

	/// Called when map audio should be paused, e.g. on combat or town screen access
	void onAudioPaused();

	/// Called when map audio should be resume, opposite to onPaused
	void onAudioResumed();

	/// Requests to display provided information inside infobox
	void showInfoBoxMessage(const std::vector<Component> & components, std::string message, int timer);

	/// Changes currently selected object
	void setSelection(const CArmedInstance *sel, bool centerView = true);

	/// Changes position on map to center selected location
	void centerOnTile(int3 on);
	void centerOnObject(const CGObjectInstance *obj);

	/// called by MapView whenever currently visible area changes
	/// visibleArea describes now visible map section measured in tiles
	void onMapViewMoved(const Rect & visibleArea, int mapLevel);

	/// called by MapView whenever tile is clicked
	void onTileLeftClicked(const int3 & mapPos);

	/// called by MapView whenever tile is hovered
	void onTileHovered(const int3 & mapPos);

	/// called by MapView whenever tile is clicked
	void onTileRightClicked(const int3 & mapPos);

	void enterCastingMode(const CSpell * sp);
	void leaveCastingMode(bool cast = false, int3 dest = int3(-1, -1, -1));

	const CGHeroInstance * getCurrentHero() const;
	const CGTownInstance * getCurrentTown() const;
	const CArmedInstance * getCurrentArmy() const;

	/// returns area of screen covered by terrain (main game area)
	Rect terrainAreaPixels() const;

	/// exits currently opened world view mode and returns to normal map
	void exitWorldView();

	/// opens world view at default scale
	void openWorldView();

	/// opens world view at specific scale
	void openWorldView(int tileSize);

	/// opens world view with specific info, e.g. after View Earth/Air is shown
	void openWorldView(const std::vector<ObjectPosInfo>& objectPositions, bool showTerrain);

};

extern std::shared_ptr<CAdventureMapInterface> adventureInt;
