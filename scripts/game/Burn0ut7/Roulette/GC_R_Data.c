[BaseContainerProps()]
class GC_R_BaseScenario : Managed
{
	[Attribute(defvalue: "", uiwidget: UIWidgets.Auto, desc: "Name of scenario refereced in the briefing")]
    string m_scenarioName;
	
	[Attribute(defvalue: "", UIWidgets.Object)]
    ref array<ref GC_R_Team> m_teamsList;	
	
	[Attribute(uiwidget: UIWidgets.Object, desc: "Briefings")]
    protected ref array<ref GC_R_Briefing> m_briefing;
	
	protected GC_R_Manager m_manager;
	
	ref array<ref GC_R_Team> m_teams = {};

	void Intialize()
	{
		m_manager = GC_R_Manager.GetInstance();
	}
	
	void Reroll();

}

[BaseContainerProps(), BaseContainerCustomTitleField("Attack and Defend")]
class GC_R_AttackDefend: GC_R_BaseScenario
{
	[Attribute(defvalue: "2", uiwidget: UIWidgets.Auto, desc: "Ratio for attackers")]
    protected float m_ratio;
	
	[Attribute(defvalue: "4200", uiwidget: UIWidgets.Auto, desc: "Time limit in seconds for defenders to win")]
    protected int m_timeLimit;
	
	[Attribute(defvalue: "1970", uiwidget: UIWidgets.Auto, desc: "Start range for year")]
    protected float m_yearStart;
	
	[Attribute(defvalue: "1990", uiwidget: UIWidgets.Auto, desc: "End range for year")]
    protected float m_yearEnd;
	
	[Attribute(defvalue: "50", uiwidget: UIWidgets.Auto, desc: "Size of nearby building search", category: "Objective")]
    protected float m_buildingSearchSize;
	
	[Attribute(defvalue: "5", uiwidget: UIWidgets.Auto, desc: "Amount of buildings needed in Building Search Size", category: "Objective")]
    protected int m_minbuildingCount;
	
	[Attribute(defvalue: "", uiwidget: UIWidgets.ResourceNamePicker, desc: "Blacklist building prefab or kind", category: "Objective")]
    protected ref array<ResourceName> m_buildingBlackList;
	
	[Attribute(defvalue: "", uiwidget: UIWidgets.Auto, desc: "Blacklist building of same name", category: "Objective")]
    protected ref array<string> m_buildingBlackListWords;
	
	[Attribute(defvalue: "{FE585088FB849703}worlds/Burn0ut7/Roulette/Prefabs/RouletteCaptureArea.et", uiwidget: UIWidgets.Auto, desc: "Prefab to spawn for the objective", category: "Objective")]
    protected ResourceName m_objectivePrefab;
	
	[Attribute(defvalue: "1000", uiwidget: UIWidgets.Auto, desc: "AO width size in meters", category: "AO")]
    protected int m_width;
	
	[Attribute(defvalue: "2000", uiwidget: UIWidgets.Auto, desc: "Min size of AO in meters", category: "AO")]
    protected int m_minAOSize;
	
	[Attribute(defvalue: "5000", uiwidget: UIWidgets.Auto, desc: "Max size of AO in meters", category: "AO")]
    protected int m_maxAOSize;

	[Attribute(defvalue: "20", uiwidget: UIWidgets.Auto, desc: "In meters if water area is more then, skip teams that aren't water capable", category: "AO")]
    protected int m_maxWaterArea;
	
	protected int m_searchAttempts;
	
	protected ref GC_R_ObjAttackDefend m_objective;
	
	ref GC_R_SpawnAttacker m_attackerSpawn;

	protected bool m_searching = false;
	
	override void Intialize()
	{
		super.Intialize();

		FindObjectiveAsync();
	}
	
	int GetMinAO()
	{
		return m_minAOSize;
	}	
	
	int GetMaxAO()
	{
		return m_maxAOSize;
	}	
	
	void SetupOBJ()
	{
		vector position = m_objective.building.GetOrigin();
		
		TILW_FactionTriggerEntity objective = TILW_FactionTriggerEntity.Cast(m_manager.SpawnPrefab(m_objectivePrefab, position));
		vector min,max;
		m_objective.building.GetBounds(min,max);
		vector size = max - min;
		float width = Math.Max(size[0],size[2]) * 1.25;
		width = Math.Max(width, 15);
		
		objective.SetRadius(width);
		objective.SetOwnerFaction(m_teams[1].GetFaction());
	}
	
