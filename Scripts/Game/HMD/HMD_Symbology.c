/*
Helment Mounted Display Mod for the Combined Special Warfare Group
Autohor: Roman Alexander "Trojan" Löwe
This is on adjusted version form the original "HDTS - Helmet Display" Mod on stem workshop:
https://reforger.armaplatform.com/workshop/7BABD785CD00B3F6-HDTS-HelmetDisplay

- Removed not needed parts for CSWG pilots
- Rearanged Layout
- Added FPV textrue (to replace the (0) symbology)
- Changed heading from degrees to compass direction
- Added fuel timer (remaing time on current fuel consumption)
*/


class HMD_Symbology : Managed
{
	static const ResourceName LAYOUT = "{61CE74373672265E}UI/Layouts/HUD/CSWG_HMD.layout";
	static const ResourceName FPV_TEXTURE = "{621A8E584823A7CA}UI/Textures/HUD/VelocityMarker-100_atlas.edds";

	//! Flip to -1 if the horizon line banks the wrong way on your installation
	static const float ROLL_SIGN = 1;

	static const float MPS_TO_KT = 1.943844;
	static const float M_TO_FT = 3.28084;
	static const float MPS_TO_FPM = 196.8504;
	static const float DEFAULT_VFOV_DEG = 74;

	static const float LADDER_LIMIT_DEG = 34;
	static const float HORIZON_HALF_WIDTH = 360;
	static const float FPV_LIMIT_DEG = 22;
	static const float SPEED_FLIGHT_MODE = 30;
	static const float BLINK_PERIOD = 0.8;

	static const float LOW_ROTOR_PCT = 95;
	static const float LOW_FUEL_PCT = 10;
	static const float SINK_RATE_FPM = -1500;
	static const float SINK_RATE_AGL_FT = 500;
	static const float ALTITUDE_WARN_AGL_FT = 50;
	static const float ALTITUDE_WARN_VSI_FPM = -200;
	static const float MARKER_RANGE_FULL = 10000;

	Widget wingetRoot;
	Widget windgetFrame;

	ImageWidget imgW_Horizon;

	ImageWidget imgW_FPV;

	TextWidget textW_Airspeed;
	TextWidget textW_AirspeedLabel;
	TextWidget textW_GroundSpeed;
	TextWidget textW_Altitude;
	TextWidget textW_AltitudeLabel;
	TextWidget textW_RadarAlt;
	TextWidget textW_VSI;
	TextWidget textW_Heading;
	TextWidget textW_HeadingTape;
	TextWidget textW_Rotor;
	TextWidget textW_Fuel;
	TextWidget textW_FuelTimer;
	TextWidget textW_GForce;
	TextWidget textW_Warning;

	// Target designation
	static const int TARGET_POOL = 12;
	static const float TARGET_DIAMOND_HALF = 16;
	static const float TARGET_BLINK_PERIOD = 1.0;
	ref array<ImageWidget> arrayImgW_TargetDiamonds = {};
	ref array<TextWidget> arrayTextW_TargetLabels = {};
	TextWidget textW_TargetLine;
	ref array<ref HDTS_Target> arrayHMD_Targets = {};
	float float_TargetBlinkClock;
	ref Color color_TargetMatched = new Color(0.35, 1, 0.45, 1);
	ref Color color_TargetOther = new Color(1, 0.72, 0.24, 1);
	static const float WAYPOINT_HALF = 11;
	static const float MARKER_HALF = 8;
	TextWidget textW_NavLine;
	ref array<ref HDTS_Target> arrayHMD_TargetsOrdered = {};
	ref Color color_Waypoint = new Color(1, 0.45, 1, 1);
	
	// Camera frame of the current frame, filled by UpdateWorldSymbology and reused for target projection
	bool cameraValid;
	vector vec_CamPos;
	vector vec_CamRight;
	vector vec_CamUp;
	vector vec_CamForward;
	float cameraFocal;
	float debug_TargetDebugClock;
	
	// Nav selection: key of the waypoint/marker the pilot picked with the cycle key, empty = provider default
	string navSelection;
	string navSelectionText = "NAV: AUTO";
	ref array<string> navKeysArray = {};

