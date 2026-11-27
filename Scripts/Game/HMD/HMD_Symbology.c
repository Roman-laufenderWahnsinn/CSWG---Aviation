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


class HMD_Symbology : Managed{
	protected static const ResourceName LAYOUT = "{61CE74373672265E}UI/Layouts/HUD/CSWG_HMD.layout";
	protected static const ResourceName FPV_TEXTURE = "{621A8E584823A7CA}UI/Textures/HUD/VelocityMarker-100_atlas.edds";

	//! Flip to -1 if the horizon line banks the wrong way on your installation
	protected static const float ROLL_SIGN = 1;

	protected static const float MPS_TO_KT = 1.943844;
	protected static const float M_TO_FT = 3.28084;
	protected static const float MPS_TO_FPM = 196.8504;
	protected static const float DEFAULT_VFOV_DEG = 74;

	protected static const float LADDER_LIMIT_DEG = 34;
	protected static const float HORIZON_HALF_WIDTH = 360;
	protected static const float FPV_LIMIT_DEG = 22;
	protected static const float SPEED_FLIGHT_MODE = 30;
	protected static const float BLINK_PERIOD = 0.8;

	protected static const float LOW_ROTOR_PCT = 95;
	protected static const float LOW_FUEL_PCT = 10;
	protected static const float SINK_RATE_FPM = -1500;
	protected static const float SINK_RATE_AGL_FT = 500;
	protected static const float ALTITUDE_WARN_AGL_FT = 50;
	protected static const float ALTITUDE_WARN_VSI_FPM = -200;
	protected static const float MARKER_RANGE_FULL = 10000;

	protected Widget wingetRoot;
	protected Widget windgetFrame;

	protected ImageWidget imgW_Horizon;

	protected ImageWidget imgW_FPV;

	protected TextWidget textW_Airspeed;
	protected TextWidget textW_AirspeedLabel;
	protected TextWidget textW_GroundSpeed;
	protected TextWidget textW_Altitude;
	protected TextWidget textW_AltitudeLabel;
	protected TextWidget textW_RadarAlt;
	protected TextWidget textW_VSI;
	protected TextWidget textW_Heading;
	protected TextWidget textW_Rotor;
	protected TextWidget textW_Fuel;
	protected TextWidget textW_FuelTimer;
	protected TextWidget textW_GForce;
	protected TextWidget textW_Warning;

	// Target designation
	protected static const int TARGET_POOL = 12;
	protected static const float TARGET_BLINK_PERIOD = 1.0;
	protected ref array<ImageWidget> arrayImgW_TargetDiamonds = {};
	protected ref array<TextWidget> arrayTextW_TargetLabels = {};
	protected TextWidget textW_TargetLine;
	protected ref array<ref HMD_Target> arrayHMD_Targets = {};
	protected float float_TargetBlinkClock;
	protected ref Color color_TargetMatched = new Color(0.35, 1, 0.45, 1);
	protected ref Color color_TargetOther = new Color(1, 0.72, 0.24, 1);
	protected TextWidget textW_NavLine;
	protected ref array<ref HMD_Target> arrayHMD_TargetsOrdered = {};
	protected ref Color color_Waypoint = new Color(1, 0.45, 1, 1);
	protected static const float BASE_SYMBOL_SIZE = 32.0;
	protected float pilotSymbolFactor = 1.0;
	
	// Camera frame of the current frame, filled by UpdateWorldSymbology and reused for target projection
	protected bool cameraValid;
	protected vector vec_CamPos;
	protected vector vec_CamRight;
	protected vector vec_CamUp;
	protected vector vec_CamForward;
	protected float cameraFocal;
	protected float debug_TargetDebugClock;
	
	// Nav selection: key of the waypoint/marker the pilot picked with the cycle key, empty = provider default
	protected string navSelection;
	protected string navSelectionText = "NAV: AUTO";
	protected ref array<string> navKeysArray = {};

	protected float renderTargetWidth;
	protected float renderTargetHeigth;
	protected float screenCenterX;
	protected float screenCenterY;

	protected int brightessHMD = 10;
	protected float blinkClockCounter;
	protected bool layoutIsDirty = true;
	protected WorldTimestamp lastTimestampFuelUpdate;
	protected string optionMode = "";
	protected int markerDetail = 2;

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
	
	float GetPilotSymbolFactor(){
		return pilotSymbolFactor;
	}

