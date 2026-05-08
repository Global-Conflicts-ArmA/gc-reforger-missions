class SCR_QRFCrewWaypointContext
{
	AIWaypoint m_wp;
	SCR_ScenarioFrameworkQRFSlotAI m_spawnSlot;
	SCR_AIGroup m_crewGroup;
	IEntity m_vehicleEntity;
	SCR_AIGroup m_passengerGroup;
}

[ BaseContainerProps(), SCR_ContainerActionTitle() ] class SCR_ScenarioFrameworkActionQRFDispacher_modified : SCR_ScenarioFrameworkActionBase
{
	[Attribute("X_QRF", UIWidgets.Auto, "Name of the QRF layer that contains pool of spawn points")] protected string m_sQRFLayerName;

	[Attribute(SCR_EQRFThresholdType.DEAD_UNITS.ToString(), UIWidgets.ComboBox, desc : "Type of the threshold threshold that will be used to determine if it should spawn QRF", "", ParamEnumArray.FromEnum(SCR_EQRFThresholdType))] protected SCR_EQRFThresholdType m_eThresholdType; // <= REMAINING_UNITS, DEAD_UNITS, PROC_LOST_UNITS

	[Attribute("5", UIWidgets.Auto, "Numeric value which interpretation depends on the m_eThresholdType f.e. if Threshold Type == REMAINING_UNITS then if Threshold >= (number of units alive) then send QRF")] protected int m_iThreshold;

	[Attribute("5", UIWidgets.Auto, "How many times this Area can send QRF where -1 == unlimited")] protected int m_iNumberOfAvailableQRFWaves; // - even if we have more QRF groups but we are out of waves then there wont be new QRF sent

	[Attribute("1", UIWidgets.Auto, "How much threat level has to be increased each time that threshold is reached")] protected float m_fThreatLevelEscalation;

	[Attribute("0", UIWidgets.Auto, "How long (in seconds) game should wait when threshold is reached before spawning QRF where 0 == imminently", "0 inf 1")] protected float m_fQRFSpawnDelay;

	[Attribute("300", UIWidgets.Auto, "How long (in seconds) game should wait until next QRF will be possible to be requested where 0 == no delay", "0 inf 1")] protected float m_fQRFNextWaveDelay;

	[Attribute(desc : "List of possible QRF groups to spawn")] protected ref array<ref SCR_QRFGroupConfig_modified> m_aGroupList;

	[Attribute(desc : "List defining maximal distance for QRF unit of given type")] protected ref array<ref SCR_QRFTypeMaxDistance> m_aQRFMaxDistanceConfig;

	[Attribute(desc : "List of waypoints for QRF that will be applied in order (to aplicable group type)")] protected ref array<ref SCR_QRFWaypointConfig> m_aWPConfig;

	[Attribute(desc : "Sound event name that will be played on dead soldier entity when QRF is requested")] protected string m_sQRFRequestedSoundEventName;

	[Attribute(desc : "Sound event name that will be played on dead soldier entity when QRF is spawned")] protected string m_sQRFSentSoundEventName;

	[Attribute(desc : "'*.acp' file which contains desired sound events", params : "acp")] protected ResourceName m_sSoundProjectFile;

	[Attribute(desc : "Actions that will be triggered once QRF are dispatched.")] ref array<ref SCR_ScenarioFrameworkActionBase> m_aActions;

	protected ref array<SCR_ScenarioFrameworkQRFSlotAI> m_aQRFSpawnPoints = {};
	protected SCR_ScenarioFrameworkArea m_AreaFramework;
	protected SCR_ScenarioFrameworkLayerBase m_QRFLayer;
	protected int m_iNumberOfSoldiersInTheArea;
	protected int m_iRemovedSoldier;
	protected float m_fNextWaveDelayClock;
	protected float m_fThreatLevel = 1;
	protected int m_iSpawnTickets;
	protected bool m_bWaitingForDelayedSpawn;
	protected bool m_bWaitingForNextWave;
	protected vector m_vTargetPosition;
	protected ref array<ref SCR_QRFVehicleSpawnConfig_modified> m_aVehicleSpawnQueueConfig = {};
	protected ref map<SCR_QRFGroupConfig_modified, float> m_mGroupNextSpawnAt = new map<SCR_QRFGroupConfig_modified, float>();
	protected ref array<ref SCR_QRFCrewWaypointContext> m_crewWaypointContexts = {};
	protected ref array<ref SCR_QRFCrewWaypointContext> m_passengerMoveContexts = {};

	private static const ResourceName MOVE_WAYPOINT_PREFAB = "{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et";
	private static const ResourceName WAIT_WAYPOINT_PREFAB = "{531EC45063C1F57B}Prefabs/AI/Waypoints/AIWaypoint_Wait.et";
	//------------------------------------------------------------------------------------------------
	//! Will tap into AI groups and monitor their numbers
	//! Will return true if there was at least one grou00p which we now observe
	protected bool WatchAIGroup(IEntity entities)
	{
		return WatchAIGroup({entities});
	}

	//------------------------------------------------------------------------------------------------
	//! Will tap into AI groups and monitor their numbers
	//! Will return true if there was at least one group which we now observe
	protected bool WatchAIGroup(array<IEntity> entities)
	{
		if (!entities)
			return false;

		bool result;
		SCR_AIGroup aiGroup;
		foreach (IEntity entity : entities)
		{
			aiGroup = GetAIGroup(entity);
			if (!aiGroup)
				continue;

			aiGroup.GetOnAgentRemoved().Remove(OnGroupCompositionChanged);
			aiGroup.GetOnAgentRemoved().Insert(OnGroupCompositionChanged);
			aiGroup.GetOnAllDelayedEntitySpawned().Remove(OnDelayedGroupMembersSpawned);
			aiGroup.GetOnAllDelayedEntitySpawned().Insert(OnDelayedGroupMembersSpawned);
			if (aiGroup.GetAgentsCount() == 0)
				m_iNumberOfSoldiersInTheArea++;
			else
				m_iNumberOfSoldiersInTheArea += aiGroup.GetAgentsCount();

			result = true;
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Retrieves AI group from entity even when it is an AI agent or SCR_ChimeraCharacter
	SCR_AIGroup GetAIGroup(IEntity entity)
	{
		if (!entity)
			return null;

		SCR_AIGroup aiGroup = SCR_AIGroup.Cast(entity);
		if (aiGroup)
			return aiGroup;

		AIAgent agent = AIAgent.Cast(entity);
		if (agent)
			return SCR_AIGroup.Cast(agent.GetParentGroup());

		if (!SCR_ChimeraCharacter.Cast(entity))
			return null;

		AIControlComponent control = AIControlComponent.Cast(entity.FindComponent(AIControlComponent));
		if (!control)
			return null;

		agent = control.GetControlAIAgent();
		if (!agent)
			return null;

		return SCR_AIGroup.Cast(agent.GetParentGroup());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDelayedGroupMembersSpawned(SCR_AIGroup group)
	{
		m_iNumberOfSoldiersInTheArea++;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnGroupCompositionChanged(SCR_AIGroup group, AIAgent agent)
	{
		if (!agent)
			return;

		if (!agent.GetControlledEntity())
			return;

		SCR_ChimeraCharacter char = SCR_ChimeraCharacter.Cast(agent.GetControlledEntity());
		if (!char)
			return;

		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(char.GetCharacterController());
		if (!controller || controller.GetLifeState() != ECharacterLifeState.DEAD)
			return;

		vector tmpVec = agent.GetControlledEntity().GetOrigin();
		if (m_vTargetPosition == vector.Zero)
			m_vTargetPosition = tmpVec;
		else
			m_vTargetPosition = vector.Lerp(m_vTargetPosition, tmpVec, 0.95);

		m_iRemovedSoldier++;
		ThresholdValidation(char)
	}

	//------------------------------------------------------------------------------------------------
	protected void ThresholdValidation(IEntity killedEntity = null)
	{
		bool startQRFProcedure;
		switch (m_eThresholdType)
		{
		case SCR_EQRFThresholdType.DEAD_UNITS:
		{
			if (m_iRemovedSoldier >= m_iThreshold)
				startQRFProcedure = true;
		}
		break;

		case SCR_EQRFThresholdType.REMAINING_UNITS:
		{
			if (m_iNumberOfSoldiersInTheArea - m_iRemovedSoldier <= m_iThreshold)
				startQRFProcedure = true;
		}
		break;

		case SCR_EQRFThresholdType.PROC_LOST_UNITS:
		{
			if (m_iNumberOfSoldiersInTheArea > 0 && (m_iNumberOfSoldiersInTheArea - m_iRemovedSoldier) * 100 / m_iNumberOfSoldiersInTheArea <= m_iThreshold)
				startQRFProcedure = true;
		}
		break;
		}

		if (!startQRFProcedure)
			return;

		if (m_bWaitingForDelayedSpawn)
		{
			if (!m_bWaitingForNextWave && m_iNumberOfAvailableQRFWaves - 1 > 0 && m_fQRFSpawnDelay > 0)
			{
				float callTime = m_fNextWaveDelayClock - GetGame().GetWorld().GetWorldTime();
				if (callTime < 0)
					callTime = 0;

				m_bWaitingForNextWave = true;
				SCR_ScenarioFrameworkSystem.GetCallQueuePausable().CallLater(StartQRFProcedure, callTime, param1 : killedEntity);
			}
			return;
		}

		if (m_bWaitingForNextWave)
			return;

		StartQRFProcedure(killedEntity);
	}

	//------------------------------------------------------------------------------------------------
	protected void ThresholdHandling()
	{
		switch (m_eThresholdType)
		{
		case SCR_EQRFThresholdType.DEAD_UNITS:
		{
			m_iRemovedSoldier = 0;
		}
		break;

		case SCR_EQRFThresholdType.REMAINING_UNITS:
		{
			m_iNumberOfSoldiersInTheArea -= m_iRemovedSoldier;
		}
		break;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void StartQRFProcedure(IEntity killedEntity)
	{
		if (m_iNumberOfAvailableQRFWaves == 0)
			return;

		float time = GetGame().GetWorld().GetWorldTime();
		if (m_fNextWaveDelayClock > time && !m_bWaitingForDelayedSpawn && !m_bWaitingForNextWave)
			return;

		if (m_fQRFSpawnDelay > 0)
		{
			if (!m_bWaitingForDelayedSpawn)
			{
				if (m_bWaitingForNextWave)
					m_bWaitingForNextWave = false;

				ThresholdHandling();
				m_fNextWaveDelayClock = time + m_fQRFNextWaveDelay * 1000;
				m_bWaitingForDelayedSpawn = true;
				SCR_ScenarioFrameworkSystem.GetCallQueuePausable().CallLater(StartQRFProcedure, m_fQRFSpawnDelay * 1000, param1 : killedEntity);
				if (killedEntity)
					DoPlaySoundOnEntityPosition(killedEntity, m_sSoundProjectFile, m_sQRFRequestedSoundEventName);

				return;
			}
			else
			{
				m_bWaitingForDelayedSpawn = false;
			}
		}
		else
		{
			ThresholdHandling();
			m_fNextWaveDelayClock = time + m_fQRFNextWaveDelay * 1000;
		}

		if (killedEntity)
			DoPlaySoundOnEntityPosition(killedEntity, m_sSoundProjectFile, m_sQRFSentSoundEventName);

		m_iNumberOfSoldiersInTheArea -= m_iRemovedSoldier;
		m_iSpawnTickets += m_fThreatLevel;
		m_fThreatLevel += m_fThreatLevelEscalation;
		if (m_aQRFSpawnPoints.IsEmpty())
			return;

		while (m_iSpawnTickets > 0)
		{
			SCR_QRFGroupConfig_modified selectedGroup = SelectRandomGroup(m_iSpawnTickets);
			if (!selectedGroup)
			{
				m_iSpawnTickets--;
				continue;
			}

			m_iSpawnTickets -= selectedGroup.GetSpawnCost();
			SCR_ScenarioFrameworkQRFSlotAI selectedSpawnpoint = SelectRandomSpawnpoint(m_aQRFSpawnPoints, selectedGroup.GetGroupType());
			if (!selectedSpawnpoint)
			{
				m_iSpawnTickets--;
				continue;
			}

			if (selectedGroup.GetNumberOfAvailableGroups() > 0)
				selectedGroup.SetNumberOfAvailableGroups(selectedGroup.GetNumberOfAvailableGroups() - 1);

			if (IsGroupOnSpawnDelay(selectedGroup))
			{
				QueueRetryForGroup(selectedGroup, selectedSpawnpoint);
				continue;
			}
			SpawnGroup(selectedGroup, selectedSpawnpoint);
		}
		if (m_iNumberOfAvailableQRFWaves > 0)
			m_iNumberOfAvailableQRFWaves--;

		m_vTargetPosition = vector.Zero;

		foreach (SCR_ScenarioFrameworkActionBase actions : m_aActions)
		{
			actions.OnActivate(m_Entity);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnGroup(SCR_QRFGroupConfig_modified selectedGroup, SCR_ScenarioFrameworkQRFSlotAI selectedSpawnPoint)
	{
		// Create child entity under selected spawn point owner
		ResourceName childPrefab = "{FCE82B7F21949782}Prefabs/Systems/ScenarioFramework/Compositions/QRFSpawnpoint.et";
		IEntity parentEntity = selectedSpawnPoint.GetOwner();
		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		parentEntity.GetWorldTransform(spawnParams.Transform);
		IEntity childEntity = GetGame().SpawnEntityPrefab(Resource.Load(childPrefab), GetGame().GetWorld(), spawnParams);

		// Parent clone under original (never parent original to itself)
		if (!childEntity)
		{
			Print("Child entity failed to spawn for QRF Dispacher", LogLevel.ERROR);
			return;
		}
		parentEntity.AddChild(childEntity, -1, EAddChildFlags.AUTO_TRANSFORM | EAddChildFlags.RECALC_LOCAL_TRANSFORM);

		// Attach/fetch slot component on child
		SCR_ScenarioFrameworkQRFSlotAI childSlot = SCR_ScenarioFrameworkQRFSlotAI.Cast(
			childEntity.FindComponent(SCR_ScenarioFrameworkQRFSlotAI));
		if (!childSlot)
		{
			Print("childSlot is invalid for QRF Dispacher", LogLevel.ERROR);
			return;
		}
		if (!selectedGroup.GetGroupPrefabName())
		{
			Print("Group Prefab is invalid for QRF Dispacher", LogLevel.ERROR);
			return;
		}

		childSlot.m_eAISkill = selectedSpawnPoint.m_eAISkill;
		childSlot.SetObjectToSpawn(selectedGroup.GetGroupPrefabName());
		childSlot.SetEnableRepeatedSpawn(true);
		childSlot.SetIsTerminated(false);

		// Not sure why waypoints get cleared...
		if (childSlot.GetNumberOfExistingWaypoints())
			childSlot.ClearWaypoints();

		childSlot.Init(m_AreaFramework, SCR_ScenarioFrameworkEActivationType.SAME_AS_PARENT);
		IEntity spawnedEntity = childSlot.GetSpawnedEntity();
		SCR_AIGroup aiGroup = GetAIGroup(spawnedEntity);

		if (aiGroup)
		{
			if (WatchAIGroup(aiGroup))
			{
				// Add waypoint from spawnpoint onto the group
				if (selectedSpawnPoint)
				{
					foreach (SCR_ScenarioFrameworkSlotWaypoint waypointSlot : selectedSpawnPoint.m_aSlotWaypoints)
					{
						AIWaypoint wp = AIWaypoint.Cast(waypointSlot.GetSpawnedEntity());
						if (!wp)
						{
							Print("Waypoint entity is invalid for QRF Dispacher", LogLevel.ERROR);
							continue;
						}
						childSlot.m_aSlotWaypoints.Insert(waypointSlot);
						childSlot.m_aWaypoints.Insert(wp);
						aiGroup.AddWaypoint(wp);
					}
				}

				vector wpPosition;
				foreach (SCR_QRFWaypointConfig wp : m_aWPConfig)
				{
					if (wp.GetOrderType() != SCR_EQRFGroupOrderType.ANY && wp.GetOrderType() != selectedGroup.GetGroupType())
						continue;

					if (wp.GetDistanceOffsetToTargetLocation())
						wpPosition = SCR_Math3D.MoveTowards(m_vTargetPosition, childSlot.GetOwner().GetOrigin(), wp.GetDistanceOffsetToTargetLocation());
					else
						wpPosition = m_vTargetPosition;

					if (float.AlmostEqual(vector.DistanceXZ(wpPosition, childSlot.GetOwner().GetOrigin()), 0, 1))
						continue;

					wpPosition[1] = GetGame().GetWorld().GetSurfaceY(wpPosition[0], wpPosition[2]);
					AIWaypoint aiWP = childSlot.CreateWaypoint(wpPosition, wp.GetWaypointPrefabName());
					if (aiWP)
						aiGroup.AddWaypoint(aiWP);
				}
			}
		}
		else
		{
			if (Vehicle.Cast(spawnedEntity))
			{
				SCR_BaseCompartmentManagerComponent compartmentManager = SCR_BaseCompartmentManagerComponent.Cast(spawnedEntity.FindComponent(SCR_BaseCompartmentManagerComponent));
				if (compartmentManager)
				{
					compartmentManager.GetOnDoneSpawningDefaultOccupants().Insert(OnFinishedSpawningVehicleOccupants);
					array<ECompartmentType> compartmentTypes = {ECompartmentType.PILOT, ECompartmentType.TURRET};
					if (selectedGroup.GetGroupType() == SCR_EQRFGroupType.MOUNTED_INFANTRY)
						compartmentTypes.Insert(ECompartmentType.CARGO);

					m_aVehicleSpawnQueueConfig.Insert(new SCR_QRFVehicleSpawnConfig_modified(compartmentManager, selectedGroup.GetGroupType(), m_vTargetPosition, childSlot, selectedSpawnPoint));
					compartmentManager.SpawnDefaultOccupants(compartmentTypes);
				}
			}
		}
		// Set the spawn delay if there is one for consecutive groups of the same type
		SetGroupSpawnDelay(selectedGroup);
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_QRFGroupConfig_modified SelectRandomGroup(int maxCost)
	{
		// first check if there even are groups that we can afford
		bool availableGroupAtFullPrice;
		array<SCR_QRFGroupConfig_modified> verifiedList = {};
		foreach (SCR_QRFGroupConfig_modified group : m_aGroupList)
		{
			if (!group)
				continue;

			if (group.GetNumberOfAvailableGroups() == -1 || group.GetNumberOfAvailableGroups() > 0)
			{
				if (group.GetSpawnCost() == maxCost)
				{
					if (!availableGroupAtFullPrice)
					{
						availableGroupAtFullPrice = true;
						verifiedList.Clear();
					}
					verifiedList.Insert(group);
				}
				else if (group.GetSpawnCost() < maxCost && !availableGroupAtFullPrice)
				{
					verifiedList.Insert(group);
				}
			}
		}

		if (availableGroupAtFullPrice)
			return verifiedList.GetRandomElement();

		SCR_QRFGroupConfig_modified returnedGroup;
		if (!verifiedList.IsEmpty())
		{
			int i, numOfTries, count = verifiedList.Count();
			while (numOfTries < 10)
			{
				i = Math.RandomInt(0, count);
				if (!returnedGroup || returnedGroup && returnedGroup.GetSpawnCost() < verifiedList[i].GetSpawnCost())
					returnedGroup = verifiedList[i];

				numOfTries++;
			}
		}

		return returnedGroup;
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_ScenarioFrameworkQRFSlotAI SelectRandomSpawnpoint(array<SCR_ScenarioFrameworkQRFSlotAI> aListOfSpawnPoints, SCR_EQRFGroupType searchedType)
	{
		float maxSpawnDistance = GetMaxDistanceForUnitType(searchedType);
		array<SCR_ScenarioFrameworkQRFSlotAI> verifiedList = {};
		foreach (SCR_ScenarioFrameworkQRFSlotAI spawn : aListOfSpawnPoints)
		{
			if (!(spawn.GetGroupType() & searchedType))
				continue;

			if (!CheckSpawnPointSafeZones(spawn.GetOwner().GetOrigin(), spawn.GetSpawnSafeZones(), searchedType))
				continue;

			if (maxSpawnDistance > -1 && vector.Distance(m_vTargetPosition, spawn.GetOwner().GetOrigin()) > maxSpawnDistance)
				continue;

			verifiedList.Insert(spawn);
		}

		int index = -1;
		if (verifiedList.Count() > 0)
			return verifiedList.GetRandomElement();

		float distanceToTarget, cloasestPosDistance = float.MAX;
		foreach (int i, SCR_ScenarioFrameworkQRFSlotAI spawn : aListOfSpawnPoints) // int i = aListOfSpawnPoints.Count() - 1; i >= 0; i--)
		{
			if (!(spawn.GetGroupType() & searchedType))
				continue;

			if (!CheckSpawnPointSafeZones(spawn.GetOwner().GetOrigin(), spawn.GetSpawnSafeZones(), searchedType))
				continue;

			distanceToTarget = vector.Distance(m_vTargetPosition, spawn.GetOwner().GetOrigin());
			if (distanceToTarget < cloasestPosDistance)
			{
				cloasestPosDistance = distanceToTarget;
				index = i;
			}
		}

		if (index == -1)
			return null;

		return aListOfSpawnPoints[index];
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true when there are no observers (players) within specified distance for given group type
	protected bool CheckSpawnPointSafeZones(vector spawnPointPosition, array<ref SCR_QRFSpawnSafeZone> spawnSafeZones, SCR_EQRFGroupType searchedType)
	{
		if (spawnSafeZones.IsEmpty())
			return true;

		array<vector> aObserversPositions = {};
		array<int> playerIds = {};
		PlayerManager playerManager = GetGame().GetPlayerManager();
		IEntity player;
		SCR_DamageManagerComponent damageManager;
		playerManager.GetPlayers(playerIds);

		foreach (int playerId : playerIds)
		{
			player = playerManager.GetPlayerControlledEntity(playerId);
			if (!player)
				continue;

			damageManager = SCR_DamageManagerComponent.GetDamageManager(player);
			if (damageManager && damageManager.GetState() != EDamageState.DESTROYED)
				aObserversPositions.Insert(player.GetOrigin());
		}

		foreach (SCR_QRFSpawnSafeZone safeZone : spawnSafeZones)
		{
			if (safeZone.GetGroupType() != searchedType)
				continue;

			foreach (vector observerPos : aObserversPositions)
			{
				if (vector.Distance(observerPos, spawnPointPosition) < safeZone.GetMinDistanceToClosestObserver())
					return false;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected float GetMaxDistanceForUnitType(SCR_EQRFGroupType unitType)
	{
		foreach (SCR_QRFTypeMaxDistance conf : m_aQRFMaxDistanceConfig)
		{
			if (conf.GetGroupType() == unitType)
				return conf.GetMaxSpawnDistance();
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Meant to be invked when vehicle finishes spawning its occupants so we can get the group of those occupants
	protected void OnFinishedSpawningVehicleOccupants(SCR_BaseCompartmentManagerComponent compartmentManager, array<IEntity> occupants, bool wasCanceled)
	{
		compartmentManager.GetOnDoneSpawningDefaultOccupants().Remove(OnFinishedSpawningVehicleOccupants);
		if (wasCanceled)
			return;

		if (occupants.IsEmpty())
			return;

		SCR_ChimeraCharacter occupant = SCR_ChimeraCharacter.Cast(occupants[0]);
		if (!occupant)
			return;

		AIControlComponent control = AIControlComponent.Cast(occupant.FindComponent(AIControlComponent));
		if (!control)
			return;

		AIAgent agent = control.GetControlAIAgent();
		if (!agent)
			return;

		SCR_AIGroup aiGroup = SCR_AIGroup.Cast(agent.GetParentGroup());
		if (!aiGroup)
			return;

		if (!WatchAIGroup(aiGroup))
			return;

		// Split off crew from original group
		SCR_AIGroup crewGroup = SplitCrewFromVic(compartmentManager, occupants, aiGroup);
		if (!crewGroup)
		{
			Print("Failed to split crew from vehicle group for QRF Dispacher", LogLevel.ERROR);
			return;
		}

		vector wpPosition;
		bool once = false;
		for (int i, count = m_aVehicleSpawnQueueConfig.Count(); i < count; i++)
		{
			if (m_aVehicleSpawnQueueConfig[i].m_VehicleCompartmentMGR != compartmentManager)
				continue;
			SCR_ScenarioFrameworkQRFSlotAI selectedSpawnpoint = m_aVehicleSpawnQueueConfig[i].m_parentSlot;
			SCR_ScenarioFrameworkQRFSlotAI childSlot = m_aVehicleSpawnQueueConfig[i].m_Slot;

			// Add waypoint from spawnpoint onto occupants first
			if (!once)
			{
				foreach (SCR_ScenarioFrameworkSlotWaypoint waypointSlot : selectedSpawnpoint.m_aSlotWaypoints)
				{
					AIWaypoint waypoint = AIWaypoint.Cast(waypointSlot.GetSpawnedEntity());
					if (waypoint)
					{
						if (SCR_ScenarioFrameworkWaypointGetOut.Cast(waypointSlot.m_Waypoint))
						{
							IEntity vehicleEntity = compartmentManager.GetOwner();
							SCR_QRFCrewWaypointContext dropCtx;
							if (crewGroup)
							{
								dropCtx = AssignCrewDropoffWaypoints(crewGroup, selectedSpawnpoint, vehicleEntity, waypointSlot);
								if (dropCtx)
									dropCtx.m_passengerGroup = aiGroup;
							}
							AssignPassengerDropoffWaypoints(aiGroup, selectedSpawnpoint, waypoint, vehicleEntity, crewGroup);
						}
						else
						{
							// childSlot.m_aWaypoints.Insert(waypoint);
							aiGroup.AddWaypoint(waypoint);
						}
					}
				}
				once = true;
			}

			foreach (SCR_QRFWaypointConfig wp : m_aWPConfig)
			{
				if (wp.GetOrderType() != SCR_EQRFGroupOrderType.ANY && wp.GetOrderType() != m_aVehicleSpawnQueueConfig[i].m_eGroupType)
					continue;

				if (wp.GetDistanceOffsetToTargetLocation())
					wpPosition = SCR_Math3D.MoveTowards(m_aVehicleSpawnQueueConfig[i].m_vTargetPosition, occupant.GetOrigin(), wp.GetDistanceOffsetToTargetLocation());
				else
					wpPosition = m_aVehicleSpawnQueueConfig[i].m_vTargetPosition;

				if (float.AlmostEqual(vector.DistanceXZ(wpPosition, occupant.GetOrigin()), 0, 1))
					continue;

				AIWaypoint aiWP = m_aVehicleSpawnQueueConfig[i].m_Slot.CreateWaypoint(wpPosition, wp.GetWaypointPrefabName());
				if (aiWP)
				{
					// childSlot.m_aWaypoints.Insert(aiWP);
					aiGroup.AddWaypoint(aiWP);
				}
			}
			m_aVehicleSpawnQueueConfig.Remove(i);
			break;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Requests game mode to broadcast sound event on the position of provided entity
	protected void DoPlaySoundOnEntityPosition(IEntity entity, string soundFileName, string soundEventName)
	{
		if (!entity)
			return;

		if (SCR_StringHelper.IsEmptyOrWhiteSpace(soundFileName))
			return;

		if (SCR_StringHelper.IsEmptyOrWhiteSpace(soundEventName))
			return;

		SCR_ScenarioFrameworkSystem scenarioFrameworkSystem = SCR_ScenarioFrameworkSystem.GetInstance();
		if (!scenarioFrameworkSystem)
			return;

		SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.Cast(entity.FindComponent(SCR_GadgetManagerComponent));
		if (!gadgetManager)
			return;

		if (!gadgetManager.GetGadgetByType(EGadgetType.RADIO))
			return;

		scenarioFrameworkSystem.PlaySoundOnEntityPosition(entity, soundFileName, soundEventName);
	}

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity entity)
	{
		SCR_ScenarioFrameworkLayerBase thisLayer = SCR_ScenarioFrameworkLayerBase.Cast(entity.FindComponent(SCR_ScenarioFrameworkLayerBase));
		if (!thisLayer)
			return;

		m_AreaFramework = thisLayer.GetParentArea();
		if (!m_AreaFramework)
			return;

		super.Init(entity);
	}

	//------------------------------------------------------------------------------------------------
	override void OnActivate(IEntity object)
	{
		if (!CanActivate())
			return;

		array<SCR_ScenarioFrameworkLayerBase> childLayers = m_AreaFramework.GetChildrenEntities();
		if (childLayers.IsEmpty())
		{
			IEntity child = m_AreaFramework.GetOwner().GetChildren();
			SCR_ScenarioFrameworkLayerBase layer;
			while (child)
			{
				layer = SCR_ScenarioFrameworkLayerBase.Cast(child.FindComponent(SCR_ScenarioFrameworkLayerBase));
				if (layer)
					childLayers.Insert(layer);

				child = child.GetSibling();
			}
			if (childLayers.IsEmpty())
				return;
		}

		m_iNumberOfSoldiersInTheArea = 0;
		foreach (SCR_ScenarioFrameworkLayerBase layer : childLayers)
		{
			if (!layer)
				continue;

			if (layer.GetName() == m_sQRFLayerName)
			{
				m_QRFLayer = layer;
				IEntity child = layer.GetOwner().GetChildren();
				SCR_ScenarioFrameworkQRFSlotAI qrfSlot;
				while (child)
				{
					qrfSlot = SCR_ScenarioFrameworkQRFSlotAI.Cast(child.FindComponent(SCR_ScenarioFrameworkQRFSlotAI));
					if (qrfSlot && !m_aQRFSpawnPoints.Contains(qrfSlot))
						m_aQRFSpawnPoints.Insert(qrfSlot);

					child = child.GetSibling();
				}
				continue;
			}
		}

		if (m_aQRFSpawnPoints && m_aQRFSpawnPoints.IsEmpty())
			Print("SCR_ScenarioFrameworkActionQRFDispacher.OnActivate: Layer " + m_AreaFramework.GetName() + " doesn't have any spawn points for its QRF dispatcher!", LogLevel.ERROR);

		CheckForAIToWatch(m_AreaFramework.GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! Recursive check for AI slot withing entity hierarchy
	protected void CheckForAIToWatch(IEntity entity)
	{
		IEntity child = entity.GetChildren();
		SCR_ScenarioFrameworkLayerBase layer;
		while (child)
		{
			layer = SCR_ScenarioFrameworkLayerBase.Cast(child.FindComponent(SCR_ScenarioFrameworkLayerBase));
			if (SCR_ScenarioFrameworkSlotAI.Cast(layer) || SCR_ScenarioFrameworkSlotTaskAI.Cast(layer))
				WatchAIGroup(layer.GetSpawnedEntities());
			else if (layer && layer.GetName() != m_sQRFLayerName)
				CheckForAIToWatch(child);

			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	override array<ref SCR_ScenarioFrameworkActionBase> GetSubActions()
	{
		return m_aActions;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsGroupOnSpawnDelay(SCR_QRFGroupConfig_modified group)
	{
		if (!group || group.GetQRFSpawnDelay() <= 0)
			return false;

		float nextSpawnAt;
		if (!m_mGroupNextSpawnAt.Find(group, nextSpawnAt))
			return false;

		return nextSpawnAt > GetGame().GetWorld().GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	protected void SetGroupSpawnDelay(SCR_QRFGroupConfig_modified group)
	{
		if (!group)
			return;

		float spawnDelay = group.GetQRFSpawnDelay();
		if (spawnDelay <= 0)
			return;

		m_mGroupNextSpawnAt.Set(group, GetGame().GetWorld().GetWorldTime() + spawnDelay * 1000);
	}

	//------------------------------------------------------------------------------------------------
	protected void QueueRetryForGroup(SCR_QRFGroupConfig_modified group, SCR_ScenarioFrameworkQRFSlotAI spawnPoint)
	{
		if (!group)
			return;

		if (!IsGroupOnSpawnDelay(group))
		{
			SpawnGroup(group, spawnPoint);
			return;
		}

		float nextSpawnAt;
		m_mGroupNextSpawnAt.Find(group, nextSpawnAt);
		float callTime = nextSpawnAt - GetGame().GetWorld().GetWorldTime();

		if (callTime > 0)
			SCR_ScenarioFrameworkSystem.GetCallQueuePausable().CallLater(QueueRetryForGroup, callTime, param1 : group, param2 : spawnPoint);
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_AIGroup SplitCrewFromVic(SCR_BaseCompartmentManagerComponent compartmentManager, array<IEntity> occupants, SCR_AIGroup aiGroup)
	{
		// Identify crew directly from each spawned occupant's compartment slot.
		// Querying the compartment manager via GetOccupantsOfType can return empty on
		// heavier maps because physical seating may not be registered yet when this
		// callback fires.
		array<IEntity> crewOccupants = {};
		foreach (IEntity occupantEnt : occupants)
		{
			ChimeraCharacter ch = ChimeraCharacter.Cast(occupantEnt);
			if (!ch)
				continue;

			SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(ch.GetCompartmentAccessComponent());
			if (!access)
				continue;

			BaseCompartmentSlot slot = access.GetCompartment();
			if (!slot)
				continue;

			ECompartmentType type = slot.GetType();
			if (type == ECompartmentType.PILOT || type == ECompartmentType.TURRET)
				crewOccupants.Insert(occupantEnt);
		}

		if (crewOccupants.IsEmpty())
		{
			Print("No crew occupants found to split from vehicle group for QRF Dispacher, occupants size: " + occupants.Count(), LogLevel.ERROR);
			return null;
		}

		// Create AI-only group (not playable/player group)
		SCR_AIGroup crewGroup = SCR_AIGroup.Cast(
			GetGame().SpawnEntityPrefab(Resource.Load("{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et")));
		if (!crewGroup)
		{
			Print("Failed to spawn crew group for QRF Dispacher", LogLevel.ERROR);
			return null;
		}

		// Keep same faction as source group
		Faction sourceFaction = aiGroup.GetFaction();
		if (sourceFaction)
			crewGroup.SetFaction(sourceFaction);

		// Move pilot/turret to crew group; cargo stays in original aiGroup
		foreach (IEntity crewEnt : crewOccupants)
		{
			aiGroup.RemoveAIEntityFromGroup(crewEnt);
			crewGroup.AddAIEntityToGroup(crewEnt);
		}

		// aiGroup = cargo/main group
		// crewGroup = pilot+turret group
		return crewGroup;
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_QRFCrewWaypointContext AssignCrewDropoffWaypoints(
		SCR_AIGroup crewGroup,
		SCR_ScenarioFrameworkQRFSlotAI spawnSlot,
		IEntity vehicleEntity,
		SCR_ScenarioFrameworkSlotWaypoint getOutSlot)
	{

		if (!crewGroup || !spawnSlot || !getOutSlot || !getOutSlot.m_Waypoint)
			return null;

		// Register vehicle so crewGroup's AI knows to DRIVE, not walk to waypoints
		SCR_AIVehicleUsageComponent vehicleUsageComp = SCR_AIVehicleUsageComponent.FindOnNearestParent(vehicleEntity, vehicleEntity);
		if (vehicleUsageComp)
			crewGroup.GetGroupUtilityComponent().AddUsableVehicle(vehicleUsageComp);

		vector center = getOutSlot.GetOwner().GetOrigin();
		center[1] = GetGame().GetWorld().GetSurfaceY(center[0], center[2]);
		float WAIT_HOLD_TIME = 5.0;

		float dropoffRadius = getOutSlot.m_Waypoint.GetWaypointCompletionRadius();
		if (dropoffRadius <= 0.0)
			dropoffRadius = 5.0;

		// Pick a random spot inside the getOut circle
		float angle = Math.RandomFloat(0, Math.PI2);
		float dist = Math.RandomFloat(0, dropoffRadius);
		vector randomSpot = Vector(
			center[0] + Math.Cos(angle) * dist,
			0,
			center[2] + Math.Sin(angle) * dist);
		randomSpot[1] = GetGame().GetWorld().GetSurfaceY(randomSpot[0], randomSpot[2]);

		// First Wait waypoint at the random spot
		AIWaypoint firstWP = spawnSlot.CreateWaypoint(randomSpot, MOVE_WAYPOINT_PREFAB);
		if (!firstWP)
			return null;
		firstWP.SetCompletionRadius(1.0);
		crewGroup.AddWaypoint(firstWP);

		// When first WP completes, place 2 more in front of the vehicle's live facing
		SCR_QRFCrewWaypointContext ctx = new SCR_QRFCrewWaypointContext();
		ctx.m_crewGroup = crewGroup;
		ctx.m_vehicleEntity = vehicleEntity;
		ctx.m_spawnSlot = spawnSlot;
		ctx.m_wp = firstWP;
		m_crewWaypointContexts.Insert(ctx);
		crewGroup.GetOnWaypointCompleted().Insert(OnCrewFirstWaypointCompleted);
		return ctx;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCrewFirstWaypointCompleted(AIWaypoint wp)
	{
		for (int i = m_crewWaypointContexts.Count() - 1; i >= 0; i--)
		{
			SCR_QRFCrewWaypointContext ctx = m_crewWaypointContexts[i];
			if (ctx.m_wp != wp)
				continue;

			ctx.m_crewGroup.GetOnWaypointCompleted().Remove(OnCrewFirstWaypointCompleted);
			m_crewWaypointContexts.Remove(i);

			// Read vehicle's live facing direction at the moment of completion
			vector forward = Vector(0, 0, 1);
			if (ctx.m_vehicleEntity)
			{
				forward = ctx.m_vehicleEntity.GetWorldTransformAxis(2);
				forward[1] = 0;
				float fwdLen = forward.Length();
				if (fwdLen > 0.001)
					forward = forward * (1.0 / fwdLen);
			}

			vector basePos;
			if (ctx.m_vehicleEntity)
				basePos = ctx.m_vehicleEntity.GetOrigin();
			else
				basePos = wp.GetOrigin();

			// 2nd waypoint: X m ahead; 3rd waypoint: Y m ahead
			AIWaypoint lastCrewWP;
			for (int j = 1; j <= 2; j++)
			{
				float fwdDist = j * 10.0;
				vector waitPos = Vector(basePos[0] + forward[0] * fwdDist,
										0,
										basePos[2] + forward[2] * fwdDist);
				waitPos[1] = GetGame().GetWorld().GetSurfaceY(waitPos[0], waitPos[2]);

				AIWaypoint newWp = ctx.m_spawnSlot.CreateWaypoint(waitPos, MOVE_WAYPOINT_PREFAB);
				if (newWp)
				{
					newWp.SetCompletionRadius(1.0);
					ctx.m_crewGroup.AddWaypoint(newWp);
					lastCrewWP = newWp;
				}
			}
			break;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPassengerMoveWaypointCompleted(AIWaypoint wp)
	{
		for (int i = m_passengerMoveContexts.Count() - 1; i >= 0; i--)
		{
			SCR_QRFCrewWaypointContext ctx = m_passengerMoveContexts[i];
			if (ctx.m_wp != wp)
				continue;

			ctx.m_passengerGroup.GetOnWaypointCompleted().Remove(OnPassengerMoveWaypointCompleted);
			m_passengerMoveContexts.Remove(i);

			if (ctx.m_vehicleEntity)
				GetGame().GetCallqueue().CallLater(EjectAllRemainingOccupants, 30000, false, ctx.m_vehicleEntity, ctx.m_crewGroup, ctx.m_passengerGroup);
			break;
		}
	}

	//------------------------------------------------------------------------------------------------
	// Force-ejects all remaining occupants (cargo, pilot, turret) still inside the vehicle.
	// Uses ejectOnTheSpot=true so each soldier teleports to their seat's world position —
	// distributed around the vehicle, no door queue.
	protected void MergeCrewIntoPassengers(SCR_AIGroup crewGroup, SCR_AIGroup passengerGroup)
	{
		if (!crewGroup || !passengerGroup)
			return;

		array<AIWaypoint> crewWPs = {};
		crewGroup.GetWaypoints(crewWPs);
		foreach (AIWaypoint crewWP : crewWPs)
			crewGroup.RemoveWaypoint(crewWP);

		array<AIAgent> crewAgents = {};
		crewGroup.GetAgents(crewAgents);
		foreach (AIAgent crewAgent : crewAgents)
		{
			IEntity crewEnt = crewAgent.GetControlledEntity();
			if (!crewEnt)
				continue;
			crewGroup.RemoveAIEntityFromGroup(crewEnt);
			passengerGroup.AddAIEntityToGroup(crewEnt);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void EjectAllRemainingOccupants(IEntity vehicleEntity, SCR_AIGroup crewGroup = null, SCR_AIGroup passengerGroup = null)
	{
		if (!vehicleEntity)
			return;

		SCR_BaseCompartmentManagerComponent compartmentMgr =
			SCR_BaseCompartmentManagerComponent.Cast(vehicleEntity.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!compartmentMgr)
			return;

		// Engage handbrake so the vehicle doesn't roll while troops disembark
		Vehicle vehicle = Vehicle.Cast(vehicleEntity);
		CarControllerComponent carController = CarControllerComponent.Cast(vehicle.GetVehicleController());
		if (carController)
		{
			carController.SetPersistentHandBrake(true);
			VehicleWheeledSimulation wheelSim = carController.GetWheeledSimulation();
			if (wheelSim)
				wheelSim.SetBreak(true, true);
		}

		array<IEntity> allOccupants = {};
		compartmentMgr.GetOccupantsOfType(allOccupants, ECompartmentType.CARGO);
		compartmentMgr.GetOccupantsOfType(allOccupants, ECompartmentType.PILOT);
		compartmentMgr.GetOccupantsOfType(allOccupants, ECompartmentType.TURRET);
		if (allOccupants.IsEmpty())
			return;

		int ejected = 0;
		foreach (IEntity occupantEnt : allOccupants)
		{
			ChimeraCharacter character = ChimeraCharacter.Cast(occupantEnt);
			if (!character)
				continue;

			SCR_CompartmentAccessComponent access =
				SCR_CompartmentAccessComponent.Cast(character.GetCompartmentAccessComponent());
			if (!access || !access.IsInCompartment())
				continue;

			BaseCompartmentSlot slot = access.GetCompartment();
			if (!slot)
				continue;

			bool ejectedNow;
			slot.EjectOccupant(true, false, ejectedNow, true);
			if (ejectedNow)
				ejected++;
		}

		if (ejected > 0)
			Print(string.Format("QRF: Force-ejected %1 stuck occupant(s)", ejected));

		MergeCrewIntoPassengers(crewGroup, passengerGroup);
	}

	//------------------------------------------------------------------------------------------------
	// Gives the passenger (cargo) group a Move waypoint to the dropoff point, then a GetOutInstant.
	// Schedules EjectAllRemainingOccupants 30s after the Move WP completes.
	protected AIWaypoint AssignPassengerDropoffWaypoints(
		SCR_AIGroup passengerGroup,
		SCR_ScenarioFrameworkQRFSlotAI spawnSlot,
		AIWaypoint getOutWaypoint,
		IEntity vehicleEntity,
		SCR_AIGroup crewGroup = null)
	{
		static const ResourceName MOVE_WAYPOINT_PREFAB = "{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et";
		static const ResourceName GET_OUT_PREFAB = "{C40316EE26846CAB}Prefabs/AI/Waypoints/AIWaypoint_GetOut.et";
		static const ResourceName GET_OUT_INSTANT_PREFAB = "{E5002E8CD9D1F4AF}Prefabs/AI/Waypoints/AIWaypoint_GetOutInstant.et";

		if (!passengerGroup || !spawnSlot || !getOutWaypoint)
			return null;

		vector dropoffPos = getOutWaypoint.GetOrigin();
		dropoffPos[1] = GetGame().GetWorld().GetSurfaceY(dropoffPos[0], dropoffPos[2]);

		float radius = getOutWaypoint.GetCompletionRadius();

		// 1) Move marker so the vehicle drives to the dropoff location
		AIWaypoint moveWP = spawnSlot.CreateWaypoint(dropoffPos, MOVE_WAYPOINT_PREFAB);
		if (moveWP)
		{
			moveWP.SetCompletionRadius(radius);
			passengerGroup.AddWaypoint(moveWP);

			// Schedule force-eject 30s after passengers complete this Move WP
			if (vehicleEntity)
			{
				SCR_QRFCrewWaypointContext passMoveCtx = new SCR_QRFCrewWaypointContext();
				passMoveCtx.m_wp = moveWP;
				passMoveCtx.m_vehicleEntity = vehicleEntity;
				passMoveCtx.m_passengerGroup = passengerGroup;
				passMoveCtx.m_crewGroup = crewGroup;
				m_passengerMoveContexts.Insert(passMoveCtx);
				passengerGroup.GetOnWaypointCompleted().Insert(OnPassengerMoveWaypointCompleted);
			}
		}

		// 2) GetOutInstant - troops exit immediately without the vehicle re-driving.
		AIWaypoint getOutInstantWP = spawnSlot.CreateWaypoint(dropoffPos, GET_OUT_INSTANT_PREFAB);
		if (getOutInstantWP)
		{
			getOutInstantWP.SetCompletionRadius(radius);

			SCR_BoardingWaypoint boardingWP = SCR_BoardingWaypoint.Cast(getOutInstantWP);
			if (boardingWP)
			{
				if (!boardingWP.m_BoardingParameters)
					boardingWP.m_BoardingParameters = new SCR_AIBoardingWaypointParameters();
				boardingWP.SetAllowance(false, false, true); // driver=NO, gunner=NO, cargo=YES
			}
			passengerGroup.AddWaypoint(getOutInstantWP);
		}
		return moveWP;
	}
}