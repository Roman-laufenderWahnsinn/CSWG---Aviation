/*
Helment Mounted Display Mod for the Combined Special Warfare Group
Autohor: Roman Alexander "Trojan" Löwe
This is on adjusted version form the original "HDTS - Helmet Display" Mod on stem workshop:
https://reforger.armaplatform.com/workshop/7BABD785CD00B3F6-HDTS-HelmetDisplay

- Changed keybinds (to stay compatible with ACM) and reduced to 3
*/

enum CSWG_HMD_Mode{
	NAV, // 0, standart
	DIM  // 1
}

modded class SCR_PlayerController{
	static const string HMD_CONTEXT = "CSWG_HMD_Context";
	static const string HMD_ACTION_MENU = "CSWG_HMD_Menu";
	static const string HMD_ACTION_NEXT = "CSWG_HMD_Next";
	static const string HMD_ACTION_OPTION = "CSWG_HMD_Option";
	static const bool HMD_REQUIRE_HELMET = true;
	static const float HMD_HINT_SECONDS = 4;
	
	
	

	ref HMD_Symbology ref_HMDsymbology;
	ref HMD_FuelComputer ref_FuelComputer;
	bool HMD_FuelComputerInitaliced = false;
	bool HMD_Enabled = true;
	bool HMD_InputReady;
	int HMD_Brightness = 10;
	string HMD_NavSelection;
	int HMD_selectedOption = 0;
	

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

	void HMD_Tick(float timeSlice){
		IEntity vehicle;
		VehicleHelicopterSimulation sim;
		IEntity character;
		if (!HMD_FindAircraft(vehicle, sim, character)){
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

	void HMD_DestroyDisplay(){
		if (!ref_HMDsymbology)
			return;

		ref_HMDsymbology.Destroy();
		ref_HMDsymbology = null;
	}

	bool HMD_FindAircraft(out IEntity vehicle, out VehicleHelicopterSimulation sim, out IEntity character){
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

	bool HMD_HasHelmet(IEntity character){
		if (!character)
			return false;

		EquipedLoadoutStorageComponent loadout = EquipedLoadoutStorageComponent.Cast(character.FindComponent(EquipedLoadoutStorageComponent));
		if (!loadout)
			return false;

		return loadout.GetClothFromArea(LoadoutHeadCoverArea) != null;
	}

	void HMD_InitInput(){
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

	void HMD_OnMenu(){
		HMD_selectedOption = (HMD_selectedOption + 1) % 2;
		if(HMD_selectedOption == CSWG_HMD_Mode.NAV){
			HMD_Hint("Naviagation Mode\nNUM1 <not used>\nNUM3 next Nav", "HMD");
			ref_HMDsymbology.SetOptionModeInfo("");
			return;
		}
		if(HMD_selectedOption == CSWG_HMD_Mode.DIM){
			HMD_Hint("Dimming Mode\nNUM1 dimmer/disable HUD\nNUM3 brighter HUD", "HMD");
			ref_HMDsymbology.SetOptionModeInfo("dimming");
			return;
		}
	}

	void HMD_OnNext(){
		if(HMD_selectedOption == CSWG_HMD_Mode.NAV){
			ref_HMDsymbology.CycleNav();
			HMD_Hint(ref_HMDsymbology.navSelectionText, "NAV");
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

	void HMD_OnOption(){
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

	void HMD_Hint(string text, string title){
		SCR_HintManagerComponent.ShowCustomHint(text, title, HMD_HINT_SECONDS);
	}
}
