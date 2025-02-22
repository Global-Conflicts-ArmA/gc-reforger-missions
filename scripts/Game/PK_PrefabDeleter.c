[EntityEditorProps(category: "GameScripted/ScriptWizard", description: "Deletes entity in certain radius.")]
class PK_PrefabDeleterEntityClass : GenericEntityClass
{
}

// The oroginal SCR_PrefabDeleter does not work correctly. Test by placing it over 10+ entities and see how it does not correctly delete all.
// This behavior was confirmed by BI dev on Discord.

// Massive thanks to Blue and Cunnah for bringing this code to life.

//------------------------------------------------------------------------------------------------
class PK_PrefabDeleterEntity : GenericEntity
{
	[Attribute(defvalue: "20", uiwidget: UIWidgets.Slider, desc: "Radius in which entities are deleted", "0 1000 1")]
	protected float m_fRadius;
			
	private ref array<IEntity> QueryEntitiesToRemove;

	//------------------------------------------------------------------------------------------------
	// Find all entities inside QueryEntitiesBySphere and add them to array. Discards any entries where name or parrents name is in list.
	// If only parrent have the specific name, all child entities must have the Hierarchy component.
	// Seems easier to add Hierarchy component, than to rename all entities.
	
	private bool QueryEntities(IEntity e)
	{

		// Add PK_PrefabDeleterComponent to all entities that should not be deleted.
		// Adding by Prefab details and by Prefab Name both had issues due to Hierarchy not always being added to prefab children. So if you are changing the name, you might as well add a new component. Hopefully an easier way to add components to multiple entities will appear
		if(e.FindComponent(PK_PrefabDeleterComponent))
		return true;

		QueryEntitiesToRemove.Insert(e);
		return true;
	
		// Below is failed attempts to find all entities without having to manually add a component to each of them.
		// Its kept for the first release to give others ideas on how to imrpove without going through the same steps.
		
		/*EntityPrefabData epd = e.GetPrefabData();
		BaseContainer bc = epd.GetPrefab();*/
		
		/*Print("PerfkEPDGetPrefab:" + epd.GetPrefab());
		Print("PerfkEPDGetPrefabName:" + epd.GetPrefabName());
		Print("PerfkEPDType:" + epd.Type());
		Print("PerfkEPDClassName:" + epd.ClassName());

		Print("PerfkBCGetName:" + bc.GetName());*/
		
		/*if(e.GetName() == "DavesDoor")
		{
			Print("PerfkBCGetParent:" + bc.GetParent());
			Print("PerfkBCGetAncestor:" + bc.GetAncestor());
		}*/
		
		
		/*Print("PerfkBCGetClassName:" + bc.GetClassName());
		Print("PerfkBCGetResourceName:" + bc.GetResourceName());*/
		
	
		
		/*if(bc.GetParent() == m_TargetEntity.GetPrefab())
		{
			return true;
		}*/
		
		
		/*foreach(string ignoredName : ignoredNames)
		{
			if(e.GetName() == ignoredName)
			{
				Print("PerfkIgnorGetPrefab:" + epd.GetPrefab());
				Print("PerfkIgnorGetPrefabName:" + epd.GetPrefabName());
				Print("PerfkIgnorType:" + epd.Type());
				Print("PerfkIgnorClassName:" + epd.ClassName());
				Print("PerfkIgnorGetName:" + bc.GetName());
				Print("PerfkIgnorGetParent:" + bc.GetParent());
				Print("PerfkIgnorGetAncestor:" + bc.GetAncestor());
				Print("PerfkIgnorGetClassName:" + bc.GetClassName());
				Print("PerfkIgnorGetResourceName:" + bc.GetResourceName());
				return true;
			}
			
			if(e.GetParent() != null) 
			{
				if(e.GetParent().GetName() == ignoredName)
				{
					return true;
				}
			}
		}*/
		
		//e.GetParent.FindComponent(BLU_ProtectDeleteComponent)*/

	}
	

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		QueryEntitiesToRemove = {};
		// server only
		RplComponent rplComponent = RplComponent.Cast(owner.FindComponent(RplComponent));
		if (rplComponent && !rplComponent.IsMaster())
			return;

		BaseWorld world = GetWorld();

		world.QueryEntitiesBySphere(owner.GetOrigin(), m_fRadius, QueryEntities);
		
		// Removes all entities. This is a hack, until the original SCR_PrefabDeleter works correctly
		foreach(IEntity e : QueryEntitiesToRemove)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(e);
		}
		
		// destroy self
		delete owner;
	}
	
	

	//------------------------------------------------------------------------------------------------
	// constructor
	//! \param[in] src
	//! \param[in] parent
	void PK_PrefabDeleterEntity(IEntitySource src, IEntity parent)
	{
		if (!GetGame().InPlayMode())
			return;
		
		SetEventMask(EntityEvent.INIT);
	}

#ifdef WORKBENCH	

	
	//------------------------------------------------------------------------------------------------
	override void _WB_AfterWorldUpdate(float timeSlice)
	{

		auto origin = GetOrigin();
		auto radiusShape = Shape.CreateSphere(COLOR_YELLOW, ShapeFlags.WIREFRAME | ShapeFlags.ONCE, origin, m_fRadius);

		super._WB_AfterWorldUpdate(timeSlice);
		
	}
	
#endif

}
