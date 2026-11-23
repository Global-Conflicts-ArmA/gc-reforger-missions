[ComponentEditorProps(category: "GC Missions/Commandos", description: "Commandos objective selection and authority.")]
class GC_CommandosManagerClass : SCR_BaseGameModeComponentClass
{
}

class GC_CommandosManager : SCR_BaseGameModeComponent
{
	[Attribute()]
	protected ref array<ref GC_CommandosObjective> m_aObjectives;
	[Attribute("5")]
	protected int m_iOrdinaryDrawCount;
	[Attribute("8")]
	protected int m_iTotalDrawLimit;
	[Attribute()]
	protected string m_sDefectorPageTitle;
	[Attribute("", UIWidgets.EditBoxMultiline)]
	protected string m_sDefectorPageContent;

	protected GC_CommandosObjective m_DefectorObjective;
	protected GC_LobbyManager m_Lobby;
	protected int m_iDefectorId;
	protected UUID m_sDefectorIdentity;
	protected bool m_bSyncingInteractions;
	protected static const string RUSSIAN_FACTION = "RHS_AFRF";
	protected static const string US_FACTION = "RHS_USAF";
	protected static const ResourceName MARKER_IMAGE = "{E23427CAC80DA8B7}UI/Textures/Icons/icons_mapMarkersUI.imageset";
	protected static const ResourceName MARKER_GLOW = "{67B3A6DC2D712B52}UI/Textures/Icons/icons_mapMarkersUI-glow.imageset";

