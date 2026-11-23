enum GC_ECommandosObjectiveKind
{
	INTERACTION,
	DESTRUCTION,
	TRACKER,
	DEFECTOR
}

[BaseContainerProps()]
class GC_CommandosObjective
{
	[Attribute()]
	string m_sName;
	[Attribute()]
	string m_sBriefing;
	[Attribute()]
	string m_sCompletionMessage;
	[Attribute("60")]
	int m_iCompletionDelay;
	[Attribute("0", UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(GC_ECommandosObjectiveKind))]
	GC_ECommandosObjectiveKind m_eKind;
	[Attribute()]
	string m_sAvailabilityFlag;
	[Attribute()]
	string m_sCompletionFlag;
	[Attribute("0.5")]
	float m_fChance;
	[Attribute()]
	ref array<string> m_aExcludedRoles;
	[Attribute("0")]
	bool m_bDestroyAll;

	ref array<ref GC_CommandosObjectiveTarget> m_aTargets = {};
	int m_iBriefingOrder;
	bool m_bSelected;
	bool m_bActive;
}

class GC_CommandosObjectiveTarget
{
	IEntity m_Entity;
	vector m_vPosition;
	bool m_bRegisterMarker;
	string m_sMarkerLabel;
	string m_sTrackMarker;
	protected SCR_HitZone m_Zone;
	protected GC_CommandosManager m_Manager;
	protected GC_CommandosObjective m_Objective;

	//------------------------------------------------------------------------------------------------
	void GC_CommandosObjectiveTarget(IEntity entity, bool registerMarker, string label)
	{
		m_Entity = entity;
		m_vPosition = entity.GetOrigin();
		m_bRegisterMarker = registerMarker;
		m_sMarkerLabel = label;
	}

	//------------------------------------------------------------------------------------------------
	void Observe(GC_CommandosManager manager, GC_CommandosObjective objective)
	{
		if (!m_Entity || m_Zone)
			return;

		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(m_Entity.FindComponent(SCR_DamageManagerComponent));
		if (!damage)
			return;
		m_Zone = SCR_HitZone.Cast(damage.GetDefaultHitZone());
		if (!m_Zone)
			return;

		m_Manager = manager;
		m_Objective = objective;
		m_Zone.GetOnDamageStateChanged().Insert(OnDamageChanged);
	}

	//------------------------------------------------------------------------------------------------
	bool IsObserving()
	{
		return m_Zone != null;
	}

	//------------------------------------------------------------------------------------------------
	void StopObserving()
	{
		if (m_Zone)
			m_Zone.GetOnDamageStateChanged().Remove(OnDamageChanged);
		m_Zone = null;
		m_Manager = null;
		m_Objective = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDamageChanged(SCR_HitZone zone)
	{
		if (!m_Manager || zone != m_Zone)
			return;
		GC_CommandosManager manager = m_Manager;
		GC_CommandosObjective objective = m_Objective;
		EDamageState state = zone.GetDamageState();
		if (state == EDamageState.DESTROYED || state == EDamageState.INTERMEDIARY)
			StopObserving();
		manager.OnObjectiveDamageChanged(objective);
	}

	//------------------------------------------------------------------------------------------------
	void ~GC_CommandosObjectiveTarget()
	{
		StopObserving();
	}
}
