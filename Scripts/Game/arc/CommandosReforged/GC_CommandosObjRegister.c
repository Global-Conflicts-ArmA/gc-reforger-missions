[ComponentEditorProps(category: "GC Missions/Commandos", description: "Registers a target with a named Commandos objective.")]
class GC_CommandosObjRegisterClass : ScriptComponentClass
{
}

class GC_CommandosObjRegister : ScriptComponent
{
	[Attribute()]
	protected string m_objectiveName;
	[Attribute("0")]
	protected bool m_registerMarker;
	[Attribute()]
	protected string m_markerName;

	//------------------------------------------------------------------------------------------------
	override protected void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!Replication.IsServer() || SCR_Global.IsEditMode() || m_objectiveName.IsEmpty())
			return;
		GC_CommandosManager manager = GC_CommandosManager.GetInstance();
		if (manager)
			manager.RegisterTarget(m_objectiveName, owner, m_registerMarker, m_markerName);
	}
}