	void SetupFW()
	{
		TILW_MissionFrameworkEntity framework = TILW_MissionFrameworkEntity.GetInstance();
		
		string defenderKey = m_teams[0].GetFaction();
		string attackerKey = m_teams[1].GetFaction();
		
		//Create flags
		TILW_FactionPlayersKilledFlag defenderFlag = TILW_FactionPlayersKilledFlag();
		defenderFlag.SetFlag("defender");
		defenderFlag.SetKey(defenderKey);
		defenderFlag.SetCasualtyRatio(0.95);

		TILW_FactionPlayersKilledFlag attackerFlag = TILW_FactionPlayersKilledFlag();
		attackerFlag.SetFlag("attacker");
		attackerFlag.SetKey(attackerKey);
		attackerFlag.SetCasualtyRatio(0.95);
		
		array<ref TILW_FactionPlayersKilledFlag> flags = {defenderFlag, attackerFlag};
		
		//Instructions
		TILW_EndGameInstruction defenderInstruction = TILW_EndGameInstruction();
		defenderInstruction.SetGameOverType(EGameOverTypes.EDITOR_FACTION_VICTORY);
		defenderInstruction.SetKey(attackerKey);		
		defenderInstruction.SetExecutionDelay(10);
		
		TILW_SendMessageInstruction defenderMsg = TILW_SendMessageInstruction();
		defenderMsg.SetTitle("Casualty Rate reached");
		defenderMsg.SetBody("Attackers win due to defender casualty rate being reached.");

		TILW_EndGameInstruction attackerInstruction = TILW_EndGameInstruction();
		attackerInstruction.SetGameOverType(EGameOverTypes.EDITOR_FACTION_VICTORY);
		attackerInstruction.SetKey(defenderKey);
		attackerInstruction.SetExecutionDelay(10);
		
		TILW_SendMessageInstruction attackerMsg = TILW_SendMessageInstruction();
		attackerMsg.SetTitle("Casualty Rate reached");
		attackerMsg.SetBody("Defenders win due to attacker casualty rate being reached.");

		TILW_EndGameInstruction timelimitInstruction = TILW_EndGameInstruction();
		timelimitInstruction.SetGameOverType(EGameOverTypes.EDITOR_FACTION_VICTORY);
		timelimitInstruction.SetKey(defenderKey);
		timelimitInstruction.SetExecutionDelay(m_timeLimit);
		
		TILW_SendMessageInstruction timelimitMsgInstruction = TILW_SendMessageInstruction();
		timelimitMsgInstruction.SetTitle("Time Limit Reached");
		timelimitMsgInstruction.SetBody("Defenders win due to time limit being reached.");
		timelimitMsgInstruction.SetExecutionDelay(m_timeLimit - 10);
		
		TILW_EndGameInstruction objectiveInstruction = TILW_EndGameInstruction();
		objectiveInstruction.SetGameOverType(EGameOverTypes.EDITOR_FACTION_VICTORY);
		objectiveInstruction.SetKey(attackerKey);

		//Conditions
		TILW_LiteralTerm defenderTerm = TILW_LiteralTerm();
		defenderTerm.SetFlag("defender");
		
		TILW_LiteralTerm atttackerTerm = TILW_LiteralTerm();
		atttackerTerm.SetFlag("attacker");
		
		TILW_LiteralTerm timeLimitTerm = TILW_LiteralTerm();
		timeLimitTerm.SetFlag("timelimit");
		timeLimitTerm.SetInvert(true);

		TILW_LiteralTerm objectiveTerm = TILW_LiteralTerm();
		objectiveTerm.SetFlag("objective");
		
		//Create events
		TILW_MissionEvent defenderEvent = TILW_MissionEvent();
		defenderEvent.SetName("defender event");
		defenderEvent.SetInstructions({defenderInstruction,defenderMsg});
		defenderEvent.SetCondition(defenderTerm);
		
		TILW_MissionEvent attackerEvent = TILW_MissionEvent();
		attackerEvent.SetName("attacker event");
		attackerEvent.SetInstructions({attackerInstruction,attackerMsg});
		attackerEvent.SetCondition(atttackerTerm);
		
		TILW_MissionEvent timeLimitEvent = TILW_MissionEvent();
		timeLimitEvent.SetName("timeLimit event");
		timeLimitEvent.SetInstructions({timelimitMsgInstruction, timelimitInstruction});
		timeLimitEvent.SetCondition(timeLimitTerm);
		
		TILW_MissionEvent objectiveEvent = TILW_MissionEvent();
		objectiveEvent.SetName("objective event");
		objectiveEvent.SetInstructions({objectiveInstruction});
		objectiveEvent.SetCondition(objectiveTerm);
		
		array<ref TILW_MissionEvent> events = new array<ref TILW_MissionEvent>;
		events.Insert(defenderEvent);
		events.Insert(attackerEvent);
		events.Insert(timeLimitEvent);
		events.Insert(objectiveEvent);
		
		framework.SetMissionEvents(events);
		framework.SetPlayersKilledFlags(flags);
	}
	
