[ComponentEditorProps(category: "GameScripted/GameMode/Components", description: "Commandos")]
class GC_CommandosManagerClass : SCR_BaseGameModeComponentClass
{
}

class GC_CommandosManager : SCR_BaseGameModeComponent
{
	[Attribute(defvalue: "5", UIWidgets.Auto)]
	protected int m_minObjectiveCount;
	
	[Attribute(defvalue: "8", UIWidgets.Auto)]
	protected int m_maxObjectiveCount;
	
	[RplProp(), Attribute(defvalue: "", UIWidgets.Object)]
	protected ref array<ref GC_CommandosObj> m_objectives;

	int m_defectorPlayerId;
	
	static GC_CommandosManager GetInstance()
	{
		return GC_CommandosManager.Cast(GetGame().GetGameMode().FindComponent(GC_CommandosManager));
	}
	
	GC_CommandosObj GetObjective(string name)
	{
		foreach(GC_CommandosObj objective : m_objectives)
		{
			if (objective.GetName() == name)
				return objective;
		}
		
		return null;
	}
	
	void CheckEndConditions()
	{
		int count = 0;
		foreach(GC_CommandosObj objective : m_objectives)
		{
			if(GC_CommandosDefector.Cast(objective))
				continue;
		
			if(objective.IsActive())
				count++;
		}
		
		if(count <= 0)
			SendHintAll("US Victory!", "All US objectives completed!")
	}
	
	void OnObjectiveCompleted(GC_CommandosObj objective)
	{
		CheckEndConditions();
		Replication.BumpMe();
	}
	
	override void OnPlayerConnected(int playerId)
	{
		foreach(GC_CommandosObj objective : m_objectives)
		{
			objective.OnPlayerConnected(playerId);
		}
	}
	
	override protected void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.INIT);
	}
	
	override protected void EOnInit(IEntity owner)
	{
		if(!Replication.IsServer())
			return;
		
		if(!GetGame().InPlayMode())
			return;
		
		array<ref GC_CommandosObj> objectivePool = {};
		foreach(GC_CommandosObj objective : m_objectives)
		{
			objective.Setup();
			if(GC_CommandosObjWeight.Cast(objective))
				continue;
			
			objectivePool.Insert(objective)
		}
		
		int objCount = 0;
		while(!objectivePool.IsEmpty() && objCount < m_minObjectiveCount)
		{
			ref GC_CommandosObj objective = objectivePool.GetRandomElement();
			objectivePool.RemoveItem(objective);

			if(!objective.IsPickable())
				continue;
			
			objective.Activate();
			objCount++;
		}
		
		//Weighted objectives
		objectivePool.Clear();
		foreach(GC_CommandosObj objective : m_objectives)
		{
			if(GC_CommandosObjWeight.Cast(objective))
				objectivePool.Insert(objective);
		}
		
		while(!objectivePool.IsEmpty() && objCount < m_maxObjectiveCount)
		{
			ref GC_CommandosObj objective = objectivePool.GetRandomElement();
			objectivePool.RemoveItem(objective);
			
			if(!objective.IsPickable())
				continue;
			
			objective.Activate();
			objCount++;
		}
		
		Replication.BumpMe();
	}
	
	override void OnGameStateChanged(SCR_EGameModeState state)
	{
		switch(state)
		{
			case SCR_EGameModeState.BRIEFING:
				BriefingStart();
				break;
			
			case SCR_EGameModeState.GAME:
				GameStart();
				break;
		}
	}
	
	protected void BriefingStart()
	{
		foreach(GC_CommandosObj objective : m_objectives)
		{
			objective.OnBriefing();
		}
	}
	
	protected void GameStart()
	{
		foreach(GC_CommandosObj objective : m_objectives)
		{
			objective.OnGameStart();
		}
	}
	
	void SendHintAll(string title, string description)
	{
		array<int> players = {};
		GetGame().GetPlayerManager().GetAllPlayers(players);
		foreach(int player : players)
		{
			SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(player));
			if(!pc)
				continue;
			
			pc.TILW_SendHintToPlayer(title, description, 15);
		}
	}
	
	void DoBark(RplId rplId)
	{
		Rpc(RPC_DoBark, rplId);
		
		if(RplSession.Mode() != RplMode.Dedicated)
			RPC_DoBark(rplId);
	}
	
	[RplRpc(RplChannel.Unreliable, RplRcver.Broadcast)]
	protected void RPC_DoBark(RplId rplId)
	{
		IEntity player = IEntity.Cast(Replication.FindItem(rplId));
		if(!player)
			return;
		
		SoundComponent soundComp = SoundComponent.Cast(player.FindComponent(SoundComponent));
		if (!soundComp)
			return;

		soundComp.SoundEvent("SOUND_DOGBARK");
	}
}

