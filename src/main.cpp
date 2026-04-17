#include "../include/Simulator.h"
#include "../include/VehicleSpec.h"
#include <iostream>
#include <iomanip>
#include <string>

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
static void printResults(const std::unordered_map<std::string, TypeStats>& results) {
    std::cout << "\n";
    std::cout << std::string(72, '=') << "\n";
    std::cout << "  eVTOL Simulation Results  (3-hour window, 20 vehicles, 3 chargers)\n";
    std::cout << std::string(72, '=') << "\n\n";

    for (const auto& spec : ALL_SPECS) {
        auto it = results.find(spec.companyName);
        if (it == results.end()) continue;
        const TypeStats& s = it->second;

        std::cout << std::left << std::setw(12) << spec.companyName << "\n";
        std::cout << std::string(40, '-') << "\n";
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "  Avg flight time     : " << s.avgFlightTimeHours()      << " h\n";
        std::cout << "  Avg flight distance : " << s.avgFlightDistanceMiles()  << " miles\n";
        std::cout << "  Avg charge time     : " << s.avgChargeTimeHours()      << " h\n";
        std::cout << "  Total faults        : " << s.totalFaults               << "\n";
        std::cout << "  Total pax-miles     : " << s.totalPassengerMiles       << "\n";
        std::cout << "  Flights recorded    : " << s.flightDurationsHours.size() << "\n";
        std::cout << "  Charge sessions     : " << s.chargeSessionsHours.size()  << "\n";
        std::cout << "\n";
    }
}

// ============================================================================
// All test code lives inside #if UNIT_TEST ... #endif.
// When UNIT_TEST == 0 the entire block below is invisible to the compiler.
// ============================================================================
#if UNIT_TEST

static int g_pass = 0, g_fail = 0;

#define RUN_TEST(name, expr) \
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
// Group 1: VehicleSpec — data table and formula correctness
// ---------------------------------------------------------------------------
static void testVehicleSpec() {
    std::cout << "\n-- VehicleSpec --\n";

    // Alpha: range = 320 / 1.6 = 200 miles
    RUN_TEST("Alpha max range", ALL_SPECS[0].maxRangeMiles() == 200.0);

    // Alpha: flight hours = 200 / 120 = 1.6667h
    RUN_TEST("Alpha max flight hours",
        std::abs(ALL_SPECS[0].maxFlightHours() - (200.0 / 120.0)) < 1e-9);

    // Bravo: range = 100 / 1.5 = 66.667 miles
    RUN_TEST("Bravo max range",
        std::abs(ALL_SPECS[1].maxRangeMiles() - (100.0 / 1.5)) < 1e-9);

    // Echo: range = 150 / 5.8 = 25.86 miles
    RUN_TEST("Echo max range",
        std::abs(ALL_SPECS[4].maxRangeMiles() - (150.0 / 5.8)) < 1e-6);
}

// ---------------------------------------------------------------------------
// Group 2: ChargerPool — resource allocation
// ---------------------------------------------------------------------------
static void testChargerPool() {
    std::cout << "\n-- ChargerPool --\n";

    ChargerPool pool(2);
    RUN_TEST("Initial free chargers == 2", pool.freeChargers() == 2);

    std::vector<int> granted;
    auto cb = [&](int id, double) { granted.push_back(id); };

    bool imm0 = pool.requestCharger(0, 0.0, cb);
    RUN_TEST("Vehicle 0 granted immediately",        imm0 == true);
    RUN_TEST("Free chargers == 1 after first grant", pool.freeChargers() == 1);

    bool imm1 = pool.requestCharger(1, 0.0, cb);
    RUN_TEST("Vehicle 1 granted immediately",         imm1 == true);
    RUN_TEST("Free chargers == 0 after second grant", pool.freeChargers() == 0);

    bool imm2 = pool.requestCharger(2, 0.0, cb);
    RUN_TEST("Vehicle 2 queued (not immediate)", imm2 == false);
    RUN_TEST("Wait queue size == 1",             pool.waitingCount() == 1);

    pool.releaseCharger(1.0);
    RUN_TEST("Vehicle 2 granted after release",          granted.size() == 3);
    RUN_TEST("Wait queue empty after release",           pool.waitingCount() == 0);
    RUN_TEST("Free chargers still 0 (given to waiter)",  pool.freeChargers() == 0);

    pool.releaseCharger(2.0);
    RUN_TEST("Free chargers == 1 after second release",  pool.freeChargers() == 1);
}

