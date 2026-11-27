/*
Helmet Mounted Display Mod for the Combined Special Warfare Group
Author: Roman Alexander "Trojan" Löwe
*/

class HMD_MapMarker_Provider : HMD_TargetProvider{
	protected static const string SOURCE = "MAP";
	protected static const float MAX_RANGE_M = 15000;
	protected static int s_iNextLogMs;
	
	protected ref array<ref HMD_Target> m_aCachedTargets = {};
	protected bool m_bHadMarkersBefore = false;

	//------------------------------------------------------------------------------------------------
	override void GetTargets(notnull IEntity vehicle, notnull array<ref HMD_Target> outTargets){
		BaseWorld world = vehicle.GetWorld();
		if (!world)
			return;

		SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
		if (!manager){
			BaseGameMode gameMode = GetGame().GetGameMode();
			if (gameMode)
				manager = SCR_MapMarkerManagerComponent.Cast(gameMode.FindComponent(SCR_MapMarkerManagerComponent));
		}

		if (!manager)
			return;

		SCR_MapMarkerConfig markerCfg = manager.GetMarkerConfig();
		int localFactionId = GetLocalFactionIndex();

		vector ownPos = vehicle.GetOrigin();
		float maxRangeSq = MAX_RANGE_M * MAX_RANGE_M;

		array<SCR_MapMarkerBase> staticMarkers = manager.GetStaticMarkers();
		if (staticMarkers && staticMarkers.Count() > 0){
			m_aCachedTargets.Clear();
			CollectMarkerList(world, markerCfg, staticMarkers, localFactionId, ownPos, maxRangeSq, m_aCachedTargets);
			m_bHadMarkersBefore = true;
		}else if (m_bHadMarkersBefore){
			// ATAK hat staticMarkers beim Schließen geleert -> Cache mit aktueller Distanz weiter nutzen
			foreach (HMD_Target cached : m_aCachedTargets){
				if (!cached)
					continue;
				float dx = cached.vectorPosition[0] - ownPos[0];
				float dz = cached.vectorPosition[2] - ownPos[2];
				if (dx * dx + dz * dz <= maxRangeSq)
					outTargets.Insert(cached);
			}
			return;
		}

		foreach (HMD_Target target : m_aCachedTargets){
			outTargets.Insert(target);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CollectMarkerList(notnull BaseWorld world, SCR_MapMarkerConfig markerCfg, notnull array<SCR_MapMarkerBase> markers, int localFactionId, vector ownPos, float maxRangeSq, notnull array<ref HMD_Target> outTargets){
		foreach (SCR_MapMarkerBase marker : markers){
			if (!marker)
				continue;

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

			// get img and color out of map marker config
			ResolveMarkerVisuals(markerCfg, marker, target);

			outTargets.Insert(target);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ResolveMarkerVisuals(SCR_MapMarkerConfig markerCfg, notnull SCR_MapMarkerBase marker, notnull HMD_Target target){
		if (!markerCfg)
			return;

		SCR_MapMarkerEntryConfig entryCfg = markerCfg.GetMarkerEntryConfigByType(marker.GetType());
		if (!entryCfg)
			return;

		// Spielergesetzte Marker (Imageset + Quad + Farbauswahl)
		SCR_MapMarkerEntryPlaced placedCfg = SCR_MapMarkerEntryPlaced.Cast(entryCfg);
		if (placedCfg){
			ResourceName imageset, imagesetGlow;
			string quadName;
			if (placedCfg.GetIconEntry(marker.GetIconEntry(), imageset, imagesetGlow, quadName)){
				target.iconTexture = imageset;
				target.iconQuad = quadName;
			}

			target.color = placedCfg.GetColorEntry(marker.GetColorEntry());
			target.tintIcon = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected int GetLocalFactionIndex(){
		Faction localFaction = SCR_FactionManager.SGetLocalPlayerFaction();
		if (!localFaction)
			return -1;

		FactionManager factionMgr = GetGame().GetFactionManager();
		if (!factionMgr)
			return -1;

		return factionMgr.GetFactionIndex(localFaction);
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController{
	override protected void OnInit(IEntity owner){
		super.OnInit(owner);
		HMD_TargetRegistry.Register(new HMD_MapMarker_Provider());
	}
}