	void SetPilotSymbolFactor(float factor){
		pilotSymbolFactor = Math.Clamp(factor, 0.25, 4.0);
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
	
	void SetMarkerDeatailSetting(int markerDetailSetting){
		markerDetail = Math.ClampInt(markerDetailSetting, 0, 3);
	}
	
	int GetMarkerDetailSetting(){
		return markerDetail;
	}
	
	void SetOptionModeInfo(string optionModeNew){
		optionMode = optionModeNew;
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

	static string NavKey(notnull HMD_Target target){
		vector p = target.vectorPosition;
		return target.targetSource + "|" + target.labelName + "|" + Math.Round(p[0]).ToString() + "/" + Math.Round(p[2]).ToString();
	}

	protected void ApplyBrightness(){
		if (!wingetRoot)
			return;
		if(brightessHMD > 10.0)
			brightessHMD = 10.0;
		wingetRoot.SetOpacity(brightessHMD/10.0);
	}

	protected void CacheWidgets(){
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

	protected void UpdateScreenMetrics(){
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

	protected void LayoutStatic(){
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

		Place(textW_GForce, left, screenCenterY + 226, 140, 22);
		Place(textW_Rotor, left, screenCenterY + 252, 140, 26);
		
		Place(textW_FuelTimer, right, screenCenterY + 226, 160, 22);
		Place(textW_Fuel, right, screenCenterY + 252, 160, 26);

		Place(textW_Warning, screenCenterX, screenCenterY + 150, 360, 32);
		Place(textW_TargetLine, screenCenterX, screenCenterY - 190, 520, 24);
		Place(textW_NavLine, screenCenterX, screenCenterY + 252, 520, 22);

		for (int i = 0; i < arrayImgW_TargetDiamonds.Count(); i++){
			if (arrayImgW_TargetDiamonds[i])
				FrameSlot.SetSize(arrayImgW_TargetDiamonds[i], BASE_SYMBOL_SIZE, BASE_SYMBOL_SIZE);
			if (arrayTextW_TargetLabels[i])
				FrameSlot.SetSize(arrayTextW_TargetLabels[i], 260, 22);
		}

		if (imgW_Horizon)
			FrameSlot.SetSize(imgW_Horizon, HORIZON_HALF_WIDTH * 2, 2);
	}

	protected void Place(Widget w, float x, float y, float width, float height){
		if (!w)
			return;

		FrameSlot.SetSize(w, width, height);
		FrameSlot.SetPos(w, x - width * 0.5, y - height * 0.5);
	}

	protected void Show(Widget w, bool visible){
		if (w)
			w.SetVisible(visible);
	}

	protected void SetText(TextWidget w, string text){
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
	
	protected string GetWarningMessage(bool engineOn, float rotorPct, float vsiFpm, float aglFt, float fuelPct, bool airborne){
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

	protected void UpdateWorldSymbology(vector velocity, float speedIAS){
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

	protected void UpdateTargets(notnull IEntity vehicle, float ownHeading, float groundKt, float timeSlice){
		int count = 0;
		if (HMD_TargetRegistry.Count() > 0)
			count = HMD_TargetRegistry.Collect(vehicle, arrayHMD_Targets);

		float_TargetBlinkClock += timeSlice;
		if (float_TargetBlinkClock > TARGET_BLINK_PERIOD)
			float_TargetBlinkClock -= TARGET_BLINK_PERIOD;
		bool blinkOn = float_TargetBlinkClock < TARGET_BLINK_PERIOD * 0.5;

		debug_TargetDebugClock += timeSlice;
		if (debug_TargetDebugClock > 5)	{
			debug_TargetDebugClock = 0;
			Print(string.Format("[HDTS] targets: providers=%1 collected=%2 camera=%3", HMD_TargetRegistry.Count(), count, cameraValid.ToString()), LogLevel.NORMAL);
		}

		if (count == 0 || !cameraValid)	{
			navKeysArray.Clear();
			HideTargets();
			return;
		}

		vector ownPos = vehicle.GetOrigin();

		bool showWaypoints = true;

		arrayHMD_TargetsOrdered.Clear();
		HMD_Target primaryTarget;
		HMD_Target primaryWaypoint;
		foreach (HMD_Target target : arrayHMD_Targets)	{
			if (!target)
				continue;

			target.filterRange = vector.Distance(ownPos, target.vectorPosition);
			if (target.targetKind == HMD_ETargetKind.MARKER && target.filterRange > MARKER_RANGE_FULL)
				continue;
			if (target.targetKind == HMD_ETargetKind.WAYPOINT && !showWaypoints)
				continue;

			if (target.isPrimary){
				if (target.targetKind == HMD_ETargetKind.TARGET && !primaryTarget)
					primaryTarget = target;
				else if (target.targetKind == HMD_ETargetKind.WAYPOINT && !primaryWaypoint)
					primaryWaypoint = target;
			}

			int index = arrayHMD_TargetsOrdered.Count();
			while (index > 0 && CompareTargets(target, arrayHMD_TargetsOrdered[index - 1]) < 0)
				index--;
			arrayHMD_TargetsOrdered.InsertAt(target, index);
		}

		navKeysArray.Clear();
		HMD_Target selected;
		foreach (HMD_Target candidate : arrayHMD_TargetsOrdered){
			if (candidate.targetKind == HMD_ETargetKind.TARGET)
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
			foreach (HMD_Target candidate : arrayHMD_TargetsOrdered){
				if (candidate.targetKind != HMD_ETargetKind.TARGET)
					continue;
				if (!primaryTarget)
					primaryTarget = candidate;
				if (candidate.codeMatched){
					primaryTarget = candidate;
					break;
				}
			}
		}
		if (!primaryWaypoint){
			foreach (HMD_Target candidate : arrayHMD_TargetsOrdered){
				if (candidate.targetKind == HMD_ETargetKind.WAYPOINT){
					primaryWaypoint = candidate;
					break;
				}
			}
		}

		int shown = 0;
		foreach (HMD_Target target : arrayHMD_TargetsOrdered){
			if (shown >= TARGET_POOL)
				break;

			ImageWidget symbol = arrayImgW_TargetDiamonds[shown];
			TextWidget label = arrayTextW_TargetLabels[shown];
			shown++;
			if (!symbol || !label)
				continue;

			vector rel = target.vectorPosition - vec_CamPos;
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

			float rotation = 45;
			bool blink = false;
			Color color = target.color;
			if (target.targetKind == HMD_ETargetKind.TARGET){
				blink = target.codeMatched;
				if (!color){
					if (target.codeMatched)
						color = color_TargetMatched;
					else
						color = color_TargetOther;
				}
			}else if (target.targetKind == HMD_ETargetKind.WAYPOINT){
				rotation = 0;
				if (!color)
					color = color_Waypoint;
			}else{
				rotation = 0;
				if (!color)
					color = color_TargetOther;
			}

			Color symbolColor = color;
			if (target.HasIconTexture()){
				if (!target.iconQuad.IsEmpty()){
					symbol.LoadImageFromSet(0, target.iconTexture, target.iconQuad);
				}else{
					symbol.LoadImageTexture(0, target.iconTexture);
				}

				rotation = 0;
				if (!target.tintIcon)
					symbolColor = Color.FromInt(Color.WHITE);
			}

			float size = BASE_SYMBOL_SIZE * target.GetIconScale() * pilotSymbolFactor;
			float halfSize = size * 0.5;

			bool visible = inView && (!blink || blinkOn);
			symbol.SetRotation(rotation);
			symbol.SetColor(symbolColor);
			label.SetColor(color);
			symbol.SetVisible(visible);
			label.SetVisible(inView);
			if (!inView)
				continue;

			FrameSlot.SetSize(symbol, size, size);
			FrameSlot.SetPos(symbol, screen[0] - halfSize, screen[1] - halfSize);
			if (markerDetail <= 0){
				label.SetText("");
			}else if (markerDetail == 1){
				label.SetText(target.labelName + "  ");
			}else if (markerDetail == 2){
				if (target.targetKind == HMD_ETargetKind.WAYPOINT)
					label.SetText(target.labelName + "  " + FormatRange(target.filterRange));
				else
					label.SetText(target.labelName + "  ");
			}else{
				label.SetText(target.labelName + "  " + FormatRange(target.filterRange));
			}
			FrameSlot.SetPos(label, screen[0] + halfSize + 6, screen[1] - 11);
		}

		for (int i = shown; i < TARGET_POOL; i++){
			Show(arrayImgW_TargetDiamonds[i], false);
			Show(arrayTextW_TargetLabels[i], false);
		}

		if (textW_TargetLine){
			if (primaryTarget){
				string source = primaryTarget.targetSource;
				if (source.IsEmpty())
					source = "TGT";

				string state;
				if (!primaryTarget.codeMatched)
					state = "  CODE MISMATCH";

				textW_TargetLine.SetText(string.Format("%1 %2  %3  %4%5", source, primaryTarget.labelName, FormatRange(primaryTarget.filterRange), FormatBearing(ownPos, primaryTarget.vectorPosition, ownHeading), state));
				
				if (primaryTarget.color)
					textW_TargetLine.SetColor(primaryTarget.color);
				else if (primaryTarget.codeMatched)
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
				string source = primaryWaypoint.targetSource;
				if (source.IsEmpty())
					source = "WPT";

				string ete;
				float groundMps = groundKt / MPS_TO_KT;
				if (groundMps > 5)
					ete = "  ETE " + FormatEte(primaryWaypoint.filterRange / groundMps);

				textW_NavLine.SetText(string.Format("%1 %2  %3  %4%5", source, primaryWaypoint.labelName, FormatRange(primaryWaypoint.filterRange), FormatBearing(ownPos, primaryWaypoint.vectorPosition, ownHeading), ete));
				if (primaryWaypoint.color)
					textW_NavLine.SetColor(primaryWaypoint.color);
				else
					textW_NavLine.SetColor(color_Waypoint);
				textW_NavLine.SetVisible(true);
			}else{
				textW_NavLine.SetVisible(false);
			}
		}
	}

	protected void HideTargets(){
		for (int i = 0; i < TARGET_POOL; i++){
			Show(arrayImgW_TargetDiamonds[i], false);
			Show(arrayTextW_TargetLabels[i], false);
		}
		Show(textW_TargetLine, false);
		Show(textW_NavLine, false);
	}
	
	protected static int CompareTargets(notnull HMD_Target a, notnull HMD_Target b){
		if (a.targetKind != b.targetKind)
			return a.targetKind - b.targetKind;

		if (a.filterRange < b.filterRange)
			return -1;
		if (a.filterRange > b.filterRange)
			return 1;
		return 0;
	}

	protected static string FormatBearing(vector from, vector to, float ownHeading)	{
		float bearing = GetBearingDeg(from, to);
		float relative = bearing - ownHeading;
		while (relative > 180)
			relative -= 360;
		while (relative < -180)
			relative += 360;

		string side = "L";
		if (relative > 0)
			side = "R";

		return string.Format("BRG %1  %2%3", Math.Round(bearing).ToString(3,0), side, Math.Round(Math.AbsFloat(relative)).ToString());
	}

	protected static string FormatEte(float seconds){
		int total = Math.Round(seconds);
		if (total < 3600)
			return (total / 60).ToString() + ":" + (total % 60).ToString(2);

		return "N.A.";
	}

	protected static string FormatRange(float metres)	{
		if (metres < 1000)
			return Math.Round(metres).ToString() + " M";

		return (Math.Round(metres / 100) / 10).ToString(1, 1) + " KM";
	}

	protected static float GetBearingDeg(vector from, vector to){
		vector delta = to - from;
		float bearing = Math.Atan2(delta[0], delta[2]) * Math.RAD2DEG;
		while (bearing < 0)
			bearing += 360;
		while (bearing >= 360)
			bearing -= 360;
		return bearing;
	}

	protected void ShowWorldSymbology(bool horizon, bool ladder, bool fpv){
		Show(imgW_Horizon, horizon);
		Show(imgW_FPV, fpv);
	}

	protected float ElevationToPixels(float elevationDeg, float camPitchDeg, float focal){
		float delta = Math.Clamp(elevationDeg - camPitchDeg, -80, 80);
		return Math.Tan(delta * Math.DEG2RAD) * focal;
	}

	protected static float GetHeadingDeg(notnull IEntity fallbackEntity){
		vector forward;
		BaseWorld world = fallbackEntity.GetWorld();
		
		if (world){
			vector camMat[4];
			world.GetCurrentCamera(camMat);
			forward = camMat[2]; // index 2 is the forward camera
		}else{
			forward = fallbackEntity.GetWorldTransformAxis(2);
		}

		forward[1] = 0;	// ignore pitch
		if (forward.LengthSq() < 0.0001)
			return 0;

		float heading = Math.Atan2(forward[0], forward[2]) * Math.RAD2DEG;
		while (heading < 0)
			heading += 360;
		while (heading >= 360)
			heading -= 360;
			
		return heading;
	}
	
	protected static string GetCardinalDirection8(float headingDeg){
		static const string DIRS[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
	
		float normalized = headingDeg;
		while (normalized < 0)
			normalized += 360;
		while (normalized >= 360)
			normalized -= 360;

		int index = Math.Floor((normalized + 22.5) / 45.0);
		return DIRS[index % 8];
	}
	
	//! Applies target icon texture/quad when provided, or keeps standard geometric symbol rendering
	protected void ApplyTargetSymbolVisual(notnull ImageWidget symbol, notnull HMD_Target target, notnull Color fallbackColor, float defaultRotation){
		if (target.HasIconTexture()){
			if (!target.iconQuad.IsEmpty()){
				symbol.LoadImageFromSet(0, target.iconTexture, target.iconQuad);
			}else{
				symbol.LoadImageTexture(0, target.iconTexture);
			}

			// Icons are upright unless explicitly rotated
			symbol.SetRotation(0);

			if (target.tintIcon)
				symbol.SetColor(fallbackColor);
			else
				symbol.SetColor(Color.FromInt(Color.WHITE));
			return;
		}

		// Default geometric symbol (diamond / square)
		symbol.SetRotation(defaultRotation);
		symbol.SetColor(fallbackColor);
	}
}
