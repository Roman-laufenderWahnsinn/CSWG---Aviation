/*
Helment Mounted Display Mod for the Combined Special Warfare Group
Autohor: Roman Alexander "Trojan" Löwe
This is on adjusted version form the original "HDTS - Helmet Display" Mod on stem workshop:
https://reforger.armaplatform.com/workshop/7BABD785CD00B3F6-HDTS-HelmetDisplay

*/

//! What a reported point is; decides the symbol shape, colouring and which line it can drive
enum HDTS_ETargetKind
{
	TARGET,		//!< Designated target (laser spot etc.): diamond, green blinking when matched, amber otherwise; drives the target line
	WAYPOINT,	//!< Navigation waypoint: box, provider colour; the primary one drives the nav line
	MARKER		//!< Map marker: small box, provider colour (affiliation); decluttered first
}

//! One symbol to draw. Filled by providers, consumed by HDTS_Symbology.
class HDTS_Target
{
	vector m_vPosition;		//!< World position of the point
	string m_sLabel;		//!< Short label drawn next to the symbol, e.g. the laser code or "WP3"
	string m_sSource;		//!< Where it comes from, e.g. "LASER", shown on the target / nav line
	HDTS_ETargetKind m_eKind = HDTS_ETargetKind.TARGET;
	bool m_bMatched = true;	//!< TARGET only: true when the target belongs to us (code matches): green and blinking, else amber
	bool m_bPrimary;		//!< The one target the target line (or, for waypoints, the nav line) describes
	ref Color m_Color;		//!< Optional colour; null = default for the kind
	float m_fRange;			//!< Filled by the display: distance from the aircraft in metres
}

//------------------------------------------------------------------------------------------------
//! Base class for anything that can feed symbols to the HMD
class HDTS_TargetProvider : Managed
{
	//! Fill outTargets with the symbols to show for the local pilot of this aircraft
	void GetTargets(notnull IEntity vehicle, notnull array<ref HDTS_Target> outTargets);
}

//------------------------------------------------------------------------------------------------
//! Static list of providers, one instance per provider class
class HDTS_TargetRegistry
{
	protected static ref array<ref HDTS_TargetProvider> s_aProviders = {};

	//------------------------------------------------------------------------------------------------
	static void Register(notnull HDTS_TargetProvider provider)
	{
		typename type = provider.Type();
		foreach (HDTS_TargetProvider existing : s_aProviders)
		{
			if (existing.Type() == type)
				return;
		}

		s_aProviders.Insert(provider);
		Print("[HDTS] Target provider registered: " + type.ToString(), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		return s_aProviders.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Collects symbols from every provider. Returns the number of symbols.
	static int Collect(notnull IEntity vehicle, notnull array<ref HDTS_Target> outTargets)
	{
		outTargets.Clear();
		foreach (HDTS_TargetProvider provider : s_aProviders)
		{
			provider.GetTargets(vehicle, outTargets);
		}

		return outTargets.Count();
	}
}
