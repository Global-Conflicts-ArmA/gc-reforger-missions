[ComponentEditorProps(category: "GC Missions/Commandos", description: "Hides the dog role's underlying character.")]
class GC_SetInvisibleClass : ScriptComponentClass
{
}

class GC_SetInvisible : ScriptComponent
{
	//------------------------------------------------------------------------------------------------
	protected void HideCharacter()
	{
		IEntity owner = GetOwner();
		if (owner && GC_CommandosManager.GetInstance())
			owner.ClearFlags(EntityFlags.VISIBLE);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (SCR_Global.IsEditMode())
			return;
		HideCharacter();
		GetGame().GetCallqueue().CallLater(HideCharacter, 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	override protected bool RplLoad(ScriptBitReader reader)
	{
		HideCharacter();
		return super.RplLoad(reader);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(HideCharacter);
		super.OnDelete(owner);
	}
}
