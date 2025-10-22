[BaseContainerProps()]
class GC_R_BaseScenario
{
	void Intialize();
	void Reroll();
}

[BaseContainerProps()]
class GC_R_AttackDefend: GC_R_BaseScenario
{
	[Attribute(defvalue: "10", uiwidget: UIWidgets.CheckBox, desc: "Amount of buildings needed in the ", category: "Objective")]
    float m_buildingSearchSize;
	
	[Attribute(defvalue: "10", uiwidget: UIWidgets.CheckBox, desc: "Amount of buildings needed in Building Search Size", category: "Objective")]
    int m_minbuildingCount;
	
	[Attribute(defvalue: "", uiwidget: UIWidgets.ResourceNamePicker, desc: "Blacklist building prefab or kind", category: "Objective")]
    ref array<ResourceName> m_buildingBlackList;
	
	protected GC_R_Manager manager;
	override void Intialize()
	{
		manager = GC_R_Manager.GetInstance();
		
		GC_R_BuidlingQuery query = new GC_R_BuidlingQuery();
		query.BuildingBlackList = m_buildingBlackList;
		
		IEntity building = manager.FindBuildingRandom(query);
		Print("GC Roulette | building = " + building);
	}
}

class GC_R_BuidlingQuery : GC_R_BaseQuery
{
	ref array<ResourceName> BuildingBlackList = {};

	override bool QueryCallback(IEntity entity)
	{
		if(entity.GetRootParent() != entity)
			return true;
		
		if(!Building.Cast(entity))
			return true;
		
		if(!BuildingBlackList.IsEmpty())
		{
		
		}
		
		m_entities.Insert(entity);

		return true;
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

	array<IEntity> GetResults()
	{
		return m_entities;
	}
}

class GC_R_Helper<Class T>
{
	static T RandomElement(array<T> items, RandomGenerator random)
	{
		if(items.IsEmpty())
			return null;
		
		int index = random.RandIntInclusive(0, items.Count());
		
		return items[index];
	}
	
	static T RandomElementR(array<ref T> items, RandomGenerator random)
	{
		if(items.IsEmpty())
			return null;
		
		int index = random.RandIntInclusive(0, items.Count());
		
		return items[index];
	}
}