	void SetupAO()
	{
		array<vector> points = new array<vector>;
		
		vector position1 = m_teams[1].m_spawn.m_position;
		vector position2 = m_objective.building.GetOrigin();

		vector direction = vector.Direction(position1, position2).Normalized();

		// Calculate perpendicular directions at 45 degrees
	    vector perpendicular1 = Vector(direction[2], 0, -direction[0]); // Rotate 90 degrees clockwise
	    vector perpendicular2 = Vector(-direction[2], 0, direction[0]); // Rotate 90 degrees counterclockwise
	
	    // Scale perpendicular vectors
	    vector offset1 = (-direction + perpendicular1) * (m_width / 2); // 45 degrees clockwise
	    vector offset2 = (-direction + perpendicular2) * (m_width / 2); // 45 degrees counterclockwise
		vector offset3 = (direction + perpendicular2) * (m_width / 2); // Opposite direction + 45 degrees counterclockwise
	    vector offset4 = (direction + perpendicular1) * (m_width / 2); // Opposite direction + 45 degrees clockwise

	    // Calculate points
	    points.Insert(position1 + offset1);
	    points.Insert(position1 + offset2);
	    points.Insert(position2 + offset3);
	    points.Insert(position2 + offset4);
		
		// Spawn AO
		m_manager.SpawnAO(points);
	}
	
	void SetupBriefing()
	{
		TimeAndWeatherManagerEntity manager = ChimeraWorld.CastFrom(GetGame().GetWorld()).GetTimeAndWeatherManager();
		int hour, minute, second;
		manager.GetHoursMinutesSeconds(hour, minute, second);
		
		string h;
		if(hour < 10)
			h =  "0" + hour.ToString();
		else
			h =  hour.ToString();
		
		string m;
		if(minute < 10)
			m =  "0" + minute.ToString();
		else
			m =  minute.ToString();
		
		string time = string.Format("%1%2",h,m);
		string year = manager.GetYear().ToString();
		string weather = WidgetManager.Translate(manager.GetCurrentWeatherState().GetLocalizedName());
		
		//Defender
		string defender, dOrbat, dVehicles;
 		m_teams[0].GetBriefing(defender, dOrbat, dVehicles);
		
		//Attacker
		string attacker, aOrbat, aVehicles;
 		m_teams[1].GetBriefing(attacker, aOrbat, aVehicles);
		
		SCR_FactionManager fm = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		
		foreach(GC_R_Briefing briefing : m_briefing)
		{
			PS_MissionDescription md = PS_MissionDescription.Cast(m_manager.SpawnPrefab(briefing.GetPrefab()));
			md.SetTitle(briefing.m_title);
			
			string description;
			if(briefing.m_variables == GC_R_EBriefingVars.General)
				description = string.Format(briefing.m_description,m_scenarioName,time,year,weather,attacker,defender);
			else
				description = string.Format(briefing.m_description,defender,dOrbat,dVehicles,attacker,aOrbat,aVehicles);
			
			md.SetTextData(description);
			
			switch(briefing.m_side){
				case GC_R_EBriefing.Team0:
					md.SetVisibleForFaction(fm.GetFactionByKey(m_teams[0].GetFaction()), true);
					md.SetVisibleForEmptyFaction(false);
					break;
				case GC_R_EBriefing.Team1:
					md.SetVisibleForFaction(fm.GetFactionByKey(m_teams[1].GetFaction()), true);
					md.SetVisibleForEmptyFaction(false);
					break;
				case GC_R_EBriefing.All:
					md.SetVisibleForEmptyFaction(true);
					md.SetShowForAnyFaction(true);
				default:
					md.SetVisibleForEmptyFaction(true);
					md.SetShowForAnyFaction(false);
			}
		}
	}
	
	void SetupMarkers()
	{
		ResourceName prefab = "{DA7538C821CFD1FE}worlds/Burn0ut7/Roulette/Prefabs/RouletteMarker.et";
		
		// Objective
		vector position = m_objective.building.GetOrigin();
		PS_ManualMarker marker = PS_ManualMarker.Cast(m_manager.SpawnPrefab(prefab, position));
		marker.SetImageSet("{E23427CAC80DA8B7}UI/Textures/Icons/icons_mapMarkersUI.imageset");
		marker.SetQuadName("circle-2");
		marker.SetColor(Color.Red);
		marker.SetSize(100);

		// Spawns
		FactionManager fm = GetGame().GetFactionManager();
		
		// Defenders
		SCR_Faction defenders = SCR_Faction.Cast(fm.GetFactionByKey(m_teams[0].GetFaction()));
		marker = PS_ManualMarker.Cast(m_manager.SpawnPrefab(prefab, m_teams[0].m_spawn.m_position));
		marker.SetUseWorldScale(false);
		marker.SetImageSet(defenders.GetFactionFlag());
		marker.SetAngles({0, 90, 0});
		marker.SetSize(40);

		// Attackers
		SCR_Faction attackers = SCR_Faction.Cast(fm.GetFactionByKey(m_teams[1].GetFaction()));
		marker = PS_ManualMarker.Cast(m_manager.SpawnPrefab(prefab, m_teams[1].m_spawn.m_position));
		marker.SetUseWorldScale(false);
		marker.SetImageSet(attackers.GetFactionFlag());
		marker.SetAngles({0, 90, 0});
		marker.SetSize(40);
	}