	float renderTargetWidth;
	float renderTargetHeigth;
	float screenCenterX;
	float screenCenterY;

	int brightessHMD = 10;
	float blinkClockCounter;
	bool layoutIsDirty = true;
	WorldTimestamp lastTimestampFuelUpdate;
	string optionMode = "";

	void HMD_Symbology(){
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;
		
		wingetRoot = workspace.CreateWidgets(LAYOUT);
		if (!wingetRoot){
			Print("[CSWG HMD] Could not create the HMD layout " + LAYOUT, LogLevel.WARNING);
			return;
		}
		
		CacheWidgets();
		ApplyBrightness();
		lastTimestampFuelUpdate = GetGame().GetWorld().GetTimestamp();
	}

	void Destroy(){
		if (wingetRoot)
			wingetRoot.RemoveFromHierarchy();

		wingetRoot = null;
	}

	bool IsValid(){
		return wingetRoot != null;
	}

	int GetBrightness(){
		return brightessHMD;
	}

	void SetBrightness(int level){
		brightessHMD = Math.ClampInt(level, 0, 100);
		ApplyBrightness();
	}

	void SetNavSelection(string key){
		navSelection = key;
		if (key.IsEmpty())
			navSelectionText = "NAV: AUTO";
	}

	string GetNavSelectionText(){
		return navSelectionText;
	}

	string CycleNav(){
		int count = navKeysArray.Count();
		if (count == 0){
			navSelection = string.Empty;
			navSelectionText = "NAV: AUTO (no waypoints or markers in view)";
			Print(navSelectionText);
			return navSelection;
		}

		int next = 0;
		if (!navSelection.IsEmpty()){
			int current = navKeysArray.Find(navSelection);
			if (current >= 0)
				next = current + 1;
		}

		if (next >= count){
			navSelection = string.Empty;
			navSelectionText = "NAV: AUTO";
			Print(navSelectionText);
			return navSelection;
		}

		navSelection = navKeysArray[next];
		navSelectionText = "NAV: " + navSelection;
		Print(navSelectionText);

		return navSelection;
	}

	static string NavKey(notnull HDTS_Target target){
		vector p = target.m_vPosition;
		return target.m_sSource + "|" + target.m_sLabel + "|" + Math.Round(p[0]).ToString() + "/" + Math.Round(p[2]).ToString();
	}

	void ApplyBrightness(){
		if (!wingetRoot)
			return;

		wingetRoot.SetOpacity(brightessHMD/10.0);
	}

	void CacheWidgets(){
		windgetFrame = wingetRoot.FindAnyWidget("Frame");
		imgW_Horizon = ImageWidget.Cast(wingetRoot.FindAnyWidget("Horizon"));
		imgW_FPV = ImageWidget.Cast(wingetRoot.FindAnyWidget("FPVimg"));
		textW_Airspeed = TextWidget.Cast(wingetRoot.FindAnyWidget("Airspeed"));
		textW_AirspeedLabel = TextWidget.Cast(wingetRoot.FindAnyWidget("AirspeedLabel"));
		textW_GroundSpeed = TextWidget.Cast(wingetRoot.FindAnyWidget("GroundSpeed"));
		textW_Altitude = TextWidget.Cast(wingetRoot.FindAnyWidget("Altitude"));
		textW_AltitudeLabel = TextWidget.Cast(wingetRoot.FindAnyWidget("AltitudeLabel"));
		textW_RadarAlt = TextWidget.Cast(wingetRoot.FindAnyWidget("RadarAlt"));
		textW_VSI = TextWidget.Cast(wingetRoot.FindAnyWidget("VSI"));
		textW_Heading = TextWidget.Cast(wingetRoot.FindAnyWidget("Heading"));
		textW_HeadingTape = TextWidget.Cast(wingetRoot.FindAnyWidget("HeadingTape"));
		textW_Rotor = TextWidget.Cast(wingetRoot.FindAnyWidget("Rotor"));
		textW_Fuel = TextWidget.Cast(wingetRoot.FindAnyWidget("Fuel"));
		textW_FuelTimer = TextWidget.Cast(wingetRoot.FindAnyWidget("FuelTimer"));
		textW_GForce = TextWidget.Cast(wingetRoot.FindAnyWidget("GForce"));
		textW_Warning = TextWidget.Cast(wingetRoot.FindAnyWidget("Warning"));
		textW_TargetLine = TextWidget.Cast(wingetRoot.FindAnyWidget("TargetLine"));
		textW_NavLine = TextWidget.Cast(wingetRoot.FindAnyWidget("NavLine"));
		
		arrayImgW_TargetDiamonds.Clear();
		arrayTextW_TargetLabels.Clear();
		for (int i = 0; i < TARGET_POOL; i++){
			ImageWidget diamond = ImageWidget.Cast(wingetRoot.FindAnyWidget("Target" + i));
			if (diamond)
				diamond.SetRotation(45);
			arrayImgW_TargetDiamonds.Insert(diamond);
			arrayTextW_TargetLabels.Insert(TextWidget.Cast(wingetRoot.FindAnyWidget("TargetLabel" + i)));
		}
		if(imgW_FPV)
			imgW_FPV.LoadImageTexture(0, FPV_TEXTURE); 
	}

