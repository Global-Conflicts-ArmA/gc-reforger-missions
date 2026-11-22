//! State used by the published RHIB splash controller after Reforger removed VehicleDust.
//! RHIB still owns particle creation, movement, and cleanup.
class VehicleDust
{
	GameMaterial m_Material;
	ParticleEffectEntity m_pParticleEffectEntity;
	vector m_vLocalDustPos;
	int m_iLastSwap;
	bool m_bWheelHasContact;
}
