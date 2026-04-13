#pragma once
#include <string>
#include <array>
using namespace std;
/**
 * Immutable specification for one eVTOL manufacturer type.
 *
 */
struct VehicleSpec {
	string companyName;
    double cruiseSpeedMph;        ///< mph
    double batteryCapacityKwh;    ///< kWh
    double timeToChargeHours;     ///< hours for a full charge
    double energyUseKwhPerMile;   ///< kWh consumed per mile at cruise
    int    passengerCount;
    double faultProbabilityPerHour; ///< probability of a fault occurring each hour
    double maxRangeMiles() const {
        return batteryCapacityKwh / energyUseKwhPerMile;
    }

    /// Max airborne duration on a full charge (hours)
    double maxFlightHours() const {
        return maxRangeMiles() / cruiseSpeedMph;
    }
};
//The five manufacturers defined in the problem statement.
inline const std::array<VehicleSpec, 5> ALL_SPECS = { {
    {"Alpha",   120, 320, 0.60, 1.6, 4, 0.25},
    {"Bravo",   100, 100, 0.20, 1.5, 5, 0.10},
    {"Charlie", 160, 220, 0.80, 2.2, 3, 0.05},
    {"Delta",    90, 120, 0.62, 0.8, 2, 0.22},
    {"Echo",     30, 150, 0.30, 5.8, 2, 0.61},
} };