	void UpdateScreenMetrics(){
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		float w = workspace.DPIUnscale(workspace.GetWidth());
		float h = workspace.DPIUnscale(workspace.GetHeight());
		if (w < 1 || h < 1)
			return;

		if (!layoutIsDirty && Math.AbsFloat(w - renderTargetWidth) < 0.5 && Math.AbsFloat(h - renderTargetHeigth) < 0.5)
			return;

		renderTargetWidth = w;
		renderTargetHeigth = h;
		screenCenterX = w * 0.5;
		screenCenterY = h * 0.5;
		layoutIsDirty = false;
		LayoutStatic();
	}

	void LayoutStatic(){
		if (windgetFrame){
			FrameSlot.SetPos(windgetFrame, 0, 0);
			FrameSlot.SetSize(windgetFrame, renderTargetWidth, renderTargetHeigth);
		}

		float left = screenCenterX - 330;
		float right = screenCenterX + 330;

		Place(imgW_FPV, screenCenterX, screenCenterY, 64, 64);

		Place(textW_Airspeed, left, screenCenterY - 10, 120, 40);
		Place(textW_AirspeedLabel, left, screenCenterY - 38, 120, 22);
		Place(textW_GroundSpeed, left, screenCenterY + 20, 120, 22);

		Place(textW_Altitude, right, screenCenterY - 10, 140, 40);
		Place(textW_AltitudeLabel, right, screenCenterY - 38, 140, 22);
		Place(textW_RadarAlt, right, screenCenterY + 20, 140, 26);
		Place(textW_VSI, right, screenCenterY + 48, 140, 22);

		Place(textW_Heading, screenCenterX, screenCenterY - 250, 110, 34);
		Place(textW_HeadingTape, screenCenterX, screenCenterY - 222, 620, 24);

		Place(textW_GForce, left, screenCenterY + 226, 140, 22);
		Place(textW_Rotor, left, screenCenterY + 252, 140, 26);
		
		Place(textW_FuelTimer, right, screenCenterY + 226, 160, 22);
		Place(textW_Fuel, right, screenCenterY + 252, 160, 26);

		Place(textW_Warning, screenCenterX, screenCenterY + 150, 360, 32);
		Place(textW_TargetLine, screenCenterX, screenCenterY - 190, 520, 24);
		Place(textW_NavLine, screenCenterX, screenCenterY - 166, 520, 22);

		for (int i = 0; i < arrayImgW_TargetDiamonds.Count(); i++){
			if (arrayImgW_TargetDiamonds[i])
				FrameSlot.SetSize(arrayImgW_TargetDiamonds[i], TARGET_DIAMOND_HALF * 2, TARGET_DIAMOND_HALF * 2);
			if (arrayTextW_TargetLabels[i])
				FrameSlot.SetSize(arrayTextW_TargetLabels[i], 260, 22);
		}

		if (imgW_Horizon)
			FrameSlot.SetSize(imgW_Horizon, HORIZON_HALF_WIDTH * 2, 2);
	}

