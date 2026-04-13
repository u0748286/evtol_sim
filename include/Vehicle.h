#pragma once
#include "VehicleSpec.h"
#include "Statistics.h"
#include <cassert>
#include <random>
#include <string>

/**
 * @brief Represents one physical eVTOL aircraft.
 *
 * Vehicles are passive data holders from the simulator's perspective.
 * The Simulator drives state transitions by calling the appropriate methods.
 *
 * State machine:
 *   FLYING  --(battery empty)--> WAITING_FOR_CHARGER
 *   WAITING_FOR_CHARGER --(charger granted)--> CHARGING
 *   CHARGING --(charge complete)--> FLYING
 *
 * If the simulation ends while a vehicle is mid-flight the partial flight
 * is recorded; partial charge sessions are not recorded (vehicle never
 * reached the charger or didn't finish charging within the window).
 */
class Vehicle {
public:
    enum class State { FLYING, WAITING_FOR_CHARGER, CHARGING };

    Vehicle(int id, const VehicleSpec& spec, std::mt19937& rng)
        : id_(id), spec_(spec), rng_(rng), state_(State::FLYING),
          flightStartTime_(0.0), chargeWaitStartTime_(0.0) {}

    // ------------------------------------------------------------------ //
    //  Accessors
    // ------------------------------------------------------------------ //
    int               id()    const { return id_; }
    const VehicleSpec& spec() const { return spec_; }
    State             state() const { return state_; }

    /// Simulation time when the current state began.
    double flightStartTime()     const { return flightStartTime_; }
    double chargeWaitStartTime() const { return chargeWaitStartTime_; }

    // ------------------------------------------------------------------ //
    //  State transitions (called by Simulator)
    // ------------------------------------------------------------------ //

    /**
     * @brief Record a completed flight and transition to WAITING_FOR_CHARGER.
     *
     * @param landTime   Simulation time of landing.
     * @param stats      TypeStats bucket to update.
     */
    void land(double landTime, TypeStats& stats) {
        assert(state_ == State::FLYING);
        auto dur = landTime - flightStartTime_;
        auto dist = dur * spec_.cruiseSpeedMph;
        auto fault = sampleFaults(dur);
        stats.flightDurationsHours.push_back(dur);
        stats.flightDistancesMiles.push_back(dist);
        stats.totalFaults += fault;
        stats.totalPassengerMiles += static_cast<double>(spec_.passengerCount) * dist;
        state_ = State::WAITING_FOR_CHARGER;

        chargeWaitStartTime_ = landTime;
    }

    /**
     * @brief Charger granted – begin charging.
     * @param grantTime Simulation time when the charger became available.
     */
    void startCharging(double grantTime) {
        assert(state_ == State::WAITING_FOR_CHARGER);
        chargeStartTime_ = grantTime;

        state_ = State::CHARGING;
    }

    /**
     * @brief Charge complete – take off again.
     *
     * @param doneTime  Simulation time when charging finished.
     * @param stats     TypeStats bucket to update.
     * @return          Time at which the next battery-empty event occurs.
     */
    double finishCharging(double doneTime, TypeStats& stats) {
        assert(state_ == State::CHARGING);
        double sessionDuration = doneTime - chargeStartTime_;
        stats.chargeSessionsHours.push_back(sessionDuration);

        state_ = State::FLYING;

        flightStartTime_ = doneTime;
        return doneTime + spec_.maxFlightHours();
    }

    /**
     * @brief Record a partial flight at simulation end (no stats update for
     *        incomplete flights – see assumption note in Simulator.cpp).
     */
    void recordPartialFlight(double simEndTime, TypeStats& stats) {
        if (state_ != State::FLYING) return;
        // Assumption: partial flights at sim end ARE counted so the stats
        // reflect actual time airborne, not just completed cycles.
        land(simEndTime, stats);
    }

private:
    /// Sample how many fault events occurred during `durationHours` of flight.
    /// Uses a Poisson-approximation via repeated Bernoulli trials on 1-minute
    /// intervals, which is consistent with the per-hour probability given.
    int sampleFaults(double durationHours) {
        // Probability of at least one fault in a small dt:
        // We treat dt = 1/60 h (1 minute) as independent trials.
        // P(fault | dt) ≈ lambda * dt  where lambda = faultProbPerHour
        // This avoids a Poisson library dependency while staying accurate.
        const double dt         = 1.0 / 60.0;
        const double pPerMinute = spec_.faultProbabilityPerHour * dt;
        int minutes = static_cast<int>(durationHours / dt);
        int faults  = 0;
        std::uniform_real_distribution<double> uniform(0.0, 1.0);
        for (int i = 0; i < minutes; ++i) {
            if (uniform(rng_) < pPerMinute) ++faults;
        }
        return faults;
    }

    int               id_;
    const VehicleSpec& spec_;
    std::mt19937&     rng_;
    State             state_;
    double            flightStartTime_;
    double            chargeWaitStartTime_;
    double            chargeStartTime_ = 0.0;
};
