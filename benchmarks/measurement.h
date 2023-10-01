
#ifndef CONCURRENT_DEFERRED_RC_MEASUREMENT_H
#define CONCURRENT_DEFERRED_RC_MEASUREMENT_H

#include <chrono>
#include <iostream>
#include <string>

struct operation_measurement {

  explicit operation_measurement(std::string name_) : name(std::move(name_)) {}

  ~operation_measurement() {
    if (measurements.empty()) {
      std::cout << name << ": No measurements" << std::endl;
    }
    else {
      std::sort(measurements.begin(), measurements.end());
      long long median = measurements[measurements.size()/2];
      long long onepc = measurements[measurements.size()/100];
      long long ninetyninepc = measurements[measurements.size()*99/100];
      std::cout << name << ": median " << median << "ns, " << "1% low " << onepc << "ns, 1% high " << ninetyninepc << "ns." << std::endl;
    }
  }

  void start_measurement() {
    start = std::chrono::steady_clock::now();
  }

  void end_measurement() {
    auto now = std::chrono::steady_clock::now();
    long long int nanos = (std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count());
    measurements.push_back(nanos);
  }

  std::string name;
  std::chrono::time_point<std::chrono::steady_clock> start;
  std::vector<long long int> measurements;
};

#endif //CONCURRENT_DEFERRED_RC_MEASUREMENT_H