	GC_R_ObjAttackDefend GetObjective()
	{
		return m_objective;
	}
	
	bool GetTeams(bool isWater)
	{
		if(isWater)
			m_teams = m_manager.SelectTeams(2, m_teamsList, true);
		else
			m_teams = m_manager.SelectTeams(2, m_teamsList);
		
		if(!m_manager.m_aForcedTeams.IsEmpty())
		{
			m_teams.Clear();
			
			foreach(int index : m_manager.m_aForcedTeams)
			{
				ref GC_R_Team team = m_teamsList[index];
				if(!team)
				{
					PrintFormat("GC Roulette | Unable to find forced team a %1!", index);
					continue;
				}
				
				m_teams.Insert(team);
			}
		}
		
		if(m_teams.Count() == 2)
			return true;
		
		return false;
	}
	
	protected void ObjectiveFound()
	{
		Print("GC Roulette | Objective found: " + m_objective.building + " - Nearby buildings: " + m_objective.buildings.Count());

		FindAttackerSpawnAsync();
	}
	
	protected void FindObjectiveAsync()
	{
		if (m_searching)
			return;

		m_searching = true;
		GetGame().GetCallqueue().CallLater(FindObjective, 1, false);
	}
	
	protected void FindObjective()
	{
		GC_R_BuidlingQuery query = new GC_R_BuidlingQuery();
		query.BlackListPrefabs = m_buildingBlackList;
		query.BlackListWords = m_buildingBlackListWords;

		for (int i = 0; i < 5; i++) // do a few iterations per frame
		{
			query.Clear();
			IEntity building = m_manager.FindBuildingRandom(query);
			if (!building)
				continue;

			query.Clear();
			array<IEntity> buildings = m_manager.FindBuildings(building.GetOrigin(), m_buildingSearchSize, query);
			if (buildings.Count() < m_minbuildingCount)
				continue;

			m_objective = new GC_R_ObjAttackDefend();
			m_objective.building = building;
			m_objective.buildings = buildings;

			m_searching = false;
			ObjectiveFound();
			
			return;
		}

		GetGame().GetCallqueue().CallLater(FindObjective, 1, false);
	}
	
	protected void AttackerSpawnFound()
	{
		Print("GC Roulette | Attacker Spawn pos found: " + m_attackerSpawn.m_position);
		m_searching = false;

		bool isWater = m_manager.CheckWater(m_maxWaterArea, m_attackerSpawn.m_position, m_objective.building.GetOrigin());
		bool foundTeams = GetTeams(isWater);
		if(!foundTeams)
		{
			Print("GC Roulette | Not enough teams, rerolling");
			return m_manager.Reroll();
		}
		
		m_teams[1].m_spawn = m_attackerSpawn;
		FindDefenderSpawnAsync();
	}
	
	protected void FindAttackerSpawnAsync()
	{
		if (m_searching)
			return;
		
		m_searching = true;
		m_attackerSpawn = new GC_R_SpawnAttacker();
		m_attackerSpawn.m_scenario = this;
		m_attackerSpawn.m_searchCount = 100;

		GetGame().GetCallqueue().CallLater(FindAttackerSpawn, 1, false);
	}
	
	protected void FindAttackerSpawn()
	{
		bool found = m_attackerSpawn.GetSpawn();
		if(found)
			return AttackerSpawnFound();
		
		if(m_attackerSpawn.m_searchCount <= 0)
		{
			Print("GC Roulette | No attacker spawn found, rerolling");
			m_manager.Reroll();
			return;
		}

		GetGame().GetCallqueue().CallLater(FindAttackerSpawn, 1, false);
	}
	
