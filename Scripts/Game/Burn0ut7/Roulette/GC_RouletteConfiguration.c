enum GC_ERouletteBriefingSide
{
	Empty,
	All,
	Team0,
	Team1,
	Team2,
	Team3,
	Team4,
	Team5
}

enum GC_ERouletteBriefingVariables
{
	General,
	Team
}

[BaseContainerProps()]
class GC_RouletteBriefing
{
	[Attribute("", UIWidgets.EditBox)]
	string m_title;

	[Attribute("", UIWidgets.EditBoxMultiline)]
	string m_description;

	[Attribute("0", UIWidgets.ComboBox, "", "", ParamEnumArray.FromEnum(GC_ERouletteBriefingSide))]
	GC_ERouletteBriefingSide m_side;

	[Attribute("0", UIWidgets.ComboBox, "", "", ParamEnumArray.FromEnum(GC_ERouletteBriefingVariables))]
	GC_ERouletteBriefingVariables m_variables;
}

[BaseContainerProps()]
class GC_RouletteElement
{
	[Attribute("", UIWidgets.ResourceNamePicker)]
	ResourceName m_prefab;

	[Attribute("", UIWidgets.ResourceNamePicker)]
	ResourceName m_vehiclePrefab;

	// Count the authored slots, including any explicit empty entries, as the legacy allocator did.
	int GetSlotCount()
	{
		if (m_prefab == string.Empty)
			return 0;

		Resource resource = Resource.Load(m_prefab);
		if (!resource || !resource.IsValid())
			return -1;
		BaseContainer container = resource.GetResource().ToBaseContainer();
		array<ResourceName> slots;
		if (!container || !container.Get("m_aUnitPrefabSlots", slots) || !slots)
			return -1;
		return slots.Count();
	}

	bool TrySelect(int target, inout int selectedSlots, out bool selected)
	{
		selected = false;
		int slots = GetSlotCount();
		if (slots < 0)
			return false;

		// Once the target is reached, accept a tie but never move farther away.
		if (selectedSlots >= target && Math.AbsInt(target - selectedSlots - slots) > Math.AbsInt(target - selectedSlots))
			return true;

		selectedSlots += slots;
		selected = true;
		return true;
	}
}

[BaseContainerProps()]
class GC_RouletteSquad : GC_RouletteElement
{
}

[BaseContainerProps()]
class GC_RoulettePlatoon : GC_RouletteElement
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref GC_RouletteSquad> m_squads;

	bool Fill(int target, inout int selectedSlots, notnull array<GC_RouletteElement> roster)
	{
		if (!m_squads)
			return true;

		int start = roster.Count();
		int squads = 0;
		foreach (GC_RouletteSquad squad : m_squads)
		{
			bool picked;
			if (!squad || !squad.TrySelect(target, selectedSlots, picked))
				return false;
			if (!picked)
				break;

			roster.Insert(squad);
			squads++;
			if (squads != 2)
				continue;

			if (!TrySelect(target, selectedSlots, picked))
				return false;
			if (!picked)
				break;
			roster.InsertAt(this, start);
		}
		return true;
	}
}

[BaseContainerProps()]
class GC_RouletteCompany : GC_RouletteElement
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref GC_RoulettePlatoon> m_platoons;

	bool Fill(int target, inout int selectedSlots, notnull array<GC_RouletteElement> roster)
	{
		if (!m_platoons)
			return true;

		int start = roster.Count();
		int platoons = 0;
		foreach (GC_RoulettePlatoon platoon : m_platoons)
		{
			int priorSize = roster.Count();
			if (!platoon || !platoon.Fill(target, selectedSlots, roster))
				return false;
			if (priorSize == roster.Count())
				continue;

			platoons++;
			if (platoons != 2)
				continue;

			bool picked;
			if (!TrySelect(target, selectedSlots, picked))
				return false;
			if (!picked)
				break;
			roster.InsertAt(this, start);
		}
		return true;
	}
}

[BaseContainerProps(), BaseContainerCustomTitleField("m_name")]
class GC_RouletteTeam
{
	[Attribute("", UIWidgets.EditBox)]
	string m_name;

	[Attribute("", UIWidgets.EditBox)]
	FactionKey m_factionKey;

	[Attribute("1", UIWidgets.Auto)]
	float m_strength;

