//! Build the selected scenario's outcome rules for the native Lobby engine.
class GC_RouletteRules
{
	static const string OBJECTIVE = "GC_RouletteObjective";

	static bool Build(FactionKey defender, FactionKey attacker, int seconds, notnull array<ref GC_EventDefinition> events)
	{
		if (defender.IsEmpty() || attacker.IsEmpty() || seconds < 10 || seconds > 86400)
			return false;
		events.Insert(Casualties("Roulette defender casualties", defender, attacker,
			"Attackers win due to defender casualty rate being reached."));
		events.Insert(Casualties("Roulette attacker casualties", attacker, defender,
			"Defenders win due to attacker casualty rate being reached."));

		GC_Event timeLimit = new GC_Event();
		timeLimit.m_sName = "Roulette time limit";
		timeLimit.m_aActions = {
			Message("Time Limit Reached", "Defenders win due to time limit being reached.", seconds - 10),
			Victory(defender, seconds)
		};
		events.Insert(timeLimit);

		GC_Event capture = new GC_Event();
		capture.m_sName = "Roulette objective";
		capture.m_aConditions = {Capture(attacker)};
		capture.m_aActions = {Victory(attacker, -1)};
		events.Insert(capture);
		foreach (GC_EventDefinition definition : events)
		{
			if (!definition.BuildRule())
				return false;
		}
		return true;
	}

	protected static GC_Event Casualties(string name, FactionKey lost, FactionKey winner, string text)
	{
		GC_PeakFactionPopulationCheck population = new GC_PeakFactionPopulationCheck();
		population.m_sFaction = lost;
		population.m_fRatio = 0.95;
		GC_CustomScriptCondition condition = new GC_CustomScriptCondition();
		condition.m_Script = population;
		GC_Event loss = new GC_Event();
		loss.m_sName = name;
		loss.m_aConditions = {condition};
		loss.m_aActions = {Message("Casualty Rate reached", text, -1), Victory(winner, 10)};
		return loss;
	}

	protected static GC_AreaCondition Capture(FactionKey attacker)
	{
		GC_CharacterAreaTargets players = new GC_CharacterAreaTargets();
		players.m_aFactions = {attacker};
		GC_LocalFactionShare share = new GC_LocalFactionShare();
		share.m_fRequiredShare = 1;
		GC_CaptureMessages messages = new GC_CaptureMessages();
		messages.m_sLocation = "the objective";
		messages.m_sCaptured = "The %1 has taken control of %2.";
		messages.m_sCapturing = "The %1 is taking control of %2.";
		messages.m_sCaptureInterrupted = "The %1 is no longer taking control of %2.";
		messages.m_sLost = "The %1 has lost control of %2.";
		messages.m_sLosing = "The %1 is losing control of %2.";
		messages.m_sLossInterrupted = "The %1 is no longer losing control of %2.";
		GC_AreaCaptureOptions timing = new GC_AreaCaptureOptions();
		timing.m_fSeconds = 300;
		timing.m_iCheckInterval = 1;
		timing.m_bUseElapsedTime = true;
		timing.m_iInterruption = 3;
		timing.m_bStopAfterFirstChange = true;
		timing.m_Messages = messages;
		GC_AreaCondition condition = new GC_AreaCondition();
		condition.m_sArea = OBJECTIVE;
		condition.m_Targets = players;
		condition.m_Requirement = share;
		condition.m_Capture = timing;
		return condition;
	}

	protected static GC_MessageAction Message(string title, string body, float delay)
	{
		GC_MessageAction action = new GC_MessageAction();
		action.m_sTitle = title;
		action.m_sMessage = body;
		action.m_bScheduleAction = delay >= 0;
		action.m_fActionDelay = delay;
		return action;
	}

	protected static GC_EndMissionAction Victory(FactionKey winner, float delay)
	{
		GC_EndMissionAction action = new GC_EndMissionAction();
		action.m_iResult = 1;
		action.m_aWinningFactions = {winner};
		action.m_bFinishImmediately = true;
		action.m_bScheduleAction = delay >= 0;
		action.m_fActionDelay = delay;
		return action;
	}
}
