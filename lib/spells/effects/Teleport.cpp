/*
 * Teleport.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "Teleport.h"
#include "Registry.h"
#include "Registry.h"
#include "../ISpellMechanics.h"
#include "../../CStack.h"
#include "../../NetPacks.h"
#include "../../battle/CBattleInfoCallback.h"

//TODO: Teleport effect

static const std::string EFFECT_NAME = "core:teleport";

namespace spells
{
namespace effects
{
VCMI_REGISTER_SPELL_EFFECT(Teleport, EFFECT_NAME);

Teleport::Teleport(const int level)
	: StackEffect(level)
{
}

Teleport::~Teleport() = default;

bool Teleport::applicable(Problem & problem, const Mechanics * m) const
{
	auto mode = m->mode;
	if(mode == Mode::AFTER_ATTACK || mode == Mode::BEFORE_ATTACK || mode == Mode::SPELL_LIKE_ATTACK || mode == Mode::MAGIC_MIRROR)
	{
		logGlobal->warn("Invalid spell cast attempt: spell %s, mode %d", m->getSpellName(), (int)mode); //should not even try to do it
		return m->adaptProblem(ESpellCastProblem::INVALID, problem);
	}
	return StackEffect::applicable(problem, m);
}

void Teleport::apply(const PacketSender * server, RNG & rng, const Mechanics * m, const EffectTarget & target) const
{
	if(target.size() == 2)
	{
		//first destination hex to move to
		const BattleHex destination = target[0].hexValue;
		if(!destination.isValid())
		{
			server->complain("Invalid teleport destination");
			return;
		}

		//second destination creature to move
		auto targetUnit = target[1].unitValue;
		if(nullptr == targetUnit)
		{
			server->complain("No unit to teleport");
			return;
		}

		if(!m->cb->battleCanTeleportTo(targetUnit, destination, spellLevel))
		{
			server->complain("Forbidden teleport.");
			return;
		}

		BattleStackMoved bsm;
		bsm.distance = -1;
		bsm.stack = targetUnit->unitId();
		std::vector<BattleHex> tiles;
		tiles.push_back(destination);
		bsm.tilesToMove = tiles;
		bsm.teleporting = true;
		server->sendAndApply(&bsm);
	}
	else
	{
		server->complain("Teleport requires 2 destinations.");
		return;
	}
}

void Teleport::apply(IBattleState * battleState, const Mechanics * m, const EffectTarget & target) const
{
	//TODO: teleport effect evaluation
}

void Teleport::serializeJsonEffect(JsonSerializeFormat & handler)
{
	//TODO: teleport options
}

EffectTarget Teleport::transformTarget(const Mechanics * m, const Target & aimPoint, const Target & spellTarget) const
{
	return aimPoint;
}

} // namespace effects
} // namespace spells
