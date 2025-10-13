[ComponentEditorProps(category: "GameScripted/GameMode/Components", description: "")]
class GC_CommandosFootPrintsClass : ScriptComponentClass
{
}

class GC_CommandosFootPrints : ScriptComponent
{
	protected static const ResourceName m_leftFootprint = "{6475257711637717}worlds/arc/CommandosReforged/Assets/Bootprint_Left.emat";
	protected static const ResourceName m_rightFootprint = "{A86F832D6E121DB3}worlds/arc/CommandosReforged/Assets/Bootprint_Right.emat";

	protected ref array<ref GC_CommandosDecal> decals = {};
	
	[Attribute(defvalue: ".75 4", uiwidget: UIWidgets.Range, desc: "X/Y Time in seconds to randomly place a foot print. Ignore Z", params: "0 9999")]
	protected vector m_timeRange;	
	
	protected int m_color;
	protected int m_maxDistance = Math.Pow(500, 2);
	protected int m_lifeTime = 600;
	protected bool m_left = false;
	protected bool m_isActive = false;
	
	override protected void OnPostInit(IEntity owner)
	{
		if(RplSession.Mode() == RplMode.Dedicated)
			return;
		
		GC_CommandosDogManager dogManager = GC_CommandosDogManager.Instance();
		if(dogManager)
			dogManager.Register(this);
	}

	void SetActive()
	{
		if(m_isActive)
			return;
		
		m_color = GetRandomColor();
		
		Loop();
		m_isActive = true;
	}
	
	void SetDeactive()
	{
		World world = GetGame().GetWorld();
		foreach(GC_CommandosDecal decal : decals)
		{
			world.RemoveDecal(decal.decal);
		}
		
		GetGame().GetCallqueue().Remove(Loop);
		decals.Clear();
		m_isActive = false;
	}
	
	protected void Loop()
	{
		int time = Math.RandomInt(m_timeRange[0], m_timeRange[1]) * 1000;
		GetGame().GetCallqueue().CallLater(Loop, time);
		
		IEntity player = GetGame().GetPlayerController().GetControlledEntity();
		float distance = vector.DistanceSqXZ(GetOwner().GetOrigin(), player.GetOrigin());
		
		if(distance > m_maxDistance)
			return;
		
		CreateFootPrint();
		//CleanUp();
	}
	
	protected void CleanUp()
	{
		Print("GRAY.CleanUp");
		World world = GetGame().GetWorld();
		float currentTime = GetGame().GetWorld().GetWorldTime();
		currentTime -= m_lifeTime * 1000;

		array<GC_CommandosDecal> decalsToRemove = {};
		foreach(GC_CommandosDecal decal : decals)
		{
			if(decal.timeSpawned > currentTime)
				break;

			world.RemoveDecal(decal.decal);
			decalsToRemove.Insert(decal);
		}
		
		foreach(GC_CommandosDecal decal : decalsToRemove)
		{
			decals.RemoveItem(decal);
		}
	}
	
	protected void CreateFootPrint()
	{	
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetOwner());
		if(!character || character.IsInVehicle())
			return;

		CharacterControllerComponent controller = character.GetCharacterController();
		if(controller.GetMovementSpeed() <= 0)
			return;
		
		TraceParam trace;
		if(!TracePosition(trace))
			return;
		
		if(m_left)
			CreateDecal(trace, m_leftFootprint, m_color);
		else
			CreateDecal(trace, m_rightFootprint, m_color);
		
		m_left = !m_left;
	}
	
	protected bool TracePosition(out TraceParam trace)
	{
		float distance = 5;
		vector direction = Vector(0, -1, 0);
		IEntity entity = GetOwner();
		vector origin = entity.GetOrigin();
		origin[1] = origin[1] + 1;
		
		TraceParam param = new TraceParam();
		param.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		param.Start = origin;
		param.End = origin + (direction * distance);
		param.Exclude = entity;
		
		float traceResult = GetGame().GetWorld().TraceMove(param, NULL);
		trace = param;

		if(traceResult == 1.0)
			return false;
		
		return true;
	}	
	
	protected void CreateDecal(TraceParam trace, ResourceName mat, int color = Color.WHITE)
	{
		Print("GRAY.Foot CreateDecal = " + this);
		
		if(!trace.TraceEnt)
			return;
		
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetOwner());
		if(!character)
			return;

		vector entityWorldTransform[4];
		vector boneTransform[4];
		TNodeId boneNodeId;
		boneNodeId = character.GetAnimation().GetBoneIndex("Camera");
		character.GetWorldTransform(entityWorldTransform);
		character.GetAnimation().GetBoneMatrix(boneNodeId, boneTransform);
		
		vector spawnPosition = character.CoordToParent(boneTransform[3]);
		spawnPosition = Vector(spawnPosition[0],character.GetOrigin()[1]+0.5,spawnPosition[2]);
		
		float angle = GetAngle(character.GetAngles()[1]);
		
		World world = GetGame().GetWorld();
		Decal decal = world.CreateDecal(
			trace.TraceEnt, // Entity 
			spawnPosition, // origin vector (position) 
			-trace.TraceNorm, // project vector 
			0.0, // nearclip
			3, // farclip
			angle, // angle 
			0.35, // size 
			1, // stretch 
			mat, //emat path
			-1,// lifetime, if <= 0 the decal is created as static
			color); //color of decal
		
		decals.Insert(GC_CommandosDecal(decal));
	}
	
	protected float GetAngle(float angle)
	{
		if (angle > 150)
		{
			return 405;
		}
		else if (angle > 100)
		{
			return 90;
		}
		else if (angle > 55)
		{
			return 45;
		}
		else if (angle > 10)
		{
			return 0;
		}
		else if (angle < -170)
		{
			return 135;
		}
		else if (angle < -120)
		{
			return 180;
		}
		else if (angle < -90)
		{
			return 225;
		}
		else if (angle < 10)
		{
			return 540;
		}
		
		return 0;
	}
	
	protected int GetRandomColor()
	{
		Color color = new Color(
			Math.RandomFloat(0, 1),
			Math.RandomFloat(0, 1),
			Math.RandomFloat(0, 1),
			1.0
		);
		
		return color.PackToInt();
	}
}

class GC_CommandosDecal
{
	Decal decal;
	float timeSpawned;
	
	void GC_CommandosDecal(Decal _decal)
	{
		decal = _decal;
		timeSpawned = GetGame().GetWorld().GetWorldTime();
	}
}