// ---------------------------------------------------------------------------
// Group 3: waitQueue_ FIFO ordering
// Vehicles must be granted chargers strictly in landing-time order.
// ---------------------------------------------------------------------------
static void testWaitQueueOrder() {
    std::cout << "\n-- waitQueue_ FIFO Order --\n";

    ChargerPool pool(1);  // single charger forces all others to queue
    std::vector<int> grantOrder;
    auto cb = [&](int id, double) { grantOrder.push_back(id); };

    pool.requestCharger(0, 0.0, cb);  // granted immediately
    pool.requestCharger(1, 0.1, cb);  // queued
    pool.requestCharger(2, 0.2, cb);  // queued
    pool.requestCharger(3, 0.3, cb);  // queued

    RUN_TEST("Three vehicles in wait queue", pool.waitingCount() == 3);

    pool.releaseCharger(1.0);
    pool.releaseCharger(2.0);
    pool.releaseCharger(3.0);

    // Strict FIFO: must be granted in arrival order 0→1→2→3
    RUN_TEST("Grant order is FIFO: 0,1,2,3",
        grantOrder == std::vector<int>({0, 1, 2, 3}));
    RUN_TEST("Wait queue empty after all released", pool.waitingCount() == 0);
}

// ---------------------------------------------------------------------------
// Group 4: TypeStats — aggregate calculations and edge cases
// ---------------------------------------------------------------------------
static void testTypeStats() {
    std::cout << "\n-- TypeStats --\n";

    TypeStats s;
    s.flightDurationsHours = { 1.0, 2.0, 3.0 };
    s.flightDistancesMiles = { 100.0, 200.0 };
    s.chargeSessionsHours  = { 0.5 };

    RUN_TEST("avgFlightTime == 2.0",
        std::abs(s.avgFlightTimeHours() - 2.0) < 1e-9);
    RUN_TEST("avgFlightDistance == 150.0",
        std::abs(s.avgFlightDistanceMiles() - 150.0) < 1e-9);
    RUN_TEST("avgChargeTime == 0.5",
        std::abs(s.avgChargeTimeHours() - 0.5) < 1e-9);

    // Edge case: empty vectors must return 0, not crash with divide-by-zero
    TypeStats empty;
    RUN_TEST("avgFlightTime empty == 0",  empty.avgFlightTimeHours()  == 0.0);
    RUN_TEST("avgChargeTime empty == 0",  empty.avgChargeTimeHours()  == 0.0);
}

// ---------------------------------------------------------------------------
// Group 5: Vehicle state machine — full lifecycle
// ---------------------------------------------------------------------------
static void testVehicleStateMachine() {
    std::cout << "\n-- Vehicle State Machine --\n";

    std::mt19937 rng(0);
    Vehicle v(0, ALL_SPECS[0] /*Alpha*/, rng);

    RUN_TEST("Initial state == FLYING", v.state() == Vehicle::State::FLYING);

    TypeStats stats;
    stats.companyName = "Alpha";

    // Land at t=1.0 (Alpha max flight = 1.6667h, so this is a partial flight)
    v.land(1.0, stats);
    RUN_TEST("After land: state == WAITING_FOR_CHARGER",
        v.state() == Vehicle::State::WAITING_FOR_CHARGER);
    RUN_TEST("Flight duration recorded",
        stats.flightDurationsHours.size() == 1);
    RUN_TEST("Flight duration == 1.0h",
        std::abs(stats.flightDurationsHours[0] - 1.0) < 1e-9);
    RUN_TEST("Distance == 120 miles",
        std::abs(stats.flightDistancesMiles[0] - 120.0) < 1e-9);
    RUN_TEST("Passenger miles == 4 * 120 = 480",
        std::abs(stats.totalPassengerMiles - 480.0) < 1e-9);

    // Charger granted at t=1.2 (waited 0.2h in queue)
    v.startCharging(1.2);
    RUN_TEST("After startCharging: state == CHARGING",
        v.state() == Vehicle::State::CHARGING);

    // Charge done at t = 1.2 + 0.6 = 1.8
    double nextEmpty = v.finishCharging(1.8, stats);
    RUN_TEST("After finishCharging: state == FLYING",
        v.state() == Vehicle::State::FLYING);
    RUN_TEST("Charge session recorded",
        stats.chargeSessionsHours.size() == 1);
    RUN_TEST("Charge session duration == 0.6h",
        std::abs(stats.chargeSessionsHours[0] - 0.6) < 1e-9);
    RUN_TEST("Next battery-empty time correct",
        std::abs(nextEmpty - (1.8 + ALL_SPECS[0].maxFlightHours())) < 1e-9);
}

