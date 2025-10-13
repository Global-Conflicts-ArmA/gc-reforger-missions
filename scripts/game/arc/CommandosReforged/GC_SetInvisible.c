[ComponentEditorProps(category: "GameScripted/GameMode/Components", description: "")]
class GC_SetInvisibleClass : ScriptComponentClass
{
}
//to.ClearFlags(EntityFlags.VISIBLE);
class GC_SetInvisible : ScriptComponent
{
	override protected void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.INIT);
	}

	override protected void EOnInit(IEntity owner)
	{
		SetInvisible();
		GetGame().GetCallqueue().CallLater(SetInvisible, 1000, true);
	}
	
	override protected bool RplLoad(ScriptBitReader reader)
	{
		SetInvisible();
		
		return super.RplLoad(reader);
	}
	
	void SetInvisible()
	{
		IEntity entity = GetOwner();
		if(entity)
			entity.ClearFlags(EntityFlags.VISIBLE);
	}
}