	protected void DefenderSpawnFound()
	{
		Print("GC Roulette | Defender Spawn pos found: " + m_teams[0].m_spawn.m_position);
		m_searching = false;
		
		GC_R_Team defender = m_teams[0];
		GC_R_Team attacker = m_teams[1];
		
		float ratio = (defender.GetStrength() - attacker.GetStrength()) + m_ratio;
		
		int defenderRatio = Math.AbsInt(m_manager.m_targetPlayerCount / (1 + ratio));
		int defenderCount = defender.GetElements(defenderRatio);
		
		int attackerRatio = Math.AbsInt(defenderCount * ratio);
		int attackerCount = attacker.GetElements(attackerRatio);
		Print("GC Roulette | Ratio: " + ratio + " - Attacker: " + attackerCount + " - Defender: " + defenderCount);
		
		vector dir = vector.Direction(defender.m_spawn.m_position, m_teams[1].m_spawn.m_position);
		float yaw = dir.ToYaw();
		bool isSucessful = m_manager.SpawnTeam(defender.m_spawn.m_position, yaw, defender);
		if(!isSucessful)
		{
			Print("GC Roulette | Team spawn failed - Defender");
			return m_manager.Reroll();
		}

		dir = vector.Direction(attacker.m_spawn.m_position, m_objective.building.GetOrigin());
		yaw = dir.ToYaw();
		isSucessful = m_manager.SpawnTeam(attacker.m_spawn.m_position, yaw, attacker);
		if(!isSucessful)
		{
			Print("GC Roulette | Team spawn failed - Attacker");
			return m_manager.Reroll();
		}
		
		SetupOBJ();
		SetupAO();
		SetupFW();
		SetupMarkers();

		int year = m_manager.GetRandom().RandIntInclusive(m_yearStart, m_yearEnd);
		m_manager.SetRandomEnvironment(year);
		
		SetupBriefing();
		
		m_manager.ResetMapMenu();
	}
	
	protected void FindDefenderSpawnAsync()
	{
		if (m_searching)
			return;
		
		m_searching = true;
		GC_R_Team defender = m_teams[0];
		GC_R_SpawnDefender spawn = new GC_R_SpawnDefender();
		spawn.m_scenario = this;
		spawn.m_searchCount = 100;
		defender.m_spawn = spawn;
		
		GetGame().GetCallqueue().CallLater(FindDefenderSpawn, 1, false);
	}
	
	protected void FindDefenderSpawn()
	{
		GC_R_SpawnDefender spawn = GC_R_SpawnDefender.Cast(m_teams[0].m_spawn);
		bool found = spawn.GetSpawn();
		if(found)
			return DefenderSpawnFound();
		
		if(m_teams[0].m_spawn.m_searchCount <= 0)
		{
			Print("GC Roulette | No defender spawn found");
			m_manager.Reroll();
			return;
		}

		GetGame().GetCallqueue().CallLater(FindDefenderSpawn, 1, false);
	}
}


enum GC_R_EBriefingVars
{
	General,
	Team
}

enum GC_R_EBriefing
{
	Empty,
	All,
	Team0,
	Team1,
	Team2,
	Team3,
	Team4,
	Team5
}

[BaseContainerProps(), BaseContainerCustomTitleField("Briefing")]
class GC_R_Briefing
{
	protected static const ResourceName PREFAB = "{3136BE42592F3B1B}PrefabsEditable/MissionDescription/EditableMissionDescription.et";
	
	[Attribute("", UIWidgets.EditBox)]
	string m_title;
	
	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBoxMultiline, desc: "Briefing description. %1 = Defender team, %2 = Attacker team, %3 = Defender Element, %4 = Attacker Element, %5 = Scenario", category: "Attack and Defend")]
	string m_description;
	
	[Attribute("0", UIWidgets.ComboBox, desc: "What side is this briefing intended for?", "", ParamEnumArray.FromEnum(GC_R_EBriefing))]
	GC_R_EBriefing m_side;	
	
	[Attribute("0", UIWidgets.ComboBox, desc: "Which variable set to use", "", ParamEnumArray.FromEnum(GC_R_EBriefingVars))]
	GC_R_EBriefingVars m_variables;
	
	ResourceName GetPrefab()
	{
		return PREFAB;
	}
}

[BaseContainerProps(), BaseContainerCustomTitleField("Team")]
class GC_R_Team
{
	[Attribute("", UIWidgets.EditBox)]
	protected string m_name;

	[Attribute("", UIWidgets.EditBox, desc: "Faction key from the faction manager")]
	protected FactionKey m_factionKey;
	
	[Attribute("1", UIWidgets.Auto, desc: "Relative strength. Less = weaker, More = stronger")]
	protected float m_strength;
	
	[Attribute("0", UIWidgets.EditBox, desc: "Use only squad callsigns from the faction manager for platoons and squads")]
	protected bool m_squadCallsignsOnly;	
	
	[Attribute("0", UIWidgets.EditBox, desc: "AOs with more then 20 meters of water will be blocked.")]
	protected bool m_waterCapable;
	
	[Attribute("", UIWidgets.Auto, desc: "This team cannot versus these faction keys.")]
    protected ref array<FactionKey> m_versusBlacklist;
	
	[Attribute(defvalue: "", UIWidgets.Object)]
    protected ref array<ref GC_R_Company> m_companies;
	
	int m_totalPlayers = 0;

