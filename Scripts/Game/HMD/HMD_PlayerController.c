/*
Helment Mounted Display Mod for the Combined Special Warfare Group
Autohor: Roman Alexander "Trojan" Löwe
This is on adjusted version form the original "HDTS - Helmet Display" Mod on stem workshop:
https://reforger.armaplatform.com/workshop/7BABD785CD00B3F6-HDTS-HelmetDisplay

- Changed keybinds (to stay compatible with ACM) and reduced to 3
- added options to dimm (0-10)
*/

enum CSWG_HMD_Mode{
	NAV,
	DIM,
	ATAK_SHOW_HIDE,
	CTG_VANILLA_SHOW_HIDE
}

modded class SCR_PlayerController{
	protected static const string HMD_CONTEXT = "CSWG_HMD_Context";
	protected static const string HMD_ACTION_MENU = "CSWG_HMD_Menu";
	protected static const string HMD_ACTION_NEXT = "CSWG_HMD_Next";
	protected static const string HMD_ACTION_OPTION = "CSWG_HMD_Option";
	protected static const bool HMD_REQUIRE_HELMET = true;
	protected static const float HMD_HINT_SECONDS = 4;
	
	protected ref HMD_Symbology ref_HMDsymbology;
	protected ref HMD_FuelComputer ref_FuelComputer;
	protected bool HMD_FuelComputerInitaliced = false;
	protected bool HMD_Enabled = true;
	protected bool HMD_InputReady;
	protected int HMD_Brightness = 10;
	protected string HMD_NavSelection;
	protected int HMD_selectedOption = 0;
	protected int HMD_markerDetail = 2;
	
	// option strings (so it is easier to give infos in hint)
	protected ref array<string> markerDetailMode = {"No text on markers", "label only", "labeln and waypoint distance", "all label and distance"};
	

	override protected void OnInit(IEntity owner){
		super.OnInit(owner);

		if (System.IsConsoleApp())
			return;

		SetEventMask(EntityEvent.FRAME);
	}

	override protected void EOnFrame(IEntity owner, float timeSlice){
		super.EOnFrame(owner, timeSlice);

		if (GetGame().GetPlayerController() != this)
			return;

		HMD_Tick(timeSlice);
	}

	protected void HMD_Tick(float timeSlice){
		IEntity vehicle;
		VehicleHelicopterSimulation sim;
		IEntity character;
		if (!HMD_FindAircraft(vehicle, sim, character)){
			HMD_DestroyDisplay();
			return;
		}
		
		if(IsPlayerInGameMaster()){
			HMD_DestroyDisplay();
			return;
		}

		HMD_InitInput();
		if (HMD_InputReady)
			GetGame().GetInputManager().ActivateContext(HMD_CONTEXT);

		bool show = HMD_Enabled && (!HMD_REQUIRE_HELMET || HMD_HasHelmet(character));
		if (!show){
			HMD_DestroyDisplay();
			return;
		}

		if (!ref_HMDsymbology){
			ref_HMDsymbology = new HMD_Symbology();
			if (!ref_HMDsymbology.IsValid()){
				ref_HMDsymbology = null;
				return;
			}

			ref_HMDsymbology.SetBrightness(HMD_Brightness);
			ref_HMDsymbology.SetNavSelection(HMD_NavSelection);
		}
		if(!HMD_FuelComputerInitaliced){
			ref_FuelComputer = new HMD_FuelComputer(vehicle);
			HMD_FuelComputerInitaliced = true;
		}

		ref_HMDsymbology.Update(vehicle, sim, timeSlice, ref_FuelComputer);
	}

	protected void HMD_DestroyDisplay(){
		if (!ref_HMDsymbology)
			return;

		ref_HMDsymbology.Destroy();
		ref_HMDsymbology = null;
	}

	protected bool HMD_FindAircraft(out IEntity vehicle, out VehicleHelicopterSimulation sim, out IEntity character){
		character = GetControlledEntity();
		if (!character)
			return false;

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(character.FindComponent(SCR_CompartmentAccessComponent));
		if (!access || !access.IsInCompartment())
			return false;

		BaseCompartmentSlot slot = access.GetCompartment();
		if (!slot || slot.GetType() != ECompartmentType.PILOT)
			return false;

		vehicle = SCR_CompartmentAccessComponent.GetVehicleIn(character);
		if (!vehicle)
			return false;

		sim = VehicleHelicopterSimulation.Cast(vehicle.FindComponent(VehicleHelicopterSimulation));
		return sim != null;
	}

	protected bool HMD_HasHelmet(IEntity character){
		if (!character)
			return false;

		EquipedLoadoutStorageComponent loadout = EquipedLoadoutStorageComponent.Cast(character.FindComponent(EquipedLoadoutStorageComponent));
		if (!loadout)
			return false;

		return loadout.GetClothFromArea(LoadoutHeadCoverArea) != null;
	}

	protected void HMD_InitInput(){
		if (HMD_InputReady)
			return;

		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;

		inputManager.AddActionListener(HMD_ACTION_MENU, EActionTrigger.DOWN, HMD_OnMenu);
		inputManager.AddActionListener(HMD_ACTION_NEXT, EActionTrigger.DOWN, HMD_OnNext);
		inputManager.AddActionListener(HMD_ACTION_OPTION, EActionTrigger.DOWN, HMD_OnOption);
		HMD_InputReady = true;
	}

	protected void HMD_OnMenu(){
		HMD_selectedOption = (HMD_selectedOption + 1) % 4;
		if(HMD_selectedOption == CSWG_HMD_Mode.NAV){
			HMD_Hint("Naviagation\nNUM7 - next: dimming\nNUM1 - cycle details\nNUM3 - select next for navigation", "HMD");
			// most likley will use NUM1 to switch NAV modes, like only wp, only jtac placed and so on, to give quick access to filtering
			ref_HMDsymbology.SetOptionModeInfo("");
			return;
		}
		if(HMD_selectedOption == CSWG_HMD_Mode.DIM){
			HMD_Hint("Dimming\nNUM7 - next: ATAK show/hide\nNUM1 - dimmer/disable HUD\nNUM3 - brighter HUD", "HMD");
			ref_HMDsymbology.SetOptionModeInfo("dimming");
			return;
		}
		if(HMD_selectedOption == CSWG_HMD_Mode.ATAK_SHOW_HIDE){
			HMD_Hint("ATAK show/hide\nNUM7 - next: Map and CTG show/hide\nNUM1 - show/hide waypoints\nNUM3 - show/hide markers", "HMD");
			ref_HMDsymbology.SetOptionModeInfo("atak show/hide");
			return;
		}
		if(HMD_selectedOption == CSWG_HMD_Mode.CTG_VANILLA_SHOW_HIDE){
			HMD_Hint("Map and CTG show/hide\nNUM7 - next:Naviagtion\nNUM1 - show/hide map markers HUD\nNUM3 - show/hide CTG markers", "ATAK WP view range");
			ref_HMDsymbology.SetOptionModeInfo("map+ctg show/hide");
			return;
		}
	}

	protected void HMD_OnNext(){
		if(HMD_selectedOption == CSWG_HMD_Mode.NAV){
			ref_HMDsymbology.CycleNav();
			HMD_Hint(ref_HMDsymbology.GetNavSelectionText(), "Nav/Mark selected");
			return;
		}
		if(HMD_selectedOption == CSWG_HMD_Mode.DIM){
			HMD_Brightness++;
			if(HMD_Brightness > 10){
				HMD_Brightness = 10;
				return;
			}
			ref_HMDsymbology.SetBrightness(HMD_Brightness);
			return;
		}
	}

	protected void HMD_OnOption(){
		if(HMD_selectedOption == CSWG_HMD_Mode.NAV){
			HMD_markerDetail = (HMD_markerDetail + 1) % 4;
			ref_HMDsymbology.SetMarkerDeatailSetting(HMD_markerDetail);
			HMD_Hint(markerDetailMode[HMD_markerDetail], "Marker details setting");
			return;
		}
		if(HMD_selectedOption == CSWG_HMD_Mode.DIM){
			HMD_Brightness--;
			if(HMD_Brightness < 0){
				HMD_Brightness = 0;
				return;
			}
			ref_HMDsymbology.SetBrightness(HMD_Brightness);
			return;
		}
		
	}

	protected void HMD_Hint(string text, string title){
		SCR_HintManagerComponent.ShowCustomHint(text, title, HMD_HINT_SECONDS);
	}
	
	protected bool IsPlayerInGameMaster(){
	    SCR_EditorManagerEntity editorManager = SCR_EditorManagerEntity.GetInstance();
    	if (!editorManager)
        	return false;

    	return editorManager.IsOpened();
	}
}