[BaseContainerProps()]
class GC_CommandosObj
{
	[Attribute(defvalue: "", UIWidgets.Auto)]
	protected string m_name;

	[Attribute(defvalue: "The %1 has been neutralized!", UIWidgets.EditBoxMultiline)]
	protected string m_objectiveCompletedMsg;
	
	[Attribute(defvalue: "60", UIWidgets.Auto)]
	protected int m_completeDelay;
	
	[Attribute(defvalue: "Destroy the objective using C4", UIWidgets.EditBoxMultiline)]
	protected string m_briefingDescription;
	
	[Attribute(defvalue: "{8B6159923D67F288}worlds/arc/CommandosReforged/Prefabs/Marker_Any.et", UIWidgets.ResourceNamePicker)]
	protected ResourceName m_markerPrefab;
	
	[Attribute(defvalue: "{CBB8E081B4DA5A39}worlds/arc/CommandosReforged/Prefabs/Obj_Blufor_Briefing.et", UIWidgets.ResourceNamePicker)]
	protected ResourceName m_briefingPrefab;
	
	protected bool m_isActive = false;
	
	protected ref array<PS_ManualMarker> m_markers = {};

	void Setup();
	
	bool IsPickable()
	{
		if(m_isActive)
			return false;
		
		return true;
	}
	
	void Activate()
	{
		Print("GRAY.OBJ selected = " + m_name);
		
		foreach(PS_ManualMarker marker : m_markers)
		{
			ActivateMarker(marker);
		}
		
		SetActive(true);
	
		if(GC_CommandosDefector.Cast(this))
			return;
		
		SpawnBriefing();
	}
	
	void ActivateMarker(PS_ManualMarker marker)
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.Transform[3] = marker.GetOrigin();
		
