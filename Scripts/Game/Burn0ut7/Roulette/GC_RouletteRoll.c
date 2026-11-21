//! One generation attempt. Terrain, team selection and environment draws share the same stream.
class GC_RouletteRoll
{
	protected ref RandomGenerator m_Random = new RandomGenerator();

	void GC_RouletteRoll(int seed)
	{
		m_Random.SetSeed(seed);
	}

	RandomGenerator GetRandom()
	{
		return m_Random;
	}

	int PickScenario(GC_RouletteCatalogue catalogue, int forcedIndex = -1)
	{
		if (!catalogue || !catalogue.m_aScenarios || catalogue.m_aScenarios.IsEmpty())
			return -1;
		int size = catalogue.m_aScenarios.Count();
		if (forcedIndex < -1 || forcedIndex >= size)
			return -1;

		int selected = m_Random.RandIntInclusive(0, size - 1);
		if (forcedIndex >= 0)
			selected = forcedIndex;
		return selected;
	}

	bool PickTeams(GC_RouletteScenario scenario, bool requiresWater, array<int> forced, out int defender, out int attacker)
	{
		defender = -1;
		attacker = -1;
		if (!scenario || !scenario.m_teamsList || scenario.m_teamsList.Count() < 2)
			return false;

		array<int> pool = {};
		foreach (int index, GC_RouletteTeam team : scenario.m_teamsList)
		{
			if (team && (!requiresWater || team.m_waterCapable))
				pool.Insert(index);
		}
		Shuffle(pool);

		array<int> pair = {};
		foreach (int first : pool)
		{
			array<int> opponents = {};
			foreach (int candidate : pool)
			{
				if (candidate != first)
					opponents.Insert(candidate);
			}
			Shuffle(opponents);

			foreach (int second : opponents)
			{
				if (!CanOppose(scenario.m_teamsList[first], scenario.m_teamsList[second]))
					continue;
				pair.Insert(first);
				pair.Insert(second);
				break;
			}
			if (!pair.IsEmpty())
				break;
		}

		Shuffle(pair);
		// Explicit admin choices apply after the random draw and retain attack/defence order.
		if (forced && !forced.IsEmpty())
		{
			if (forced.Count() != 2)
				return false;
			foreach (int index : forced)
			{
				if (!scenario.m_teamsList.IsIndexValid(index) || !scenario.m_teamsList[index])
					return false;
			}
			pair.Copy(forced);
		}
		if (pair.Count() != 2)
			return false;
		defender = pair[0];
		attacker = pair[1];
		return true;
	}

	protected bool CanOppose(GC_RouletteTeam first, GC_RouletteTeam second)
	{
		return first.m_factionKey != second.m_factionKey
			&& (!first.m_versusBlacklist || !first.m_versusBlacklist.Contains(second.m_factionKey))
			&& (!second.m_versusBlacklist || !second.m_versusBlacklist.Contains(first.m_factionKey));
	}

	protected void Shuffle(notnull array<int> indices)
	{
		for (int index = indices.Count() - 1; index > 0; index--)
		{
			int other = m_Random.RandIntInclusive(0, index);
			int value = indices[index];
			indices[index] = indices[other];
			indices[other] = value;
		}
	}

	GC_RouletteRosterPlan BuildRoster(GC_RouletteScenario scenario, int defender, int attacker, int playerTarget)
	{
		if (!scenario || !scenario.m_teamsList || playerTarget <= 0
			|| !scenario.m_teamsList.IsIndexValid(defender) || !scenario.m_teamsList.IsIndexValid(attacker))
			return null;

		GC_RouletteTeam defendingTeam = scenario.m_teamsList[defender];
		GC_RouletteTeam attackingTeam = scenario.m_teamsList[attacker];
		if (!defendingTeam || !attackingTeam)
			return null;

		float attackRatio = defendingTeam.m_strength - attackingTeam.m_strength + scenario.m_ratio;
		if (attackRatio == -1)
			return null;

		GC_RouletteRosterPlan plan = new GC_RouletteRosterPlan();
		int defenderTarget = Math.AbsInt(playerTarget / (1 + attackRatio));
		if (!defendingTeam.TryBuildRoster(defenderTarget, plan.m_aDefenders, plan.m_iDefenderSlots) || plan.m_iDefenderSlots <= 0)
			return null;
		int attackerTarget = Math.AbsInt(plan.m_iDefenderSlots * attackRatio);
		if (!attackingTeam.TryBuildRoster(attackerTarget, plan.m_aAttackers, plan.m_iAttackerSlots) || plan.m_iAttackerSlots <= 0)
			return null;
		return plan;
	}
}

class GC_RouletteRosterPlan
{
	ref array<GC_RouletteElement> m_aDefenders = {};
	ref array<GC_RouletteElement> m_aAttackers = {};
	int m_iDefenderSlots;
	int m_iAttackerSlots;
}
