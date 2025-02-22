[ComponentEditorProps(category: "GC/Component", description: "I protect")]
class PK_PrefabDeleterComponentClass : ScriptComponentClass
{
}

class PK_PrefabDeleterComponent : ScriptComponent
{
	[Attribute("1", UIWidgets.Auto, "Keep Entity after Prefab Deleter", category: "Prefab Deleter")]
	protected bool m_keepEntityAfterDelete;
}