	//------------------------------------------------------------------------------------------------
	static GC_CommandosManager GetInstance()
	{
		GC_LobbyGameMode mode = GC_LobbyGameMode.GetGameMode();
		if (!mode)
			return null;
		return GC_CommandosManager.Cast(mode.FindComponent(GC_CommandosManager));
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsActiveAuthority()
	{
		return Replication.IsServer() && GetInstance() == this && !SCR_Global.IsEditMode();
	}

	//------------------------------------------------------------------------------------------------
	protected GC_CommandosObjective FindObjective(string name)
	{
		if (!m_aObjectives)
			return null;
		foreach (GC_CommandosObjective objective : m_aObjectives)
		{
			if (objective && objective.m_sName == name)
				return objective;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	void RegisterTarget(string name, IEntity entity, bool registerMarker, string label)
	{
		if (!IsActiveAuthority() || !entity || entity.GetWorld() != GetOwner().GetWorld())
			return;
		GC_CommandosObjective objective = FindObjective(name);
		if (!objective)
			return;
		foreach (GC_CommandosObjectiveTarget existing : objective.m_aTargets)
		{
			if (existing.m_Entity == entity)
				return;
		}
		GC_CommandosObjectiveTarget target = new GC_CommandosObjectiveTarget(entity, registerMarker, label);
		objective.m_aTargets.Insert(target);
		if (objective.m_bActive && objective.m_eKind != GC_ECommandosObjectiveKind.INTERACTION)
			target.Observe(this, objective);
		if (m_Lobby)
			PublishTarget(objective, target, objective.m_aTargets.Count() - 1);
	}

	//------------------------------------------------------------------------------------------------
	protected void SelectObjectives()
	{
		array<GC_CommandosObjective> ordinary = {};
		array<GC_CommandosObjective> defectors = {};
		foreach (GC_CommandosObjective objective : m_aObjectives)
		{
			if (objective.m_eKind == GC_ECommandosObjectiveKind.DEFECTOR)
				defectors.Insert(objective);
			else
				ordinary.Insert(objective);
		}

		int selected;
		while (selected < m_iOrdinaryDrawCount && !ordinary.IsEmpty())
		{
			int index = Math.RandomInt(0, ordinary.Count());
			GC_CommandosObjective objective = ordinary[index];
			objective.m_iBriefingOrder = 10 + selected;
			ActivateObjective(objective);
			array<string> factions = {US_FACTION};
			m_Lobby.SetBriefing(objective.m_iBriefingOrder, "Objective: " + objective.m_sName,
				objective.m_sBriefing, factions, SCR_EGameModeState.BRIEFING);
			ordinary.RemoveOrdered(index);
			selected++;
		}
		while (selected < m_iTotalDrawLimit && !defectors.IsEmpty())
		{
			int index = Math.RandomInt(0, defectors.Count());
			GC_CommandosObjective candidate = defectors[index];
			defectors.RemoveOrdered(index);
			if (Math.RandomFloat01() > candidate.m_fChance)
				continue;

			m_DefectorObjective = candidate;
			ActivateObjective(candidate);
			break;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ActivateObjective(GC_CommandosObjective objective)
	{
		objective.m_bSelected = true;
		objective.m_bActive = true;
		if (objective.m_eKind == GC_ECommandosObjectiveKind.INTERACTION)
			return;

		foreach (GC_CommandosObjectiveTarget target : objective.m_aTargets)
			target.Observe(this, objective);
	}

	//------------------------------------------------------------------------------------------------
	protected GC_BriefingMarker MakeMarker(string name, vector position, string label, string faction,
		int color, bool lowPriority, bool unassigned)
	{
		array<string> factions = {faction};
		GC_BriefingMarker marker = new GC_BriefingMarker();
		float rotation;
		if (!lowPriority)
			rotation = -90;
		marker.Set(name, MARKER_IMAGE, MARKER_GLOW, "objective-marker", position, rotation,
			true, 60, 0, label, factions, SCR_EGameModeState.PREGAME, false, true, color);
		if (lowPriority)
			marker.SetDrawOrder(5, -1);
		else
			marker.SetDrawOrder(100, 10000);
		if (unassigned)
			marker.SetListedFactionAudience(true);
		return marker;
	}

	//------------------------------------------------------------------------------------------------
	protected void PublishTarget(GC_CommandosObjective objective, GC_CommandosObjectiveTarget target, int index)
	{
		if (objective.m_eKind == GC_ECommandosObjectiveKind.DEFECTOR)
			return;
		string key = string.Format("%1_%2", objective.m_sAvailabilityFlag, index);
		if (target.m_bRegisterMarker)
		{
			m_Lobby.UpsertBriefingMarker(MakeMarker(key + "_Default", target.m_vPosition, objective.m_sName,
				RUSSIAN_FACTION, Color.BLACK, true, true));
			if (objective.m_bSelected)
				m_Lobby.UpsertBriefingMarker(MakeMarker(key + "_US", target.m_vPosition, objective.m_sName,
					US_FACTION, Color.RED, false, false));
		}
		if (objective.m_bSelected && objective.m_eKind == GC_ECommandosObjectiveKind.TRACKER)
		{
			target.m_sTrackMarker = key + "_Track";
			m_Lobby.UpsertBriefingMarker(MakeMarker(target.m_sTrackMarker, target.m_vPosition,
				objective.m_sName, US_FACTION, Color.Violet.PackToInt(), false, false));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateTrackers()
	{
		if (!IsActiveAuthority() || !m_Lobby)
			return;
		foreach (GC_CommandosObjective objective : m_aObjectives)
		{
			if (!objective.m_bSelected || objective.m_eKind != GC_ECommandosObjectiveKind.TRACKER)
				continue;
			foreach (GC_CommandosObjectiveTarget target : objective.m_aTargets)
			{
				if (!target.m_Entity)
					continue;
				GC_BriefingMarker oldMarker = m_Lobby.GetBriefingMarker(target.m_sTrackMarker);
				vector position = target.m_Entity.GetOrigin();
				if (!oldMarker || position == oldMarker.GetWorldPosition())
					continue;
				GC_BriefingMarker update = new GC_BriefingMarker();
				update.CopyFrom(oldMarker);
				update.SetWorldTransform(position, oldMarker.GetRotation());
				m_Lobby.UpsertBriefingMarker(update);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnObjectiveDamageChanged(GC_CommandosObjective objective)
	{
		if (!IsActiveAuthority() || !objective || !objective.m_bActive)
			return;
		if (objective.m_bDestroyAll)
		{
			foreach (GC_CommandosObjectiveTarget target : objective.m_aTargets)
			{
				if (target.IsObserving())
					return;
			}
		}
		CompleteObjective(objective);
	}

	//------------------------------------------------------------------------------------------------
	protected void CompleteObjective(GC_CommandosObjective objective)
	{
		objective.m_bActive = false;
		foreach (GC_CommandosObjectiveTarget target : objective.m_aTargets)
			target.StopObserving();
		string description = string.Format(objective.m_sCompletionMessage, objective.m_sName);
		GetGame().GetCallqueue().CallLater(ShowCompletion, objective.m_iCompletionDelay * 1000, false, description);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowCompletion(string message)
	{
		if (!IsActiveAuthority())
			return;
		Rpc(RpcDo_ShowCompletion, message);
		if (RplSession.Mode() != RplMode.Dedicated)
			RpcDo_ShowCompletion(message);
	}

	//------------------------------------------------------------------------------------------------
	void Bark(IEntity dog)
	{
		if (!IsActiveAuthority() || !dog || dog.GetWorld() != GetOwner().GetWorld())
			return;
		RplId id = Replication.FindItemId(dog);
		if (id == RplId.Invalid())
			return;
		Rpc(RpcDo_Bark, id);
		if (RplSession.Mode() != RplMode.Dedicated)
			RpcDo_Bark(id);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Unreliable, RplRcver.Broadcast)]
	protected void RpcDo_Bark(RplId id)
	{
		if (GetInstance() != this)
			return;
		IEntity dog = IEntity.Cast(Replication.FindItem(id));
		if (!dog)
			return;
		SoundComponent sound = SoundComponent.Cast(dog.FindComponent(SoundComponent));
		if (sound)
			sound.SoundEvent("SOUND_DOGBARK");
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_ShowCompletion(string message)
	{
		if (GetInstance() == this)
			SCR_HintManagerComponent.ShowCustomHint(message, "Objective Completed", 15);
	}

	//------------------------------------------------------------------------------------------------
	protected void SyncInteractions()
	{
		if (!IsActiveAuthority() || m_bSyncingInteractions || !m_Lobby || !m_Lobby.AreMissionInteractionsActive())
			return;
		m_bSyncingInteractions = true;
		foreach (GC_CommandosObjective objective : m_aObjectives)
		{
			if (!objective.m_bSelected || objective.m_eKind != GC_ECommandosObjectiveKind.INTERACTION)
				continue;
			if (objective.m_bActive && m_Lobby.GetFlag(objective.m_sCompletionFlag))
				CompleteObjective(objective);
			if (objective.m_bActive && !m_Lobby.GetFlag(objective.m_sAvailabilityFlag))
				m_Lobby.CallFlag(objective.m_sAvailabilityFlag);
			else if (!objective.m_bActive && m_Lobby.GetFlag(objective.m_sAvailabilityFlag))
				m_Lobby.ResetFlag(objective.m_sAvailabilityFlag);
		}
		m_bSyncingInteractions = false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsEligibleDefector(int playerId)
	{
		if (!m_Lobby || !m_DefectorObjective || playerId <= 0)
			return false;
		Faction faction = m_Lobby.GetPlayerFaction(playerId);
		GC_Slot slot = m_Lobby.GetSlotByPlayerId(playerId);
		if (!faction || faction.GetFactionKey() != RUSSIAN_FACTION || !slot)
			return false;

		SCR_UIInfo role = GC_Slot.GetPrefabRoleInfo(slot.GetAuthoredPrefab());
		return role && (!m_DefectorObjective.m_aExcludedRoles
			|| !m_DefectorObjective.m_aExcludedRoles.Contains(role.GetName()));
	}

	//------------------------------------------------------------------------------------------------
	protected void AssignDefector()
	{
		if (!m_DefectorObjective || m_iDefectorId > 0)
			return;
		array<int> candidates = {};
		GetGame().GetPlayerManager().GetPlayers(candidates);
		while (!candidates.IsEmpty())
		{
			int index = Math.RandomInt(0, candidates.Count());
			int playerId = candidates[index];
			candidates.RemoveOrdered(index);
			if (!IsEligibleDefector(playerId))
				continue;

			m_iDefectorId = playerId;
			m_sDefectorIdentity = m_Lobby.GetSlotByPlayerId(playerId).GetUUID();
			SendDefectorBriefing(playerId);
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void SendDefectorBriefing(int playerId)
	{
		array<ref GC_Briefing> pages = {};
		array<ref GC_BriefingMarker> markers = {};
		GC_Briefing briefing = new GC_Briefing();
		briefing.Set(15, m_sDefectorPageTitle, m_sDefectorPageContent, null, SCR_EGameModeState.BRIEFING);
		pages.Insert(briefing);
		GC_Briefing objective = new GC_Briefing();
		objective.Set(16, "Objective: " + m_DefectorObjective.m_sName, m_DefectorObjective.m_sBriefing,
			null, SCR_EGameModeState.BRIEFING);
		pages.Insert(objective);
		foreach (int index, GC_CommandosObjectiveTarget target : m_DefectorObjective.m_aTargets)
		{
			if (!target.m_bRegisterMarker)
				continue;
			string label = target.m_sMarkerLabel;
			if (label.IsEmpty())
				label = m_DefectorObjective.m_sName;
			string key = string.Format("CommandosDefector_%1", index);
			markers.Insert(MakeMarker(key, target.m_vPosition, label, RUSSIAN_FACTION, Color.RED, true, false));
		}
		m_Lobby.SetPrivateBriefingForPlayer(playerId, pages, markers);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPlayerAffiliationChanged(int playerId)
	{
		if (!IsActiveAuthority() || !m_Lobby || !m_Lobby.IsPrivateBriefingPhase() || m_iDefectorId <= 0)
			return;

		GC_Slot slot = m_Lobby.GetSlotByPlayerId(playerId);
		if (!slot || !IsEligibleDefector(playerId))
			return;
		if ((!m_sDefectorIdentity.IsEmpty() && slot.GetUUID() == m_sDefectorIdentity)
			|| (m_sDefectorIdentity.IsEmpty() && playerId == m_iDefectorId))
			SendDefectorBriefing(playerId);
	}

	//------------------------------------------------------------------------------------------------
	override void OnGameStateChanged(SCR_EGameModeState state)
	{
		if (!IsActiveAuthority() || !m_Lobby)
			return;
		if (state == SCR_EGameModeState.BRIEFING)
			AssignDefector();
		else if (state == SCR_EGameModeState.GAME)
			GetGame().GetCallqueue().Call(SyncInteractions);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override protected void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		if (!IsActiveAuthority() || !m_aObjectives)
			return;
		m_Lobby = GC_LobbyManager.GetManager();
		if (!m_Lobby)
			return;
		SelectObjectives();
		foreach (GC_CommandosObjective objective : m_aObjectives)
		{
			foreach (int index, GC_CommandosObjectiveTarget target : objective.m_aTargets)
				PublishTarget(objective, target, index);
		}
		GetGame().GetCallqueue().CallLater(UpdateTrackers, 45000, true);
		GC_LobbyManager.GetMissionStateChanged().Insert(SyncInteractions);
		m_Lobby.m_OnPlayerAffiliationChanged.Insert(OnPlayerAffiliationChanged);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GC_LobbyManager.GetMissionStateChanged().Remove(SyncInteractions);
		if (m_Lobby)
			m_Lobby.m_OnPlayerAffiliationChanged.Remove(OnPlayerAffiliationChanged);
		GetGame().GetCallqueue().Remove(SyncInteractions);
		GetGame().GetCallqueue().Remove(ShowCompletion);
		GetGame().GetCallqueue().Remove(UpdateTrackers);
		if (m_aObjectives)
		{
			foreach (GC_CommandosObjective objective : m_aObjectives)
			{
				foreach (GC_CommandosObjectiveTarget target : objective.m_aTargets)
					target.StopObserving();
			}
		}
		super.OnDelete(owner);
	}
}