	void Place(Widget w, float x, float y, float width, float height){
		if (!w)
			return;

		FrameSlot.SetSize(w, width, height);
		FrameSlot.SetPos(w, x - width * 0.5, y - height * 0.5);
	}

	void Show(Widget w, bool visible){
		if (w)
			w.SetVisible(visible);
	}

	void SetText(TextWidget w, string text){
		if (w)
			w.SetText(text);
	}

	void Update(notnull IEntity vehicle, notnull VehicleHelicopterSimulation sim, float timeSlice, HMD_FuelComputer fuelComputer){
		if (!wingetRoot)
			return;

		UpdateScreenMetrics();

		blinkClockCounter += timeSlice;
		if (blinkClockCounter > BLINK_PERIOD)
			blinkClockCounter -= BLINK_PERIOD;
		bool blinkOn = blinkClockCounter < BLINK_PERIOD * 0.5;

		vector velocity = sim.GetVelocity();
		vector velocityLocal = sim.GetVelocityLocal();
		float forwardKt = velocityLocal[2] * MPS_TO_KT;
		vector horizontal = velocity;
		horizontal[1] = 0;
		float groundKt = horizontal.Length() * MPS_TO_KT;
		float vsiFpm = velocity[1] * MPS_TO_FPM;

		vector origin = vehicle.GetOrigin();
		float baroFt = origin[1] * M_TO_FT;
		float aglMeters = sim.GetAltitudeAGL();
		if (aglMeters < 0){
			BaseWorld world = vehicle.GetWorld();
			if (world)
				aglMeters = origin[1] - world.GetSurfaceY(origin[0], origin[2]);
		}
		float aglFt = Math.Max(aglMeters, 0) * M_TO_FT;

		float heading = GetHeadingDeg(vehicle);

		float rotorPct = 0;
		if (sim.RotorCount() > 0){
			float target = sim.RotorGetRPMTarget(0);
			if (target > 1)
				rotorPct = sim.RotorGetRPM(0) / target * 100;
		}
		bool engineOn = sim.EngineIsOn();

		float fuelPct = -1;
		FuelManagerComponent fuel = FuelManagerComponent.Cast(vehicle.FindComponent(FuelManagerComponent));
		if (fuel && fuel.GetTotalMaxFuel() > 0)
			fuelPct = fuel.GetTotalFuel() / fuel.GetTotalMaxFuel() * 100;

		float gForce = sim.GetGForceMagnitude();

		string speedText;
		if (forwardKt < -1)
			speedText = "R" + Math.Round(-forwardKt).ToString();
		else
			speedText = Math.Round(Math.Max(forwardKt, 0)).ToString();
		SetText(textW_Airspeed, speedText);
		SetText(textW_AirspeedLabel, "KIAS");
		SetText(textW_GroundSpeed, "GS " + Math.Round(groundKt).ToString());

		SetText(textW_Altitude, Math.Round(baroFt).ToString());
		SetText(textW_AltitudeLabel, "FT MSL");
		if (aglFt < 1500) // ToDo Change depending on Airframe
			SetText(textW_RadarAlt, "R " + Math.Round(aglFt).ToString());
		else
			SetText(textW_RadarAlt, "R ----");

		int vsiRounded = Math.Round(vsiFpm / 50) * 50;
		string vsiText;
		if (vsiRounded > 0)
			vsiText = "+" + vsiRounded.ToString();
		else
			vsiText = vsiRounded.ToString();
		SetText(textW_VSI, vsiText + " FPM");

		SetText(textW_Heading, GetCardinalDirection8(heading));
		SetText(textW_HeadingTape, BuildHeadingTape(heading));

		if (engineOn)
			SetText(textW_Rotor, "NR " + Math.Round(rotorPct).ToString() + "%");
		else
			SetText(textW_Rotor, "NR " + Math.Round(rotorPct).ToString() + "% ENG OFF");

		if (fuelPct >= 0)
			SetText(textW_Fuel, "FUEL " + Math.Round(fuelPct).ToString() + "%");
		else
			SetText(textW_Fuel, "FUEL --");
		
		WorldTimestamp currentTime = GetGame().GetWorld().GetTimestamp();
		float deltaLastUpdate = currentTime.DiffSeconds(lastTimestampFuelUpdate);
		if (deltaLastUpdate > 3.0){
			lastTimestampFuelUpdate = currentTime;
			SetText(textW_FuelTimer, fuelComputer.remaingTimeOnFuel(vehicle, deltaLastUpdate));
		}

		SetText(textW_GForce, string.Format("%1 G", (Math.Round(gForce * 10) / 10).ToString(1, 1)));

		bool airborne = !sim.HasAnyGroundContact();
		string warning = GetWarningMessage(engineOn, rotorPct, vsiFpm, aglFt, fuelPct, airborne);
		if (textW_Warning){
			textW_Warning.SetText(warning);
			textW_Warning.SetVisible(!warning.IsEmpty() && blinkOn);
		}

		UpdateWorldSymbology(velocity, forwardKt);
		UpdateTargets(vehicle, heading, groundKt, timeSlice);
	}
	
