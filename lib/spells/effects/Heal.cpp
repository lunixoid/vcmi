/*
 * Heal.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "Heal.h"
#include "Registry.h"
#include "../ISpellMechanics.h"

#include "../../NetPacks.h"
#include "../../CStack.h"
#include "../../battle/IBattleState.h"
#include "../../battle/CBattleInfoCallback.h"
#include "../../serializer/JsonSerializeFormat.h"


static const std::string EFFECT_NAME = "core:heal";

namespace spells
{
namespace effects
{

VCMI_REGISTER_SPELL_EFFECT(Heal, EFFECT_NAME);

Heal::Heal(const int level)
	: StackEffect(level),
	healLevel(EHealLevel::HEAL),
	healPower(EHealPower::PERMANENT),
	minFullUnits(0)
{

}

Heal::~Heal() = default;

void Heal::apply(const PacketSender * server, RNG & rng, const Mechanics * m, const EffectTarget & target) const
{

}

void Heal::apply(IBattleState * battleState, const Mechanics * m, const EffectTarget & target) const
{

}

bool Heal::isValidTarget(const Mechanics * m, const battle::Unit * unit) const
{
	const bool onlyAlive = healLevel == EHealLevel::HEAL;
	const bool validInGenaral = unit->isValidTarget(!onlyAlive);

	if(!validInGenaral)
		return false;

	//TODO: Heal::isValidTarget

	if(unit->alive())
	{
		//check if unit have HP to heal

		auto insuries = unit->getTotalHealth() - unit->getAvailableHealth();

		if(insuries < (minFullUnits == 0 ? 1 : minFullUnits * unit->unitMaxHealth()))
			return false;
	}
	else
	{
        //check if alive unit do not block resurrection
	}
}

void Heal::serializeJsonEffect(JsonSerializeFormat & handler)
{
	static const std::vector<std::string> HEAL_LEVEL_MAP =
	{
		"heal",
		"resurrect",
		"overHeal"
	};

	static const std::vector<std::string> HEAL_POWER_MAP =
	{
		"oneBattle",
		"permanent"
	};

	handler.serializeEnum("healLevel", healLevel, EHealLevel::HEAL, HEAL_LEVEL_MAP);
	handler.serializeEnum("healPower", healPower, EHealPower::PERMANENT, HEAL_POWER_MAP);
	handler.serializeInt("minFullUnits", minFullUnits);
}


} // namespace effects
} // namespace spells
