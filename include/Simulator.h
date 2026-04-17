#pragma once

// Set to 1 to enable unit test helpers, 0 for release build
#define UNIT_TEST 1

#include "Vehicle.h"
#include "ChargerPool.h"
#include "Statistics.h"
#include "VehicleSpec.h"

#include <vector>
#include <queue>
#include <memory>
#include <random>
#include <unordered_map>
#include <functional>
using namespace std;
/**
 * @brief Event-driven eVTOL simulation.
 *
 * Design overview
 * ---------------
 * Time advances by processing a priority-queue of future events (smallest
 * time first).  Each event carries the simulation time and a lambda that
 * performs the state transition.  This gives O(E log E) complexity and
 * avoids the inaccuracy of fixed time-step approaches.
 *
 * Events
 * ------
 *  BATTERY_EMPTY  – vehicle runs out of power and lands; asks ChargerPool
 *                   for a charger (may be deferred if all busy).
 *  CHARGE_DONE    – vehicle finishes charging; immediately takes off and
 *                   schedules its next BATTERY_EMPTY event.
 *
 * The ChargerPool callback fires immediately or when a charger is released,
 * injecting a CHARGE_DONE event into the queue.
 *
 * Assumptions documented here
 * ---------------------------
 * 1. Vehicles start fully charged at t=0 and immediately take off.
 * 2. A vehicle that runs out of battery after simEnd is ignored (it never
 *    lands within the window).
 * 3. Partial flights at simEnd ARE recorded (pro-rated stats).
 * 4. A vehicle waiting for a charger at simEnd does NOT get a charge session
 *    recorded (never started charging).
 * 5. Vehicles that start charging but don't finish by simEnd: the partial
 *    charge session is NOT recorded in chargeSessionsHours.
 */
class Simulator {
public:
    static constexpr int    NUM_VEHICLES = 20;
    static constexpr int    NUM_CHARGERS = 3;
    static constexpr double SIM_DURATION = 3.0; ///< hours

    /**
     * @param seed  RNG seed; use different seeds for independent runs.
     */
    explicit Simulator(unsigned int seed = 42);

    /**
     * @brief Run the full 3-hour simulation.
     * @return Per-type statistics indexed by company name.
     */
    std::unordered_map<std::string, TypeStats> run();

#if UNIT_TEST
    // Expose internal queue size for unit testing only.
    // Compiled out entirely when UNIT_TEST == 0.
    int pendingEventCount() const {
        return static_cast<int>(eventQueue_.size());
    }
#endif

private:
    // ------------------------------------------------------------------ //
    //  Internal event type
    // ------------------------------------------------------------------ //
    
    struct Event {
        double time;
        std::function<void()> action;
        bool operator > (const Event &other) const {
            return time > other.time;
        }
    };
    void scheduleEvent(double time, std::function<void()> f);
    void handleBatteryEmpty(int vehicleId);
    void handleChargeDone(int vehicleId, double chargerGrantedAt);
    // ------------------------------------------------------------------ //
    //  State
    // ------------------------------------------------------------------ //
    
    // stats bucket per spec index (0-4)
    
    std::mt19937 rng_;
    ChargerPool chargerPool_;
    double currentTime_ = 0.0;

    // Map vehicleId -> spec index (0-4) for O(1) stats lookup

    std::vector<std::unique_ptr<Vehicle>> vehicles_;
    std::array<TypeStats, 5 > stats_;
    std::priority_queue<Event, vector<Event>, greater<Event>> eventQueue_;

    std::unordered_map<int, int> vehicleSpecIndex_;

};
