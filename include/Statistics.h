#pragma once
#include <string>
#include <vector>

/**
 * @brief Accumulates raw data for one vehicle type across the whole simulation.
 *
 * Intentionally stores raw lists so callers can compute mean/std-dev
 * or any other aggregate without losing information.
 */
struct TypeStats {
    std::string companyName;

    std::vector<double> flightDurationsHours;  ///< one entry per completed flight
    std::vector<double> flightDistancesMiles;  ///< one entry per completed flight
    std::vector<double> chargeSessionsHours;   ///< one entry per completed charge
    int    totalFaults         = 0;
    double totalPassengerMiles = 0.0;

    // ------------------------------------------------------------------ //
    //  Computed aggregates (called after simulation ends)
    // ------------------------------------------------------------------ //

    double avgFlightTimeHours() const {
        if (flightDurationsHours.empty()) return 0.0;
        double sum = 0;
        for (double v : flightDurationsHours) sum += v;
        return sum / static_cast<double>(flightDurationsHours.size());
    }

    double avgFlightDistanceMiles() const {
        if (flightDistancesMiles.empty()) return 0.0;
        double sum = 0;
        for (double v : flightDistancesMiles) sum += v;
        return sum / static_cast<double>(flightDistancesMiles.size());
    }

    double avgChargeTimeHours() const {
        if (chargeSessionsHours.empty()) return 0.0;
        double sum = 0;
        for (double v : chargeSessionsHours) sum += v;
        return sum / static_cast<double>(chargeSessionsHours.size());
    }
};