	string GetWarningMessage(bool engineOn, float rotorPct, float vsiFpm, float aglFt, float fuelPct, bool airborne){
		if (airborne && !engineOn)
			return "ENGINE OFF";
	
		if (airborne && engineOn && rotorPct > 5 && rotorPct < LOW_ROTOR_PCT)
			return "LOW ROTOR";
	
		if (airborne && vsiFpm < SINK_RATE_FPM && aglFt < SINK_RATE_AGL_FT)
			return "SINK RATE";
	
		if (airborne && aglFt < ALTITUDE_WARN_AGL_FT && vsiFpm < ALTITUDE_WARN_VSI_FPM)
			return "ALTITUDE";
	
		if (fuelPct >= 0 && fuelPct < LOW_FUEL_PCT)
			return "LOW FUEL";
	
		if(optionMode != "")
			return optionMode;
		
		return string.Empty;
	}

	void UpdateWorldSymbology(vector velocity, float speedIAS){
		CameraManager cameraManager = GetGame().GetCameraManager();
		CameraBase camera;
		if (cameraManager)
			camera = cameraManager.CurrentCamera();

		if (!camera){
			ShowWorldSymbology(false, false, false);
			cameraValid = false;
			return;
		}

		vector camMat[4];
		camera.GetWorldTransform(camMat);
		vector right = camMat[0];
		vector up = camMat[1];
		vector forward = camMat[2];

		float fov = camera.GetVerticalFOV();
		if (fov < 5 || fov > 170)
			fov = DEFAULT_VFOV_DEG;
		float focal = (renderTargetHeigth * 0.5) / Math.Tan(fov * 0.5 * Math.DEG2RAD);

		vec_CamPos = camMat[3];
		vec_CamRight = right;
		vec_CamUp = up;
		vec_CamForward = forward;
		cameraFocal = focal;
		cameraValid = true;

		float camPitch = Math.Asin(Math.Clamp(forward[1], -1, 1)) * Math.RAD2DEG;
		float sx = right[1];
		float sy = up[1];
		float len = Math.Sqrt(sx * sx + sy * sy);
		if (len < 0.001){
			sx = 0;
			sy = 1;
		}else{
			sx = sx / len;
			sy = sy / len;
		}
		float lineAngle = Math.Atan2(sx, sy) * Math.RAD2DEG * ROLL_SIGN;

		if (imgW_Horizon){
			float d = ElevationToPixels(0, camPitch, focal);
			bool visible = Math.AbsFloat(camPitch) < LADDER_LIMIT_DEG + 30;
			imgW_Horizon.SetVisible(visible);
			if (visible){
				FrameSlot.SetPos(imgW_Horizon, screenCenterX + sx * d - HORIZON_HALF_WIDTH, screenCenterY - sy * d - 1);
				imgW_Horizon.SetRotation(lineAngle);
			}
		}

		if (imgW_FPV){
			bool visible = speedIAS >= SPEED_FLIGHT_MODE;
			if (visible){
				float vx = vector.Dot(velocity, right);
				float vy = vector.Dot(velocity, up);
				float vz = vector.Dot(velocity, forward);
				if (vz <= 0.5){
					visible = false;
				}else{
					float ax = Math.Clamp(Math.Atan2(vx, vz) * Math.RAD2DEG, -FPV_LIMIT_DEG, FPV_LIMIT_DEG);
					float ay = Math.Clamp(Math.Atan2(vy, vz) * Math.RAD2DEG, -FPV_LIMIT_DEG, FPV_LIMIT_DEG);
					float px = Math.Tan(ax * Math.DEG2RAD) * focal;
					float py = Math.Tan(ay * Math.DEG2RAD) * focal;
					FrameSlot.SetPos(imgW_FPV, screenCenterX + px - 30, screenCenterY - py - 28);
				}
			}
			imgW_FPV.SetVisible(visible);
		}
	}

