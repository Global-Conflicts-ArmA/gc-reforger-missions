[BaseContainerProps()]
class GC_R_BaseScenario
{
	void Intialize();
	void Reroll();
}

[BaseContainerProps(), BaseContainerCustomTitleField("Attack and Defend")]
class GC_R_AttackDefend: GC_R_BaseScenario
{
	[Attribute(defvalue: "50", uiwidget: UIWidgets.CheckBox, desc: "Amount of buildings needed in the ", category: "Objective")]
    float m_buildingSearchSize;
	
	[Attribute(defvalue: "10", uiwidget: UIWidgets.CheckBox, desc: "Amount of buildings needed in Building Search Size", category: "Objective")]
    int m_minbuildingCount;
	
	[Attribute(defvalue: "", uiwidget: UIWidgets.ResourceNamePicker, desc: "Blacklist building prefab or kind", category: "Objective")]
    ref array<ResourceName> m_buildingBlackList;
	
	[Attribute(defvalue: "", uiwidget: UIWidgets.Auto, desc: "Blacklist building of same name", category: "Objective")]
    ref array<string> m_buildingBlackListWords;
	
	protected GC_R_Manager m_manager;
	protected ref GC_R_ObjAttackDefend m_objective;
	
	ref 	array<ref GC_R_Team> m_teams = {};
	
	protected bool m_searching = false;
	
	override void Intialize()
	{
		m_manager = GC_R_Manager.GetInstance();

		//FindObjectiveAsync();
		m_teams = m_manager.SelectTeams(2, m_manager.m_teams);
		
		foreach(GC_R_Team team : m_teams)
			Print("GC Roulette | Team: " + team.GetName());

		//(int count, array<ref GC_R_Team> teams)
	}
	
	protected void ObjectiveFound()
	{
		Print("GC Roulette | Objective found: " + m_objective.building);
		Print("GC Roulette | Nearby buildings: " + m_objective.buildings.Count());
		
		
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
}

[BaseContainerProps(), BaseContainerCustomTitleField("Team")]
class GC_R_Team
{
	[Attribute("", UIWidgets.EditBox)]
	protected string m_name;

	[Attribute("", UIWidgets.EditBox, desc: "What is this teams faction key in the faction manager?")]
	protected FactionKey m_factionKey;
	
	[Attribute("1", UIWidgets.Auto, desc: "Relative strength. 1 = 1980s baseline. Less = weaker, More = stronger")]
	protected float m_strength;
	
	[Attribute("0", UIWidgets.EditBox, desc: "Use only squad callsigns from the faction manager")]
	protected bool m_squadCallsignsOnly;
	
	[Attribute("", UIWidgets.Auto, desc: "This team cannot versus these faction keys.")]
    protected ref array<FactionKey> m_versusBlacklist;
	
	[Attribute(defvalue: "", UIWidgets.Object)]
    protected ref array<ref GC_R_Company> m_companies;
	
	[Attribute(defvalue: "", UIWidgets.Object)]
    protected ref array<ref GC_R_AssetPack> m_assets;
	
	string GetName() { return m_name; }
	
	FactionKey GetFaction(){ return m_factionKey; }
	
	float GetStrength(){ return m_strength; }
	
	bool GetCallsign(){ return m_squadCallsignsOnly; }
	
	array<FactionKey> GetBlacklist(){ return m_versusBlacklist; }
	
	array<ref GC_R_Company> GetCompanies(){ return m_companies; }
	
	bool IsBlacklisted(GC_R_Team team)
	{
		FactionKey otherFaction = team.GetFaction();
		
		if(otherFaction == m_factionKey)
			return true;
		
		if(m_versusBlacklist.Contains(otherFaction))
			return true;
	
		return false;
	}
}

[BaseContainerProps(), BaseContainerCustomTitleField("Company")]
class GC_R_Company : GC_R_ElementBase
{
	[Attribute(defvalue: "", UIWidgets.Object)]
    protected ref array<ref GC_R_Platoon> m_platoons;

	array<ref GC_R_Platoon> GetPlatoons()
	{
		return m_platoons;
	}
}

[BaseContainerProps(), BaseContainerCustomTitleField("Platoon")]
class GC_R_Platoon : GC_R_ElementBase
{
	[Attribute(defvalue: "", UIWidgets.Object)]
    protected ref array<ref GC_R_Squad> m_squads;
	
	array<ref GC_R_Squad> GetSquads()
	{
		return m_squads;
	}
}

[BaseContainerProps()]
class GC_R_Squad : GC_R_ElementBase {}

[BaseContainerProps()]
class GC_R_ElementBase
{
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Prefab to spawn in for this element.")]
	protected ResourceName m_prefab;

	ResourceName GetPrefab()
	{
		return m_prefab;
	}
	
	int GetPlayerCount()
	{
		array<ResourceName> slots;
		Resource.Load(m_prefab).GetResource().ToBaseContainer().Get("m_aUnitPrefabSlots", slots);

		return slots.Count();
	}
}

enum GC_R_EAssetType
{
	Walk,
	Ground,
	Air,
	Water
}

[BaseContainerProps()]
class GC_R_Asset
{
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Asset prefab")]
	protected ResourceName m_prefab;
	
	[Attribute("1", UIWidgets.Auto, desc: "Relative strength. 1 = Armed HMMV. Less = weaker, More = stronger")]
	protected float m_strength;

	[Attribute("0", UIWidgets.Auto, desc: "Spawn enough of these vehicles for the team")]
	protected bool m_transport;
	
	ResourceName GetPrefab()
	{
		return m_prefab;
	}
	
	int GetPlayerCount()
	{
		array<ResourceName> slots;
		Resource.Load(m_prefab).GetResource().ToBaseContainer().Get("m_aUnitPrefabSlots", slots);

		return slots.Count();
	}
}

[BaseContainerProps()]
class GC_R_AssetPack
{
	[Attribute(defvalue: "0", UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(GC_R_EAssetType))]
    protected GC_R_EAssetType m_type;
	
	[Attribute(defvalue: "", UIWidgets.Object)]
    protected ref array<ref GC_R_Asset> m_assets;
}



class GC_R_ObjBase{}

class GC_R_ObjAttackDefend : GC_R_ObjBase
{
	IEntity building;
	ref array<IEntity> buildings = {};
	
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