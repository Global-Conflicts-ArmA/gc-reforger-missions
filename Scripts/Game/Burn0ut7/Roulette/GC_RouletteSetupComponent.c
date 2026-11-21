[ComponentEditorProps(category: "GC Missions/Roulette", description: "Generate a seeded Roulette roster before role selection.")]
class GC_RouletteSetupComponentClass : GC_LobbySetupGateComponentClass
{
}

class GC_RouletteSetupComponent : GC_LobbySetupGateComponent
{
	[Attribute("", UIWidgets.ResourceNamePicker, "", "conf")]
	protected ResourceName m_sMapConfig;

	[Attribute("100", UIWidgets.EditBox)]
	protected int m_iPlayerTarget;

	[Attribute("0", UIWidgets.EditBox, "Zero chooses a new seed on startup.")]
	protected int m_iStartupSeed;

	protected static const int MAX_ROLLS = 8;
	protected static const int MAX_OBJECTIVE_CANDIDATES = 500;
	protected static const int MAX_ATTACKER_BEARINGS = 100;
	protected static const int CANDIDATES_PER_STEP = 5;

	protected GC_LobbyManager m_Manager;
	protected ref GC_RouletteMap m_Map;
	protected ref GC_RouletteCatalogue m_Catalogue;
	protected ref GC_RouletteRosterPlacement m_Placement;
	protected ref GC_RoulettePresentation m_Presentation;
	protected ref GC_RouletteRoll m_Roll;
	protected ref GC_RouletteTerrain m_Terrain;
	protected GC_RouletteScenario m_Scenario;
	protected IEntity m_Objective;
	protected int m_iRemainingRolls;
	protected int m_iObjectiveAttempts;
	protected int m_iAttackerAttempts;
	protected int m_iScenarioIndex;
	protected int m_iSeed;
	protected bool m_bPublished;
	protected GC_RadiusAreaEntity m_CaptureArea;
	protected ref array<ref GC_EventDefinition> m_aMatchEvents;

	override bool ProvidesMatchEvents()
	{
		return true;
	}

	override array<ref GC_EventDefinition> GetMatchEvents()
	{
		return m_aMatchEvents;
	}


	override bool IsLobbySetupReady()
	{
		return m_bPublished && !m_Roll;
	}

	override void OnWorldPostProcess(World world)
	{
		super.OnWorldPostProcess(world);
		if (!Replication.IsServer() || SCR_Global.IsEditMode() || GC_LobbyGameMode.GetGameMode() != GetOwner())
			return;

		m_Manager = GC_LobbyManager.GetManager();
		if (!m_Manager || !ReadMapConfig())
		{
			Print("GC Roulette | cannot load startup configuration", LogLevel.ERROR);
			return;
		}

		m_Placement = new GC_RouletteRosterPlacement(m_Manager);
		m_Presentation = new GC_RoulettePresentation(m_Manager);
		m_iRemainingRolls = MAX_ROLLS;
		if (m_iStartupSeed != 0)
			m_iRemainingRolls = 1;
		BeginRoll(m_iStartupSeed);
	}

	protected bool ReadMapConfig()
	{
		Resource resource = Resource.Load(m_sMapConfig);
		if (!resource || !resource.IsValid())
			return false;
		BaseContainer root = resource.GetResource().ToBaseContainer();
		if (!root)
			return false;
		m_Map = GC_RouletteMap.Cast(BaseContainerTools.CreateInstanceFromContainer(root));
		if (!m_Map)
			return false;
		m_Catalogue = m_Map.LoadCatalogue();
		return m_Catalogue && m_Catalogue.m_aScenarios && !m_Catalogue.m_aScenarios.IsEmpty();
	}

