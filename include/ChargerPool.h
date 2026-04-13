#pragma once
#include <queue>
#include <functional>
#include <cassert>

/**
 * @brief Models a pool of N identical chargers shared by all vehicles.
 *
 * Vehicles call requestCharger() when they land.  If a charger is free they
 * get one immediately; otherwise they are queued (FIFO).  When a vehicle
 * finishes charging it calls releaseCharger(), which hands the charger to the
 * next waiter (if any) via the supplied callback.
 *
 * This class is intentionally free of simulation-time logic so it can be
 * unit-tested without running a full simulation.
 */
class ChargerPool {
public:
    using VehicleId = int;
    /// Callback signature: (vehicleId, chargerGrantedAtTime)
    using GrantCallback = std::function<void(VehicleId, double)>;

    explicit ChargerPool(int numChargers)
        : totalChargers_(numChargers), freeChargers_(numChargers) {
        assert(numChargers > 0);
    }

    /**
     * @brief Try to acquire a charger for a vehicle.
     *
     * @param vehicleId   Identifier of the requesting vehicle.
     * @param currentTime Simulation time of the request (hours).
     * @param onGrant     Called immediately if a charger is free, or later
     *                    when one becomes available.
     * @return true  if the charger was granted immediately.
     * @return false if the vehicle was queued.
     */
    bool requestCharger(VehicleId vehicleId, double currentTime,
                        GrantCallback onGrant) {
        if (freeChargers_ > 0) {
            freeChargers_--;
            onGrant(vehicleId, currentTime);
            return true;
        }
        waitQueue_.push({ vehicleId,onGrant });
        return false;
    }

    /**
     * @brief Release a charger.  If vehicles are waiting the first one is
     *        granted immediately via its stored callback.
     *
     * @param releaseTime Simulation time at which the charger becomes free.
     */
    void releaseCharger(double releaseTime) {
        if (waitQueue_.empty()) {
            freeChargers_++;
            return;
        }
        auto [id, cb] = waitQueue_.front();
        waitQueue_.pop();

        cb(id, releaseTime);

    }

    int freeChargers()  const { return freeChargers_; }
    int waitingCount()  const { return static_cast<int>(waitQueue_.size()); }
    int totalChargers() const { return totalChargers_; }

private:
    struct WaitEntry {
        VehicleId    vehicleId;
        GrantCallback callback;
    };

    int totalChargers_;
    int freeChargers_;
    std::queue<WaitEntry> waitQueue_;
};
