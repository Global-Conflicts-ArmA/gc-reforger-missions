//! Publishes the selected scenario through the existing briefing and AO owners.
class GC_RoulettePresentation
{
	static const string BOUNDARY = "GC_RouletteBoundary";
	static const string OBJECTIVE_MARKER = "GC_RouletteObjectiveMarker";
	static const string DEFENDER_MARKER = "GC_RouletteDefenderMarker";
	static const string ATTACKER_MARKER = "GC_RouletteAttackerMarker";
	protected static const ResourceName CIRCLE = "{E23427CAC80DA8B7}UI/Textures/Icons/icons_mapMarkersUI.imageset";
	protected static const ResourceName CIRCLE_GLOW = "{67B3A6DC2D712B52}UI/Textures/Icons/icons_mapMarkersUI-glow.imageset";

	protected GC_AOLimitEntity m_Boundary;
	protected GC_MapShapeComponent m_Mask;
	protected GC_LobbyManager m_Manager;
	protected int m_iPublishedPages;

	void GC_RoulettePresentation(GC_LobbyManager manager)
	{
		m_Manager = manager;
		IEntity mode = manager.GetOwner();
		m_Boundary = GC_AOLimitEntity.Cast(mode.GetWorld().FindEntityByName(BOUNDARY));
		if (m_Boundary && m_Boundary.GetWorld() == mode.GetWorld())
			m_Mask = GC_MapShapeComponent.Cast(m_Boundary.FindComponent(GC_MapShapeComponent));
	}

	bool CanPublish(GC_RouletteScenario scenario, FactionKey defender, FactionKey attacker, array<vector> polygon)
	{
		if (GC_LobbyManager.GetManager() != m_Manager || !m_Boundary || !m_Mask
			|| FlagTexture(defender).IsEmpty() || FlagTexture(attacker).IsEmpty()
			|| !GC_MapPolygon.IsValid(polygon) || !scenario.m_briefing || scenario.m_briefing.Count() > 30)
			return false;
		foreach (int index, GC_RouletteBriefing page : scenario.m_briefing)
		{
			if (!page || !GC_RouletteBriefing.IsSupportedSide(page.m_side)
				|| page.m_title.Length() > GC_LobbyBriefingCatalog.MAX_BRIEFING_TITLE_LENGTH
				|| (index >= m_iPublishedPages && m_Manager.GetBriefing(index)))
				return false;
		}
		return true;
	}

	static void GetPolygon(vector attacker, vector objective, int width, notnull array<vector> points)
	{
		float halfWidth = width / 2;
		vector forward = vector.Direction(attacker, objective).Normalized();
		vector right = Vector(forward[2], 0, -forward[0]);
		points.Insert(attacker + (-forward + right) * halfWidth);
		points.Insert(attacker + (-forward - right) * halfWidth);
		points.Insert(objective + (forward - right) * halfWidth);
		points.Insert(objective + (forward + right) * halfWidth);
	}

	bool Publish(GC_RouletteScenario scenario, GC_RouletteTeam defenderTeam, GC_RouletteTeam attackerTeam,
		GC_RouletteRosterPlan roster, vector objective, vector defender, vector attacker, array<vector> polygon)
	{
		if (!m_Boundary.SetPolygon(polygon, false, "") || !m_Boundary.SetEnabled(true) || !m_Mask.SetEnabled(true))
			return false;

		if (!PublishPages(scenario, defenderTeam, attackerTeam, roster))
			return false;
		GC_BriefingMarker marker = new GC_BriefingMarker();
		marker.Set(OBJECTIVE_MARKER, CIRCLE, CIRCLE_GLOW, "circle-2", objective, 90, true, 100, 0, "",
			null, SCR_EGameModeState.PREGAME, false, true, Color.RED);
		return m_Manager.UpsertBriefingMarker(marker)
			&& PublishFlag(DEFENDER_MARKER, defenderTeam.m_factionKey, defender)
			&& PublishFlag(ATTACKER_MARKER, attackerTeam.m_factionKey, attacker);
	}

	protected bool PublishFlag(string name, FactionKey key, vector position)
	{
		ResourceName flag = FlagTexture(key);
		if (flag.IsEmpty())
			return false;
		GC_BriefingMarker marker = new GC_BriefingMarker();
		marker.Set(name, flag, "", "", position, 90, false, 40, 0, "",
			null, SCR_EGameModeState.PREGAME);
		return m_Manager.UpsertBriefingMarker(marker);
	}

