[ComponentEditorProps(category: "GC Missions/Commandos", description: "Dog possession, local tracks and bark input.")]
class GC_CommandosDogManagerClass : ScriptComponentClass
{
}

class GC_CommandosDogManager : ScriptComponent
{
	protected static const ResourceName DOG = "{CDB8EF4A6115A227}worlds/arc/CommandosReforged/Prefabs/OP/Character_GC_Cdos_DOG.et";
	protected GC_LobbyPlayerController m_Controller;
	protected bool m_bActive;
	protected bool m_bOwnsVoiceDisable;

	//------------------------------------------------------------------------------------------------
	static GC_CommandosDogManager GetLocal()
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller || !GC_CommandosManager.GetInstance())
			return null;
		return GC_CommandosDogManager.Cast(controller.FindComponent(GC_CommandosDogManager));
	}

	//------------------------------------------------------------------------------------------------
	static bool IsDog(IEntity entity)
	{
		if (!entity)
			return false;
		EntityPrefabData data = entity.GetPrefabData();
		return data && data.GetPrefab() && SCR_BaseContainerTools.IsKindOf(data.GetPrefab(), DOG);
	}

	//------------------------------------------------------------------------------------------------
	bool IsDogControlled()
	{
		return m_bActive && GetLocal() == this && IsDog(m_Controller.GetControlledEntity());
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdatePossession(IEntity previous, IEntity current)
	{
		SetActive(GetLocal() == this && IsDog(current));
	}

	//------------------------------------------------------------------------------------------------
	protected void SetActive(bool active)
	{
		if (active == m_bActive)
			return;
		m_bActive = active;
		SCR_VONController voice = SCR_VONController.Cast(m_Controller.FindComponent(SCR_VONController));
		InputManager input = GetGame().GetInputManager();
		if (active)
		{
			input.AddActionListener("MouseLeft", EActionTrigger.DOWN, Bark);
			if (voice)
				m_bOwnsVoiceDisable = voice.SetVONDisabled(true);
		}
		else
		{
			input.RemoveActionListener("MouseLeft", EActionTrigger.DOWN, Bark);
			if (voice && m_bOwnsVoiceDisable)
				voice.SetVONDisabled(false);
			m_bOwnsVoiceDisable = false;
		}
		GC_CommandosFootPrints.SetLocalTracking(active);
	}

	//------------------------------------------------------------------------------------------------
	protected void Bark()
	{
		if (IsDogControlled())
			Rpc(RpcAsk_Bark);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Unreliable, RplRcver.Server)]
	protected void RpcAsk_Bark()
	{
		GC_CommandosManager manager = GC_CommandosManager.GetInstance();
		if (!Replication.IsServer() || !manager || !m_Controller)
			return;
		IEntity dog = m_Controller.GetControlledEntity();
		if (IsDog(dog))
			manager.Bark(dog);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_Controller = GC_LobbyPlayerController.Cast(owner);
		if (m_Controller && RplSession.Mode() != RplMode.Dedicated && !SCR_Global.IsEditMode())
			m_Controller.m_OnControlledEntityChanged.Insert(UpdatePossession);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		SetActive(false);
		if (m_Controller)
			m_Controller.m_OnControlledEntityChanged.Remove(UpdatePossession);
		super.OnDelete(owner);
	}
}
