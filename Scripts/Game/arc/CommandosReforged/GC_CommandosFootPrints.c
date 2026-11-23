[ComponentEditorProps(category: "GC Missions/Commandos", description: "Produces local tracks for the dog role.")]
class GC_CommandosFootPrintsClass : ScriptComponentClass
{
}

class GC_CommandosFootPrints : ScriptComponent
{
	[Attribute(".75 4", UIWidgets.Range, params: "0 9999")]
	protected vector m_timeRange;

	protected ref array<Decal> m_aDecals = {};
	protected int m_iColor;
	protected bool m_bLeft;
	protected bool m_bActive;
	protected static ref array<GC_CommandosFootPrints> s_aTargets = {};
	protected static const ResourceName LEFT_FOOT = "{6475257711637717}worlds/arc/CommandosReforged/Assets/Bootprint_Left.emat";
	protected static const ResourceName RIGHT_FOOT = "{A86F832D6E121DB3}worlds/arc/CommandosReforged/Assets/Bootprint_Right.emat";

	//------------------------------------------------------------------------------------------------
	static void SetLocalTracking(bool active)
	{
		foreach (GC_CommandosFootPrints target : s_aTargets)
			target.SetTracking(active);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetTracking(bool active)
	{
		if (m_bActive == active)
			return;
		m_bActive = active;
		if (active)
		{
			Color color = new Color(Math.RandomFloat01(), Math.RandomFloat01(), Math.RandomFloat01(), 1);
			m_iColor = color.PackToInt();
			LeaveTrack();
			return;
		}
		GetGame().GetCallqueue().Remove(LeaveTrack);
		World world = GetOwner().GetWorld();
		foreach (Decal decal : m_aDecals)
			world.RemoveDecal(decal);
		m_aDecals.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected void LeaveTrack()
	{
		GC_CommandosDogManager dogManager = GC_CommandosDogManager.GetLocal();
		if (!dogManager || !dogManager.IsDogControlled())
		{
			SetTracking(false);
			return;
		}
		int delay = Math.RandomInt(m_timeRange[0], m_timeRange[1]) * 1000;
		GetGame().GetCallqueue().CallLater(LeaveTrack, delay);

		IEntity dog = GetGame().GetPlayerController().GetControlledEntity();
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetOwner());
		if (!dog || !character || character.IsInVehicle()
			|| vector.DistanceSqXZ(character.GetOrigin(), dog.GetOrigin()) > 250000)
			return;
		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller || controller.GetMovementSpeed() <= 0)
			return;
		CreateTrack(character);
	}

	//------------------------------------------------------------------------------------------------
	protected void CreateTrack(SCR_ChimeraCharacter character)
	{
		TraceParam trace = new TraceParam();
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.Start = character.GetOrigin() + "0 1 0";
		trace.End = trace.Start - "0 5 0";
		trace.Exclude = character;
		World world = character.GetWorld();
		if (world.TraceMove(trace, null) == 1 || !trace.TraceEnt)
			return;

		vector bone[4];
		TNodeId camera = character.GetAnimation().GetBoneIndex("Camera");
		character.GetAnimation().GetBoneMatrix(camera, bone);
		vector position = character.CoordToParent(bone[3]);
		position[1] = character.GetOrigin()[1] + 0.5;
		ResourceName material = RIGHT_FOOT;
		if (m_bLeft)
			material = LEFT_FOOT;
		Decal decal = world.CreateDecal(trace.TraceEnt, position, -trace.TraceNorm, 0, 3,
			TrackAngle(character.GetAngles()[1]), 0.35, 1, material, -1, m_iColor);
		if (decal)
			m_aDecals.Insert(decal);
		m_bLeft = !m_bLeft;
	}

	//------------------------------------------------------------------------------------------------
	protected float TrackAngle(float angle)
	{
		if (angle > 150)
			return 405;
		if (angle > 100)
			return 90;
		if (angle > 55)
			return 45;
		if (angle >= 10)
			return 0;
		if (angle < -170)
			return 135;
		if (angle < -120)
			return 180;
		if (angle < -90)
			return 225;
		return 540;
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (RplSession.Mode() == RplMode.Dedicated || SCR_Global.IsEditMode())
			return;
		s_aTargets.Insert(this);
		GC_CommandosDogManager manager = GC_CommandosDogManager.GetLocal();
		if (manager && manager.IsDogControlled())
			SetTracking(true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		SetTracking(false);
		s_aTargets.RemoveItem(this);
		super.OnDelete(owner);
	}
}