	void UpdateTargets(notnull IEntity vehicle, float ownHeading, float groundKt, float timeSlice){
		int count = 0;
		if (HDTS_TargetRegistry.Count() > 0)
			count = HDTS_TargetRegistry.Collect(vehicle, arrayHMD_Targets);

		float_TargetBlinkClock += timeSlice;
		if (float_TargetBlinkClock > TARGET_BLINK_PERIOD)
			float_TargetBlinkClock -= TARGET_BLINK_PERIOD;
		bool blinkOn = float_TargetBlinkClock < TARGET_BLINK_PERIOD * 0.5;

		debug_TargetDebugClock += timeSlice;
		if (debug_TargetDebugClock > 5)	{
			debug_TargetDebugClock = 0;
			Print(string.Format("[HDTS] targets: providers=%1 collected=%2 camera=%3", HDTS_TargetRegistry.Count(), count, cameraValid.ToString()), LogLevel.NORMAL);
		}

		if (count == 0 || !cameraValid)	{
			navKeysArray.Clear();
			HideTargets();
			return;
		}

		vector ownPos = vehicle.GetOrigin();

		bool showWaypoints = true;

		arrayHMD_TargetsOrdered.Clear();
		HDTS_Target primaryTarget;
		HDTS_Target primaryWaypoint;
		foreach (HDTS_Target target : arrayHMD_Targets)	{
			if (!target)
				continue;

			target.m_fRange = vector.Distance(ownPos, target.m_vPosition);
			if (target.m_eKind == HDTS_ETargetKind.MARKER && target.m_fRange > MARKER_RANGE_FULL)
				continue;
			if (target.m_eKind == HDTS_ETargetKind.WAYPOINT && !showWaypoints)
				continue;

			if (target.m_bPrimary){
				if (target.m_eKind == HDTS_ETargetKind.TARGET && !primaryTarget)
					primaryTarget = target;
				else if (target.m_eKind == HDTS_ETargetKind.WAYPOINT && !primaryWaypoint)
					primaryWaypoint = target;
			}

			int index = arrayHMD_TargetsOrdered.Count();
			while (index > 0 && CompareTargets(target, arrayHMD_TargetsOrdered[index - 1]) < 0)
				index--;
			arrayHMD_TargetsOrdered.InsertAt(target, index);
		}

		navKeysArray.Clear();
		HDTS_Target selected;
		foreach (HDTS_Target candidate : arrayHMD_TargetsOrdered){
			if (candidate.m_eKind == HDTS_ETargetKind.TARGET)
				continue;
			string key = NavKey(candidate);
			navKeysArray.Insert(key);
			if (!selected && !navSelection.IsEmpty() && key == navSelection)
				selected = candidate;
		}
		if (selected){
			primaryWaypoint = selected;
			// Keep the selected point drawn even when it is beyond the symbol pool
			int selectedIndex = arrayHMD_TargetsOrdered.Find(selected);
			if (selectedIndex >= TARGET_POOL){
				arrayHMD_TargetsOrdered.RemoveOrdered(selectedIndex);
				arrayHMD_TargetsOrdered.InsertAt(selected, 0);
			}
		}

		if (!primaryTarget){
			foreach (HDTS_Target candidate : arrayHMD_TargetsOrdered){
				if (candidate.m_eKind != HDTS_ETargetKind.TARGET)
					continue;
				if (!primaryTarget)
					primaryTarget = candidate;
				if (candidate.m_bMatched){
					primaryTarget = candidate;
					break;
				}
			}
		}
		if (!primaryWaypoint){
			foreach (HDTS_Target candidate : arrayHMD_TargetsOrdered){
				if (candidate.m_eKind == HDTS_ETargetKind.WAYPOINT){
					primaryWaypoint = candidate;
					break;
				}
			}
		}

		int shown = 0;
		foreach (HDTS_Target target : arrayHMD_TargetsOrdered){
			if (shown >= TARGET_POOL)
				break;

			ImageWidget symbol = arrayImgW_TargetDiamonds[shown];
			TextWidget label = arrayTextW_TargetLabels[shown];
			shown++;
			if (!symbol || !label)
				continue;

			vector rel = target.m_vPosition - vec_CamPos;
			float vx = vector.Dot(rel, vec_CamRight);
			float vy = vector.Dot(rel, vec_CamUp);
			float vz = vector.Dot(rel, vec_CamForward);
			vector screen = vector.Zero;
			bool inView = vz > 1;
			if (inView){
				screen[0] = screenCenterX + vx / vz * cameraFocal;
				screen[1] = screenCenterY - vy / vz * cameraFocal;
				inView = screen[0] >= 0 && screen[0] <= renderTargetWidth && screen[1] >= 0 && screen[1] <= renderTargetHeigth;
			}

			float half = TARGET_DIAMOND_HALF;
			float rotation = 45;
			bool blink = false;
			Color color = target.m_Color;
			if (target.m_eKind == HDTS_ETargetKind.TARGET){
				blink = target.m_bMatched;
				if (!color){
					if (target.m_bMatched)
						color = color_TargetMatched;
					else
						color = color_TargetOther;
				}
			}else if (target.m_eKind == HDTS_ETargetKind.WAYPOINT){
				half = WAYPOINT_HALF;
				rotation = 0;
				if (!color)
					color = color_Waypoint;
			}else{
				half = MARKER_HALF;
				if (target == primaryWaypoint)
					half = WAYPOINT_HALF;
				rotation = 0;
				if (!color)
					color = color_TargetOther;
			}

			bool visible = inView && (!blink || blinkOn);
			symbol.SetRotation(rotation);
			symbol.SetColor(color);
			label.SetColor(color);
			symbol.SetVisible(visible);
			label.SetVisible(inView);
			if (!inView)
				continue;

			FrameSlot.SetSize(symbol, half * 2, half * 2);
			FrameSlot.SetPos(symbol, screen[0] - half, screen[1] - half);
			label.SetText(target.m_sLabel + "  " + FormatRange(target.m_fRange));
			FrameSlot.SetPos(label, screen[0] + half + 6, screen[1] - 11);
		}

		for (int i = shown; i < TARGET_POOL; i++){
			Show(arrayImgW_TargetDiamonds[i], false);
			Show(arrayTextW_TargetLabels[i], false);
		}

		if (textW_TargetLine){
			if (primaryTarget){
				string source = primaryTarget.m_sSource;
				if (source.IsEmpty())
					source = "TGT";

				string state;
				if (!primaryTarget.m_bMatched)
					state = "  CODE MISMATCH";

				textW_TargetLine.SetText(string.Format("%1 %2  %3  %4%5", source, primaryTarget.m_sLabel, FormatRange(primaryTarget.m_fRange), FormatBearing(ownPos, primaryTarget.m_vPosition, ownHeading), state));
				if (primaryTarget.m_Color)
					textW_TargetLine.SetColor(primaryTarget.m_Color);
				else if (primaryTarget.m_bMatched)
					textW_TargetLine.SetColor(color_TargetMatched);
				else
					textW_TargetLine.SetColor(color_TargetOther);
				textW_TargetLine.SetVisible(true);
			}else{
				textW_TargetLine.SetVisible(false);
			}
		}

		if (textW_NavLine){
			if (primaryWaypoint){
				string source = primaryWaypoint.m_sSource;
				if (source.IsEmpty())
					source = "WPT";

				string ete;
				float groundMps = groundKt / MPS_TO_KT;
				if (groundMps > 5)
					ete = "  ETE " + FormatEte(primaryWaypoint.m_fRange / groundMps);

				textW_NavLine.SetText(string.Format("%1 %2  %3  %4%5", source, primaryWaypoint.m_sLabel, FormatRange(primaryWaypoint.m_fRange), FormatBearing(ownPos, primaryWaypoint.m_vPosition, ownHeading), ete));
				if (primaryWaypoint.m_Color)
					textW_NavLine.SetColor(primaryWaypoint.m_Color);
				else
					textW_NavLine.SetColor(color_Waypoint);
				textW_NavLine.SetVisible(true);
			}else{
				textW_NavLine.SetVisible(false);
			}
		}
	}

