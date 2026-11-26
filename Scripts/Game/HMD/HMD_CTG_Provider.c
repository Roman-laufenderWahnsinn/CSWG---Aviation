/*
Helment Mounted Display Mod for the Combined Special Warfare Group
Autohor: Roman Alexander "Trojan" Löwe
This is on adjusted version form the original "HDTS_CTGCompat" Mod on stem workshop:
https://reforger.armaplatform.com/workshop/73A1C04D5E2B9F18-HDTS_CTGCompat

*/
class HMD_CTG_Provider : HMD_TargetProvider{
	protected static const string SOURCE = "CTG";
	//! Graphics further away than this are not reported at all (the display declutters nearer still)
	protected static const float MAX_RANGE_M = 15000;
	//! A line or route contributes at most this many vertices
	protected static const int MAX_LINE_VERTICES = 8;
	//! Status poll interval; the open map polls at a similar rate
	protected static const int POLL_MS = 5000;

	protected bool m_bSyncRequested;
	protected int m_iNextPollMs;
	protected static int s_iNextLogMs;

	protected static ref Color s_ColorBlufor = Color.FromInt(0xFF237EBE);
	protected static ref Color s_ColorOpfor = Color.FromInt(0xFFC73838);
	protected static ref Color s_ColorIndfor = Color.FromInt(0xFF41914B);
	protected static ref Color s_ColorCivilian = Color.FromInt(0xFF9B46B4);
	protected static ref Color s_ColorUnknown = Color.FromInt(0xFFDEB300);

