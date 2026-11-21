//! Terrain queries run only while preparing a roll. Each caller chooses how many candidates to try.
class GC_RouletteTerrain
{
	protected static const float EDGE_BUFFER = 500;
	protected static const float DISCOVERY_RADIUS = 250;
	protected static const float WATER_STEP = 10;
	protected static const float NO_TERRAIN = -256;

	protected BaseWorld m_World;
	protected GC_RouletteScenario m_Scenario;
	protected float m_MapLimit;
	protected ref array<IEntity> m_aBuildings = {};
	protected vector m_vQueryCenter;
	protected float m_fQueryRadius;
	protected bool m_bOccupied;

	protected array<IEntity> m_IgnoredObstacles;
	protected ref array<vector> m_aReservedPositions = {};

	void SetPlacementState(array<IEntity> ignored)
	{
		m_IgnoredObstacles = ignored;
		m_aReservedPositions.Clear();
	}

	void ReserveGroupPosition(vector position)
	{
		m_aReservedPositions.Insert(position);
	}

	void GC_RouletteTerrain(BaseWorld world, IEntity worldEntity, GC_RouletteScenario scenario)
	{
		m_World = world;
		m_Scenario = scenario;
		if (!world || !worldEntity)
			return;

		vector mins, maxs;
		worldEntity.GetWorldBounds(mins, maxs);
		int size = Math.Round(Math.Min(maxs[0] - mins[0], maxs[2] - mins[2]));
		// Subscenes may extend beyond the terrain grid. Find the last loaded cell.
		for (int cell = size - 1; cell > EDGE_BUFFER; cell--)
		{
			if (world.GetSurfaceY(cell, cell) == NO_TERRAIN)
				continue;
			m_MapLimit = cell - EDGE_BUFFER;
			break;
		}
	}

	//! One attempt. A caller can defer the next attempt to another frame.
	IEntity FindObjectiveCandidate(notnull RandomGenerator random)
	{
		if (!m_Scenario || m_MapLimit <= EDGE_BUFFER)
			return null;
		vector pos = Vector(random.RandFloatXY(EDGE_BUFFER, m_MapLimit), 0,
			random.RandFloatXY(EDGE_BUFFER, m_MapLimit));
		pos[1] = m_World.GetSurfaceY(pos[0], pos[2]);
		FindBuildings(pos, DISCOVERY_RADIUS);
		if (m_aBuildings.IsEmpty())
			return null;

		// Broadphase visitation order can change without any terrain changes.
		SortCandidates();
		IEntity candidate = m_aBuildings[random.RandIntInclusive(0, m_aBuildings.Count() - 1)];
		FindBuildings(candidate.GetOrigin(), m_Scenario.m_buildingSearchSize);
		bool qualifies = m_aBuildings.Count() >= m_Scenario.m_minbuildingCount;
		m_aBuildings.Clear();
		if (qualifies)
			return candidate;
		return null;
	}

	protected void SortCandidates()
	{
		for (int index = 1; index < m_aBuildings.Count(); index++)
		{
			IEntity current = m_aBuildings[index];
			int slot = index;
			while (slot > 0 && ComesBefore(current, m_aBuildings[slot - 1]))
			{
				m_aBuildings[slot] = m_aBuildings[slot - 1];
				slot--;
			}
			m_aBuildings[slot] = current;
		}
	}

	protected bool ComesBefore(IEntity left, IEntity right)
	{
		vector a = left.GetOrigin();
		vector b = right.GetOrigin();
		if (a[0] != b[0])
			return a[0] < b[0];
		if (a[2] != b[2])
			return a[2] < b[2];
		if (a[1] != b[1])
			return a[1] < b[1];
		string nameA = left.GetPrefabData().GetPrefabName();
		string nameB = right.GetPrefabData().GetPrefabName();
		return nameA.Compare(nameB) < 0;
	}

	protected void FindBuildings(vector position, float radius)
	{
		m_aBuildings.Clear();
		if (radius <= 0)
			return;
		m_vQueryCenter = position;
		m_fQueryRadius = radius;
		vector extent = Vector(radius, radius, radius);
		m_World.QueryEntitiesByAABB(position - extent, position + extent, CollectBuilding, null, EQueryEntitiesFlags.STATIC);
	}

	protected bool CollectBuilding(IEntity entity)
	{
		if (!Building.Cast(entity) || entity.GetParent() || !IntersectsSearch(entity))
			return true;
		EntityPrefabData data = entity.GetPrefabData();
		if (!data || data.GetPrefabName().IsEmpty())
			return true;
		if (m_Scenario.m_buildingBlackListWords)
		{
			foreach (string word : m_Scenario.m_buildingBlackListWords)
			{
				if (data.GetPrefabName().Contains(word))
					return true;
			}
		}
		BaseContainer prefab = data.GetPrefab();
		if (prefab && m_Scenario.m_buildingBlackList)
		{
			foreach (ResourceName blocked : m_Scenario.m_buildingBlackList)
			{
				if (SCR_BaseContainerTools.IsKindOf(prefab, blocked))
					return true;
			}
		}
		m_aBuildings.Insert(entity);
		return true;
	}

