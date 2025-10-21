//SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
//Print("X: " + mapEntity.GetMapSizeX());
//Print("Y: " + mapEntity.GetMapSizeY());

[ComponentEditorProps(category: "GameScripted/GameMode/Components/GC", description: "Roulette")]
class GC_RouletteManagerClass : GameEntityClass
{
}

class GC_RouletteManager : GameEntity
{
	void GC_RouletteManager(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.INIT);
	}

	override protected void EOnInit(IEntity owner)
	{
		Print("GC Roulette | EOnInit");
		Intialize();

		//GetGame().GetCallqueue().CallLater(Intialize, 10000, true);
	}
	
	void Intialize()
	{
		Print("GC Roulette | Map size: " + GetMapSize());
	}
	
	float GetMapSize()
	{
		BaseWorld world = GetGame().GetWorld();
		vector mins, max;
		GetGame().GetWorldEntity().GetWorldBounds(mins, max);
		float size = Math.Round(Math.Min(max[0] - mins[0], max[2] - mins[2]));

		while(size > 0)
		{
			size--;
			
			float y = world.GetSurfaceY(size,size);
			if(y != -256)
				break;
		}

		return Math.Max(0,size);
	}
}