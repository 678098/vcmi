/*
 * CBattleAnimations.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../../lib/battle/BattleHex.h"
#include "../../lib/CSoundBase.h"
#include "../widgets/Images.h"

VCMI_LIB_NAMESPACE_BEGIN

class CStack;

VCMI_LIB_NAMESPACE_END

class CBattleInterface;
class CCreatureAnimation;
class CBattleAnimation;
struct CatapultProjectileInfo;
struct StackAttackedInfo;

/// Base class of battle animations
class CBattleAnimation
{

protected:
	CBattleInterface * owner;
	bool initialized;

	std::vector<CBattleAnimation *> & pendingAnimations();
	std::shared_ptr<CCreatureAnimation> stackAnimation(const CStack * stack);
	bool stackFacingRight(const CStack * stack);
	void setStackFacingRight(const CStack * stack, bool facingRight);

	virtual bool init() = 0; //to be called - if returned false, call again until returns true
	bool checkInitialConditions(); //determines if this animation is earliest of all

public:
	ui32 ID; //unique identifier

	bool isInitialized();
	bool tryInitialize();
	virtual void nextFrame() {} //call every new frame
	virtual ~CBattleAnimation();

	CBattleAnimation(CBattleInterface * _owner);
};

/// Sub-class which is responsible for managing the battle stack animation.
class CBattleStackAnimation : public CBattleAnimation
{
public:
	std::shared_ptr<CCreatureAnimation> myAnim; //animation for our stack, managed by CBattleInterface
	const CStack * stack; //id of stack whose animation it is

	CBattleStackAnimation(CBattleInterface * _owner, const CStack * _stack);

	void shiftColor(const ColorShifter * shifter);
	void rotateStack(BattleHex hex);
};

/// This class is responsible for managing the battle attack animation
class CAttackAnimation : public CBattleStackAnimation
{
	bool soundPlayed;

protected:
	BattleHex dest; //attacked hex
	bool shooting;
	CCreatureAnim::EAnimType group; //if shooting is true, print this animation group
	const CStack *attackedStack;
	const CStack *attackingStack;
	int attackingStackPosBeforeReturn; //for stacks with return_after_strike feature

	const CCreature * getCreature();
public:
	void nextFrame() override;
	bool checkInitialConditions();

	CAttackAnimation(CBattleInterface *_owner, const CStack *attacker, BattleHex _dest, const CStack *defender);
	~CAttackAnimation();
};

/// Animation of a defending unit
class CDefenceAnimation : public CBattleStackAnimation
{
	CCreatureAnim::EAnimType getMyAnimType();
	std::string getMySound();

	void startAnimation();

	const CStack * attacker; //attacking stack
	bool rangedAttack; //if true, stack has been attacked by shooting
	bool killed; //if true, stack has been killed

	float timeToWait; // for how long this animation should be paused
public:
	bool init() override;
	void nextFrame() override;

	CDefenceAnimation(StackAttackedInfo _attackedInfo, CBattleInterface * _owner);
	~CDefenceAnimation();
};

class CDummyAnimation : public CBattleAnimation
{
private:
	int counter;
	int howMany;
public:
	bool init() override;
	void nextFrame() override;

	CDummyAnimation(CBattleInterface * _owner, int howManyFrames);
};

/// Hand-to-hand attack
class CMeleeAttackAnimation : public CAttackAnimation
{
public:
	bool init() override;

	CMeleeAttackAnimation(CBattleInterface * _owner, const CStack * attacker, BattleHex _dest, const CStack * _attacked);
};

/// Base class for all animations that play during stack movement
class CStackMoveAnimation : public CBattleStackAnimation
{
public:
	BattleHex currentHex;

protected:
	CStackMoveAnimation(CBattleInterface * _owner, const CStack * _stack, BattleHex _currentHex);
};

/// Move animation of a creature
class CMovementAnimation : public CStackMoveAnimation
{
private:
	std::vector<BattleHex> destTiles; //full path, includes already passed hexes
	ui32 curentMoveIndex; // index of nextHex in destTiles

	BattleHex oldPos; //position of stack before move

	double begX, begY; // starting position
	double distanceX, distanceY; // full movement distance, may be negative if creture moves topleft

	double timeToMove; // full length of movement animation
	double progress; // range 0 -> 1, indicates move progrees. 0 = movement starts, 1 = move ends

public:
	bool init() override;
	void nextFrame() override;

	CMovementAnimation(CBattleInterface *_owner, const CStack *_stack, std::vector<BattleHex> _destTiles, int _distance);
	~CMovementAnimation();
};

/// Move end animation of a creature
class CMovementEndAnimation : public CStackMoveAnimation
{
public:
	bool init() override;

	CMovementEndAnimation(CBattleInterface * _owner, const CStack * _stack, BattleHex destTile);
	~CMovementEndAnimation();
};

/// Move start animation of a creature
class CMovementStartAnimation : public CStackMoveAnimation
{
public:
	bool init() override;

	CMovementStartAnimation(CBattleInterface * _owner, const CStack * _stack);
};

/// Class responsible for animation of stack chaning direction (left <-> right)
class CReverseAnimation : public CStackMoveAnimation
{
public:
	bool priority; //true - high, false - low
	bool init() override;

	void setupSecondPart();

	CReverseAnimation(CBattleInterface * _owner, const CStack * stack, BattleHex dest, bool _priority);
	~CReverseAnimation();
};

class CRangedAttackAnimation : public CAttackAnimation
{

	void setAnimationGroup();
	void initializeProjectile();
	void emitProjectile();
	void emitExplosion();

protected:
	bool projectileEmitted;

	virtual CCreatureAnim::EAnimType getUpwardsGroup() const = 0;
	virtual CCreatureAnim::EAnimType getForwardGroup() const = 0;
	virtual CCreatureAnim::EAnimType getDownwardsGroup() const = 0;

	virtual void createProjectile(const Point & from, const Point & dest) const = 0;

public:
	CRangedAttackAnimation(CBattleInterface * owner_, const CStack * attacker, BattleHex dest, const CStack * defender);
	~CRangedAttackAnimation();

	bool init() override;
	void nextFrame() override;
};

/// Shooting attack
class CShootingAnimation : public CRangedAttackAnimation
{
	CCreatureAnim::EAnimType getUpwardsGroup() const override;
	CCreatureAnim::EAnimType getForwardGroup() const override;
	CCreatureAnim::EAnimType getDownwardsGroup() const override;

	void createProjectile(const Point & from, const Point & dest) const override;

public:
	CShootingAnimation(CBattleInterface * _owner, const CStack * attacker, BattleHex dest, const CStack * defender);

};

/// Catapult attack
class CCatapultAnimation : public CShootingAnimation
{
private:
	bool explosionEmitted;
	int catapultDamage;

public:
	CCatapultAnimation(CBattleInterface * _owner, const CStack * attacker, BattleHex dest, const CStack * defender, int _catapultDmg = 0);

	void createProjectile(const Point & from, const Point & dest) const override;
	void nextFrame() override;
};

class CCastAnimation : public CRangedAttackAnimation
{
	const CSpell * spell;

	CCreatureAnim::EAnimType findValidGroup( const std::vector<CCreatureAnim::EAnimType> candidates ) const;
	CCreatureAnim::EAnimType getUpwardsGroup() const override;
	CCreatureAnim::EAnimType getForwardGroup() const override;
	CCreatureAnim::EAnimType getDownwardsGroup() const override;

	void createProjectile(const Point & from, const Point & dest) const override;

public:
	CCastAnimation(CBattleInterface * owner_, const CStack * attacker, BattleHex dest_, const CStack * defender, const CSpell * spell);
};

struct CPointEffectParameters
{
	std::vector<Point> positions;
	std::vector<BattleHex> tiles;
	std::string animation;

	soundBase::soundID sound = soundBase::invalid;
	BattleHex boundHex = BattleHex::INVALID;
	bool aligntoBottom = false;
	bool waitForSound = false;
	bool screenFill = false;
};

/// Class that plays effect at one or more positions along with (single) sound effect
class CPointEffectAnimation : public CBattleAnimation
{
	soundBase::soundID sound;
	bool soundPlayed;
	bool soundFinished;
	bool effectFinished;
	int effectFlags;

	std::shared_ptr<CAnimation>	animation;
	std::vector<Point> positions;
	std::vector<BattleHex> battlehexes;

	bool alignToBottom() const;
	bool waitForSound() const;
	bool forceOnTop() const;
	bool screenFill() const;

	void onEffectFinished();
	void onSoundFinished();
	void clearEffect();

	void playSound();
	void playEffect();

public:
	enum EEffectFlags
	{
		ALIGN_TO_BOTTOM = 1,
		WAIT_FOR_SOUND  = 2,
		FORCE_ON_TOP    = 4,
		SCREEN_FILL     = 8,
	};

	/// Create animation with screen-wide effect
	CPointEffectAnimation(CBattleInterface * _owner, soundBase::soundID sound, std::string animationName, int effects = 0);

	/// Create animation positioned at point(s). Note that positions must be are absolute, including battleint position offset
	CPointEffectAnimation(CBattleInterface * _owner, soundBase::soundID sound, std::string animationName, Point pos                 , int effects = 0);
	CPointEffectAnimation(CBattleInterface * _owner, soundBase::soundID sound, std::string animationName, std::vector<Point> pos    , int effects = 0);

	/// Create animation positioned at certain hex(es)
	CPointEffectAnimation(CBattleInterface * _owner, soundBase::soundID sound, std::string animationName, BattleHex hex             , int effects = 0);
	CPointEffectAnimation(CBattleInterface * _owner, soundBase::soundID sound, std::string animationName, std::vector<BattleHex> hex, int effects = 0);

	CPointEffectAnimation(CBattleInterface * _owner, soundBase::soundID sound, std::string animationName, Point pos, BattleHex hex,   int effects = 0);
	 ~CPointEffectAnimation();

	bool init() override;
	void nextFrame() override;
};

/// Base class (e.g. for use in dynamic_cast's) for "animations" that wait for certain event
class CWaitingAnimation : public CBattleAnimation
{
protected:
	CWaitingAnimation(CBattleInterface * owner_);
public:
	void nextFrame() override;
};

/// Class that waits till projectile of certain shooter hits a target
class CWaitingProjectileAnimation : public CWaitingAnimation
{
	const CStack * shooter;
public:
	CWaitingProjectileAnimation(CBattleInterface * owner_, const CStack * shooter);

	bool init() override;
};