	[Attribute("0", UIWidgets.CheckBox)]
	bool m_squadCallsignsOnly;

	[Attribute("0", UIWidgets.CheckBox)]
	bool m_waterCapable;

	[Attribute("", UIWidgets.Auto)]
	ref array<FactionKey> m_versusBlacklist;

	[Attribute("", UIWidgets.Object)]
	ref array<ref GC_RouletteCompany> m_companies;

	//! The plan holds references to the authored elements. Runtime entities remain separately owned.
	bool TryBuildRoster(int target, notnull array<GC_RouletteElement> roster, out int slots)
	{
		roster.Clear();
		slots = 0;
		if (target < 0 || !m_companies)
			return false;

		foreach (GC_RouletteCompany company : m_companies)
		{
			if (!company || !company.Fill(target, slots, roster))
			{
				roster.Clear();
				slots = 0;
				return false;
			}
		}
		return true;
	}
}

[BaseContainerProps(), BaseContainerCustomTitleField("m_scenarioName")]
class GC_RouletteScenario
{
	[Attribute("", UIWidgets.EditBox)]
	string m_scenarioName;

	[Attribute("", UIWidgets.Object)]
	ref array<ref GC_RouletteTeam> m_teamsList;

	[Attribute("", UIWidgets.Object)]
	ref array<ref GC_RouletteBriefing> m_briefing;

	[Attribute("2", UIWidgets.Auto)]
	float m_ratio;

	[Attribute("4200", UIWidgets.Auto)]
	int m_timeLimit;

	[Attribute("1970", UIWidgets.Auto)]
	float m_yearStart;

	[Attribute("1990", UIWidgets.Auto)]
	float m_yearEnd;

	[Attribute("50", UIWidgets.Auto)]
	float m_buildingSearchSize;

	[Attribute("5", UIWidgets.Auto)]
	int m_minbuildingCount;

	[Attribute("", UIWidgets.ResourceNamePicker)]
	ref array<ResourceName> m_buildingBlackList;

	[Attribute("", UIWidgets.Auto)]
	ref array<string> m_buildingBlackListWords;

	[Attribute("1000", UIWidgets.Auto)]
	int m_width;

	[Attribute("2000", UIWidgets.Auto)]
	int m_minAOSize;

	[Attribute("5000", UIWidgets.Auto)]
	int m_maxAOSize;

	[Attribute("20", UIWidgets.Auto)]
	int m_maxWaterArea;
}

[BaseContainerProps()]
class GC_RouletteCatalogue
{
	[Attribute("", UIWidgets.Object)]
	ref array<ref GC_RouletteScenario> m_aScenarios;
}

[BaseContainerProps()]
class GC_RouletteScenarioOverride
{
	[Attribute("0", UIWidgets.Auto)]
	int m_iScenarioIndex;

	[Attribute("-1", UIWidgets.Auto)]
	int m_minAOSize;
}

[BaseContainerProps()]
class GC_RouletteMap
{
	[Attribute("", UIWidgets.ResourceNamePicker, "", "conf")]
	ResourceName m_sCatalogue;

	[Attribute("", UIWidgets.Object)]
	ref array<ref GC_RouletteScenarioOverride> m_aOverrides;

	GC_RouletteCatalogue LoadCatalogue()
	{
		if (m_sCatalogue == string.Empty)
			return null;
		Resource resource = Resource.Load(m_sCatalogue);
		if (!resource || !resource.IsValid())
			return null;
		BaseContainer config = resource.GetResource().ToBaseContainer();
		if (!config)
			return null;
		return GC_RouletteCatalogue.Cast(BaseContainerTools.CreateInstanceFromContainer(config));
	}

	int GetMinimumAOSize(GC_RouletteCatalogue catalogue, int index)
	{
		if (!catalogue || !catalogue.m_aScenarios || !catalogue.m_aScenarios.IsIndexValid(index)
			|| !catalogue.m_aScenarios[index])
			return -1;

		if (m_aOverrides)
		{
			foreach (GC_RouletteScenarioOverride entry : m_aOverrides)
			{
				if (entry && entry.m_iScenarioIndex == index && entry.m_minAOSize > 0)
					return entry.m_minAOSize;
			}
		}
		return catalogue.m_aScenarios[index].m_minAOSize;
	}
}
