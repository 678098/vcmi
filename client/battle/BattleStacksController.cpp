/*
 * BattleStacksController.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "BattleStacksController.h"

#include "BattleSiegeController.h"
#include "BattleInterfaceClasses.h"
#include "BattleInterface.h"
#include "BattleAnimationClasses.h"
#include "BattleFieldController.h"
#include "BattleEffectsController.h"
#include "BattleProjectileController.h"
#include "BattleControlPanel.h"
#include "BattleRenderer.h"
#include "CreatureAnimation.h"

#include "../CPlayerInterface.h"
#include "../CMusicHandler.h"
#include "../CGameInfo.h"
#include "../gui/CAnimation.h"
#include "../gui/CGuiHandler.h"
#include "../gui/Canvas.h"

#include "../../CCallback.h"
#include "../../lib/battle/BattleHex.h"
#include "../../lib/CGameState.h"
#include "../../lib/CStack.h"
#include "../../lib/CondSh.h"

static void onAnimationFinished(const CStack *stack, std::weak_ptr<CreatureAnimation> anim)
{
	std::shared_ptr<CreatureAnimation> animation = anim.lock();
	if(!animation)
		return;

	if (animation->isIdle())
	{
		const CCreature *creature = stack->getCreature();

		if (animation->framesInGroup(ECreatureAnimType::MOUSEON) > 0)
		{
			if (CRandomGenerator::getDefault().nextDouble(99.0) < creature->animation.timeBetweenFidgets *10)
				animation->playOnce(ECreatureAnimType::MOUSEON);
			else
				animation->setType(ECreatureAnimType::HOLDING);
		}
		else
		{
			animation->setType(ECreatureAnimType::HOLDING);
		}
	}
	// always reset callback
	animation->onAnimationReset += std::bind(&onAnimationFinished, stack, anim);
}

BattleStacksController::BattleStacksController(BattleInterface & owner):
	owner(owner),
	activeStack(nullptr),
	mouseHoveredStack(nullptr),
	stackToActivate(nullptr),
	selectedStack(nullptr),
	stackCanCastSpell(false),
	creatureSpellToCast(uint32_t(-1)),
	animIDhelper(0)
{
	//preparing graphics for displaying amounts of creatures
	amountNormal     = IImage::createFromFile("CMNUMWIN.BMP");
	amountPositive   = IImage::createFromFile("CMNUMWIN.BMP");
	amountNegative   = IImage::createFromFile("CMNUMWIN.BMP");
	amountEffNeutral = IImage::createFromFile("CMNUMWIN.BMP");

	static const ColorShifterMultiplyAndAddExcept shifterNormal  ({150,  50, 255, 255}, {0,0,0,0}, {255, 231, 132, 255});
	static const ColorShifterMultiplyAndAddExcept shifterPositive({ 50, 255,  50, 255}, {0,0,0,0}, {255, 231, 132, 255});
	static const ColorShifterMultiplyAndAddExcept shifterNegative({255,  50,  50, 255}, {0,0,0,0}, {255, 231, 132, 255});
	static const ColorShifterMultiplyAndAddExcept shifterNeutral ({255, 255,  50, 255}, {0,0,0,0}, {255, 231, 132, 255});

	amountNormal->adjustPalette(&shifterNormal);
	amountPositive->adjustPalette(&shifterPositive);
	amountNegative->adjustPalette(&shifterNegative);
	amountEffNeutral->adjustPalette(&shifterNeutral);

	std::vector<const CStack*> stacks = owner.curInt->cb->battleGetAllStacks(true);
	for(const CStack * s : stacks)
	{
		stackAdded(s);
	}
}

BattleHex BattleStacksController::getStackCurrentPosition(const CStack * stack) const
{
	if ( !stackAnimation.at(stack->ID)->isMoving())
		return stack->getPosition();

	if (stack->hasBonusOfType(Bonus::FLYING))
		return BattleHex::HEX_AFTER_ALL;

	for (auto & anim : currentAnimations)
	{
		// certainly ugly workaround but fixes quite annoying bug
		// stack position will be updated only *after* movement is finished
		// before this - stack is always at its initial position. Thus we need to find
		// its current position. Which can be found only in this class
		if (CStackMoveAnimation *move = dynamic_cast<CStackMoveAnimation*>(anim))
		{
			if (move->stack == stack)
				return move->currentHex;
		}
	}
	return stack->getPosition();
}

void BattleStacksController::collectRenderableObjects(BattleRenderer & renderer)
{
	auto stacks = owner.curInt->cb->battleGetAllStacks(false);

	for (auto stack : stacks)
	{
		if (stackAnimation.find(stack->ID) == stackAnimation.end()) //e.g. for summoned but not yet handled stacks
			continue;

		//FIXME: hack to ignore ghost stacks
		if ((stackAnimation[stack->ID]->getType() == ECreatureAnimType::DEAD || stackAnimation[stack->ID]->getType() == ECreatureAnimType::HOLDING) && stack->isGhost())
			continue;

		auto layer = stackAnimation[stack->ID]->isDead() ? EBattleFieldLayer::CORPSES : EBattleFieldLayer::STACKS;
		auto location = getStackCurrentPosition(stack);

		renderer.insert(layer, location, [this, stack]( BattleRenderer::RendererRef renderer ){
			showStack(renderer, stack);
		});

		if (stackNeedsAmountBox(stack))
		{
			renderer.insert(EBattleFieldLayer::STACK_AMOUNTS, location, [this, stack]( BattleRenderer::RendererRef renderer ){
				showStackAmountBox(renderer, stack);
			});
		}
	}
}

void BattleStacksController::stackReset(const CStack * stack)
{
	//FIXME: there should be no more ongoing animations. If not - then some other method created animations but did not wait for them to end
	assert(!owner.animsAreDisplayed.get());
	owner.waitForAnims();

	auto iter = stackAnimation.find(stack->ID);

	if(iter == stackAnimation.end())
	{
		logGlobal->error("Unit %d have no animation", stack->ID);
		return;
	}

	auto animation = iter->second;

	if(stack->alive() && animation->isDeadOrDying())
	{
		addNewAnim(new CResurrectionAnimation(owner, stack));
	}

	static const ColorShifterMultiplyAndAdd shifterClone ({255, 255, 0, 255}, {0, 0, 255, 0});

	if (stack->isClone())
	{
		animation->shiftColor(&shifterClone);
	}

	owner.waitForAnims();
}

void BattleStacksController::stackAdded(const CStack * stack)
{
	// Tower shooters have only their upper half visible
	static const int turretCreatureAnimationHeight = 235;

	stackFacingRight[stack->ID] = stack->side == BattleSide::ATTACKER; // must be set before getting stack position

	Point coords = getStackPositionAtHex(stack->getPosition(), stack);

	if(stack->initialPosition < 0) //turret
	{
		assert(owner.siegeController);

		const CCreature *turretCreature = owner.siegeController->getTurretCreature();

		stackAnimation[stack->ID] = AnimationControls::getAnimation(turretCreature);
		stackAnimation[stack->ID]->pos.h = turretCreatureAnimationHeight;

		coords = owner.siegeController->getTurretCreaturePosition(stack->initialPosition);
	}
	else
	{
		stackAnimation[stack->ID] = AnimationControls::getAnimation(stack->getCreature());
		stackAnimation[stack->ID]->onAnimationReset += std::bind(&onAnimationFinished, stack, stackAnimation[stack->ID]);
		stackAnimation[stack->ID]->pos.h = stackAnimation[stack->ID]->getHeight();
	}
	stackAnimation[stack->ID]->pos.x = coords.x;
	stackAnimation[stack->ID]->pos.y = coords.y;
	stackAnimation[stack->ID]->pos.w = stackAnimation[stack->ID]->getWidth();
	stackAnimation[stack->ID]->setType(ECreatureAnimType::HOLDING);
}

void BattleStacksController::setActiveStack(const CStack *stack)
{
	if (activeStack) // update UI
		stackAnimation[activeStack->ID]->setBorderColor(AnimationControls::getNoBorder());

	activeStack = stack;

	if (activeStack) // update UI
		stackAnimation[activeStack->ID]->setBorderColor(AnimationControls::getGoldBorder());

	owner.controlPanel->blockUI(activeStack == nullptr);
}

void BattleStacksController::setHoveredStack(const CStack *stack)
{
	if ( stack == mouseHoveredStack )
		 return;

	if (mouseHoveredStack)
		stackAnimation[mouseHoveredStack->ID]->setBorderColor(AnimationControls::getNoBorder());

	// stack must be alive and not active (which uses gold border instead)
	if (stack && stack->alive() && stack != activeStack)
	{
		mouseHoveredStack = stack;

		if (mouseHoveredStack)
		{
			stackAnimation[mouseHoveredStack->ID]->setBorderColor(AnimationControls::getBlueBorder());
			if (stackAnimation[mouseHoveredStack->ID]->framesInGroup(ECreatureAnimType::MOUSEON) > 0)
				stackAnimation[mouseHoveredStack->ID]->playOnce(ECreatureAnimType::MOUSEON);
		}
	}
	else
		mouseHoveredStack = nullptr;
}

bool BattleStacksController::stackNeedsAmountBox(const CStack * stack) const
{
	BattleHex currentActionTarget;
	if(owner.curInt->curAction)
	{
		auto target = owner.curInt->curAction->getTarget(owner.curInt->cb.get());
		if(!target.empty())
			currentActionTarget = target.at(0).hexValue;
	}

	if(stack->hasBonusOfType(Bonus::SIEGE_WEAPON) && stack->getCount() == 1) //do not show box for singular war machines, stacked war machines with box shown are supported as extension feature
		return false;

	if (!owner.battleActionsStarted) // do not perform any further checks since they are related to actions that will only occur after intro music
		return true;

	if(!stack->alive())
		return false;

	if(stack->getCount() == 0) //hide box when target is going to die anyway - do not display "0 creatures"
		return false;

	for(auto anim : currentAnimations) //no matter what other conditions below are, hide box when creature is playing hit animation
	{
		auto hitAnimation = dynamic_cast<CDefenceAnimation*>(anim);
		if(hitAnimation && (hitAnimation->stack->ID == stack->ID))
			return false;
	}

	if(owner.curInt->curAction)
	{
		if(owner.curInt->curAction->stackNumber == stack->ID) //stack is currently taking action (is not a target of another creature's action etc)
		{
			if(owner.curInt->curAction->actionType == EActionType::WALK || owner.curInt->curAction->actionType == EActionType::SHOOT) //hide when stack walks or shoots
				return false;

			else if(owner.curInt->curAction->actionType == EActionType::WALK_AND_ATTACK && currentActionTarget != stack->getPosition()) //when attacking, hide until walk phase finished
				return false;
		}

		if(owner.curInt->curAction->actionType == EActionType::SHOOT && currentActionTarget == stack->getPosition()) //hide if we are ranged attack target
			return false;
	}
	return true;
}

std::shared_ptr<IImage> BattleStacksController::getStackAmountBox(const CStack * stack)
{
	std::vector<si32> activeSpells = stack->activeSpells();

	if ( activeSpells.empty())
		return amountNormal;

	int effectsPositivness = 0;

	for ( auto const & spellID : activeSpells)
		effectsPositivness += CGI->spellh->objects.at(spellID)->positiveness;

	if (effectsPositivness > 0)
		return amountPositive;

	if (effectsPositivness < 0)
		return amountNegative;

	return amountEffNeutral;
}

void BattleStacksController::showStackAmountBox(Canvas & canvas, const CStack * stack)
{
	//blitting amount background box
	auto amountBG = getStackAmountBox(stack);

	const int sideShift = stack->side == BattleSide::ATTACKER ? 1 : -1;
	const int reverseSideShift = stack->side == BattleSide::ATTACKER ? -1 : 1;
	const BattleHex nextPos = stack->getPosition() + sideShift;
	const bool edge = stack->getPosition() % GameConstants::BFIELD_WIDTH == (stack->side == BattleSide::ATTACKER ? GameConstants::BFIELD_WIDTH - 2 : 1);
	const bool moveInside = !edge && !owner.fieldController->stackCountOutsideHex(nextPos);

	int xAdd = (stack->side == BattleSide::ATTACKER ? 220 : 202) +
			(stack->doubleWide() ? 44 : 0) * sideShift +
			(moveInside ? amountBG->width() + 10 : 0) * reverseSideShift;
	int yAdd = 260 + ((stack->side == BattleSide::ATTACKER || moveInside) ? 0 : -15);

	canvas.draw(amountBG, stackAnimation[stack->ID]->pos.topLeft() + Point(xAdd, yAdd));

	//blitting amount
	Point textPos = stackAnimation[stack->ID]->pos.topLeft() + amountBG->dimensions()/2 + Point(xAdd, yAdd);

	canvas.drawText(textPos, EFonts::FONT_TINY, Colors::WHITE, ETextAlignment::CENTER, makeNumberShort(stack->getCount()));
}

void BattleStacksController::showStack(Canvas & canvas, const CStack * stack)
{
	stackAnimation[stack->ID]->nextFrame(canvas, facingRight(stack)); // do actual blit
	stackAnimation[stack->ID]->incrementFrame(float(GH.mainFPSmng->getElapsedMilliseconds()) / 1000);
}

void BattleStacksController::updateBattleAnimations()
{
	for (auto & elem : currentAnimations)
	{
		if (!elem)
			continue;

		if (elem->isInitialized())
			elem->nextFrame();
		else
			elem->tryInitialize();
	}

	bool hadAnimations = !currentAnimations.empty();
	vstd::erase(currentAnimations, nullptr);

	if (hadAnimations && currentAnimations.empty())
	{
		//anims ended
		owner.controlPanel->blockUI(activeStack == nullptr);
		owner.animsAreDisplayed.setn(false);
	}
}

void BattleStacksController::addNewAnim(CBattleAnimation *anim)
{
	currentAnimations.push_back(anim);
	owner.animsAreDisplayed.setn(true);
}

void BattleStacksController::stackActivated(const CStack *stack) //TODO: check it all before game state is changed due to abilities
{
	stackToActivate = stack;
	owner.waitForAnims();
	if (stackToActivate) //during waiting stack may have gotten activated through show
		owner.activateStack();
}

void BattleStacksController::stackRemoved(uint32_t stackID)
{
	if (getActiveStack() && getActiveStack()->ID == stackID)
	{
		BattleAction *action = new BattleAction();
		action->side = owner.defendingHeroInstance ? (owner.curInt->playerID == owner.defendingHeroInstance->tempOwner) : false;
		action->actionType = EActionType::CANCEL;
		action->stackNumber = getActiveStack()->ID;
		owner.givenCommand.setn(action);
		setActiveStack(nullptr);
	}
}

void BattleStacksController::stacksAreAttacked(std::vector<StackAttackedInfo> attackedInfos)
{
	for(auto & attackedInfo : attackedInfos)
	{
		if (!attackedInfo.attacker)
			continue;

		bool needsReverse =
				owner.curInt->cb->isToReverse(
					attackedInfo.defender->getPosition(),
					attackedInfo.attacker->getPosition(),
					facingRight(attackedInfo.defender),
					attackedInfo.attacker->doubleWide(),
					facingRight(attackedInfo.attacker));

		if (needsReverse)
			addNewAnim(new CReverseAnimation(owner, attackedInfo.defender, attackedInfo.defender->getPosition()));
	}

	for(auto & attackedInfo : attackedInfos)
	{
		addNewAnim(new CDefenceAnimation(attackedInfo, owner));

		if (attackedInfo.battleEffect != EBattleEffect::INVALID)
			owner.effectsController->displayEffect(EBattleEffect::EBattleEffect(attackedInfo.battleEffect), attackedInfo.defender->getPosition());

		if (attackedInfo.spellEffect != SpellID::NONE)
			owner.displaySpellEffect(attackedInfo.spellEffect, attackedInfo.defender->getPosition());
	}
	owner.waitForAnims();

	for (auto & attackedInfo : attackedInfos)
	{
		if (attackedInfo.rebirth)
		{
			owner.effectsController->displayEffect(EBattleEffect::RESURRECT, soundBase::RESURECT, attackedInfo.defender->getPosition());
			addNewAnim(new CResurrectionAnimation(owner, attackedInfo.defender));
		}

		if (attackedInfo.cloneKilled)
			stackRemoved(attackedInfo.defender->ID);
	}
	owner.waitForAnims();
}

void BattleStacksController::stackMoved(const CStack *stack, std::vector<BattleHex> destHex, int distance)
{
	assert(destHex.size() > 0);

	//FIXME: there should be no more ongoing animations. If not - then some other method created animations but did not wait for them to end
	assert(!owner.animsAreDisplayed.get());

	if(shouldRotate(stack, stack->getPosition(), destHex[0]))
		addNewAnim(new CReverseAnimation(owner, stack, destHex[0]));

	addNewAnim(new CMovementStartAnimation(owner, stack));
	owner.waitForAnims();

	addNewAnim(new CMovementAnimation(owner, stack, destHex, distance));
	owner.waitForAnims();

	addNewAnim(new CMovementEndAnimation(owner, stack, destHex.back()));
	owner.waitForAnims();
}

void BattleStacksController::stackAttacking( const CStack *attacker, BattleHex dest, const CStack *defender, bool shooting )
{
	bool needsReverse =
			owner.curInt->cb->isToReverse(
				attacker->getPosition(),
				defender->getPosition(),
				facingRight(attacker),
				attacker->doubleWide(),
				facingRight(defender));

	if (needsReverse)
		addNewAnim(new CReverseAnimation(owner, attacker, attacker->getPosition()));

	owner.waitForAnims();

	if (shooting)
	{
		addNewAnim(new CShootingAnimation(owner, attacker, dest, defender));
	}
	else
	{
		addNewAnim(new CMeleeAttackAnimation(owner, attacker, dest, defender));
	}

	// do not wait - waiting will be done at stacksAreAttacked
	// waitForAnims();
}

bool BattleStacksController::shouldRotate(const CStack * stack, const BattleHex & oldPos, const BattleHex & nextHex) const
{
	Point begPosition = getStackPositionAtHex(oldPos,stack);
	Point endPosition = getStackPositionAtHex(nextHex, stack);

	if((begPosition.x > endPosition.x) && facingRight(stack))
		return true;
	else if((begPosition.x < endPosition.x) && !facingRight(stack))
		return true;

	return false;
}


void BattleStacksController::endAction(const BattleAction* action)
{
	//FIXME: there should be no more ongoing animations. If not - then some other method created animations but did not wait for them to end
	assert(!owner.animsAreDisplayed.get());
	owner.waitForAnims();

	//check if we should reverse stacks
	TStacks stacks = owner.curInt->cb->battleGetStacks(CBattleCallback::MINE_AND_ENEMY);

	for (const CStack *s : stacks)
	{
		bool shouldFaceRight  = s && s->side == BattleSide::ATTACKER;

		if (s && facingRight(s) != shouldFaceRight && s->alive() && stackAnimation[s->ID]->isIdle())
		{
			addNewAnim(new CReverseAnimation(owner, s, s->getPosition()));
		}
	}
	owner.waitForAnims();
}

void BattleStacksController::startAction(const BattleAction* action)
{
	setHoveredStack(nullptr);
}

void BattleStacksController::activateStack()
{
	if ( !currentAnimations.empty())
		return;

	if ( !stackToActivate)
		return;

	owner.trySetActivePlayer(stackToActivate->owner);

	setActiveStack(stackToActivate);
	stackToActivate = nullptr;

	const CStack * s = getActiveStack();
	if(!s)
		return;

	//set casting flag to true if creature can use it to not check it every time
	const auto spellcaster = s->getBonusLocalFirst(Selector::type()(Bonus::SPELLCASTER));
	const auto randomSpellcaster = s->getBonusLocalFirst(Selector::type()(Bonus::RANDOM_SPELLCASTER));
	if(s->canCast() && (spellcaster || randomSpellcaster))
	{
		stackCanCastSpell = true;
		if(randomSpellcaster)
			creatureSpellToCast = -1; //spell will be set later on cast
		else
			creatureSpellToCast = owner.curInt->cb->battleGetRandomStackSpell(CRandomGenerator::getDefault(), s, CBattleInfoCallback::RANDOM_AIMED); //faerie dragon can cast only one spell until their next move
		//TODO: what if creature can cast BOTH random genie spell and aimed spell?
		//TODO: faerie dragon type spell should be selected by server
	}
	else
	{
		stackCanCastSpell = false;
		creatureSpellToCast = -1;
	}
}

void BattleStacksController::setSelectedStack(const CStack *stack)
{
	selectedStack = stack;
}

const CStack* BattleStacksController::getSelectedStack() const
{
	return selectedStack;
}

const CStack* BattleStacksController::getActiveStack() const
{
	return activeStack;
}

bool BattleStacksController::facingRight(const CStack * stack) const
{
	return stackFacingRight.at(stack->ID);
}

bool BattleStacksController::activeStackSpellcaster()
{
	return stackCanCastSpell;
}

SpellID BattleStacksController::activeStackSpellToCast()
{
	if (!stackCanCastSpell)
		return SpellID::NONE;
	return SpellID(creatureSpellToCast);
}

Point BattleStacksController::getStackPositionAtHex(BattleHex hexNum, const CStack * stack) const
{
	Point ret(-500, -500); //returned value
	if(stack && stack->initialPosition < 0) //creatures in turrets
		return owner.siegeController->getTurretCreaturePosition(stack->initialPosition);

	static const Point basePos(-190, -139); // position of creature in topleft corner
	static const int imageShiftX = 30; // X offset to base pos for facing right stacks, negative for facing left

	ret.x = basePos.x + 22 * ( (hexNum.getY() + 1)%2 ) + 44 * hexNum.getX();
	ret.y = basePos.y + 42 * hexNum.getY();

	if (stack)
	{
		if(facingRight(stack))
			ret.x += imageShiftX;
		else
			ret.x -= imageShiftX;

		//shifting position for double - hex creatures
		if(stack->doubleWide())
		{
			if(stack->side == BattleSide::ATTACKER)
			{
				if(facingRight(stack))
					ret.x -= 44;
			}
			else
			{
				if(!facingRight(stack))
					ret.x += 44;
			}
		}
	}
	//returning
	return ret + owner.pos.topLeft();

}