	//------------------------------------------------------------------------------------------------
	override void GetTargets(notnull IEntity vehicle, notnull array<ref HMD_Target> outTargets){
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		BaseWorld world = vehicle.GetWorld();
		if (!controller || !world)
			return;

		// Keep the client copy alive the way the open map does: one sync request, then status polls
		int now = System.GetTickCount();
		string status;
		if (now >= m_iNextPollMs){
			m_iNextPollMs = now + POLL_MS;
			if (!m_bSyncRequested){
				m_bSyncRequested = true;
				controller.CTG_RequestSync();
			}else{
				status = controller.CTG_GetStatus();
			}
		}

		array<ref CTG_GraphicRecord> records = controller.CTG_GetRecords();
		if (!records)
			return;

		vector ownPos = vehicle.GetOrigin();
		int before = outTargets.Count();
		foreach (CTG_GraphicRecord record : records){
			if (!record || !record.Body)
				continue;

			AddGraphic(world, ownPos, record.Body, outTargets);
		}

		if (now >= s_iNextLogMs){
			s_iNextLogMs = now + POLL_MS;
			Print(string.Format("[CSWG_HMD_CTG] records=%1 reported=%2 status=%3", records.Count(), outTargets.Count() - before, status), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void AddGraphic(notnull BaseWorld world, vector ownPos, notnull CTG_GraphicBody body, notnull array<ref HMD_Target> outTargets)	{
		if (!body.Points || body.Points.IsEmpty())
			return;

		string label = LabelFor(body);
		Color color = ColorFor(body);

		// Lines and routes: every vertex, numbered along the line
		if (IsLineLike(body.Kind) && body.Points.Count() > 1){
			int count = Math.Min(body.Points.Count(), MAX_LINE_VERTICES);
			for (int i = 0; i < count; i++)	{
				AddPoint(world, ownPos, body.Points[i], label + " " + (i + 1).ToString(), color, outTargets);
			}
			return;
		}

		// Areas and multi-point targets: the centre
		if (body.Points.Count() > 1 && (CTG_GraphicBody.IsAreaKind(body.Kind) || CTG_GraphicBody.IsTwoPointKind(body.Kind) || CTG_GraphicBody.IsThreePointKind(body.Kind))){
			vector centre = vector.Zero;
			foreach (vector point : body.Points){
				centre = centre + point;
			}
			centre = centre / body.Points.Count();
			AddPoint(world, ownPos, centre, label, color, outTargets);
			return;
		}

		// Everything else (symbols, points, task glyphs) sits on its first point
		AddPoint(world, ownPos, body.Points[0], label, color, outTargets);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddPoint(notnull BaseWorld world, vector ownPos, vector point, string label, Color color, notnull array<ref HMD_Target> outTargets){
		float dx = point[0] - ownPos[0];
		float dz = point[2] - ownPos[2];
		if (dx * dx + dz * dz > MAX_RANGE_M * MAX_RANGE_M)
			return;

		HMD_Target target = new HMD_Target();
		target.targetKind = HMD_ETargetKind.MARKER;
		target.vectorPosition = Vector(point[0], world.GetSurfaceY(point[0], point[2]), point[2]);
		target.targetSource = SOURCE;
		target.labelName = label;
		target.color = color;
		outTargets.Insert(target);
	}

	//------------------------------------------------------------------------------------------------
	//! Open lines of the mod plus the generic line, route, boundary and attack graphics
	protected static bool IsLineLike(CTG_GraphicKind kind){
		if (CTG_GraphicBody.IsOpenLineKind(kind))
			return true;

		return kind == CTG_GraphicKind.LINE || kind == CTG_GraphicKind.ROUTE || kind == CTG_GraphicKind.BOUNDARY
			|| kind == CTG_GraphicKind.AVENUE_OF_APPROACH || kind == CTG_GraphicKind.MAIN_ATTACK || kind == CTG_GraphicKind.SUPPORTING_ATTACK;
	}

	//------------------------------------------------------------------------------------------------
	//! Unit symbols: affiliation colour as on the map; other graphics: their stroke colour
	protected static Color ColorFor(notnull CTG_GraphicBody body){
		if (body.Kind != CTG_GraphicKind.SYMBOL)
			return Color.FromInt(body.StrokeColor);

		int affiliation = body.Affiliation;
		if (affiliation == EMilitarySymbolIdentity.BLUFOR || affiliation == EMilitarySymbolIdentity.ASSUMED_BLUFOR)
			return s_ColorBlufor;
		if (affiliation == EMilitarySymbolIdentity.OPFOR || affiliation == EMilitarySymbolIdentity.ASSUMED_OPFOR)
			return s_ColorOpfor;
		if (affiliation == EMilitarySymbolIdentity.INDFOR || affiliation == EMilitarySymbolIdentity.ASSUMED_INDFOR)
			return s_ColorIndfor;
		if (affiliation == EMilitarySymbolIdentity.CIVILIAN || affiliation == EMilitarySymbolIdentity.ASSUMED_CIVILIAN)
			return s_ColorCivilian;
		return s_ColorUnknown;
	}

	//------------------------------------------------------------------------------------------------
	//! "LZ FALCON", "TRP 2", "1/A INFANTRY": the map prefix of the graphic plus its label
	protected static string LabelFor(notnull CTG_GraphicBody body){
		string label;
		if (body.Kind == CTG_GraphicKind.SYMBOL){
			label = body.Label;
			if (label.IsEmpty())
				label = body.Designation;
			if (label.IsEmpty()){
				CTG_CatalogEntry entry = CTG_Catalog.Find(body.SymbolKey);
				if (entry)
					label = entry.Name;
				else
					label = body.SymbolKey;
			}
		}else{
			string prefix = PrefixFor(body.Kind);
			label = body.Label;
			if (label.IsEmpty())
				label = prefix;
			else if (!prefix.IsEmpty())
				label = prefix + " " + label;
		}

		label.ToUpper();
		return label;
	}

	//------------------------------------------------------------------------------------------------
	//! Same abbreviations the map uses for its graphic labels
	protected static string PrefixFor(CTG_GraphicKind kind){
		switch (kind){
			case CTG_GraphicKind.LINE: return "LINE";
			case CTG_GraphicKind.ROUTE: return "RTE";
			case CTG_GraphicKind.ARROW: return "";
			case CTG_GraphicKind.BOUNDARY: return "BDRY";
			case CTG_GraphicKind.AREA: return "AREA";
			case CTG_GraphicKind.PHASE_LINE: return "PL";
			case CTG_GraphicKind.AXIS_OF_ADVANCE: return "AXIS";
			case CTG_GraphicKind.AVENUE_OF_APPROACH: return "AA";
			case CTG_GraphicKind.MAIN_ATTACK: return "MAIN ATK";
			case CTG_GraphicKind.SUPPORTING_ATTACK: return "SPT ATK";
			case CTG_GraphicKind.OBJECTIVE_AREA: return "OBJ";
			case CTG_GraphicKind.ASSEMBLY_AREA: return "AA";
			case CTG_GraphicKind.ENGAGEMENT_AREA: return "EA";
			case CTG_GraphicKind.MINEFIELD: return "MINES";
			case CTG_GraphicKind.OBSTACLE_BELT: return "OBST BELT";
			case CTG_GraphicKind.ROADBLOCK: return "RDBLK";
			case CTG_GraphicKind.WIRE: return "WIRE";
			case CTG_GraphicKind.GENERIC_OBSTACLE: return "OBST";
			case CTG_GraphicKind.BREACH_POINT: return "BP";
			case CTG_GraphicKind.BREACH_LANE: return "LANE";
			case CTG_GraphicKind.BYPASS: return "BYPASS";
			case CTG_GraphicKind.CLEAR: return "CLEAR";
			case CTG_GraphicKind.SECURE: return "SECURE";
			case CTG_GraphicKind.REDUCE: return "REDUCE";
			case CTG_GraphicKind.NAMED_AREA_OF_INTEREST: return "NAI";
			case CTG_GraphicKind.TARGET_AREA_OF_INTEREST: return "TAI";
			case CTG_GraphicKind.BATTLE_POSITION: return "BP";
			case CTG_GraphicKind.SUPPORT_BY_FIRE_POSITION: return "SBF";
			case CTG_GraphicKind.ATTACK_BY_FIRE_POSITION: return "ABF";
			case CTG_GraphicKind.CHECKPOINT: return "CP";
			case CTG_GraphicKind.PASSAGE_POINT: return "PP";
			case CTG_GraphicKind.RELEASE_POINT: return "RP";
			case CTG_GraphicKind.TARGET_REFERENCE_POINT: return "TRP";
			case CTG_GraphicKind.POINT_TARGET: return "TGT";
			case CTG_GraphicKind.LINEAR_TARGET: return "TGT";
			case CTG_GraphicKind.RECTANGULAR_TARGET: return "TGT";
			case CTG_GraphicKind.CIRCULAR_TARGET: return "TGT";
			case CTG_GraphicKind.NO_FIRE_AREA: return "NFA";
			case CTG_GraphicKind.RESTRICTIVE_FIRE_AREA: return "RFA";
			case CTG_GraphicKind.RESTRICTIVE_FIRE_LINE: return "RFL";
			case CTG_GraphicKind.AIR_CONTROL_POINT: return "ACP";
			case CTG_GraphicKind.AIR_CORRIDOR: return "AC";
			case CTG_GraphicKind.RESTRICTED_OPERATIONS_ZONE: return "ROZ";
			case CTG_GraphicKind.AIRSPACE_COORDINATION_AREA: return "ACA";
			case CTG_GraphicKind.LANDING_ZONE: return "LZ";
			case CTG_GraphicKind.PICKUP_ZONE: return "PZ";
			case CTG_GraphicKind.FARP: return "FARP";
			case CTG_GraphicKind.LINE_OF_DEPARTURE: return "LD";
			case CTG_GraphicKind.LIMIT_OF_ADVANCE: return "LOA";
			case CTG_GraphicKind.LINE_OF_CONTACT: return "LC";
			case CTG_GraphicKind.FORWARD_LINE_OF_OWN_TROOPS: return "FLOT";
			case CTG_GraphicKind.ATTACK_POSITION: return "ATK POS";
			case CTG_GraphicKind.ASSAULT_POSITION: return "ASLT POS";
			case CTG_GraphicKind.AREA_OF_OPERATIONS: return "AO";
			case CTG_GraphicKind.COORDINATION_POINT: return "COORD";
			case CTG_GraphicKind.CONTACT_POINT: return "CT";
			case CTG_GraphicKind.LINKUP_POINT: return "LU";
			case CTG_GraphicKind.RALLY_POINT: return "RLY";
			case CTG_GraphicKind.OBSERVATION_POST: return "OP";
			case CTG_GraphicKind.FORWARD_OBSERVER: return "FO";
			case CTG_GraphicKind.IRREGULAR_TARGET: return "TGT";
			case CTG_GraphicKind.LINEAR_SMOKE_TARGET: return "SMK";
			case CTG_GraphicKind.FINAL_PROTECTIVE_FIRE: return "FPF";
			case CTG_GraphicKind.FREE_FIRE_AREA: return "FFA";
			case CTG_GraphicKind.POSITION_AREA_FOR_ARTILLERY: return "PAA";
			case CTG_GraphicKind.COORDINATED_FIRE_LINE: return "CFL";
			case CTG_GraphicKind.FIRE_SUPPORT_COORDINATION_LINE: return "FSCL";
			case CTG_GraphicKind.LOW_LEVEL_TRANSIT_ROUTE: return "LLTR";
			case CTG_GraphicKind.MINIMUM_RISK_ROUTE: return "MRR";
			case CTG_GraphicKind.STANDARD_USE_ARMY_AIRCRAFT_FLIGHT_ROUTE: return "SAAFR";
			case CTG_GraphicKind.UAS_ROUTE: return "UAS";
			case CTG_GraphicKind.OBSTACLE_ZONE: return "OBST ZONE";
			case CTG_GraphicKind.ANTI_TANK_DITCH: return "AT DITCH";
			case CTG_GraphicKind.ANTI_TANK_WALL: return "AT WALL";
			case CTG_GraphicKind.FORTIFIED_TRENCH_LINE: return "TRENCH";
			case CTG_GraphicKind.CASUALTY_COLLECTION_POINT: return "CCP";
			case CTG_GraphicKind.MEDEVAC_PICKUP_POINT: return "MEDEVAC";
			case CTG_GraphicKind.LOGISTICS_RELEASE_POINT: return "LRP";
			case CTG_GraphicKind.MAINTENANCE_COLLECTION_POINT: return "MCP";
			case CTG_GraphicKind.REARM_REFUEL_RESUPPLY_POINT: return "R3P";
			case CTG_GraphicKind.REFUEL_ON_THE_MOVE_POINT: return "ROM";
		}

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! Registers the provider once the game is up (the player controller exists on every machine
//! that has a player, which is exactly where the HMD is drawn)
modded class SCR_PlayerController{
	override protected void OnInit(IEntity owner){
		super.OnInit(owner);
		HMD_TargetRegistry.Register(new HMD_CTG_Provider());
	}
}