	protected void BeginRoll(int seed)
	{
		m_iRemainingRolls--;
		m_iSeed = seed;
		if (m_iSeed == 0)
			m_iSeed = Math.RandomInt(1, int.MAX);
		m_Roll = new GC_RouletteRoll(m_iSeed);
		m_iScenarioIndex = m_Roll.PickScenario(m_Catalogue);
		m_Scenario = m_Catalogue.m_aScenarios[m_iScenarioIndex];
		m_Terrain = new GC_RouletteTerrain(GetOwner().GetWorld(), GetGame().GetWorldEntity(), m_Scenario);
		m_iObjectiveAttempts = 0;
		m_iAttackerAttempts = 0;
		m_Objective = null;
		// Wait for the world to leave post-processing before replicating new groups.
		GetGame().GetCallqueue().CallLater(AdvanceRoll, 100, false);
	}

	protected void AdvanceRoll()
	{
		if (!m_Roll || !m_Placement || !m_Placement.CanReplace())
		{
			ClearRoll();
			return;
		}
		if (!m_Scenario)
		{
			FailRoll("missing scenario");
			return;
		}

		if (!m_Objective)
		{
			for (int attempt = 0; attempt < CANDIDATES_PER_STEP && !m_Objective && m_iObjectiveAttempts < MAX_OBJECTIVE_CANDIDATES; attempt++)
			{
				m_iObjectiveAttempts++;
				m_Objective = m_Terrain.FindObjectiveCandidate(m_Roll.GetRandom());
			}
			if (!m_Objective)
			{
				if (m_iObjectiveAttempts >= MAX_OBJECTIVE_CANDIDATES)
					FailRoll("no objective");
				else
					GetGame().GetCallqueue().CallLater(AdvanceRoll, 1, false);
				return;
			}
		}

		vector objective = m_Objective.GetOrigin();
		int minimum = m_Map.GetMinimumAOSize(m_Catalogue, m_iScenarioIndex);
		if (minimum <= 0 || minimum >= m_Scenario.m_maxAOSize)
		{
			FailRoll("invalid objective distance");
			return;
		}
		for (int bearing = 0; bearing < CANDIDATES_PER_STEP && m_iAttackerAttempts < MAX_ATTACKER_BEARINGS; bearing++)
		{
			m_iAttackerAttempts++;
			vector attacker;
			if (!m_Terrain.FindAttackerCandidate(m_Roll.GetRandom(), objective, minimum, m_Scenario.m_maxAOSize, attacker))
				continue;
			PlaceRoll(objective, attacker);
			return;
		}
		if (m_iAttackerAttempts >= MAX_ATTACKER_BEARINGS)
			FailRoll("no attacker position");
		else
			GetGame().GetCallqueue().CallLater(AdvanceRoll, 1, false);
	}

	protected void PlaceRoll(vector objective, vector attacker)
	{
		bool water = m_Terrain.CrossesWater(attacker, objective, m_Scenario.m_maxWaterArea);
		int defenderTeam, attackerTeam;
		if (!m_Roll.PickTeams(m_Scenario, water, null, defenderTeam, attackerTeam))
		{
			FailRoll("no opposing teams");
			return;
		}

		vector defender;
		if (!m_Terrain.FindClearGround(objective, 15, 500, defender))
		{
			FailRoll("no defender position");
			return;
		}
		GC_RouletteRosterPlan plan = m_Roll.BuildRoster(m_Scenario, defenderTeam, attackerTeam, m_iPlayerTarget);
		array<ref GC_EventDefinition> events = {};
		GC_RouletteTeam defending = m_Scenario.m_teamsList[defenderTeam];
		GC_RouletteTeam attacking = m_Scenario.m_teamsList[attackerTeam];
		if (!plan || !GC_RouletteRules.Build(defending.m_factionKey, attacking.m_factionKey, m_Scenario.m_timeLimit, events))
		{
			FailRoll("could not prepare roster or objectives");
			return;
		}

		vector mins, maxs;
		m_Objective.GetBounds(mins, maxs);
		vector size = maxs - mins;
		float radius = Math.Max(15, Math.Max(size[0], size[2]) * 1.25);
		array<vector> polygon = {};
		GC_RoulettePresentation.GetPolygon(attacker, objective, m_Scenario.m_width, polygon);
		if (!m_Presentation.CanPublish(m_Scenario, defending.m_factionKey, attacking.m_factionKey, polygon)
			|| !PlaceRosterAndObjective(plan, objective, defender, attacker, radius))
		{
			FailRoll("could not place roster or objective");
			return;
		}

		ApplyEnvironment();
		if (!m_Presentation.Publish(m_Scenario, defending, attacking, plan, objective, defender, attacker, polygon))
		{
			m_bPublished = false;
			FailRoll("could not publish briefing or boundary");
			return;
		}
		m_aMatchEvents = events;
		m_bPublished = true;
		PrintFormat("GC Roulette | roster ready | seed=%1 scenario=%2 defenderSlots=%3 attackerSlots=%4",
			m_iSeed, m_Scenario.m_scenarioName, plan.m_iDefenderSlots, plan.m_iAttackerSlots);
		ClearRoll();
	}