		PS_ManualMarker marker2 = PS_ManualMarker.Cast(GetGame().SpawnEntityPrefab(Resource.Load("{2E08E7B7914EB585}worlds/arc/CommandosReforged/Prefabs/Objective_Marker.et"), GetGame().GetWorld(), params));
		marker2.SetDescription(m_name);
	}
	
	void OnBriefing();
	
	void OnGameStart();
	
	void OnPlayerConnected(int playerId);

	void ActivateLocal();
	
	void Complete()
	{
		if(!Replication.IsServer())
			return;
		
		if(!m_isActive)
			return;

		GC_CommandosManager cm = GC_CommandosManager.GetInstance();
		
		string description = string.Format(m_objectiveCompletedMsg, m_name);
		GetGame().GetCallqueue().CallLater(cm.SendHintAll, m_completeDelay * 1000, false,"Objective Completed", description);
		cm.OnObjectiveCompleted(this);
		
		SetActive(false);
	}

	string GetName()
	{
		return m_name;
	}

	bool IsActive()
	{
		return m_isActive;
	}
	
	protected void SetActive(bool state)
	{
		if(state == m_isActive)
			return;
		
		m_isActive = state;
	}
	
	void RegisterMarker(IEntity entity)
	{
		if(!Replication.IsServer())
			return;
		
		EntitySpawnParams params = new EntitySpawnParams();
		params.Transform[3] = entity.GetOrigin();
		
		IEntity prefab = GetGame().SpawnEntityPrefab(Resource.Load(m_markerPrefab), GetGame().GetWorld(), params);
		PS_ManualMarker marker = PS_ManualMarker.Cast(prefab);
		
		marker.SetDescription(m_name);

		m_markers.Insert(marker);
		
		if(m_isActive)
			ActivateMarker(marker);
	}
	
	void RegisterObjective(IEntity entity);
	
	PS_MissionDescription SpawnBriefing()
	{
		PS_MissionDescription brief;
		if(RplSession.Mode() == RplMode.Dedicated)
		{
			brief = PS_MissionDescription.Cast(GetGame().SpawnEntityPrefab(Resource.Load(m_briefingPrefab)));
		}else{
			brief = PS_MissionDescription.Cast(GetGame().SpawnEntityPrefabLocal(Resource.Load(m_briefingPrefab)));
		}
		brief.SetTitle("Objective: " + m_name);
		brief.SetTextData(m_briefingDescription);
		return brief;
	}
	
	//! From snapshot to packet.
	// Takes snapshot and compresses it into packet. Opposite of Decode()
	static void Encode(SSnapSerializerBase snapshot, ScriptCtx ctx, ScriptBitSerializer packet)
	{
		snapshot.EncodeString(packet);
		snapshot.EncodeBool(packet);
	}

	//! From packet to snapshot.
	// Takes packet and decompresses it into snapshot. Opposite of Encode()
	static bool Decode(ScriptBitSerializer packet, ScriptCtx ctx, SSnapSerializerBase snapshot)
	{
		snapshot.DecodeString(packet);
		snapshot.DecodeBool(packet);
		return true;
	}

	//! Snapshot to snapshot comparison.
	// Compares two snapshots to see whether they are the same or not
	static bool SnapCompare(SSnapSerializerBase lhs, SSnapSerializerBase rhs, ScriptCtx ctx)
	{
		return lhs.CompareStringSnapshots(rhs)
		&& lhs.CompareSnapshots(rhs, 4);
	}

	//! Property mem to snapshot comparison.
	// Compares instance and a snapshot to see if any property has changed enough to require a new snapshot
	static bool PropCompare(GC_CommandosObj prop, SSnapSerializerBase snapshot, ScriptCtx ctx)
	{
		return snapshot.CompareString(prop.m_name)
		&& snapshot.CompareBool(prop.m_isActive);
	}

	//! Property mem to snapshot extraction.
	// Extracts relevant properties from an instance of type T into snapshot. Opposite of Inject()
	static bool Extract(GC_CommandosObj prop, ScriptCtx ctx, SSnapSerializerBase snapshot)
	{
		snapshot.SerializeString(prop.m_name);
		snapshot.SerializeBool(prop.m_isActive);
		return true;
	}

	//! Snapshot to property memory injection.
	// Injects relevant properties from snapshot into an instance of type T . Opposite of Extract()
	static bool Inject(SSnapSerializerBase snapshot, ScriptCtx ctx, GC_CommandosObj prop)
	{
		snapshot.SerializeString(prop.m_name);
		snapshot.SerializeBool(prop.m_isActive);
		return true;
	}
}

[BaseContainerProps()]
class GC_CommandosObjWeight : GC_CommandosObj
{
	[Attribute(defvalue: "1", UIWidgets.Auto, desc: "0-1 Chance of being selected. 1 = 100%, 0 = 0%")]
	protected float m_chance;
	
	override bool IsPickable()
	{
		float random = Math.RandomFloat01();
		if(random > m_chance)
			return false;
		
		return super.IsPickable();
	}
}

[BaseContainerProps()]
class GC_CommandosDefector : GC_CommandosObjWeight
{
	[Attribute(defvalue: "", UIWidgets.Auto, desc: "Name of slots to not select")]
	protected ref array<string> m_blacklist;
	
