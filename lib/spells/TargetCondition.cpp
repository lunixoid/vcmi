/*
 * TargetCondition.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "TargetCondition.h"

#include "ISpellMechanics.h"

#include "../GameConstants.h"
#include "../CBonusTypeHandler.h"
#include "../CStack.h"
#include "../battle/CBattleInfoCallback.h"

#include "../serializer/JsonSerializeFormat.h"
#include "../VCMI_Lib.h"
#include "../CModHandler.h"


namespace spells
{

class BonusCondition : public TargetCondition::Item
{
public:
	BonusTypeID type;

	BonusCondition(BonusTypeID type_);

protected:

	bool check(const CSpell * spell, const IStackState * target) const override;
};

class CreatureCondition : public TargetCondition::Item
{
public:
	CreatureID type;

	CreatureCondition(CreatureID type_);

	bool check(const CSpell * spell, const IStackState * target) const override;
};


TargetCondition::Item::Item()
{

}

TargetCondition::Item::~Item() = default;

bool TargetCondition::Item::isReceptive(const CSpell * spell, const IStackState * target) const
{
	bool result = check(spell, target);

	if(inverted)
		return !result;
	else
		return result;
}

TargetCondition::TargetCondition()
{

}

TargetCondition::~TargetCondition()
{

}

bool TargetCondition::isReceptive(const CBattleInfoCallback * cb, const Caster * caster, const CSpell * spell, const IStackState * target) const
{
	const IBonusBearer * obj = target->unitAsBearer();

	if(!check(absolute, spell, target))
		return false;

	{
		//spell-based spell immunity (only ANTIMAGIC in OH3) is treated as absolute
		std::stringstream cachingStr;
		cachingStr << "type_" << Bonus::LEVEL_SPELL_IMMUNITY << "source_" << Bonus::SPELL_EFFECT;

		TBonusListPtr levelImmunitiesFromSpell = obj->getBonuses(Selector::type(Bonus::LEVEL_SPELL_IMMUNITY).And(Selector::sourceType(Bonus::SPELL_EFFECT)), cachingStr.str());

		if(levelImmunitiesFromSpell->size() > 0 && levelImmunitiesFromSpell->totalValue() >= spell->level && spell->level)
			return false;
	}
	{
		//SPELL_IMMUNITY absolute case
		std::stringstream cachingStr;
		cachingStr << "type_" << Bonus::SPELL_IMMUNITY << "subtype_" << spell->id.toEnum() << "addInfo_1";
		if(obj->hasBonus(Selector::typeSubtypeInfo(Bonus::SPELL_IMMUNITY, spell->id.toEnum(), 1), cachingStr.str()))
			return false;
	}
	//check receptivity
	if(spell->isPositive() && obj->hasBonusOfType(Bonus::RECEPTIVE)) //accept all positive spells
		return true;

	//Orb of vulnerability
	//FIXME: Orb of vulnerability mechanics is not such trivial (issue 1791)
	const bool battleWideNegation = obj->hasBonusOfType(Bonus::NEGATE_ALL_NATURAL_IMMUNITIES, 0);
	const bool heroNegation = obj->hasBonusOfType(Bonus::NEGATE_ALL_NATURAL_IMMUNITIES, 1);
	//anyone can cast on artifact holder`s stacks
	if(heroNegation)
		return true;
	//this stack is from other player
	//todo: NEGATE_ALL_NATURAL_IMMUNITIES special cases: dispell, chain lightning
	else if(battleWideNegation)
	{
		if(!cb->battleMatchOwner(caster->getOwner(), target, false))
			return true;
	}

	if(!check(normal, spell, target))
		return false;

	//Check elemental immunities

	bool elementalImmune = false;

	spell->forEachSchool([&](const SpellSchoolInfo & cnf, bool & stop)
	{
		auto element = cnf.immunityBonus;

		if(obj->hasBonusOfType(element, 0)) //always resist if immune to all spells altogether
		{
			elementalImmune = true;
			stop = true;
		}
		else if(!spell->isPositive()) //negative or indifferent
		{
			if((spell->isDamageSpell() && obj->hasBonusOfType(element, 2)) || obj->hasBonusOfType(element, 1))
			{
				elementalImmune = true;
				stop = true;
			}
		}
	});

	if(elementalImmune)
		return false;

	TBonusListPtr levelImmunities = obj->getBonuses(Selector::type(Bonus::LEVEL_SPELL_IMMUNITY));

	if(obj->hasBonusOfType(Bonus::SPELL_IMMUNITY, spell->id)
		|| (levelImmunities->size() > 0 && levelImmunities->totalValue() >= spell->level && spell->level))
	{
		return true;
	}

	return true;
}

void TargetCondition::serializeJson(JsonSerializeFormat & handler)
{
	//TODO
	if(handler.saving)
	{
		logGlobal->error("Spell target condition saving is not supported");
		return;
	}

	absolute.clear();
	normal.clear();

	{
		auto anyOf = handler.enterStruct("anyOf");
		loadConditions(anyOf->getCurrent(), false, false);
	}

	{
		auto allOf = handler.enterStruct("allOf");
		loadConditions(allOf->getCurrent(), true, false);
	}

	{
		auto noneOf = handler.enterStruct("noneOf");
		loadConditions(noneOf->getCurrent(), true, true);
	}
}

bool TargetCondition::check(const ItemVector & condition, const CSpell * spell, const IStackState * target) const
{
	bool nonExclusiveCheck = false;
	bool nonExclusiveExits = false;

	for(auto & item : condition)
	{
		if(item->exclusive)
		{
			if(!item->isReceptive(spell, target))
				return false;
		}
		else
		{
			if(item->isReceptive(spell, target))
				nonExclusiveCheck = true;
			nonExclusiveExits = true;
		}
	}

	if(nonExclusiveExits)
		return nonExclusiveCheck;
	else
		return true;
}

void TargetCondition::loadConditions(const JsonNode & source, bool exclusive, bool inverted)
{
	for(auto & keyValue : source.Struct())
	{
		bool isAbsolute;

		const JsonNode & value = keyValue.second;

		if(value.String() == "absolute")
			isAbsolute = true;
		else if(value.String() == "normal")
			isAbsolute = false;
		else
			continue;

		std::shared_ptr<Item> item;

		std::string scope, type, identifier;

		VLC->modh->parseIdentifier(keyValue.first, scope, type, identifier);

		if(type == "bonus")
		{
			//TODO: support custom bonus types

			auto it = bonusNameMap.find(identifier);
			if(it == bonusNameMap.end())
			{
				logMod->error("Invalid bonus type in spell target condition %s", keyValue.first);
				continue;
			}

			item = std::make_shared<BonusCondition>(it->second);
		}
		else if(type == "creature")
		{
			auto rawId = VLC->modh->identifiers.getIdentifier(scope, type, identifier, true);

			if(!rawId)
			{
				logMod->error("Invalid creature type in spell target condition %s", keyValue.first);
				continue;
			}

			item = std::make_shared<CreatureCondition>(CreatureID(rawId.get()));
		}
		else
		{
			logMod->error("Unsupported spell target condition %s", keyValue.first);
			continue;
		}

		item->exclusive = exclusive;
		item->inverted = inverted;

		if(isAbsolute)
			absolute.push_back(item);
		else
			normal.push_back(item);
	}
}

///BonusCondition
BonusCondition::BonusCondition(BonusTypeID type_)
	:type(type_)
{

}

bool BonusCondition::check(const CSpell * spell, const IStackState * target) const
{
	const IBonusBearer * bearer = target->unitAsBearer();

	return bearer->hasBonus(Selector::type(type));
}

///CreatureCondition
CreatureCondition::CreatureCondition(CreatureID type_)
	:type(type_)
{

}

bool CreatureCondition::check(const CSpell * spell, const IStackState * target) const
{
	return target->creatureId() == type;
}


} // namespace spells