	void HideTargets(){
		for (int i = 0; i < TARGET_POOL; i++){
			Show(arrayImgW_TargetDiamonds[i], false);
			Show(arrayTextW_TargetLabels[i], false);
		}
		Show(textW_TargetLine, false);
		Show(textW_NavLine, false);
	}
	
	static int CompareTargets(notnull HDTS_Target a, notnull HDTS_Target b){
		if (a.m_eKind != b.m_eKind)
			return a.m_eKind - b.m_eKind;

		if (a.m_fRange < b.m_fRange)
			return -1;
		if (a.m_fRange > b.m_fRange)
			return 1;
		return 0;
	}

	static string FormatBearing(vector from, vector to, float ownHeading)	{
		float bearing = GetBearingDeg(from, to);
		float relative = bearing - ownHeading;
		while (relative > 180)
			relative -= 360;
		while (relative < -180)
			relative += 360;

		string side = "L";
		if (relative >= 0)
			side = "R";

		return string.Format("BRG %1  %2%3", Math.Round(bearing).ToString(3), side, Math.Round(Math.AbsFloat(relative)).ToString());
	}

	static string FormatEte(float seconds){
		int total = Math.Round(seconds);
		if (total < 3600)
			return (total / 60).ToString() + ":" + (total % 60).ToString(2);

		return "N.A.";
	}