	protected bool PlaceRosterAndObjective(GC_RouletteRosterPlan plan, vector objective, vector defender, vector attacker, float radius)
	{
		if (!(radius >= 0.1 && radius <= 100000))
			return false;
		GC_RadiusAreaEntity area = m_CaptureArea;
		if (!area)
		{
			BaseWorld world = GetOwner().GetWorld();
			if (world.FindEntityByName(GC_RouletteRules.OBJECTIVE))
				return false;
			area = GC_RadiusAreaEntity.Cast(GetGame().SpawnEntity(GC_RadiusAreaEntity, world));
			if (!area)
				return false;
			area.SetName(GC_RouletteRules.OBJECTIVE);
		}
		if (!m_Placement.Replace(m_Terrain, plan, objective, defender, attacker))
		{
			if (area != m_CaptureArea)
				SCR_EntityHelper.DeleteEntityAndChildren(area);
			return false;
		}
		area.SetOrigin(objective);
		area.SetRadius(radius);
		m_CaptureArea = area;
		return true;
	}

	protected void ApplyEnvironment()
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetOwner().GetWorld());
		if (!world)
			return;
		TimeAndWeatherManagerEntity weather = world.GetTimeAndWeatherManager();
		if (!weather)
			return;

		RandomGenerator random = m_Roll.GetRandom();
		int year = random.RandIntInclusive(m_Scenario.m_yearStart, m_Scenario.m_yearEnd);
		int month = random.RandIntInclusive(1, 12);
		int day = random.RandIntInclusive(1, 28);
		weather.SetDate(year, month, day, true);

		float sunrise, sunset;
		if (!weather.GetSunriseHour(sunrise) || !weather.GetSunsetHour(sunset) || sunset - sunrise < 4)
		{
			sunrise = 8;
			sunset = 18;
		}
		weather.SetTimeOfTheDay(Math.Clamp(random.RandFloatXY(sunrise + 2, sunset - 2), 0, 24), true);

		array<ref WeatherState> states = {};
		weather.GetWeatherStatesList(states);
		if (states.IsEmpty())
			return;
		WeatherState state = states[random.RandIntInclusive(0, states.Count() - 1)];
		if (state)
			weather.ForceWeatherTo(false, state.GetStateName());
	}

	protected void FailRoll(string reason)
	{
		PrintFormat("GC Roulette | roll failed | seed=%1 reason=%2", m_iSeed, reason, level: LogLevel.WARNING);
		ClearRoll();
		if (m_iRemainingRolls > 0 && m_Placement.CanReplace())
			BeginRoll(0);
	}

	protected void ClearRoll()
	{
		m_Roll = null;
		m_Terrain = null;
		m_Scenario = null;
		m_Objective = null;
	}

	override void OnGameStateChanged(SCR_EGameModeState state)
	{
		super.OnGameStateChanged(state);
		if (m_Roll && state != SCR_EGameModeState.PREGAME && state != SCR_EGameModeState.SLOTTING)
			StopGeneration();
	}

	override void OnGameEnd()
	{
		StopGeneration();
		super.OnGameEnd();
	}

	protected void StopGeneration()
	{
		if (GetGame())
			GetGame().GetCallqueue().Remove(AdvanceRoll);
		ClearRoll();
	}

	void ~GC_RouletteSetupComponent()
	{
		StopGeneration();
	}
}