	static int m_defectorId;
	static GC_CommandosDefector m_defectorObj;
	
	override bool IsPickable()
	{
		if(m_defectorObj)
			return false;
		
		return super.IsPickable();
	}
	
	override void Activate()
	{
		super.Activate();
		
		m_defectorObj = this;
	}
	
	override void OnBriefing()
	{
		if(!Replication.IsServer())
			return;
		
		if(!m_isActive)
			return;
		
		int id = FindDefector();
		if(!id)
			return;
		
		m_defectorId = id;
		
		GC_CommandosManager cm = GC_CommandosManager.GetInstance();
		cm.m_defectorPlayerId = m_defectorId;
		RPC_ActivateLocal(m_defectorId);
	}
	
	protected int FindDefector()
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		
		array<int> players = {};
		pm.GetAllPlayers(players);
		
		while(!players.IsEmpty())
		{
			int playerId = players.GetRandomElement();
			players.RemoveItem(playerId);
			
			playableManager.GetPlayerFactionKey(playerId);
			string faction = playableManager.GetPlayerFactionKey(playerId);
			if(faction != "RHS_AFRF")
				continue;
			
			RplId playableId = playableManager.GetPlayableByPlayer(playerId);
			if(!playableId || !playableId.IsValid())
				continue;
			
			PS_PlayableContainer pc = playableManager.GetPlayableById(playableId);
			if(!pc)
				continue;
			
			if(m_blacklist.Contains(pc.GetRoleName()))
				continue;
			
			Print("GRAY.FindDefector = " + SCR_PlayerNamesFilterCache.GetInstance().GetPlayerDisplayName(playerId));
			return playerId;
		}
		
		return null;
	}
	
	override void OnPlayerConnected(int playerId)
	{
		if(!m_defectorId)
			return;
		
		if(m_defectorId != playerId)
			return;

		RPC_ActivateLocal(playerId);
	}
	
	protected void RPC_ActivateLocal(int playerId)
	{
		GC_CommandosPlayerController pc = GC_CommandosPlayerController.GetInstance(playerId);
		if(!pc)
			return;

		pc.ActivateObjective(m_name);
	}
	
	override void RegisterMarker(IEntity entity);
}

[BaseContainerProps()]
class GC_CommandosDefectorDestroy : GC_CommandosDefector
{
	[Attribute(defvalue: "", UIWidgets.Auto)]
	protected bool m_destroyAll;
	
	protected int objCount = 0;
	protected ref array<IEntity> entities = {};
	
	override void Activate()
	{
		super.Activate();

		foreach(IEntity entity : entities)
		{
			ActivateObjective(entity);
		}
	}
	
	void ActivateObjective(IEntity entity)
	{
		SCR_DamageManagerComponent dmc = SCR_DamageManagerComponent.Cast(entity.FindComponent(SCR_DamageManagerComponent));
		if(!dmc)
			return;
		
		SCR_HitZone hz = SCR_HitZone.Cast(dmc.GetDefaultHitZone());
		if(!hz)
			return;
		
		hz.GetOnDamageStateChanged().Insert(OnDamageStateChangedHZ);
		objCount++;
	}
	
	void OnDamageStateChangedHZ(SCR_HitZone hitZone)
	{
		EDamageState state = hitZone.GetDamageState();
		if (state == EDamageState.DESTROYED || state == EDamageState.INTERMEDIARY)
		{
			hitZone.GetOnDamageStateChanged().Remove(OnDamageStateChangedHZ);
			objCount--;
		}
		
		if(m_destroyAll)
		{
			if(objCount <= 0)
				Complete();
		}
		else
		{
			Complete();
		}
	}
	
	override void RegisterObjective(IEntity entity)
	{
		entities.Insert(entity);

		if(m_isActive)
			ActivateObjective(entity);
	}
	