// ---------------------------------------------------------------------------
// Group 6: eventQueue_ — scheduling and discard logic
// Uses pendingEventCount() which is only compiled when UNIT_TEST == 1.
// ---------------------------------------------------------------------------
static void testEventQueue() {
    std::cout << "\n-- eventQueue_ Scheduling --\n";

    Simulator sim(42);

    // Before run(), no events are scheduled
    RUN_TEST("No events before run()", sim.pendingEventCount() == 0);

    // scheduleEvent is private so we drive it indirectly by calling run()
    // and observing side effects, OR we test the discard boundary by
    // constructing a minimal Simulator and checking counts after run().
    // Here we verify the queue drains completely after a full run.
    sim.run();
    RUN_TEST("eventQueue_ fully drained after run()", sim.pendingEventCount() == 0);

    // Second simulator: verify initial count before any scheduling
    Simulator sim2(99);
    RUN_TEST("Fresh sim2 starts with 0 pending events", sim2.pendingEventCount() == 0);

    // After run(), queue must again be empty
    sim2.run();
    RUN_TEST("sim2 eventQueue_ drained after run()", sim2.pendingEventCount() == 0);
}

// ---------------------------------------------------------------------------
// Group 7: Vehicle time variables
// Verifies flightStartTime_, chargeWaitStartTime_, chargeStartTime_
// are set correctly at each state transition.
// ---------------------------------------------------------------------------
static void testVehicleTimeVariables() {
    std::cout << "\n-- Vehicle Time Variables --\n";

    std::mt19937 rng(0);
    Vehicle v(0, ALL_SPECS[0] /*Alpha*/, rng);
    TypeStats stats;
    stats.companyName = "Alpha";

    // t=0: vehicle starts flying, flightStartTime_ initialised to 0
    RUN_TEST("flightStartTime_ == 0 at construction",
        std::abs(v.flightStartTime() - 0.0) < 1e-9);

    // Land at t=1.5 → chargeWaitStartTime_ must be set to landing time
    v.land(1.5, stats);
    RUN_TEST("chargeWaitStartTime_ == 1.5 after landing",
        std::abs(v.chargeWaitStartTime() - 1.5) < 1e-9);

    // Charger granted at t=2.0 → chargeStartTime_ must record grant time
    v.startCharging(2.0);
    RUN_TEST("chargeStartTime_ == 2.0 after startCharging",
        std::abs(v.chargeStartTime() - 2.0) < 1e-9);

    // Finish charging at t=2.6 → flightStartTime_ reset to finish time
    double nextEmpty = v.finishCharging(2.6, stats);
    RUN_TEST("flightStartTime_ == 2.6 after finishCharging",
        std::abs(v.flightStartTime() - 2.6) < 1e-9);

    // nextEmpty = 2.6 + maxFlightHours (Alpha = 200/120 = 1.6667h)
    RUN_TEST("nextEmpty == 2.6 + maxFlightHours",
        std::abs(nextEmpty - (2.6 + ALL_SPECS[0].maxFlightHours())) < 1e-9);

    // Verify wait duration = chargeStartTime_ - chargeWaitStartTime_ = 0.5h
    double waitDuration = v.chargeStartTime() - v.chargeWaitStartTime();
    // After finishCharging chargeWaitStartTime_ still holds the last landing time (1.5)
    // and chargeStartTime_ still holds the grant time (2.0)
    RUN_TEST("Wait duration == chargeStart - chargeWaitStart == 0.5h",
        std::abs(waitDuration - 0.5) < 1e-9);

    // Second flight cycle: land again at t=3.0
    v.land(3.0, stats);
    RUN_TEST("chargeWaitStartTime_ updated to 3.0 on second landing",
        std::abs(v.chargeWaitStartTime() - 3.0) < 1e-9);

    // Immediate charger at t=3.0 (no wait)
    v.startCharging(3.0);
    RUN_TEST("chargeStartTime_ == 3.0 when no wait",
        std::abs(v.chargeStartTime() - 3.0) < 1e-9);
    RUN_TEST("Zero wait when charger available immediately",
        std::abs(v.chargeStartTime() - v.chargeWaitStartTime()) < 1e-9);
}

// ---------------------------------------------------------------------------
int main() {
    std::cout << "=== eVTOL Unit Tests ===\n";

    testVehicleSpec();
    testChargerPool();
    testWaitQueueOrder();
    testTypeStats();
    testVehicleStateMachine();
    testEventQueue();
    testVehicleTimeVariables();

    std::cout << "\n=== Summary: " << g_pass << " passed, "
              << g_fail << " failed ===\n";
    return g_fail > 0 ? 1 : 0;
}

// ============================================================================
// UNIT_TEST == 0: only simulation runs, zero test code in binary
// ============================================================================
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

#endif // UNIT_TEST
