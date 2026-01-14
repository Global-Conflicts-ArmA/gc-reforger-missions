
//Roulette Commands

modded class SCR_PlayerController : PlayerController
{
	void GC_R_SendCommandServer(string command, string data)
	{
		Rpc(GC_R_RPC_SendCommandServer, command, data);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void GC_R_RPC_SendCommandServer(string command, string data)
	{
		GC_R_Manager manager = GC_R_Manager.GetInstance();
		if(!manager)
			return;

		manager.InvokeCommand(this, command, data);
	}
	
	void GC_R_SendChatMsg(string message)
	{
		Rpc(GC_R_RPC_SendChatMsg, message);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void GC_R_RPC_SendChatMsg(string message)
	{
		SCR_ChatComponent cc = SCR_ChatComponent.Cast(FindComponent(SCR_ChatComponent));
	    if (!cc)
	        return;
	    cc.ShowMessage(message);
	}
}