	void SpawnMarker(IEntity entity)
	{
		GC_CommandosObjRegister objRegister = GC_CommandosObjRegister.Cast(entity.FindComponent(GC_CommandosObjRegister));
		if(!objRegister || !objRegister.m_registerMarker)
			return;

		EntitySpawnParams params = new EntitySpawnParams();
		params.Transform[3] = entity.GetOrigin();
		PS_ManualMarker marker = PS_ManualMarker.Cast(GetGame().SpawnEntityPrefab(Resource.Load(m_markerPrefab), GetGame().GetWorld(), params));

		string name;
		if(objRegister.m_registerMarker)
			name = objRegister.m_markerName;
		
		if(name.IsEmpty())
			name = m_name;
		
		marker.SetDescription(name);
		marker.SetColor(Color.Red);
		
		SCR_FactionManager fm = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		Faction faction = fm.GetFactionByKey("RHS_AFRF");
		marker.SetVisibleForFaction(faction, true);
	}
	
	override void ActivateLocal()
	{
		Print("GRAY.ActivateLocal = " + m_name);
		foreach(IEntity entity : entities)
		{
			SpawnMarker(entity);
		}
		
		PS_MissionDescription brief = SpawnBriefing();
		brief.SetShowForAnyFaction(true);
		
		GetGame().SpawnEntityPrefabLocal(Resource.Load("{E2A878A34285BDFB}worlds/arc/CommandosReforged/Prefabs/Defector_Brief.et"));
		GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.BriefingMapMenu);
	}
}

[BaseContainerProps()]
class GC_CommandosTracker : GC_CommandosDestroyed
{
	[Attribute(defvalue: "45", uiwidget: UIWidgets.Auto, desc: "Seconds to update tracker marker")]
	protected int m_updateDelay;
	
	protected ref array<ref GC_CommandosTrackerData> m_trackers = {};
	
	override void Activate()
	{
		super.Activate();
		
		foreach(IEntity entity : entities)
		{
			RegisterTracker(entity);
		}
		GetGame().GetCallqueue().CallLater(UpdateTracker, m_updateDelay * 1000, true)
	}
	
	override void RegisterObjective(IEntity entity)
	{
		super.RegisterObjective(entity);
		
		if(m_isActive)
			RegisterTracker(entity);
	}
	
	void UpdateTracker()
	{
		foreach(GC_CommandosTrackerData tracker : m_trackers)
		{
			tracker.UpdateTracker();
		}
	}
	
	void RegisterTracker(IEntity entity)
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.Transform[3] = entity.GetOrigin();
		PS_ManualMarker marker = PS_ManualMarker.Cast(GetGame().SpawnEntityPrefab(Resource.Load(m_markerPrefab), GetGame().GetWorld(), params));
		
		marker.SetDescription(m_name);
		marker.SetColor(Color.Violet);

		GC_CommandosTrackerData tracker = GC_CommandosTrackerData(entity, marker);
		m_trackers.Insert(tracker);
	}
}

[BaseContainerProps()]
class GC_CommandosDestroyed : GC_CommandosObj
{
	[Attribute(defvalue: "", UIWidgets.Auto)]
	protected bool m_destroyAll;
	
	protected int objCount = 0;
	protected ref array<IEntity> entities = {};
	
	override void Activate()
	{
		super.Activate();

		foreach(IEntity entity : entities)
		{
			ActivateObjective(entity);
		}
	}
	
	void ActivateObjective(IEntity entity)
	{
		SCR_DamageManagerComponent dmc = SCR_DamageManagerComponent.Cast(entity.FindComponent(SCR_DamageManagerComponent));
		if(!dmc)
			return;
		
		SCR_HitZone hz = SCR_HitZone.Cast(dmc.GetDefaultHitZone());
		if(!hz)
			return;
		
		hz.GetOnDamageStateChanged().Insert(OnDamageStateChangedHZ);
		objCount++;
	}
	
