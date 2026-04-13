#include "../include/Simulator.h"
#include <stdexcept>
#include <iostream>
#include <numeric>

// ---------------------------------------------------------------------------
Simulator::Simulator(unsigned int seed)
	: rng_(seed), chargerPool_(NUM_CHARGERS) {

	// -----------------------------------------------------------------------
	// Randomly assign vehicle types so total == NUM_VEHICLES.
	// -----------------------------------------------------------------------
	std::array<int, ALL_SPECS.size()> counts{};
	int remaining = NUM_VEHICLES;

	for (int i = 0; i < ALL_SPECS.size() - 1;i ++) {
		std::uniform_int_distribution<int> dist(0, remaining);
		auto cnt = dist(rng_);
		counts[i] = cnt;
		remaining -= cnt;

	}
	counts.back() = remaining;

	// assign spec to vehicles
	int globalId = 0;
	for (int i = 0; i < ALL_SPECS.size(); i++) {
		const VehicleSpec& spec = ALL_SPECS[i];
		std::cout << "  " << spec.companyName << ": " << counts[i] << "\n";

		// Initialize stats bucket
		stats_[i].companyName = spec.companyName;

		for (int j = 0; j < counts[i]; ++j) {
			vehicleSpecIndex_[globalId] = i;
			vehicles_.push_back(
				std::make_unique<Vehicle>(globalId, spec, rng_));
			++globalId;
		}
	}

}

// ---------------------------------------------------------------------------
void Simulator::scheduleEvent(double time, std::function<void()> action) {
	if (time > SIM_DURATION) {
		return;
	}
	eventQueue_.push({time, action});
}

// ---------------------------------------------------------------------------
std::unordered_map<std::string, TypeStats> Simulator::run() {
	for (auto &v:vehicles_) {
		int id = v->id();
		auto timePt = v->spec().maxFlightHours();
		scheduleEvent(timePt, [this, id]() {
			handleBatteryEmpty(id);
			});
	}

	while (!eventQueue_.empty()) {
		auto ev = eventQueue_.top();
		eventQueue_.pop();
		currentTime_ = ev.time;
		ev.action();
	}

	for (auto& vPtr : vehicles_) {
		int specIdx = vehicleSpecIndex_.at(vPtr->id());
		vPtr->recordPartialFlight(SIM_DURATION, stats_[specIdx]);
	}

	std::unordered_map<std::string, TypeStats> result;
	for (auto& s : stats_) {
		if (!s.companyName.empty())
			result[s.companyName] = s;
	}
	return result;
}

// ---------------------------------------------------------------------------
void Simulator::handleBatteryEmpty(int vehicleId) {
	auto& v = vehicles_[vehicleId];
	auto sepcId = vehicleSpecIndex_[vehicleId];
	
	v->land(currentTime_, stats_[sepcId]);

	chargerPool_.requestCharger(vehicleId, currentTime_, [this ](int vehicleId, double grantTime){
		handleChargeDone(vehicleId, grantTime); }
	);
}

// ---------------------------------------------------------------------------
// Called when a charger is granted to vehicleId at grantTime.
// We start charging, then schedule a CHARGE_DONE event.
void Simulator::handleChargeDone(int vehicleId, double grantTime) {
	//start charging
	auto& v = vehicles_[vehicleId];
	auto sepcId = vehicleSpecIndex_[vehicleId];
	v->startCharging(grantTime);
	double chargeFinishTime = grantTime + v->spec().timeToChargeHours;

	scheduleEvent(chargeFinishTime, [this, vehicleId, chargeFinishTime]() {
		//finished charging
		chargerPool_.releaseCharger(chargeFinishTime);
		

		//next charging
		int      specIdx = vehicleSpecIndex_.at(vehicleId);
		TypeStats& stats = stats_[specIdx];
		auto& sv = vehicles_[vehicleId];
		auto nextEmptyAt = sv->finishCharging(chargeFinishTime, stats);
		scheduleEvent(nextEmptyAt, [this,vehicleId]() {
			handleBatteryEmpty(vehicleId);
			});
		});
}