	protected ResourceName FlagTexture(FactionKey key)
	{
		SCR_Faction faction = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(key));
		if (!faction)
			return "";
		return faction.GetFactionFlag();
	}

	protected bool PublishPages(GC_RouletteScenario scenario, GC_RouletteTeam defender, GC_RouletteTeam attacker, GC_RouletteRosterPlan roster)
	{
		string defenders = OrderOfBattle(roster.m_aDefenders);
		string attackers = OrderOfBattle(roster.m_aAttackers);
		string defendingVehicles = VehicleList(roster.m_aDefenderVehicles);
		string attackingVehicles = VehicleList(roster.m_aAttackerVehicles);
		string time, year, weather;
		ReadEnvironment(time, year, weather);

		foreach (int index, GC_RouletteBriefing page : scenario.m_briefing)
		{
			string contents;
			if (page.m_variables == GC_ERouletteBriefingVariables.General)
				contents = string.Format(page.m_description, scenario.m_scenarioName, time, year, weather, attacker.m_name, defender.m_name);
			else
				contents = string.Format(page.m_description, defender.m_name, defenders, defendingVehicles, attacker.m_name, attackers, attackingVehicles);

			array<string> audience = {};
			GC_EBriefingAudience visibility = GC_EBriefingAudience.DEFAULT;
			if (page.m_side == GC_ERouletteBriefingSide.Team0)
				audience.Insert(defender.m_factionKey);
			else if (page.m_side == GC_ERouletteBriefingSide.Team1)
				audience.Insert(attacker.m_factionKey);
			else if (page.m_side == GC_ERouletteBriefingSide.Empty)
				visibility = GC_EBriefingAudience.LISTED_AND_UNASSIGNED;
			if (!m_Manager.SetBriefing(index, page.m_title, contents, audience, SCR_EGameModeState.PREGAME, false, true, visibility))
				return false;
		}
		for (int index = scenario.m_briefing.Count(); index < m_iPublishedPages; index++)
			m_Manager.RemoveBriefing(index);
		m_iPublishedPages = scenario.m_briefing.Count();
		return true;
	}

	protected string OrderOfBattle(array<GC_RouletteElement> elements)
	{
		int companies, platoons, squads;
		foreach (GC_RouletteElement element : elements)
		{
			if (GC_RouletteCompany.Cast(element))
				companies++;
			else if (GC_RoulettePlatoon.Cast(element))
				platoons++;
			else
				squads++;
		}
		string summary;
		if (companies)
			summary += string.Format("%1x Company\n", companies);
		if (platoons)
			summary += string.Format("%1x Platoon\n", platoons);
		if (squads)
			summary += string.Format("%1x Squad\n", squads);
		return summary;
	}

	protected string VehicleList(array<IEntity> vehicles)
	{
		map<string, int> counts = new map<string, int>();
		foreach (IEntity vehicle : vehicles)
		{
			SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.GetEditableEntity(vehicle);
			string name;
			if (editable)
				name = WidgetManager.Translate(editable.GetDisplayName());
			if (name.IsEmpty())
				name = vehicle.GetName();
			if (name.IsEmpty())
				name = vehicle.Type().ToString();
			counts.Set(name, counts.Get(name) + 1);
		}

		string summary;
		foreach (string name, int count : counts)
			summary += string.Format("%1x - %2\n", count, name);
		return summary;
	}

	protected void ReadEnvironment(out string time, out string year, out string weatherName)
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(m_Manager.GetOwner().GetWorld());
		if (!world)
			return;
		TimeAndWeatherManagerEntity weather = world.GetTimeAndWeatherManager();
		if (!weather)
			return;
		int hour, minute, second;
		weather.GetHoursMinutesSeconds(hour, minute, second);
		string hours = hour.ToString();
		string minutes = minute.ToString();
		if (hour < 10)
			hours = "0" + hours;
		if (minute < 10)
			minutes = "0" + minutes;
		time = hours + minutes;
		year = weather.GetYear().ToString();
		WeatherState state = weather.GetCurrentWeatherState();
		if (state)
			weatherName = WidgetManager.Translate(state.GetLocalizedName());
	}
}