	void OnDamageStateChangedHZ(SCR_HitZone hitZone)
	{
		EDamageState state = hitZone.GetDamageState();
		if (state == EDamageState.DESTROYED || state == EDamageState.INTERMEDIARY)
		{
			hitZone.GetOnDamageStateChanged().Remove(OnDamageStateChangedHZ);
			objCount--;
		}
		
		if(m_destroyAll)
		{
			if(objCount <= 0)
				Complete();
		}
		else
		{
			Complete();
		}
	}
	
	override void RegisterObjective(IEntity entity)
	{
		entities.Insert(entity);
		
		if(m_isActive)
			ActivateObjective(entity);
	}
}

class GC_CommandosInteraction : ScriptedUserAction
{
	[Attribute(uiwidget: UIWidgets.Auto, desc: "The name of the objective class to complete.")]
	protected string m_objectiveName;
	
	[Attribute(uiwidget: UIWidgets.Auto, desc: "Allowed user faction keys.")]
	protected ref array<string> m_factionKeys;
	
	[Attribute(uiwidget: UIWidgets.Auto, desc: "Allow the defector to complete.")]
	protected bool m_defectorObjective;
	
	protected GC_CommandosManager cm;
	
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		cm = GC_CommandosManager.GetInstance();
	}
	
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		GC_CommandosObj objective = cm.GetObjective(m_objectiveName);
		if(objective)
			objective.Complete();
	}
	
	override bool CanBePerformedScript(IEntity user)
	{
		return IsAvailable(user);
	}
	
	override bool CanBeShownScript(IEntity user)
	{
		return IsAvailable(user);
	}
	
	bool IsAvailable(IEntity user)
	{
		if(!cm.GetObjective(m_objectiveName).IsActive())
			return false;
		
		if(m_defectorObjective)
		{
			if(cm.m_defectorPlayerId == GetGame().GetPlayerController().GetPlayerId())
				return true;
		}
		
		SCR_ChimeraCharacter cc = SCR_ChimeraCharacter.Cast(user);
		if (!cc || !m_factionKeys.Contains(cc.GetFactionKey()))
			return false;
		
		return true;
	}
	
	override bool HasLocalEffectOnlyScript()
	{
		return false;
	}
	
	override bool CanBroadcastScript()
	{
		return true;
	}
}

[ComponentEditorProps(category: "GameScripted/GameMode/Components", description: "Commandos")]
class GC_CommandosObjRegisterClass : ScriptComponentClass
{
}

class GC_CommandosObjRegister : ScriptComponent
{
	[Attribute(uiwidget: UIWidgets.Auto, desc: "The name of the objective class to affect")]
	protected string m_objectiveName;
	
	[Attribute(uiwidget: UIWidgets.Auto, desc: "Spawn marker here")]
	bool m_registerMarker;
	
	[Attribute(uiwidget: UIWidgets.Auto, desc: "The name of the marker")]
	string m_markerName;
	
	override protected void OnPostInit(IEntity owner)
	{
		GC_CommandosObj objective = GC_CommandosManager.GetInstance().GetObjective(m_objectiveName);
		if(!objective)
			return;
		
		if(m_registerMarker)
			objective.RegisterMarker(owner);
		
		objective.RegisterObjective(owner);
	}
}

class GC_CommandosTrackerData
{
	IEntity m_entity;
	PS_ManualMarker m_marker;
	
	void GC_CommandosTrackerData(IEntity entity, PS_ManualMarker marker)
	{
		m_entity = entity;
		m_marker = marker;
	}
	
	void UpdateTracker()
	{
		m_marker.UpdatePosition(m_entity.GetOrigin());
	}
}

modded class PS_ManualMarker
{
	void UpdatePosition(vector position)
	{
		RPC_ActivateObjective(position);
		Rpc(RPC_ActivateObjective, position);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_ActivateObjective(vector position)
	{
		SetOrigin(position);
	}
}
