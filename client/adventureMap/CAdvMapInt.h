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

VCMI_LIB_NAMESPACE_END

class CButton;
class IImage;
class CAnimImage;
class CGStatusBar;
class CAdvMapPanel;
class CAdvMapWorldViewPanel;
class CAnimation;
class CTerrainRect;
class CResDataBar;
class CHeroList;
class CTownList;
class CInfoBar;
class CMinimap;

struct MapDrawingInfo;

enum class EAdvMapMode
{
	NORMAL,
	WORLD_VIEW
};

/// That's a huge class which handles general adventure map actions and
/// shows the right menu(questlog, spellbook, end turn,..) from where you
/// can get to the towns and heroes.
class CAdvMapInt : public CIntObject
{
	//TODO: remove
	friend class CPlayerInterface;
	friend class CTerrainRect;

private:
	enum EDirections {LEFT=1, RIGHT=2, UP=4, DOWN=8};
	enum EGameStates {NA, INGAME, WAITING};

	struct WorldViewOptions
	{
		bool showAllTerrain; //for expert viewEarth
		std::vector<ObjectPosInfo> iconPositions;
		WorldViewOptions();
		void clear();
		void adjustDrawingInfo(MapDrawingInfo & info);
	};

	bool swipeEnabled;
	bool swipeMovementRequested;
	Point swipeTargetPosition;

	EGameStates state;

	ui8 anim, animValHitCount; //animation frame
	ui8 heroAnim, heroAnimValHitCount; //animation frame

	/// top left corner of visible map part
	//int3 position;

	EAdvMapMode mode;

	WorldViewOptions worldViewOptions;

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

	std::shared_ptr<CTerrainRect> terrain;
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
	void handleSwipeUpdate();

	void showMoveDetailsInStatusbar(const CGHeroInstance & hero, const CGPathNode & pathNode);

	const CGObjectInstance *getActiveObject(const int3 &tile);

	boost::optional<Point> keyToMoveDirection(const SDL_Keycode & key);

	bool redrawOnNextFrame;
public:
	CAdvMapInt();

	// CIntObject interface implementation

	void activate() override;
	void deactivate() override;

	void show(SDL_Surface * to) override;
	void showAll(SDL_Surface * to) override;

	void keyPressed(const SDL_Keycode & key) override;
	void keyReleased(const SDL_Keycode & key) override;
	void mouseMoved (const Point & cursorPosition) override;

	// public interface

	void requestRedrawMapOnNextFrame();

	void select(const CArmedInstance *sel, bool centerView = true);
	void centerOn(int3 on, bool fade = false);
	void centerOn(const CGObjectInstance *obj, bool fade = false);
	int3 verifyPos(int3 ver);

	bool isHeroSleeping(const CGHeroInstance *hero);
	void setHeroSleeping(const CGHeroInstance *hero, bool sleep);
	int getNextHeroIndex(int startIndex); //for Next Hero button - cycles awake heroes with movement only

	void setPlayer(PlayerColor Player);
	void startHotSeatWait(PlayerColor Player);
	void startTurn();
	void initializeNewTurn();
	void endingTurn();
	void aiTurnStarted();

	void quickCombatLock(); //should be called when quick battle started
	void quickCombatUnlock();

	void tileLClicked(const int3 &mapPos);
	void tileHovered(const int3 &mapPos);
	void tileRClicked(const int3 &mapPos);

	void enterCastingMode(const CSpell * sp);
	void leaveCastingMode(bool cast = false, int3 dest = int3(-1, -1, -1));
	const CGHeroInstance * curHero() const;
	const CGTownInstance * curTown() const;
	const CArmedInstance * curArmy() const;

	void updateMoveHero(const CGHeroInstance *h, tribool hasPath = boost::logic::indeterminate);
	void updateNextHero(const CGHeroInstance *h);

	/// returns area of screen covered by terrain (main game area)
	Rect terrainAreaPixels() const;

	/// returs visible section of game map, in tiles
	Rect terrainAreaTiles() const;

	/// changes current adventure map mode; used to switch between default view and world view; scale is ignored if EAdvMapMode == NORMAL
	void changeMode(EAdvMapMode newMode);
	void changeMode(EAdvMapMode newMode, int tileSize);
};

extern std::shared_ptr<CAdvMapInt> adventureInt;