	ref GC_R_SpawnBase m_spawn;
	
	string GetName() { return m_name; }
	
	bool GetWaterCapable() { return m_waterCapable; }
	
	FactionKey GetFaction(){ return m_factionKey; }
	
	float GetStrength(){ return m_strength; }
	
	bool GetCallsign(){ return m_squadCallsignsOnly; }
	
	array<FactionKey> GetBlacklist(){ return m_versusBlacklist; }
	
	array<ref GC_R_Company> GetCompanies(){ return m_companies; }
	
	ref array<GC_R_ElementBase> m_elements = {};
	
	bool IsBlacklisted(GC_R_Team team)
	{
		FactionKey otherFaction = team.GetFaction();
		
		if(otherFaction == m_factionKey)
			return true;
		
		if(m_versusBlacklist.Contains(otherFaction))
			return true;
	
		return false;
	}
	
	int GetElements(int targetPlayers)
	{
		array<GC_R_ElementBase> outElements = {};
		m_totalPlayers = 0;
		
		foreach (GC_R_Company company : m_companies)
		{
			array<GC_R_ElementBase> elements = company.Fill(targetPlayers, this);
			foreach(GC_R_ElementBase element : elements)
				outElements.Insert(element);
		}
		
		m_elements = Order(outElements);
		
		int totalCount = 0;
		foreach(GC_R_ElementBase element : outElements)
		{
			totalCount += element.Count();
		}
		
		return totalCount;
	}
	
	void GetElementCounts(out int company, out int platoon, out int squad)
	{
		company = 0;
		platoon = 0;
		squad = 0;
		
		foreach(GC_R_ElementBase element : m_elements)
		{
			if(GC_R_Company.Cast(element))
				company++;
			
			if(GC_R_Platoon.Cast(element))
				platoon++;
			
			if(GC_R_Squad.Cast(element))
				squad++;
		}
	}
	
	void GetBriefing(out string name, out string orbat, out string vehicles)
	{
		name = m_name;
		
		int company, platoon, squad;
		GetElementCounts(company, platoon, squad);
		
		if(company)
			orbat += company.ToString() + "x Company\n";
		
		if(platoon)
			orbat += platoon.ToString() + "x Platoon\n";
		
		if(squad)
			orbat += squad.ToString() + "x Squad\n";
		
		map<string, int> vehicleMap = GetVehicleMap();
		foreach(string _name, int count: vehicleMap)
		{
			vehicles += string.Format("%1x - %2\n",count,_name);
		}
	}

	map<string, int> GetVehicleMap()
	{
		map<string, int> vehicles = new map<string, int>;
		
		foreach(GC_R_ElementBase element : m_elements)
		{
			IEntity vehicle = element.m_vehicle;
			if(!vehicle)
				continue;

			SCR_EditableVehicleComponent editable = SCR_EditableVehicleComponent.Cast(vehicle.FindComponent(SCR_EditableVehicleComponent));
			string name = WidgetManager.Translate(editable.GetDisplayName());
		
			int count = vehicles.Get(name);
			count++;
			
			vehicles.Set(name, count);
		}
		
		return vehicles;
	}
	
    array<GC_R_ElementBase> Order(array<GC_R_ElementBase> pickedElements)
    {
        array<GC_R_ElementBase> ordered = {};

        foreach (GC_R_Company company : m_companies)
        {
            if (pickedElements.Contains(company)) 
                ordered.Insert(company);

            foreach (GC_R_Platoon platoon : company.GetPlatoons())
            {
                if (pickedElements.Contains(platoon))
                    ordered.Insert(platoon);

                foreach (GC_R_Squad squad : platoon.GetSquads())
                {
                    if (pickedElements.Contains(squad))
                        ordered.Insert(squad);
                }
            }
        }
		
        return ordered;
    }
}

[BaseContainerProps(), BaseContainerCustomTitleField("Company")]
class GC_R_Company : GC_R_ElementBase
{
	[Attribute(defvalue: "", UIWidgets.Object)]
    protected ref array<ref GC_R_Platoon> m_platoons;

	void SetParents()
	{
		foreach(GC_R_Platoon platoon : m_platoons)
		{
			platoon.SetParent(this);
		}
	}
	
	array<ref GC_R_Platoon> GetPlatoons()
	{
		return m_platoons;
	}
	
	array<GC_R_ElementBase> Fill(int target, GC_R_Team team)
	{
		array<GC_R_ElementBase> elements = {};
		int platoons = 0;
		bool picked = false;
		
		SetParents();
		
		foreach(GC_R_Platoon platoon : m_platoons)
		{
			array<GC_R_ElementBase> outElements = platoon.Fill(target, team);

			if(outElements.Count() == 0)
				continue;

			platoons++;
			foreach(GC_R_ElementBase element : outElements)
				elements.Insert(element);

			if(platoons >= 2 && !picked)
			{
				int finalCount = Count() + team.m_totalPlayers;
				if(!IsCloser(finalCount, team.m_totalPlayers, target) && team.m_totalPlayers >= target)
					break;	
				
				elements.Insert(this);
				team.m_totalPlayers = finalCount;
				picked = true;
			}
		}
		
		return elements;
	}
}

