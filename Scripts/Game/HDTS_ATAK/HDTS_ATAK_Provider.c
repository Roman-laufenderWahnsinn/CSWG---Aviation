//------------------------------------------------------------------------------------------------
//! Bridge between "Kova x ATAK" (GAZ_MarkerManagerComponent and GAZ_WaypointController, the data
//! the ATAK map and the "Kova x ATAK Minimap" draw) and the HDTS helmet display.
//!
//! Every ATAK marker the pilot's faction can see becomes a box on the HMD, coloured by its
//! affiliation (hostile red, friendly blue, neutral green, unknown yellow) and labelled with the
//! marker text or, when there is none, its symbol name. Every waypoint, own or shared by a
//! faction member, becomes a waypoint box in its ATAK colour; the newest own waypoint is the
//! active one and drives the nav line (bearing, range, time to go).
//!
//! Read-only: nothing is sent back to the ATAK, no server component is involved.
//------------------------------------------------------------------------------------------------
class HDTS_ATAK_Provider : HMD_TargetProvider
{
	protected static const string SOURCE_MARKER = "MRK";
	protected static const string SOURCE_WAYPOINT = "WPT";
	//! Markers further away than this are not reported at all (the display declutters nearer still)
	protected static const float MARKER_MAX_RANGE_M = 15000;
	//! Diagnostics: one summary line every five seconds while the HMD asks for targets
	protected static int s_iNextLogMs;

	//------------------------------------------------------------------------------------------------
	override void GetTargets(notnull IEntity vehicle, notnull array<ref HMD_Target> outTargets)
	{
		BaseWorld world = vehicle.GetWorld();
		if (!world)
			return;

		vector ownPos = vehicle.GetOrigin();
		int before = outTargets.Count();
		AddWaypoints(world, ownPos, outTargets);
		int waypoints = outTargets.Count() - before;
		AddMarkers(world, ownPos, outTargets);
		int markers = outTargets.Count() - before - waypoints;

		int now = System.GetTickCount();
		if (now >= s_iNextLogMs)
		{
			s_iNextLogMs = now + 5000;
			GAZ_MarkerManagerComponent manager = GAZ_MarkerManagerComponent.GetInstance();
			int hasManager = 0;
			if (manager)
				hasManager = 1;
			int total = -1;
			if (manager && manager.GetMarkers())
				total = manager.GetMarkers().Count();
			Print(string.Format("[HDTS_ATAK] manager=%1 markersTotal=%2 markersReported=%3 waypointsTotal=%4 waypointsReported=%5 own=%6", hasManager, total, markers, GAZ_WaypointController.Count(), waypoints, ownPos.ToString()), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Own waypoints ("WP3") and the ones faction members shared ("WP2 Razgriz")
	protected void AddWaypoints(notnull BaseWorld world, vector ownPos, notnull array<ref HMD_Target> outTargets)
	{
		int count = GAZ_WaypointController.Count();
		if (count == 0)
			return;

		GAZ_WaypointEntry active = GAZ_WaypointController.GetNewestOwn();
		PlayerManager players = GetGame().GetPlayerManager();
		for (int i = 0; i < count; i++)
		{
			GAZ_WaypointEntry entry = GAZ_WaypointController.Get(i);
			if (!entry)
				continue;

			HMD_Target target = new HMD_Target();
			target.m_eKind = HMD_ETargetKind.WAYPOINT;
			target.m_vPosition = Vector(entry.m_fWorldX, world.GetSurfaceY(entry.m_fWorldX, entry.m_fWorldZ), entry.m_fWorldZ);
			target.m_sSource = SOURCE_WAYPOINT;
			target.m_Color = GAZ_PlayerMarkerPalette.Get(entry.m_iColorIdx);
			target.m_bPrimary = (entry == active);

			if (entry.m_bIsRemote)
			{
				target.m_sLabel = "WP" + entry.m_iOwnerWpId.ToString();
				string owner;
				if (players)
					owner = players.GetPlayerName(entry.m_iOwnerPid);
				if (!owner.IsEmpty())
					target.m_sLabel = target.m_sLabel + " " + owner;
			}
			else
			{
				target.m_sLabel = "WP" + entry.m_iId.ToString();
			}

			if (entry.m_bArrived)
				target.m_sLabel = target.m_sLabel + " ARR";

			outTargets.Insert(target);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! User-placed ATAK markers. The server only sends a faction its own markers, so the local
	//! list is already what this pilot may see.
	protected void AddMarkers(notnull BaseWorld world, vector ownPos, notnull array<ref HMD_Target> outTargets)
	{
		GAZ_MarkerManagerComponent manager = GAZ_MarkerManagerComponent.GetInstance();
		if (!manager)
			return;

		array<ref GAZ_MarkerData> markers = manager.GetMarkers();
		if (!markers)
			return;

		float maxRangeSq = MARKER_MAX_RANGE_M * MARKER_MAX_RANGE_M;
		foreach (GAZ_MarkerData marker : markers)
		{
			// Negative ids are local placeholders the server has not confirmed yet
			if (!marker || marker.m_iId < 0)
				continue;

			float dx = marker.m_fWorldX - ownPos[0];
			float dz = marker.m_fWorldZ - ownPos[2];
			if (dx * dx + dz * dz > maxRangeSq)
				continue;

			GAZ_Sidc sidc = marker.ToSidc();
			string label = marker.m_sLabel;
			if (label.IsEmpty())
				label = sidc.Label();

			HMD_Target target = new HMD_Target();
			target.m_eKind = HMD_ETargetKind.MARKER;
			target.m_vPosition = Vector(marker.m_fWorldX, world.GetSurfaceY(marker.m_fWorldX, marker.m_fWorldZ), marker.m_fWorldZ);
			target.m_sSource = SOURCE_MARKER;
			label.ToUpper();
			target.m_sLabel = label;
			target.m_Color = GAZ_Sidc.AffiliationColor(sidc.affiliation);
			outTargets.Insert(target);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Registers the provider once the game is up (the player controller exists on every machine
//! that has a player, which is exactly where the HMD is drawn)
modded class SCR_PlayerController
{
	override protected void OnInit(IEntity owner)
	{
		super.OnInit(owner);
		HMD_TargetRegistry.Register(new HDTS_ATAK_Provider());
	}
}
