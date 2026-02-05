[ComponentEditorProps()]
class GC_R_ManagerClass : GameEntityClass
{
}

class GC_R_Manager : GameEntity
{
	[Attribute(defvalue: "500", UIWidgets.Auto)]
	protected float m_mapEdgeBuffer;
	
	[Attribute(defvalue: "100", uiwidget: UIWidgets.Auto, desc: "Target player count")]
    int m_targetPlayerCount;
	
	[Attribute(defvalue: "", UIWidgets.Object)]
    ref array<ref GC_R_BaseScenario> m_scenarios;
	
	[Attribute(defvalue: "", uiwidget: UIWidgets.Auto, desc: "Forced seed.")]
    int m_iForcedSeed;
	
	protected const static ResourceName m_AOPrefab = "{09D028F45D7163FF}worlds/Burn0ut7/Roulette/Prefabs/RouletteAO.et";
	
	protected const static float m_searchDistance = 250;
	
	protected static GC_R_Manager m_instance;
	protected BaseWorld m_world;
	protected float m_worldSize;
	protected ref RandomGenerator m_random = new RandomGenerator();
	protected ref GC_R_BaseScenario m_currentScenario;
	protected ref array<IEntity> m_entites = {};
	
	protected int m_iForcedScenario = -1;
	ref array<int> m_aForcedTeams = {};
	
	protected int m_seed;
	
	void GC_R_Manager(IEntitySource src, IEntity parent)
	{
		if(!GetGame().InPlayMode())
			return;
		
		SetEventMask(EntityEvent.INIT);
	}

	override protected void EOnInit(IEntity owner)
	{
		if(!GetGame().InPlayMode())
			return;
		
		ClientIntialize();
		
		RplComponent rpl = RplComponent.Cast(owner.FindComponent(RplComponent));
		if(!rpl || rpl.Role() != RplRole.Authority)
			return;
		
		Intialize();
	}

	void NewScenario()
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if(gameMode.GetState() == SCR_EGameModeState.GAME)
			return;
		
		GC_R_BaseScenario selected = GC_R_Helper<GC_R_BaseScenario>.RandomElementR(m_scenarios, m_random);
		
		if(m_iForcedScenario != -1)
			m_currentScenario = GC_R_BaseScenario.Cast(m_scenarios[m_iForcedScenario].Clone());
		else
			m_currentScenario = GC_R_BaseScenario.Cast(selected.Clone());


		for (int i = m_entites.Count() - 1; i >= 0; i--)
		{
		    IEntity entity = m_entites[i];
		    if (entity)
		        SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}

		m_entites.Clear();
		