[BaseContainerProps(), BaseContainerCustomTitleField("Platoon")]
class GC_R_Platoon : GC_R_ElementBase
{
	[Attribute(defvalue: "", UIWidgets.Object)]
    protected ref array<ref GC_R_Squad> m_squads;
	
	void SetParents()
	{
		foreach(GC_R_Squad squad : m_squads)
		{
			squad.SetParent(this);
		}
	}
	
	array<ref GC_R_Squad> GetSquads()
	{
		return m_squads;
	}
	
	array<GC_R_ElementBase> Fill(int target, GC_R_Team team)
	{
		array<GC_R_ElementBase> elements = {};
		int squads = 0;
		bool picked = false;
		
		SetParents();
		
		foreach(GC_R_Squad squad : m_squads)
		{
			int finalCount = squad.Count() + team.m_totalPlayers;
			
			if(!IsCloser(finalCount, team.m_totalPlayers, target) && team.m_totalPlayers >= target)
				break;
			
			squads++;
			elements.Insert(squad);
			team.m_totalPlayers = finalCount;
			
			if(squads >= 2 && !picked)
			{
				finalCount = Count() + team.m_totalPlayers;
				if(!IsCloser(finalCount, team.m_totalPlayers, target) && team.m_totalPlayers >= target)
					break;	

				elements.Insert(this);
				team.m_totalPlayers = finalCount;
				picked = true;
			}
		}
		
		return elements;
	}
}

[BaseContainerProps()]
class GC_R_Squad : GC_R_ElementBase {}

[BaseContainerProps()]
class GC_R_ElementBase
{
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Player prefab to spawn in")]
	protected ResourceName m_prefab;
	
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Vehicle prefab")]
	protected ResourceName m_vehiclePrefab;

	protected GC_R_ElementBase m_parent;
	
	IEntity m_vehicle;
	IEntity m_group;
	
	void SetParent(GC_R_ElementBase parent)
	{
		m_parent = parent;
	}

	GC_R_ElementBase GetParent()
	{
		return m_parent;
	}
	
	ResourceName GetPrefab()
	{
		return m_prefab;
	}
	
	ResourceName GetVehicle()
	{
		return m_vehiclePrefab;
	}
	
	int Count()
	{
		if(!m_prefab)
			return 0;
			
		array<ResourceName> slots;
		Resource.Load(m_prefab).GetResource().ToBaseContainer().Get("m_aUnitPrefabSlots", slots);
		
		return slots.Count();
	}
	
	protected bool IsCloser(int newTotal, int currentTotal, int target)
	{
		return Math.AbsInt(target - newTotal) <= Math.AbsInt(target - currentTotal);
	}
}

enum GC_R_EAssetFlags
{
	Ground = 1 << 0,
	Air = 1 << 1,
	Water = 1 << 2
}

class GC_R_ObjBase{}

class GC_R_ObjAttackDefend : GC_R_ObjBase
{
	IEntity building;
	ref array<IEntity> buildings = {};
}

class GC_R_SpawnBase
{
	bool m_found = false;
	vector m_position;
	GC_R_Team m_team;
	int m_searchCount;
}

class GC_R_SpawnAttacker : GC_R_SpawnBase
{
	ref GC_R_AttackDefend m_scenario;
	
	bool GetSpawn()
	{
		for (int i = 0; i < 5; i++)
		{
			if(SearchLine())
			{
				m_found = true;
				return true;
			}
			
			m_searchCount--;
		}
		
		return false;
	}
	
	bool SearchLine()
	{
		GC_R_Manager manager = GC_R_Manager.GetInstance();
		
		float radius = 15;
		float searchDistance = 500;
		
		float angle = manager.GetRandom().RandFloatXY(0, Math.PI2);
		
		float stepDistance = searchDistance * 2;
		
		float distance = m_scenario.GetMinAO();
		float maxDistance = m_scenario.GetMaxAO();
		
		vector worldSize = Vector(manager.GetMapSize(), 0, manager.GetMapSize());
		vector edgeBuffer = Vector(manager.EdgeBuffer(), 0, manager.EdgeBuffer());
		vector objPos = m_scenario.GetObjective().building.GetOrigin();
		
		while(distance < maxDistance)
		{
			float directionX = Math.Cos(angle);
	    		float directionZ = Math.Sin(angle);
			
			vector position = Vector(directionX, 0, directionZ) * distance + objPos;
			position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
			
			if(!manager.InXZBounds(position, edgeBuffer, worldSize))
				break;

			bool found = manager.FindEmptyPosition(position, radius, searchDistance);
			if(found)
			{
				if(manager.SurfaceIsWater(position))
					return false;
				
				m_position = position;
				return true;
			}

			distance += stepDistance;
		}

		return false;
	}
}

