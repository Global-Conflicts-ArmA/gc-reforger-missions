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
	protected ref GC_RouletteChatCommands m_Chat;
	protected int m_iChatAttempts;
	protected int m_iForcedScenario = -1;
	protected ref array<int> m_aForcedTeams = {};
	protected int m_iPublishedScenarioIndex = -1;
	protected int m_iPublishedSeed;
	protected int m_iRequester;
	protected string m_sRequesterIdentity;
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
		if (SCR_Global.IsEditMode() || GC_LobbyGameMode.GetGameMode() != GetOwner())
			return;
		if (RplSession.Mode() != RplMode.Dedicated)
		{
			m_Chat = new GC_RouletteChatCommands();
			AttachChat();
		}
		if (!Replication.IsServer())
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

	static GC_RouletteSetupComponent GetActive()
	{
		GC_LobbyGameMode mode = GC_LobbyGameMode.GetGameMode();
		if (!mode)
			return null;
		return GC_RouletteSetupComponent.Cast(mode.FindComponent(GC_RouletteSetupComponent));
	}

	protected void AttachChat()
	{
		if (GetActive() != this || !m_Chat || m_iChatAttempts++ >= 20)
			return;
		if (!m_Chat.Attach())
			GetGame().GetCallqueue().CallLater(AttachChat, 250, false);
	}

	string RunCommand(int requester, string command, string input)
	{
		if (!Replication.IsServer() || GetActive() != this || !m_Manager || GC_LobbyManager.GetManager() != m_Manager)
			return "Roulette is unavailable.";

		input.TrimInPlace();
		bool mutating = command == "reroll" || command == "players"
			|| (!input.IsEmpty() && (command == "scenario" || command == "teams"));
		if (requester <= 0 || !GetGame().GetPlayerManager().IsPlayerConnected(requester))
		{
			if (mutating)
				GC_AdminActionLogger.Log(requester, "roulette-" + command, "setup", "rejected-unresolved-requester");
			return "Roulette is unavailable.";
		}
		if (command == "seed")
		{
			if (!m_bPublished)
				return "No Roulette seed has been published.";
			return string.Format("Roulette seed: %1", m_iPublishedSeed);
		}
		if (command == "scenario" && input.IsEmpty())
			return ListChoices(false);
		if (command == "teams" && input.IsEmpty())
			return ListChoices(true);
		if (command != "reroll" && command != "players" && command != "scenario" && command != "teams")
			return "Unknown Roulette command.";

		string action = "roulette-" + command;
		if (!m_Manager.AuthorizeAdminAction(requester, action, "setup"))
			return "Log in as an administrator to change Roulette setup.";
		if (m_Roll || !m_Placement || !m_Placement.CanReplace())
		{
			GC_AdminActionLogger.Log(requester, action, "setup", "rejected-roster-unavailable");
			return "Roulette can change during setup when no generated roles have been claimed.";
		}

		string result = ApplyCommand(requester, command, input);
		if (result.IsEmpty())
		{
			GC_AdminActionLogger.Log(requester, action, "setup", "rejected-invalid-arguments");
			return "Invalid arguments. Use /scenario <index>, /teams <defender> <attacker>, /players <target greater than 10> or /reroll [seed].";
		}

		string outcome = "succeeded";
		if (command == "reroll")
			outcome = "queued";
		GC_AdminActionLogger.Log(requester, action, "setup", outcome);
		return result;
	}

	protected string ApplyCommand(int requester, string command, string input)
	{
		int value;
		if (command == "teams")
		{
			int scenarioIndex = m_iForcedScenario;
			if (scenarioIndex < 0)
				scenarioIndex = m_iPublishedScenarioIndex;
			if (!m_Catalogue || !m_Catalogue.m_aScenarios.IsIndexValid(scenarioIndex))
				return "";
			array<string> arguments = {};
			input.Split(" ", arguments, true);
			int second;
			if (arguments.Count() != 2 || !ParseInt(arguments[0], value) || !ParseInt(arguments[1], second)
				|| !m_Catalogue.m_aScenarios[scenarioIndex].m_teamsList
				|| !m_Catalogue.m_aScenarios[scenarioIndex].m_teamsList.IsIndexValid(value)
				|| !m_Catalogue.m_aScenarios[scenarioIndex].m_teamsList.IsIndexValid(second))
				return "";
			m_aForcedTeams = {value, second};
			return string.Format("Teams for the next roll: %1 %2", value, second);
		}

		if (!input.IsEmpty() && !ParseInt(input, value))
			return "";
		switch (command)
		{
			case "players":
				if (value <= 10)
					return "";
				m_iPlayerTarget = value;
				return string.Format("Player target for the next roll: %1", value);
			case "scenario":
				if (input.IsEmpty() || !m_Catalogue || !m_Catalogue.m_aScenarios.IsIndexValid(value))
					return "";
				m_iForcedScenario = value;
				return string.Format("Scenario for the next roll: %1", m_Catalogue.m_aScenarios[value].m_scenarioName);
			case "reroll":
				m_sRequesterIdentity = SCR_PlayerIdentityUtils.GetPlayerIdentityId(requester);
				if (m_sRequesterIdentity.IsEmpty())
					return "";
				m_iRequester = requester;
				m_iRemainingRolls = MAX_ROLLS;
				if (value != 0)
					m_iRemainingRolls = 1;
				BeginRoll(value);
				return "Roulette reroll queued.";
		}
		return "";
	}

	protected string ListChoices(bool teams)
	{
		if (!m_Catalogue || !m_Catalogue.m_aScenarios)
			return "The Roulette catalogue is unavailable.";
		array<string> choices = {};
		if (teams)
		{
			if (!m_Catalogue.m_aScenarios.IsIndexValid(m_iPublishedScenarioIndex))
				return "No Roulette scenario has been published.";
			foreach (int index, GC_RouletteTeam team : m_Catalogue.m_aScenarios[m_iPublishedScenarioIndex].m_teamsList)
				choices.Insert(string.Format("%1: %2", index, team.m_name));
		}
		else
		{
			foreach (int index, GC_RouletteScenario scenario : m_Catalogue.m_aScenarios)
				choices.Insert(string.Format("%1: %2", index, scenario.m_scenarioName));
		}
		string list;
		foreach (string choice : choices)
		{
			if (!list.IsEmpty())
				list += " | ";
			list += choice;
		}
		return list;
	}

	protected static bool ParseInt(string input, out int result)
	{
		result = 0;
		if (input.IsEmpty() || input.Length() > 11)
			return false;
		bool negative = input.StartsWith("-");
		int start = 0;
		if (negative || input.StartsWith("+"))
			start = 1;
		if (start == input.Length())
			return false;

		for (int index = start; index < input.Length(); index++)
		{
			int digit = "0123456789".IndexOf(input.Substring(index, 1));
			if (digit < 0)
				return false;
			if (result < (int.MIN + digit) / 10)
				return false;
			result = result * 10 - digit;
		}
		if (!negative)
		{
			if (result == int.MIN)
				return false;
			result = -result;
		}
		return true;
	}

	protected bool ValidateRerollRequester()
	{
		if (!m_iRequester)
			return true;
		if (!m_Manager.AuthorizeAdminAction(m_iRequester, "roulette-reroll", "pending"))
			return false;
		if (m_sRequesterIdentity == SCR_PlayerIdentityUtils.GetPlayerIdentityId(m_iRequester))
			return true;
		GC_AdminActionLogger.Log(m_iRequester, "roulette-reroll", "pending", "rejected-requester-changed");
		return false;
	}

	protected void FinishReroll(string outcome)
	{
		if (m_iRequester)
			GC_AdminActionLogger.Log(m_iRequester, "roulette-reroll", string.Format("seed=%1", m_iSeed), outcome);
		m_iRequester = 0;
		m_sRequesterIdentity = "";
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
		{
			m_iSeed = System.GetUnixTime() ^ (System.GetTickCount() << 16) ^ Math.RandomInt(1, 1073741824);
			if (m_iSeed == 0)
				m_iSeed = 1;
		}
		m_Roll = new GC_RouletteRoll(m_iSeed);
		m_iScenarioIndex = m_Roll.PickScenario(m_Catalogue, m_iForcedScenario);
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
		if (!m_Roll || !m_Placement || !m_Placement.CanReplace() || !ValidateRerollRequester())
		{
			FinishReroll("cancelled");
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
		if (!m_Roll.PickTeams(m_Scenario, water, m_aForcedTeams, defenderTeam, attackerTeam))
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
		m_iPublishedScenarioIndex = m_iScenarioIndex;
		m_iPublishedSeed = m_iSeed;
		FinishReroll("succeeded");
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
		else
			FinishReroll("no-valid-roll");
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
		DetachChat();
		super.OnGameEnd();
	}

	protected void StopGeneration()
	{
		if (m_Roll)
			FinishReroll("cancelled");
		if (GetGame())
			GetGame().GetCallqueue().Remove(AdvanceRoll);
		ClearRoll();
	}

	protected void DetachChat()
	{
		if (GetGame())
			GetGame().GetCallqueue().Remove(AttachChat);
		if (m_Chat)
			m_Chat.Detach();
	}

	void ~GC_RouletteSetupComponent()
	{
		StopGeneration();
		DetachChat();
	}
}
