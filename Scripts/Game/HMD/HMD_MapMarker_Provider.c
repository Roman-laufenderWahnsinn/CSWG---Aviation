/*
Helmet Mounted Display Mod for the Combined Special Warfare Group
Author: Roman Alexander "Trojan" Löwe
*/

class HMD_MapMarker_Provider : HMD_TargetProvider
{
	protected static const string SOURCE = "MAP";
	protected static const float MAX_RANGE_M = 15000;
	protected static int s_iNextLogMs;

	//------------------------------------------------------------------------------------------------
	override void GetTargets(notnull IEntity vehicle, notnull array<ref HMD_Target> outTargets)
	{
		BaseWorld world = vehicle.GetWorld();
		if (!world)
			return;

		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (!manager)
		{
			BaseGameMode gameMode = GetGame().GetGameMode();
			if (gameMode)
				manager = SCR_MapMarkerManagerComponent.Cast(gameMode.FindComponent(SCR_MapMarkerManagerComponent));
		}

		if (!manager)
			return;

		array<SCR_MapMarkerBase> markers = manager.GetStaticMarkers();
		if (!markers || markers.IsEmpty())
			return;

		vector ownPos = vehicle.GetOrigin();
		float maxRangeSq = MAX_RANGE_M * MAX_RANGE_M;
		int before = outTargets.Count();

		foreach (SCR_MapMarkerBase marker : markers)
		{
			if (!marker)
				continue;

			// Falls nur von Spielern gesetzte Marker gewünscht sind, Server-Marker überspringen:
			//if (marker.IsServerMarker())
				//continue;

			int pos[2];
			marker.GetWorldPos(pos);

			float worldX = pos[0];
			float worldZ = pos[1];

			float dx = worldX - ownPos[0];
			float dz = worldZ - ownPos[2];
			if (dx * dx + dz * dz > maxRangeSq)
				continue;

			string label = marker.GetCustomText();
			label.Trim();
			if (label.IsEmpty())
				label = "MRK";
			label.ToUpper();

			HMD_Target target = new HMD_Target();
			target.targetKind = HMD_ETargetKind.MARKER;
			target.vectorPosition = Vector(worldX, world.GetSurfaceY(worldX, worldZ), worldZ);
			target.targetSource = SOURCE;
			target.labelName = label;
			outTargets.Insert(target);
		}

		int now = System.GetTickCount();
		if (now >= s_iNextLogMs)
		{
			s_iNextLogMs = now + 5000;
			Print(string.Format("[CSWG_HMD_MAP] total=%1 reported=%2", markers.Count(), outTargets.Count() - before), LogLevel.NORMAL);
		}
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	override protected void OnInit(IEntity owner)
	{
		super.OnInit(owner);
		HMD_TargetRegistry.Register(new HMD_MapMarker_Provider());
	}
}