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

#if  RUN_TESTS 
// ---------------------------------------------------------------------------
// Minimal test harness
// ---------------------------------------------------------------------------
static int g_pass = 0, g_fail = 0;

#define TEST(name, expr) \
    do { \
        if (expr) { \
            std::cout << "[PASS] " << name << "\n"; \
            ++g_pass; \
        } else { \
            std::cout << "[FAIL] " << name << "\n"; \
            ++g_fail; \
        } \
    } while (0)

// ---------------------------------------------------------------------------
// VehicleSpec tests
// ---------------------------------------------------------------------------
static void testVehicleSpec() {
    std::cout << "\n-- VehicleSpec --\n";

    // Alpha: range = 320 / 1.6 = 200 miles
    TEST("Alpha max range", ALL_SPECS[0].maxRangeMiles() == 200.0);

    // Alpha: flight hours = 200 / 120 = 1.6667 h
    TEST("Alpha max flight hours",
        std::abs(ALL_SPECS[0].maxFlightHours() - (200.0 / 120.0)) < 1e-9);

    // Bravo: range = 100 / 1.5 = 66.667 miles
    TEST("Bravo max range",
        std::abs(ALL_SPECS[1].maxRangeMiles() - (100.0 / 1.5)) < 1e-9);

    // Echo: energy use 5.8 kWh/mi, capacity 150 kWh → 25.86 miles
    TEST("Echo max range",
        std::abs(ALL_SPECS[4].maxRangeMiles() - (150.0 / 5.8)) < 1e-6);
}

// ---------------------------------------------------------------------------
// ChargerPool tests
// ---------------------------------------------------------------------------
static void testChargerPool() {
    std::cout << "\n-- ChargerPool --\n";

    // 2-charger pool
    ChargerPool pool(2);
    TEST("Initial free chargers == 2", pool.freeChargers() == 2);

    std::vector<int> granted;
    auto cb = [&](int id, double /*t*/) { granted.push_back(id); };

    // Grant vehicle 0 immediately
    bool imm0 = pool.requestCharger(0, 0.0, cb);
    TEST("Vehicle 0 granted immediately", imm0 == true);
    TEST("Free chargers == 1 after first grant", pool.freeChargers() == 1);

    // Grant vehicle 1 immediately
    bool imm1 = pool.requestCharger(1, 0.0, cb);
    TEST("Vehicle 1 granted immediately", imm1 == true);
    TEST("Free chargers == 0 after second grant", pool.freeChargers() == 0);

    // Vehicle 2 must wait
    bool imm2 = pool.requestCharger(2, 0.0, cb);
    TEST("Vehicle 2 queued (not immediate)", imm2 == false);
    TEST("Wait queue size == 1", pool.waitingCount() == 1);

    // Release one charger → vehicle 2 gets it
    pool.releaseCharger(1.0);
    TEST("Vehicle 2 granted after release", granted.size() == 3);
    TEST("Wait queue empty after release", pool.waitingCount() == 0);
    TEST("Free chargers still 0 (given to waiter)", pool.freeChargers() == 0);

    // Release second charger → pool has 1 free
    pool.releaseCharger(2.0);
    TEST("Free chargers == 1 after second release", pool.freeChargers() == 1);
}

// ---------------------------------------------------------------------------
// TypeStats tests
// ---------------------------------------------------------------------------
static void testTypeStats() {
    std::cout << "\n-- TypeStats --\n";

    TypeStats s;
    s.flightDurationsHours = { 1.0, 2.0, 3.0 };
    s.flightDistancesMiles = { 100.0, 200.0 };
    s.chargeSessionsHours = { 0.5 };

    TEST("avgFlightTime == 2.0",
        std::abs(s.avgFlightTimeHours() - 2.0) < 1e-9);
    TEST("avgFlightDistance == 150.0",
        std::abs(s.avgFlightDistanceMiles() - 150.0) < 1e-9);
    TEST("avgChargeTime == 0.5",
        std::abs(s.avgChargeTimeHours() - 0.5) < 1e-9);

    TypeStats empty;
    TEST("avgFlightTime empty == 0", empty.avgFlightTimeHours() == 0.0);
}

// ---------------------------------------------------------------------------
// Vehicle state-machine tests
// ---------------------------------------------------------------------------
static void testVehicleLandAndCharge() {
    std::cout << "\n-- Vehicle state machine --\n";

    std::mt19937 rng(0);
    Vehicle v(0, ALL_SPECS[0] /*Alpha*/, rng);

    TEST("Initial state == FLYING", v.state() == Vehicle::State::FLYING);

    TypeStats stats;
    stats.companyName = "Alpha";

    // Land at t=1.0 (Alpha max flight = 1.6667h so 1.0h is partial)
    v.land(1.0, stats);
    TEST("After land state == WAITING", v.state() == Vehicle::State::WAITING_FOR_CHARGER);
    TEST("Flight duration recorded", stats.flightDurationsHours.size() == 1);
    TEST("Flight duration == 1.0h", std::abs(stats.flightDurationsHours[0] - 1.0) < 1e-9);
    TEST("Distance == 120 miles", std::abs(stats.flightDistancesMiles[0] - 120.0) < 1e-9);
    TEST("Pax miles == 4*120=480", std::abs(stats.totalPassengerMiles - 480.0) < 1e-9);

    // Charger granted at t=1.2
    v.startCharging(1.2);
    TEST("After startCharging state == CHARGING", v.state() == Vehicle::State::CHARGING);

    // Charge done at t=1.2+0.6=1.8
    double nextEmpty = v.finishCharging(1.8, stats);
    TEST("After finishCharging state == FLYING", v.state() == Vehicle::State::FLYING);
    TEST("Charge session recorded", stats.chargeSessionsHours.size() == 1);
    TEST("Charge session == 0.6h",
        std::abs(stats.chargeSessionsHours[0] - 0.6) < 1e-9);
    // Next empty = 1.8 + 200/120 = 1.8 + 1.6667
    TEST("Next empty time correct",
        std::abs(nextEmpty - (1.8 + ALL_SPECS[0].maxFlightHours())) < 1e-9);
}
static void testWaitQueueOrder() {
    ChargerPool pool(1);  // 只有1个桩
    std::vector<int> grantOrder;
    auto cb = [&](int id, double) { grantOrder.push_back(id); };

    pool.requestCharger(0, 0.0, cb);  // 立刻拿到
    pool.requestCharger(1, 0.1, cb);  // 进队列
    pool.requestCharger(2, 0.2, cb);  // 进队列
    pool.requestCharger(3, 0.3, cb);  // 进队列

    pool.releaseCharger(1.0);  // 给id=1
    pool.releaseCharger(2.0);  // 给id=2
    pool.releaseCharger(3.0);  // 给id=3

    // 严格按入队顺序：1→2→3
    TEST("waitQueue FIFO order", grantOrder == std::vector<int>{0, 1, 2, 3});
}
// ---------------------------------------------------------------------------
int main() {
    std::cout << "=== eVTOL Unit Tests ===\n";
    testVehicleSpec();
    testChargerPool();
    testTypeStats();
    testVehicleLandAndCharge();

    std::cout << "\n=== Summary: " << g_pass << " passed, "
        << g_fail << " failed ===\n";
    return g_fail > 0 ? 1 : 0;
}
#else
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

#endif //  TEST


