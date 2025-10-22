[ComponentEditorProps()]
class GC_R_ManagerClass : GameEntityClass
{
}

class GC_R_Manager : GameEntity
{
	[Attribute(defvalue: "", UIWidgets.Object)]
    ref array<ref GC_R_BaseScenario> m_scenarios;	
	
	[Attribute(defvalue: "500", UIWidgets.Auto)]
	float m_mapEdgeBuffer;
	
	protected const float m_searchDistance = 250;
	
	protected static GC_R_Manager m_instance;
	protected BaseWorld m_world;
	protected float m_worldSize;
	protected ref RandomGenerator m_random = new RandomGenerator();
	protected GC_R_BaseScenario m_currentScenario;
	
	void GC_R_Manager(IEntitySource src, IEntity parent)
	{
		if(!GetGame().InPlayMode())
			return;
		
		SetEventMask(EntityEvent.INIT);
	}

	override protected void EOnInit(IEntity owner)
	{
		Intialize();
	}
	
	void NewScenario()
	{
		int index = m_random.RandIntInclusive(0, m_scenarios.Count());
		m_currentScenario = m_scenarios[index];
		
		Print("GC Roulette | NewScenario = " + m_currentScenario);
		m_currentScenario.Intialize()	
	}
	
	void Intialize()
	{
		Print("GC Roulette | Intialize");

		m_instance = this;
		SetSeed(System.GetUnixTime());
		SetSeed(1);
		m_world = GetGame().GetWorld();
		m_worldSize = GetMapSize();
		
		NewScenario();
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

			int index = m_random.RandIntInclusive(0, buildings.Count());
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
	
	
	static GC_R_Manager GetInstance()
	{
		return m_instance;
	}
	
	void SetSeed(int seed = 0)
	{
		m_random.SetSeed(seed);
		Print("GC Roulette | Seed = " + seed);
	}
	
	float GetMapSize()
	{
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
		
		size = Math.Max(0,size);
		Print("GC Roulette | Map size = " + size);
		
		return size;
	}
	
	void SetTime(float hours24 = -1, bool dayOnly = true)
	{
		TimeAndWeatherManagerEntity manager = ChimeraWorld.CastFrom(m_world).GetTimeAndWeatherManager();
		if (!manager)
			return;
	
		float sunrise, sunset;
		if (!manager.GetSunriseHour(sunrise) || !manager.GetSunsetHour(sunset))
		{
			sunrise = 8;
			sunset = 18;
		}
	
		float startTime = 0;
		float endTime = 24;
	
		if (dayOnly)
		{
			startTime = sunrise + 2;
			endTime = sunset - 2;
		}
	
		if (hours24 == -1)
			hours24 = m_random.RandFloatXY(startTime, endTime);
	
		hours24 = Math.Clamp(hours24, 0.0, 24.0);
		manager.SetTimeOfTheDay(hours24, true);
	}
}