	bool FindAttackerCandidate(notnull RandomGenerator random, vector objective, int minimumDistance, int maximumDistance, out vector position)
	{
		if (minimumDistance < 0 || minimumDistance >= maximumDistance || m_MapLimit <= EDGE_BUFFER)
			return false;
		float bearing = random.RandFloatXY(0, Math.PI2);
		vector direction = Vector(Math.Cos(bearing), 0, Math.Sin(bearing));
		for (float distance = minimumDistance; distance < maximumDistance; distance += 1000)
		{
			vector candidate = objective + direction * distance;
			if (candidate[0] < EDGE_BUFFER || candidate[2] < EDGE_BUFFER
				|| candidate[0] > m_MapLimit || candidate[2] > m_MapLimit)
				return false;
			if (FindClearGround(candidate, 15, 500, position))
				return true;
		}
		return false;
	}

	bool FindClearGround(vector center, float radius, float distance, out vector position, float maxPitch = 15)
	{
		if (!m_World || radius <= 0 || distance < radius)
			return false;
		int rings = Math.Ceil(distance / radius);
		float width = radius * Math.Sqrt(3);
		for (int ring = 0; ring < rings; ring++)
		{
			for (int x = -ring; x <= ring; x++)
			{
				int low = Math.Max(-ring - x, -ring);
				int high = Math.Min(ring - x, ring);
				int step = 1;
				if (Math.AbsInt(x) != ring)
					step = high - low;
				for (int y = low; y <= high; y += step)
				{
					vector candidate = center + Vector(width * x, 0, radius * (x + 2 * y));
					if (vector.DistanceXZ(candidate, center) > distance - radius)
						continue;
					candidate[1] = m_World.GetSurfaceY(candidate[0], candidate[2]);
					if (candidate[1] == NO_TERRAIN || !IsClear(candidate, radius) || IsWater(candidate))
						continue;
					if (TraceGround(candidate, maxPitch, position))
						return true;
				}
			}
		}
		return false;
	}

	bool CrossesWater(vector from, vector to, float threshold)
	{
		if (!m_World || threshold <= 0)
			return false;
		from[1] = 0;
		to[1] = 0;
		vector direction = to - from;
		float length = direction.Length();
		direction.Normalize();
		float consecutive;
		for (float offset = 0; offset <= length; offset += WATER_STEP)
		{
			vector position = from + direction * offset;
			position[1] = m_World.GetSurfaceY(position[0], position[2]);
			if (IsWater(position))
				consecutive += WATER_STEP;
			else
				consecutive = 0;
			if (consecutive >= threshold)
				return true;
		}
		return false;
	}

	protected bool IsWater(vector position)
	{
		vector point, extents;
		vector transform[4];
		EWaterSurfaceType kind;
		ChimeraWorldUtils.TryGetWaterSurface(m_World, position, point, kind, transform, extents);
		return kind != EWaterSurfaceType.WST_NONE;
	}

	protected bool IsClear(vector position, float radius)
	{
		foreach (vector occupied : m_aReservedPositions)
		{
			if (vector.DistanceXZ(occupied, position) <= radius)
				return false;
		}
		m_vQueryCenter = position;
		m_fQueryRadius = radius;
		m_bOccupied = false;
		vector extent = Vector(radius, radius, radius);
		m_World.QueryEntitiesByAABB(position - extent, position + extent, CollectObstacle, null, EQueryEntitiesFlags.ALL);
		return !m_bOccupied;
	}

	protected bool CollectObstacle(IEntity entity)
	{
		if (!entity || entity.GetParent() || !entity.GetPhysics() || !IntersectsSearch(entity)
			|| (m_IgnoredObstacles && m_IgnoredObstacles.Contains(entity)))
			return true;
		m_bOccupied = true;
		return false;
	}

	protected bool IntersectsSearch(IEntity entity)
	{
		vector mins, maxs;
		entity.GetWorldBounds(mins, maxs);
		float distance;
		return SCR_Global.IntersectBoxSphere(m_vQueryCenter, m_fQueryRadius, mins, maxs, distance) || distance == 0;
	}

	protected bool TraceGround(vector candidate, float maxPitch, out vector position)
	{
		TraceParam trace = new TraceParam();
		trace.Flags = TraceFlags.ANY_CONTACT | TraceFlags.WORLD | TraceFlags.OCEAN | TraceFlags.ENTS;
		trace.Start = candidate + vector.Up * 0.01;
		trace.End = candidate - vector.Up * 0.01;
		float hit = m_World.TraceMove(trace);
		if (hit <= 0 || hit >= 1 || Math.Acos(trace.TraceNorm[1]) * Math.RAD2DEG > maxPitch)
			return false;
		position = vector.Lerp(trace.Start, trace.End, hit);
		return true;
	}
}