class GC_R_SpawnDefender : GC_R_SpawnBase
{
	ref GC_R_AttackDefend m_scenario;
	
	bool GetSpawn()
	{
		for (int i = 0; i < 5; i++)
		{
			if(Search())
			{
				m_found = true;
				return true;
			}
			
			m_searchCount--;
		}
		
		return false;
	}
	
	bool Search()
	{
		float radius = 15;
		float searchDistance = 500;
		
		GC_R_Manager manager = GC_R_Manager.GetInstance();
		vector position = m_scenario.GetObjective().building.GetOrigin();
		
		bool found = manager.FindEmptyPosition(position, radius, searchDistance);
		if(found)
		{
			if(manager.SurfaceIsWater(position))
				return false;
			
			m_position = position;
			
			return true;
		}
		
		return false;
	}
}

class GC_R_BuidlingQuery : GC_R_BaseQuery
{
	ref array<ResourceName> BlackListPrefabs = {};
	ref array<string> BlackListWords = {};

	override bool QueryCallback(IEntity entity)
	{
		if(entity.GetRootParent() != entity)
			return true;
		
		if(!Building.Cast(entity))
			return true;
		
		if(IsEntityBL(entity))
			return true;
		
		if(!BlackListPrefabs.IsEmpty())
			if(ArrayContainsKind(entity, BlackListPrefabs))
				return true;
		
		m_entities.Insert(entity);

		return true;
	}
	
	bool IsEntityBL(IEntity entity)
	{
		string prefabName = entity.GetPrefabData().GetPrefabName();
		if(prefabName.IsEmpty())
			return true;
		
		foreach(string word: BlackListWords)
			if(prefabName.Contains(word))
				return true;
		
		return false;
	}
}

class GC_R_PhysQuery : GC_R_BaseQuery
{
	ref array<IEntity> m_ignoreEntities;
	bool m_found;
	
	override bool QueryCallback(IEntity entity)
	{
		if(entity != entity.GetRootParent())
			return true;
		
		if(m_ignoreEntities && m_ignoreEntities.Contains(entity))
			return true;
		
		if(!entity.GetPhysics())
			return true;
		
		m_found = true;
		
		return false;
	}
}

class GC_R_BaseQuery
{
	protected ref array<IEntity> m_entities = {};
	
	bool QueryCallback(IEntity entity)
	{
		m_entities.Insert(entity);
		
		return true;
	}

	void Clear()
	{
		m_entities.Clear();
	}
	
	array<IEntity> GetResults()
	{
		return m_entities;
	}
	
	bool ArrayContainsKind(IEntity entity, array<ResourceName> list)
	{
		EntityPrefabData epd = entity.GetPrefabData();
		if (!epd)
			return false;

		BaseContainer bc = epd.GetPrefab();
		if (!bc)
			return false;
		
		foreach (ResourceName rn : list)
			if (SCR_BaseContainerTools.IsKindOf(bc, rn))
				return true;

		return false;
	}
}

class GC_R_Helper<Class T>
{
	static void Shuffle(inout array<T> items, RandomGenerator random)
	{
		for (int i = items.Count() - 1; i > 0; i--)
		{
			int j = random.RandIntInclusive(0, i);
			T temp = items[i];
			items[i] = items[j];
			items[j] = temp;
		}
	}
	
	static void ShuffleR(inout array<ref T> items, RandomGenerator random)
	{
		for (int i = items.Count() - 1; i > 0; i--)
		{
			int j = random.RandIntInclusive(0, i);
			T temp = items[i];
			items[i] = items[j];
			items[j] = temp;
		}
	}
	
	static T RandomElement(array<T> items, RandomGenerator random)
	{
		if(items.IsEmpty())
			return null;
		
		int index = random.RandIntInclusive(0, items.Count()-1);
		return items[index];
	}
	
	static T RandomElementR(array<ref T> items, RandomGenerator random)
	{
		if(items.IsEmpty())
			return null;
		
		int index = random.RandIntInclusive(0, items.Count()-1);
		return items[index];
	}
	
	static int RandomIndex(array<T> items, RandomGenerator random)
	{
		if(items.IsEmpty())
			return null;
		
		return random.RandIntInclusive(0, items.Count()-1);
	}
	
	static int RandomIndexR(array<ref T> items, RandomGenerator random)
	{
		if(items.IsEmpty())
			return null;
		
		return random.RandIntInclusive(0, items.Count()-1);
	}
}