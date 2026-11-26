/*
Helment Mounted Display Mod for the Combined Special Warfare Group
Autohor: Roman Alexander "Trojan" Löwe
This is on adjusted version form the original "HDTS - Helmet Display" Mod on stem workshop:
https://reforger.armaplatform.com/workshop/7BABD785CD00B3F6-HDTS-HelmetDisplay

*/

//! What a reported point is; decides the symbol shape, colouring and which line it can drive
enum HMD_ETargetKind{
	TARGET,		//!< Designated target (laser spot etc.): diamond, green blinking when matched, amber otherwise; drives the target line
	WAYPOINT,	//!< Navigation waypoint: box, provider colour; the primary one drives the nav line
	MARKER		//!< Map marker: small box, provider colour (affiliation); decluttered first
}

//! One symbol to draw. Filled by providers, consumed by HMD_Symbology.
class HMD_Target{
	vector vectorPosition;					//!< World position of the point
	string labelName;						//!< Short label drawn next to the symbol, e.g. the laser code or "WP3"
	string targetSource;					//!< Where it comes from, e.g. "LASER", shown on the target / nav line
	HMD_ETargetKind targetKind = HMD_ETargetKind.TARGET;
	bool codeMatched = true;				//!< TARGET only: true when the target belongs to us (code matches): green and blinking, else amber
	bool isPrimary;							//!< The one target the target line (or, for waypoints, the nav line) describes
	ref Color color;						//!< Optional colour; null = default for the kind
	float filterRange;						//!< Filled by the display: distance from the aircraft in metres
	
	// Optional graphic fields for ImageWidget / texture rendering
	ResourceName iconTexture;				//!< Optional texture/imageset resource path; empty = use default shape
	string iconQuad;						//!< Optional quad/sub-image name when iconTexture is an imageset
	bool tintIcon = true;					//!< True = apply `color` tint to the graphic; false = keep native graphic colours
	bool HasIconTexture(){
		return !iconTexture.IsEmpty();
	}
}

//------------------------------------------------------------------------------------------------
//! Base class for anything that can feed symbols to the HMD
class HMD_TargetProvider : Managed{
	//! Fill outTargets with the symbols to show for the local pilot of this aircraft
	void GetTargets(notnull IEntity vehicle, notnull array<ref HMD_Target> outTargets);
}

//------------------------------------------------------------------------------------------------
//! Static list of providers, one instance per provider class
class HMD_TargetRegistry{
	protected static ref array<ref HMD_TargetProvider> s_aProviders = {};

	//------------------------------------------------------------------------------------------------
	static void Register(notnull HMD_TargetProvider provider){
		typename type = provider.Type();
		foreach (HMD_TargetProvider existing : s_aProviders){
			if (existing.Type() == type)
				return;
		}

		s_aProviders.Insert(provider);
		Print("[CSWG_HMD] Target provider registered: " + type.ToString(), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	static int Count(){
		return s_aProviders.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Collects symbols from every provider. Returns the number of symbols.
	static int Collect(notnull IEntity vehicle, notnull array<ref HMD_Target> outTargets){
		outTargets.Clear();
		foreach (HMD_TargetProvider provider : s_aProviders)
		{
			provider.GetTargets(vehicle, outTargets);
		}

		return outTargets.Count();
	}
}