		Print("GC Roulette | NewScenario = " + m_currentScenario.m_scenarioName);
		m_currentScenario.Intialize();
	}
	
	protected void ClientIntialize()
	{
		Print("GC Roulette | ClientIntialize");
		InitCommands();
	}
	
	protected void Intialize()
	{
		Print("GC Roulette | Intialize");
		
		SetSeed();
		
		m_instance = this;
		m_world = GetGame().GetWorld();
		m_worldSize = GetMapSize();
		
		NewScenario();
	}
	
	void Reroll()
	{
		SetSeed();
		NewScenario();
	}
	
	array<ref GC_R_Team> SelectTeams(int count, array<ref GC_R_Team> teams, bool isWaterCapable = false)
	{
		if(teams.IsEmpty() || teams.Count() < count)
			return null;
		
		array<ref GC_R_Team> pool = {};
		foreach (GC_R_Team team : teams)
		{
			if(isWaterCapable && !team.GetWaterCapable())
				continue;
			
			pool.Insert(team);
		}
		GC_R_Helper<GC_R_Team>.ShuffleR(pool, m_random);
		
		array<ref GC_R_Team> selected = {};
		
		foreach (GC_R_Team team : pool)
		{
			array<ref GC_R_Team> opponents = FindOpponents(team, count - 1, pool);
			if(!opponents.IsEmpty())
			{
				selected.Insert(team);
				foreach (GC_R_Team opp : opponents)
					selected.Insert(opp);
				
				break;
			}
		}
		
		if(selected.Count() < count)
			Print("GC Roulette | Failed to find enough valid teams (" + selected.Count() + "/" + count + ")", LogLevel.ERROR);
		
		GC_R_Helper<GC_R_Team>.ShuffleR(selected, m_random);
		
		return selected;
	}

	array<ref GC_R_Team> FindOpponents(GC_R_Team baseTeam, int needed, array<ref GC_R_Team> pool)
	{
		array<ref GC_R_Team> opponents = {};
		array<ref GC_R_Team> shuffled = {};
		
		foreach (GC_R_Team team : pool)
			if(baseTeam != team)
				shuffled.Insert(team);

		GC_R_Helper<GC_R_Team>.ShuffleR(shuffled, m_random);
		
		foreach (GC_R_Team other : shuffled)
		{
			if(baseTeam.IsBlacklisted(other) || other.IsBlacklisted(baseTeam))
				continue;
			
			bool conflict = false;
			foreach (GC_R_Team opp : opponents)
			{
				if(other.IsBlacklisted(opp) || opp.IsBlacklisted(other))
				{
					conflict = true;
					break;
				}
			}
	
			if(conflict)
				continue;
			
			opponents.Insert(other);
			
			if(opponents.Count() >= needed)
				break;
		}
		
		if(opponents.Count() < needed)
				return {};
		
		return opponents;
	}
	
	IEntity FindBuildingRandom(notnull GC_R_BaseQuery query)
	{
		IEntity building;
		
		while(building == null)
		{
			vector position = Vector(
				m_random.RandFloatXY(m_mapEdgeBuffer, m_worldSize),
				0,
				m_random.RandFloatXY(m_mapEdgeBuffer, m_worldSize)
			);
			position[1] = m_world.GetSurfaceY(position[0], position[2]);
			
			array<IEntity> buildings = FindBuildings(position, m_searchDistance, query);
			if(buildings.IsEmpty())
				continue;
			
			//Why doesn't helper work with IEntity?
			//int index = GC_R_Helper<IEntity>.RandomIndex(buildings, m_random);
			int index = m_random.RandIntInclusive(0, buildings.Count()-1);
			building = buildings[index];
		}
		
		return building;
	}
		
	array<IEntity> FindBuildings(vector position, float searchDistance, notnull GC_R_BaseQuery query)
	{
		m_world.QueryEntitiesBySphere(position, searchDistance, query.QueryCallback, null, EQueryEntitiesFlags.STATIC);
		
		array<IEntity> buildings = query.GetResults();
		
		return buildings;
	}
	
	bool FindEmptyPosition(inout vector position, float radius, float maxDistance, bool allowWater = false, float maxPitch = 15)
	{
		if(radius <= 0 || maxDistance <= 0)
			return false;
		
		float cellWidth = radius * Math.Sqrt(3);
		float cellHeight = radius * 2;
		int maxRadiusSteps = Math.Ceil(maxDistance / radius);
		
		for (int ring = 0; ring < maxRadiusSteps; ring++)
		{
			for (int x = -ring; x <= ring; x++)
			{
				float posX = cellWidth * x;
				float posY = cellHeight * (x - SCR_Math.fmod(x, 1)) * 0.5;
	
				int yMin = Math.Max(-ring -x, -ring);
				int yMax = Math.Min(ring -x, ring);
				int yStep;
				if(Math.AbsInt(x) == ring)
					yStep = 1;
				else 
					yStep = yMax - yMin;
	
				for (int y = yMin; y <= yMax; y += yStep)
				{
					vector searchPos = position + Vector(posX, 0, posY + cellHeight * y);
					if(vector.DistanceXZ(searchPos, position) > maxDistance - radius)
						continue;
	
					searchPos[1] = m_world.GetSurfaceY(searchPos[0], searchPos[2]);
		
					if(IsPositionEmpty(searchPos, radius))
					{
						if(!allowWater && SurfaceIsWater(searchPos))
							continue;
						
						float pitch = GetTerrainPitch(searchPos);
						if(pitch == -1 || pitch > maxPitch)
							continue;

						position = searchPos;
						return true;
					}
				}
			}
		}
		
		return false;
	}
	
	bool IsPositionEmpty(vector position, float radius)
	{
		GC_R_PhysQuery query = new GC_R_PhysQuery();

		m_world.QueryEntitiesBySphere(position, radius, query.QueryCallback, null, EQueryEntitiesFlags.ALL);
		if(!query.m_found)
			return true;

		return false;
	}
	
	bool SpawnTeam(vector position, float yaw, GC_R_Team team)
	{
		Print("GC Roulette | Team " + team.GetName() + " - " + team.m_elements);
		array<IEntity> tempEntities = {};
		bool found;
		
		bool firstCompany = true;
		bool firstPlatoon = true;
		bool firstSquad = true;
		
		int companyIndex = 0;
		int platoonIndex = 0;
		int squadIndex = 0;
		
		foreach(GC_R_ElementBase element : team.m_elements)
		{
			ResourceName prefab = element.GetPrefab();
			ResourceName vehiclePrefab = element.GetVehicle();
			vector endPosition = position;
			if(vehiclePrefab)
			{
				
				found = FindEmptyPosition(endPosition, 5, 200, false, 10);
				if(!found)
				{
					Print("GC Roulette | spawn team POS not found : " + vehiclePrefab + " - Pos : " + position);
					break;
				}
	
				IEntity vehicle = SpawnPrefab(vehiclePrefab, endPosition, yaw);
				element.m_vehicle = vehicle;
				
				vector min,max;
				vehicle.GetBounds(min,max);
				
				vector size = max - min;

				vector forward = vector.FromYaw(yaw);
				vector right   = vector.FromYaw(yaw + 90);

				vector offset = vector.Zero;
				offset += right * (size[0] + 1);
				offset += forward * (size[2] * 0.5);
				
				endPosition += offset;
			}else{
				float searchSize = element.Count();
				found = FindEmptyPosition(endPosition, searchSize, 300, false);
				if(!found)
				{
					Print("GC Roulette | spawn team POS not found : " + prefab + " - Pos : " + position);
					break;
				}
			}
			
			if(prefab)
			{
				SCR_AIGroup group =  SCR_AIGroup.Cast(SpawnPrefab(prefab, endPosition, yaw));
				element.m_group = group;
				AIFormationComponent formation = AIFormationComponent.Cast(group.FindComponent(AIFormationComponent));
				if(formation)
					formation.SetFormation("File");
				
				SCR_CallsignGroupComponent cgc = SCR_CallsignGroupComponent.Cast(group.FindComponent(SCR_CallsignGroupComponent));
				if(cgc)
				{
					typename elementType = element.Type();
					switch(elementType)
					{
						case GC_R_Company:
							if(!firstCompany)
								companyIndex++;
		
							platoonIndex = 0;
							squadIndex = 0;
							
							firstCompany = false;
							break;
						case GC_R_Platoon:
							if(!firstPlatoon)
								platoonIndex++;
							squadIndex = 1;
						
							firstPlatoon = false;
							break;
						case GC_R_Squad:
							if(!firstSquad)
								squadIndex++;
						
							if(firstSquad)
								squadIndex = 2;
						
							firstSquad = false;
							break;
					}
					
					cgc.ReAssignGroupCallsign(companyIndex, platoonIndex, squadIndex);
				}
				
				ResourceName cone = "{25C8B889A777399D}Prefabs/Props/Infrastructure/ConeTraffic/ConeTraffic_01_red.et";
				EntitySpawnParams spawnParams = new EntitySpawnParams();
				spawnParams.Transform[3] = endPosition;
				IEntity tempEntity = GetGame().SpawnEntityPrefabLocal(Resource.Load(cone), null, spawnParams);
				tempEntities.Insert(tempEntity);
			}
		}
		
		foreach(IEntity entity : tempEntities)
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
		
		return found;
	}
	
	bool CheckWater(float max, vector start, vector end)
	{
		static const float STEP = 10.0;
	
		vector flatStart = start;
		vector flatEnd   = end;
		flatStart[1] = 0;
		flatEnd[1]   = 0;
	
		vector dir = flatEnd - flatStart;
		float total = dir.Length();
	
		dir.Normalize();
	
		float waterArea = 0;
		float traveled = 0;
	
		while (traveled <= total)
		{
			vector position = flatStart + dir * traveled;
	
			if (SurfaceIsWater(position))
				waterArea += STEP;
			else	
				waterArea = 0;

			if (waterArea >= max)
				return true;
	
			traveled += STEP;
		}
	
		return false;
	}
	
	IEntity SpawnPrefab(ResourceName prefab, vector position = vector.Zero, float yaw = 0)
	{
		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.Transform[3] = position;
		
		IEntity entity = GetGame().SpawnEntityPrefab(Resource.Load(prefab), null, spawnParams);

		entity.SetYawPitchRoll(Vector(yaw,0,0));
		
		if(entity)
			m_entites.Insert(entity);
		
		return entity;
	}
	
	IEntity SpawnAO(array<vector> points)
	{
		IEntity entity = SpawnPrefab(m_AOPrefab);
		
		TILW_AOLimitComponent AOLimitComp = TILW_AOLimitComponent.Cast(entity.FindComponent(TILW_AOLimitComponent));
		AOLimitComp.SetPoints(points);
		
		TILW_MapShapeComponent coverComp = TILW_MapShapeComponent.Cast(entity.FindComponent(TILW_MapShapeComponent));
		coverComp.SetPoints3D(points);
		
		GetGame().GetCallqueue().CallLater(coverComp.SetPoints3D, 100, false, points);
		
		return entity;
	}
	
	protected ref array<ref Shape> m_aShapes = {};
	float GetTerrainPitch(inout vector position)
	{
		float surfaceY = m_world.GetSurfaceY(position[0], position[2]);
		position[1] = surfaceY;
		
		TraceParam param = new TraceParam();
		param.Flags = TraceFlags.ANY_CONTACT | TraceFlags.WORLD | TraceFlags.OCEAN | TraceFlags.ENTS;
		param.Start = position + vector.Up * 0.01;
		param.End = position - vector.Up * 0.01;
		
		float percent = m_world.TraceMove(param);
		if(percent == 0)
			return -1;
		
		position = vector.Lerp(param.Start, param.End, percent);
		float pitch = Math.Acos(param.TraceNorm[1]) * Math.RAD2DEG;
		
		return pitch;
	}
	
	bool SurfaceIsWater(vector pos)
	{
	    pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
	    vector outWaterSurfacePoint;
	    EWaterSurfaceType outType;
	    vector transformWS[4];
	    vector obbExtents;
		
	    ChimeraWorldUtils.TryGetWaterSurface(m_world, pos, outWaterSurfacePoint, outType, transformWS, obbExtents);
		
	    return outType != EWaterSurfaceType.WST_NONE;
	}
	
	static GC_R_Manager GetInstance()
	{
		return m_instance;
	}
	
	RandomGenerator GetRandom()
	{
		return m_random;
	}
	
	void SetSeed(int seed = -1)
	{
		if(m_iForcedSeed)
		{
			m_seed = m_iForcedSeed;
			Print("GC Roulette | Forced Seed = " + m_seed);
			return;
		}
		
		if(seed == -1)
		{
			int t = System.GetUnixTime();
			int tc = System.GetTickCount();
			int r = Math.RandomInt(int.MIN, int.MAX);
	
			// Mix bits to avoid predictable correlation
			seed = t ^ (tc << 16) ^ (r << 1);
			seed ^= (seed << 13);
			seed ^= (seed >> 17);
			seed ^= (seed << 5);
		}
		
		m_random.SetSeed(seed);
		m_seed = seed;
		Print("GC Roulette | Seed = " + seed);
	}
	
	float EdgeBuffer()
	{
		return m_mapEdgeBuffer;
	}
	
	float GetMapSize()
	{
		if(m_worldSize)
			return m_worldSize;
		
		vector mins, max;
		GetGame().GetWorldEntity().GetWorldBounds(mins, max);
		float size = Math.Round(Math.Min(max[0] - mins[0], max[2] - mins[2]));

		while(size > 0)
		{
			size--;
			
			float y = m_world.GetSurfaceY(size,size);
			if(y != -256)
				break;
		}
		
		size = Math.Max(0,size - m_mapEdgeBuffer);
		Print("GC Roulette | Map size = " + size);
		
		return size;
	}
	
	static bool InXZBounds(vector position, vector min, vector max)
    {
		if(position[0] < min[0] || position[0] > max[0])
			return false;
		
		if(position[2] < min[2] || position[2] > max[2])
			return false;
		
		return true;
    }
	
	
	void SetTime(float hours24 = -1, bool dayOnly = true)
	{
		TimeAndWeatherManagerEntity manager = ChimeraWorld.CastFrom(m_world).GetTimeAndWeatherManager();
		if(!manager)
			return;
	
		float sunrise, sunset;
		if(!manager.GetSunriseHour(sunrise) || !manager.GetSunsetHour(sunset))
		{
			sunrise = 8;
			sunset = 18;
		}
	
		float startTime = 0;
		float endTime = 24;
	
		if(dayOnly)
		{
			startTime = sunrise + 2;
			endTime = sunset - 2;
		}
	
		if(hours24 == -1)
			hours24 = m_random.RandFloatXY(startTime, endTime);
	
		hours24 = Math.Clamp(hours24, 0.0, 24.0);
		manager.SetTimeOfTheDay(hours24, true);
	}
	
	void SetDate(int year = -1, int month = -1, int day = -1)
	{
		TimeAndWeatherManagerEntity manager = ChimeraWorld.CastFrom(m_world).GetTimeAndWeatherManager();
		if(!manager)
			return;
		
		if(year == -1)
		{
			year = m_random.RandIntInclusive(1950,2050);
		}
		
		if(month == -1)
		{
			month = m_random.RandIntInclusive(1,12);
		}
		
		if(day == -1)
		{
			day = m_random.RandIntInclusive(1,28);
		}
		
		manager.SetDate(year, month, day, true);
	}
	
	void SetWeather()
	{
		TimeAndWeatherManagerEntity manager = ChimeraWorld.CastFrom(m_world).GetTimeAndWeatherManager();
		if(!manager)
			return;
		
		array<ref WeatherState> weatherStates = {};
		manager.GetWeatherStatesList(weatherStates);
		
		WeatherState selected = GC_R_Helper<WeatherState>.RandomElementR(weatherStates, m_random);
		
		manager.ForceWeatherTo(false, selected.GetStateName());
	}
	
	void SetRandomEnvironment()
	{
		SetDate();
		SetTime();
		SetWeather();
	}
	
	int GetScenarioIndex()
	{
		if (!m_currentScenario)
			return -1;
	
		string name = m_currentScenario.m_scenarioName;
	
		for (int i = 0; i < m_scenarios.Count(); i++)
		{
			GC_R_BaseScenario scenario = m_scenarios[i];
			if (!scenario)
				continue;
	
			if (scenario.m_scenarioName == name)
				return i;
		}
	
		return -1;
	}
	
	void SetRandomEnvironment(int year)
	{
		SetDate(year);
		SetTime();
		SetWeather();
	}
	
	protected void InitCommands()
	{
	    SCR_ChatPanelManager chatPanelManager = SCR_ChatPanelManager.GetInstance();
	    if(!chatPanelManager)
	        return;
	    chatPanelManager.GetCommandInvoker("seed").Insert(ClientSeed);
	    chatPanelManager.GetCommandInvoker("reroll").Insert(ClientReroll);
	    chatPanelManager.GetCommandInvoker("players").Insert(ClientPlayers);
	    chatPanelManager.GetCommandInvoker("teams").Insert(ClientTeams);
	    chatPanelManager.GetCommandInvoker("scenario").Insert(ClientScenario);
	}
	
	void InvokeCommand(SCR_PlayerController pc, string command, string data)
	{
		switch(command)
		{
			case "seed":
				return CommandSeed(pc, command, data);
			case "reroll":
				return CommandReroll(pc, command, data);
			case "players":
				return CommandPlayers(pc, command, data);
			case "scenario":
				return CommandScenario(pc, command, data);
			case "teams":
				return CommandTeams(pc, command, data);
		}
	}
	
	protected void ClientScenario(SCR_ChatPanel panel, string data)
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if(!pc)
			return;
		
		pc.GC_R_SendCommandServer("scenario", data)
	}
	
	protected void CommandScenario(SCR_PlayerController pc, string command, string data)
	{
		data.TrimInPlace();
	
		if (data.IsEmpty())
		{
			SendScenarioList(pc);
			return;
		}
	
		if (!SCR_Global.IsAdmin(pc.GetPlayerId()))
		{
			pc.GC_R_SendChatMsg("Unable to complete command - Not admin");
			return;
		}
	
		int index = data.ToInt(-1);
		if (index == -1)
		{
			pc.GC_R_SendChatMsg("Invalid scenario index. Usage: /scenario 0");
			return;
		}
	
		if (index < 0 || index >= m_scenarios.Count())
		{
			pc.GC_R_SendChatMsg(string.Format("Index out of range (0-%1).", m_scenarios.Count() - 1));
			return;
		}
	
		m_iForcedScenario = index;
		pc.GC_R_SendChatMsg(string.Format("Forced scenario set: %1 (%2)", index, m_scenarios[index].m_scenarioName));
	}
	
	protected void SendScenarioList(SCR_PlayerController pc)
	{
		for (int i = 0; i < m_scenarios.Count(); i++)
		{
			GC_R_BaseScenario scenario = m_scenarios[i];
			if (!scenario)
				continue;
	
			pc.GC_R_SendChatMsg(string.Format("Scenario %1: %2", i, scenario.m_scenarioName));
		}
	}
	
	protected void ClientTeams(SCR_ChatPanel panel, string data)
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if(!pc)
			return;
		
		pc.GC_R_SendCommandServer("teams", data)
	}
	
	protected void CommandTeams(SCR_PlayerController pc, string command, string data)
	{
		data.TrimInPlace();
	
		if (data.IsEmpty())
		{
			SendTeamsList(pc);
			return;
		}
	
		if (!SCR_Global.IsAdmin(pc.GetPlayerId()))
		{
			pc.GC_R_SendChatMsg("Unable to complete command - Not admin");
			return;
		}
	
		array<int> teams = {};
		if (!TryParseIntList(data, teams))
		{
			pc.GC_R_SendChatMsg("Usage: /teams 0 1 2");
			return;
		}
	
		if (!ValidateTeamIndices(pc, teams))
			return;
	
		m_aForcedTeams = teams;
	
		pc.GC_R_SendChatMsg("Forced teams set: " + FormatIntList(teams));
	}
	
	protected void SendTeamsList(SCR_PlayerController pc)
	{
		if (!m_currentScenario)
		{
			pc.GC_R_SendChatMsg("No scenario selected.");
			return;
		}
	
		int scenarioIndex = GetScenarioIndex();
		pc.GC_R_SendChatMsg(string.Format("Scenario %1: %2", scenarioIndex, m_currentScenario.m_scenarioName));
	
		for (int i = 0; i < m_currentScenario.m_teamsList.Count(); i++)
		{
			GC_R_Team team = m_currentScenario.m_teamsList[i];
			if (!team)
				continue;
	
			pc.GC_R_SendChatMsg(string.Format("Team %1: %2", i, team.GetName()));
		}
	}
	
	protected bool TryParseIntList(string data, notnull array<int> outInts)
	{
		outInts.Clear();
	
		data.TrimInPlace();
		if (data.IsEmpty())
			return false;
	
		data.Replace(",", " ");
	
		array<string> parts = {};
		data.Split(" ", parts, true);
	
		foreach (string part : parts)
		{
			part.TrimInPlace();
	
			if (part.IsEmpty())
				continue;
	
			int v = part.ToInt(-1);
			if (v == -1)
				continue;
	
			outInts.Insert(v);
		}
	
		if (outInts.IsEmpty())
			return false;
	
		return true;
	}
	
	protected bool ValidateTeamIndices(SCR_PlayerController pc, notnull array<int> teams)
	{
		if (!m_currentScenario)
		{
			pc.GC_R_SendChatMsg("No scenario selected.");
			return false;
		}
	
		int maxIdx = m_currentScenario.m_teamsList.Count() - 1;
		if (maxIdx < 0)
		{
			pc.GC_R_SendChatMsg("Scenario has no teams.");
			return false;
		}
	
		foreach (int id : teams)
		{
			if (id < 0 || id > maxIdx)
			{
				pc.GC_R_SendChatMsg(string.Format("Invalid team index: %1 (0-%2).", id, maxIdx));
				return false;
			}
		}
	
		return true;
	}
	
	protected string FormatIntList(notnull array<int> values)
	{
		string outString;
		for (int i = 0; i < values.Count(); i++)
		{
			if (i > 0)
				outString = outString + " ";
	
			outString = outString + values[i];
		}
	
		return outString;
	}

	
	protected void ClientSeed(SCR_ChatPanel panel, string data)
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if(!pc)
			return;
		
		pc.GC_R_SendCommandServer("seed", "")
	}
	
	protected void CommandSeed(SCR_PlayerController pc, string command, string data)
	{
		string msg = "Seed: " + m_seed.ToString();
		pc.GC_R_SendChatMsg(msg);
	}
	
	protected void ClientReroll(SCR_ChatPanel panel, string data)
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if(!pc)
			return;
		
		pc.GC_R_SendCommandServer("reroll", data)
	}
	
	protected void CommandReroll(SCR_PlayerController pc, string command, string data)
	{
		if(!SCR_Global.IsAdmin(pc.GetPlayerId()))
			return pc.GC_R_SendChatMsg("Unable to complete command - Not admin");
		
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if(gameMode.GetState() == SCR_EGameModeState.GAME)
			return pc.GC_R_SendChatMsg("Unable to complete command - Game already started");
		
		pc.GC_R_SendChatMsg("Rerolling, please wait ...");
		
		int seed = data.ToInt();
		if(seed)
			SetSeed(seed);
		else
			SetSeed();
		
		NewScenario();
	}
	
	protected void ClientPlayers(SCR_ChatPanel panel, string data)
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if(!pc)
			return;
		
		pc.GC_R_SendCommandServer("players", data)
	}
	
	protected void CommandPlayers(SCR_PlayerController pc, string command, string data)
	{
		if(!SCR_Global.IsAdmin(pc.GetPlayerId()))
			return pc.GC_R_SendChatMsg("Unable to complete command - Not admin");
		
		int num = data.ToInt();
		if(!num || num <= 10)
			return pc.GC_R_SendChatMsg("Unable to complete command - Invaild number");
		
		m_targetPlayerCount = num;
		pc.GC_R_SendChatMsg("Player target count set to " + num);
	}

	void ResetMapMenu()
	{
		RPC_ResetMapMenu();
		Rpc(RPC_ResetMapMenu);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_ResetMapMenu()
	{
		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.PreviewMapMenu);
	}
}