#include "../include/Simulator.h"
#include "../include/VehicleSpec.h"
#include <iostream>
#include <iomanip>
#include <string>
#define RUN_TESTS  0
static void printResults(const std::unordered_map<std::string, TypeStats>& results) {
    std::cout << "\n";
    std::cout << std::string(72, '=') << "\n";
    std::cout << "  eVTOL Simulation Results  (3-hour window, 20 vehicles, 3 chargers)\n";
    std::cout << std::string(72, '=') << "\n\n";

    // Print in spec order for consistency
    for (const auto& spec : ALL_SPECS) {
        auto it = results.find(spec.companyName);
        if (it == results.end()) continue;
        const TypeStats& s = it->second;

        std::cout << std::left << std::setw(12) << spec.companyName << "\n";
        std::cout << std::string(40, '-') << "\n";
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "  Avg flight time     : " << s.avgFlightTimeHours()     << " h\n";
        std::cout << "  Avg flight distance : " << s.avgFlightDistanceMiles()  << " miles\n";
        std::cout << "  Avg charge time     : " << s.avgChargeTimeHours()      << " h\n";
        std::cout << "  Total faults        : " << s.totalFaults               << "\n";
        std::cout << "  Total pax-miles     : " << s.totalPassengerMiles       << "\n";
        std::cout << "  Flights recorded    : " << s.flightDurationsHours.size() << "\n";
        std::cout << "  Charge sessions     : " << s.chargeSessionsHours.size()  << "\n";
        std::cout << "\n";
    }
}


int main(int argc, char* argv[]) {
    unsigned int seed = 42;
    if (argc > 1) {
        try { seed = static_cast<unsigned int>(std::stoul(argv[1])); }
        catch (...) { std::cerr << "Invalid seed argument; using 42.\n"; }
    }

    std::cout << "Running eVTOL simulation (seed=" << seed << ")...\n\n";
    Simulator sim(seed);
    auto results = sim.run();
    printResults(results);
    return 0;
}




