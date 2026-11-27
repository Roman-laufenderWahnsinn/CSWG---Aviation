// Helper class for fuel calculation to display a remaining time on fuel

class HMD_FuelComputer {
	protected float lastFuelState;
	
	void HMD_FuelComputer(notnull IEntity vehicle){
		FuelManagerComponent fuelComp = FuelManagerComponent.Cast(vehicle.FindComponent(FuelManagerComponent));
		lastFuelState = fuelComp.GetTotalFuel();
	}
	
	
	//!calculates and returns a string with min:sec on remaing fuel with current average consumption
	string remaingTimeOnFuel(notnull IEntity vehicle, float deltaLastUpdate){
		FuelManagerComponent fuelComp = FuelManagerComponent.Cast(vehicle.FindComponent(FuelManagerComponent));
		float remainingFuel = fuelComp.GetTotalFuel();
		float usedFuel = lastFuelState - remainingFuel;
		
		if (usedFuel == 0.0)
			return "--";
		if (deltaLastUpdate == 0.0)
			return "--";
		
		lastFuelState = remainingFuel;
		float burnRatePerSecond = usedFuel / deltaLastUpdate;

		if (burnRatePerSecond < 0.0)
			return "--";
		
		float remainingSeconds = remainingFuel / burnRatePerSecond;
		
		if(remainingSeconds < 1.0) {
			return "--";
		}
		int total = Math.Round(remainingSeconds);
		return (total / 60).ToString() + ":" + (total % 60).ToString(2);
	}
}