	static string FormatRange(float metres)	{
		if (metres < 1000)
			return Math.Round(metres).ToString() + " M";

		return (Math.Round(metres / 100) / 10).ToString(1, 1) + " KM";
	}

	static float GetBearingDeg(vector from, vector to){
		vector delta = to - from;
		float bearing = Math.Atan2(delta[0], delta[2]) * Math.RAD2DEG;
		while (bearing < 0)
			bearing += 360;
		while (bearing >= 360)
			bearing -= 360;
		return bearing;
	}

	void ShowWorldSymbology(bool horizon, bool ladder, bool fpv){
		Show(imgW_Horizon, horizon);
		Show(imgW_FPV, fpv);
	}

	float ElevationToPixels(float elevationDeg, float camPitchDeg, float focal){
		float delta = Math.Clamp(elevationDeg - camPitchDeg, -80, 80);
		return Math.Tan(delta * Math.DEG2RAD) * focal;
	}

	string BuildHeadingTape(float heading){
		int centre = Math.Round(heading / 10);
		string tape;
		for (int offset = -3; offset <= 3; offset++){
			int token = centre + offset;
			while (token < 0)
				token += 36;
			token = token % 36;

			string label;
			if (token == 0)
				label = "N";
			else if (token == 9)
				label = "E";
			else if (token == 18)
				label = "S";
			else if (token == 27)
				label = "W";
			else
				label = token.ToString(2);

			if (offset == 0)
				label = "[" + label + "]";

			if (offset > -3)
				tape += "  .  ";
			tape += label;
		}
		return tape;
	}

	static float GetHeadingDeg(notnull IEntity entity){
		vector forward = entity.GetWorldTransformAxis(2);
		forward[1] = 0;
		if (forward.LengthSq() < 0.0001)
			return 0;

		float heading = Math.Atan2(forward[0], forward[2]) * Math.RAD2DEG;
		while (heading < 0)
			heading += 360;
		while (heading >= 360)
			heading -= 360;
		return heading;
	}
	
	static string GetCardinalDirection8(float headingDeg){
		static const string DIRS[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
	
		float normalized = headingDeg;
		while (normalized < 0)
			normalized += 360;
		while (normalized >= 360)
			normalized -= 360;

		int index = Math.Floor((normalized + 22.5) / 45.0);
		return DIRS[index % 8];
	}
	
	void SetOptionModeInfo(string optionModeNew){
		optionMode = optionModeNew;
	}
}
