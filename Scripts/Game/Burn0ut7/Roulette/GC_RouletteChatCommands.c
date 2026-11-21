//! The owning Lobby controller supplies the requester. No command accepts a player ID from chat.
modded class GC_LobbyPlayerController
{
	void GC_SendRouletteCommand(string command, string argument)
	{
		if (GetGame().GetPlayerController() != this || !GC_RouletteSetupComponent.GetActive())
			return;
		if (Replication.IsServer())
			GC_RpcAskRoulette(command, argument);
		else
			Rpc(GC_RpcAskRoulette, command, argument);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void GC_RpcAskRoulette(string command, string argument)
	{
		if (!Replication.IsServer() || command.Length() > 16 || argument.Length() > 96)
			return;
		GC_RouletteSetupComponent setup = GC_RouletteSetupComponent.GetActive();
		if (!setup || GetGame().GetPlayerManager().GetPlayerController(GetPlayerId()) != this)
			return;
		string response = setup.RunCommand(GetPlayerId(), command, argument);
		if (GetGame().GetPlayerController() == this)
			GC_RpcDoRouletteReply(response);
		else
			Rpc(GC_RpcDoRouletteReply, response);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void GC_RpcDoRouletteReply(string response)
	{
		if (GetGame().GetPlayerController() != this || !GC_RouletteSetupComponent.GetActive())
			return;
		SCR_ChatComponent chat = SCR_ChatComponent.Cast(FindComponent(SCR_ChatComponent));
		if (chat)
			chat.ShowMessage(response);
	}
}

//! Client registration exists only on the active Roulette world and is removed on exit.
class GC_RouletteChatCommands
{
	protected SCR_ChatPanelManager m_Chat;

	bool Attach()
	{
		m_Chat = SCR_ChatPanelManager.GetInstance();
		if (!m_Chat)
			return false;
		m_Chat.GetCommandInvoker("seed").Insert(OnSeed);
		m_Chat.GetCommandInvoker("reroll").Insert(OnReroll);
		m_Chat.GetCommandInvoker("players").Insert(OnPlayers);
		m_Chat.GetCommandInvoker("scenario").Insert(OnScenario);
		m_Chat.GetCommandInvoker("teams").Insert(OnTeams);
		return true;
	}

	void Detach()
	{
		if (!m_Chat)
			return;
		m_Chat.GetCommandInvoker("seed").Remove(OnSeed);
		m_Chat.GetCommandInvoker("reroll").Remove(OnReroll);
		m_Chat.GetCommandInvoker("players").Remove(OnPlayers);
		m_Chat.GetCommandInvoker("scenario").Remove(OnScenario);
		m_Chat.GetCommandInvoker("teams").Remove(OnTeams);
		m_Chat = null;
	}

	protected void OnSeed(SCR_ChatPanel panel, string text) { Send("seed", text); }
	protected void OnReroll(SCR_ChatPanel panel, string text) { Send("reroll", text); }
	protected void OnPlayers(SCR_ChatPanel panel, string text) { Send("players", text); }
	protected void OnScenario(SCR_ChatPanel panel, string text) { Send("scenario", text); }
	protected void OnTeams(SCR_ChatPanel panel, string text) { Send("teams", text); }

	protected void Send(string command, string text)
	{
		GC_LobbyPlayerController controller = GC_LobbyPlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
			controller.GC_SendRouletteCommand(command, text);
	}
}
