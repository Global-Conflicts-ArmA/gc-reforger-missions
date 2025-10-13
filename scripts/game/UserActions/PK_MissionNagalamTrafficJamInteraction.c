// THIS SCRIPT DOES NOT WORK PROPERLY, DO NOT USE IT AS INSPIRATION
class PK_MissionNagalamTrafficJamInteraction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.Auto, "If false, prevent dead or destroyed entities from being counted.", category: "Trigger Filter")]
	protected bool m_allowDestroyed;
	
	/*string m_specialCount_start = "10";
	string m_specialCount;
	string m_rescueStatus;
	
	protected string m_factionKeyUS = "US";
	protected string m_factionKeyGCINS = "GC_INSURGENT";
	
	float m_queryRadius = 150;

	ref map<string, int> m_curAliveFactionPlayers = new map<string, int>();
	ref map<string, int> m_maxAliveFactionPlayers = new map<string, int>();
	ref map<string, int> m_factionAIDeaths = new map<string, int>();
	
	
	
	protected ref array<string> m_aObjectiveVehicleEntities = {
	    "M1025_armed_M2HB_USAF_D1",
	    "M1025_armed_M2HB_USAF_D2",
	    "M998_covered_long_USAF_D1",
	    "Ural4320_ammo_tan1",
	    "Ural4320_ammo_tan2",
	    "Ural4320_tanker_tan1",
	    "Ural4320_transport_tan1"
	};
	
	protected ref array<string> m_aObjectiveRescueEntities = {
	    "rescueUnit_1",
		"rescueUnit_2",
		"rescueUnit_3",
		"rescueUnit_4",
		"rescueUnit_5",
		"rescueUnit_6"
	};
*/

	// Array of variable names
	protected ref array<string> m_aObjectiveVehicleEntities = {
	    "pk_sVehicle_01_returned",
	    "pk_sVehicle_02_returned",
	    "pk_sVehicle_03_returned",
	    "pk_sVehicle_04_returned",
	    "pk_sVehicle_05_returned",
	    "pk_sVehicle_06_returned",
		"pk_sVehicle_07_returned"
	};
	
	// Array of variable names
	protected ref array<string> m_aObjectiveRescueEntities = {
	    "pk_sWounded_01_returned",
	    "pk_sWounded_02_returned",
	    "pk_sWounded_03_returned",
	    "pk_sWounded_04_returned",
	    "pk_sWounded_05_returned",
	    "pk_sWounded_06_returned"
	};
	
	

		
	int m_iObjectiveVehicleInitCount = m_aObjectiveVehicleEntities.Count();
	int m_iObjectiveRescueInitCount = m_aObjectiveRescueEntities.Count();
	
	
	
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		
		int m_iObjectiveVehicleCount = 0;
		int m_iObjectiveRescueCount = 0; 
		
		/*IEntity m_BaseFlag = GetGame().GetWorld().FindEntityByName("BaseFlag");

		
		TILW_MissionFrameworkEntity mfe = TILW_MissionFrameworkEntity.GetInstance();
		
		
		float m_TotalDeathsUS = (mfe.m_maxAliveFactionPlayers.Get(m_factionKeyUS) - mfe.m_curAliveFactionPlayers.Get(m_factionKeyUS));
		float m_TotalDeathsAiUS = mfe.m_factionAIDeaths.Get(m_factionKeyUS);
		float m_TotalDeathsAiGC_INS = mfe.m_factionAIDeaths.Get(m_factionKeyGCINS); */
		

		/*
		SCR_ScenarioFrameworkSystem scenarioFrameworkSystem = SCR_ScenarioFrameworkSystem.GetInstance();
		if (!scenarioFrameworkSystem)
			return;
		
		string pk_sWounded_01_returned;
		string pk_sWounded_02_returned;
		string pk_sWounded_03_returned;
		string pk_sWounded_04_returned;
		string pk_sWounded_05_returned;
		string pk_sWounded_06_returned;
		
		scenarioFrameworkSystem.GetVariable("pk_sWounded_01_returned", pk_sWounded_01_returned);
		scenarioFrameworkSystem.GetVariable("pk_sWounded_02_returned", pk_sWounded_02_returned);
		scenarioFrameworkSystem.GetVariable("pk_sWounded_03_returned", pk_sWounded_03_returned);
		scenarioFrameworkSystem.GetVariable("pk_sWounded_04_returned", pk_sWounded_04_returned);
		scenarioFrameworkSystem.GetVariable("pk_sWounded_05_returned", pk_sWounded_05_returned);
		scenarioFrameworkSystem.GetVariable("pk_sWounded_06_returned", pk_sWounded_06_returned);
		
		
		if(pk_sWounded_01_returned == "true") 
			m_iObjectiveRescueCount++;
		
		if(pk_sWounded_02_returned == "true") 
			m_iObjectiveRescueCount++;
		
		if(pk_sWounded_03_returned == "true") 
			m_iObjectiveRescueCount++;
		
		if(pk_sWounded_04_returned == "true") 
			m_iObjectiveRescueCount++;
		
		if(pk_sWounded_05_returned == "true") 
			m_iObjectiveRescueCount++;
		
		if(pk_sWounded_06_returned == "true") 
			m_iObjectiveRescueCount++;
		
		Print("PK Value : " + m_iObjectiveRescueCount );
		*/
		
		
		SCR_ScenarioFrameworkSystem scenarioFrameworkSystem = SCR_ScenarioFrameworkSystem.GetInstance();
		if (!scenarioFrameworkSystem)
			return;
		
		
		
		foreach (string varName : m_aObjectiveVehicleEntities)
		{
			string value;
		    scenarioFrameworkSystem.GetVariable(varName, value);
		    if (value == "true")
		        m_iObjectiveVehicleCount++;
		}
		
		

		foreach (string varName : m_aObjectiveRescueEntities)
		{
			string value;
		    scenarioFrameworkSystem.GetVariable(varName, value);
		    if (value == "true")
		        m_iObjectiveRescueCount++;
		}
		
		

		
		Print("PK Value : " + m_iObjectiveVehicleCount);
		Print("PK Value : " + m_iObjectiveRescueCount);
		
		
		string m_sObjectiveVehicleString = "Returned: " + m_iObjectiveVehicleCount + "/" +  m_iObjectiveVehicleInitCount;
		string m_sObjectiveRescueString = "Returned: " + m_iObjectiveRescueCount + "/" +  m_iObjectiveRescueInitCount;
		

		// Overall Mission Status
		//string m_overallMissionStatus = m_rescueStatus(m_iObjectiveRescueCount, m_iObjectiveVehicleCount);
		string m_overallMissionStatus = CheckMissionOutcome(m_iObjectiveVehicleCount, m_iObjectiveRescueCount);
		
		
		
		
		
		// Get the singleton instance of the Hint Manager
		SCR_HintManagerComponent hintComponent = SCR_HintManagerComponent.GetInstance();
		
		// Show a custom hint
		// Parameters: (string bodyText, string title, float durationInSeconds)

		hintComponent.ShowCustomHint("Rescue operation status \n " + m_sObjectiveRescueString + "\n\n Vehicle operation status \n "  + m_sObjectiveVehicleString, m_overallMissionStatus, 10, false);
		
		//hintComponent.ShowCustomHint("Rescue operation status \n " + m_sObjectiveRescueString + "\n\n Vehicle operation status \n "  + m_sObjectiveVehicleString + "\n\n US Casualty: " + m_TotalDeathsUS + "\n\n US AI Casualty: " + m_TotalDeathsAiUS + "\n\n Insurgent Casualty: " + m_TotalDeathsAiGC_INS, m_overallMissionStatus, 10, false);
		
	}
	
	
	string MissionOutcome = "";
	
	string CheckMissionOutcome(int VehicleCount, int RescueCount)
	{
	    if (VehicleCount >= 8 && RescueCount >= 6)
	    {
	        return "Major Victory";
	    }
	    else if ((VehicleCount >= 6 && RescueCount >= 5) || (VehicleCount >= 7 && RescueCount >= 4) || (VehicleCount >= 4 && RescueCount >= 6))
	    {
	        return "Minor Victory";
	    }
	    else if ((VehicleCount >= 3 && RescueCount >= 3) || (VehicleCount >= 4 && RescueCount >= 2) || (VehicleCount >= 2 && RescueCount >= 4))
	    {
	        return "Draw";
	    }
	    else if (VehicleCount >= 1 || RescueCount >= 1)
	    {
	        return "Minor Defeat";
	    }
	    else
	    {
	        return "Major Defeat";
	    }
	}	
	
	
	string m_rescueStatus(int VehicleCount, int RescueCount)
	{
	    if (VehicleCount <= 2 && RescueCount <= 2) return "Major Defeat";
	    if (VehicleCount <= 1 && RescueCount <= 1) return "Major Defeat";
	    if (RescueCount == 6) return "Average";
	    if (RescueCount <= 9) return "Above Average";
	    if (RescueCount == 10) return "Excellent";
	    return "Unknown value";
	}
	
	//! IsEntityDestroyed checks if the entity is still alive. This works for anything with an SCR_DamageManagerComponent.
	static bool IsEntityDestroyed(IEntity entity)
	{
		SCR_DamageManagerComponent damageManager = SCR_DamageManagerComponent.GetDamageManager(entity);
		return (damageManager && damageManager.GetState() == EDamageState.DESTROYED);
	}


}
