[BaseContainerProps()]
class GC_R_BaseScenario
{
	void Intialize();
	void Reroll();
}

[BaseContainerProps()]
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
	protected bool m_searching = false;
	
	override void Intialize()
	{
		m_manager = GC_R_Manager.GetInstance();

		FindObjectiveAsync();
	
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

class GC_R_ObjBase{};

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