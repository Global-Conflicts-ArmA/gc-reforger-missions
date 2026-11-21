//! Owns only entities authored by a Roulette roll. Native groups and Lobby own all slots and membership.
class GC_RouletteRosterPlacement
{
	protected GC_LobbyManager m_Manager;
	protected BaseWorld m_World;
	protected ref array<IEntity> m_aActive = {};

	void GC_RouletteRosterPlacement(GC_LobbyManager manager)
	{
		m_Manager = manager;
		if (manager && manager.GetOwner())
			m_World = manager.GetOwner().GetWorld();
	}

	bool CanReplace()
	{
		if (!m_Manager || GC_LobbyManager.GetManager() != m_Manager || !Replication.IsServer())
			return false;
		SCR_EGameModeState phase = m_Manager.GetLobbyPhase();
		if (phase != SCR_EGameModeState.PREGAME && phase != SCR_EGameModeState.SLOTTING)
			return false;
		foreach (IEntity entity : m_aActive)
		{
			SCR_AIGroup group = SCR_AIGroup.Cast(entity);
			if (group && !m_Manager.CanRetireMissionGroup(group))
				return false;
		}
		return true;
	}

	bool Replace(GC_RouletteTerrain terrain, GC_RouletteRosterPlan plan, vector objective, vector defender, vector attacker)
	{
		if (!terrain || !plan || !CanReplace())
			return false;

		array<IEntity> pending = {};
		array<ref GC_RouletteCallsign> callsigns = {};
		plan.m_aDefenderVehicles.Clear();
		plan.m_aAttackerVehicles.Clear();
		terrain.SetPlacementState(m_aActive);
		bool prepared = PlaceTeam(terrain, plan.m_aDefenders, defender, vector.Direction(defender, attacker).ToYaw(), pending, callsigns, plan.m_aDefenderVehicles)
			&& PlaceTeam(terrain, plan.m_aAttackers, attacker, vector.Direction(attacker, objective).ToYaw(), pending, callsigns, plan.m_aAttackerVehicles);
		terrain.SetPlacementState(null);
		if (!prepared || !CanReplace())
		{
			Remove(pending);
			return false;
		}

		Remove(m_aActive);
		m_aActive.Copy(pending);
		SCR_CallsignManagerComponent manager = SCR_CallsignManagerComponent.Cast(m_Manager.GetOwner().FindComponent(SCR_CallsignManagerComponent));
		if (manager)
		{
			foreach (GC_RouletteCallsign call : callsigns)
				manager.GC_TryAssignMissionCallsign(call.m_Group, call.m_iCompany, call.m_iPlatoon, call.m_iSquad);
		}
		return true;
	}

	protected bool PlaceTeam(GC_RouletteTerrain terrain, array<GC_RouletteElement> elements, vector start, float yaw, array<IEntity> pending, array<ref GC_RouletteCallsign> callsigns, array<IEntity> vehicles)
	{
		int companyIndex;
		int platoonIndex;
		int squadIndex;
		bool seenCompany, seenPlatoon, seenSquad;
		foreach (GC_RouletteElement element : elements)
		{
			if (!element)
				return false;
			vector position;
			int slots = element.GetSlotCount();
			if (slots < 0 || (slots == 0 && element.m_prefab != string.Empty))
				return false;
			if (element.m_vehiclePrefab != string.Empty)
			{
				if (!terrain.FindClearGround(start, 5, 200, position, 10))
					return false;
				IEntity vehicle = Spawn(element.m_vehiclePrefab, position, yaw, pending);
				if (!vehicle)
					return false;
				vehicles.Insert(vehicle);
				vector mins, maxs;
				vehicle.GetBounds(mins, maxs);
				vector size = maxs - mins;
				position += vector.FromYaw(yaw + 90) * (size[0] + 1) + vector.FromYaw(yaw) * (size[2] * 0.5);
			}
			else if (slots > 0 && !terrain.FindClearGround(start, slots, 300, position))
				return false;

			if (element.m_prefab == string.Empty)
				continue;
			SCR_AIGroup group = SCR_AIGroup.Cast(Spawn(element.m_prefab, position, yaw, pending));
			if (!group || !group.IsPlayable() || !m_Manager.GetRegisteredGroups().Contains(group)
				|| m_Manager.GetSlotsForGroup(group).Count() != slots
				|| (RplSession.Mode() != RplMode.None && !Replication.FindItemId(group).IsValid()))
				return false;

			terrain.ReserveGroupPosition(position);
			AIFormationComponent formation = AIFormationComponent.Cast(group.FindComponent(AIFormationComponent));
			if (formation)
				formation.SetFormation("File");

			SCR_CallsignGroupComponent callsign = SCR_CallsignGroupComponent.Cast(group.FindComponent(SCR_CallsignGroupComponent));
			if (!callsign)
				continue;
			if (GC_RouletteCompany.Cast(element))
			{
				if (seenCompany)
					companyIndex++;
				platoonIndex = 0;
				squadIndex = 0;
				seenCompany = true;
			}
			else if (GC_RoulettePlatoon.Cast(element))
			{
				if (seenPlatoon)
					platoonIndex++;
				squadIndex = 1;
				seenPlatoon = true;
			}
			else
			{
				if (seenSquad)
					squadIndex++;
				else
					squadIndex = 2;
				seenSquad = true;
			}
			callsigns.Insert(new GC_RouletteCallsign(group, companyIndex, platoonIndex, squadIndex));
		}
		return true;
	}

	protected IEntity Spawn(ResourceName prefab, vector position, float yaw, array<IEntity> pending)
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return null;
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), params.Transform);
		params.Transform[3] = position;
		IEntity entity = GetGame().SpawnEntityPrefab(resource, m_World, params);
		if (entity)
			pending.Insert(entity);
		return entity;
	}

	protected void Remove(notnull array<IEntity> entities)
	{
		for (int index = entities.Count() - 1; index >= 0; index--)
		{
			IEntity entity = entities[index];
			if (!entity || entity.IsDeleted())
				continue;
			SCR_AIGroup group = SCR_AIGroup.Cast(entity);
			if (group && m_Manager.GetRegisteredGroups().Contains(group))
				m_Manager.RetireMissionGroup(group);
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}
		entities.Clear();
	}
}

class GC_RouletteCallsign
{
	SCR_AIGroup m_Group;
	int m_iCompany;
	int m_iPlatoon;
	int m_iSquad;

	void GC_RouletteCallsign(SCR_AIGroup group, int company, int platoon, int squad)
	{
		m_Group = group;
		m_iCompany = company;
		m_iPlatoon = platoon;
		m_iSquad = squad;
	}
}
