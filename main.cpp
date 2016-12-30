#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include "original_cacher.h"
#include "smart_cacher.cpp"
#include "utils.h"

const unsigned int NUM_POINTS = 100000;
const unsigned int INIT_SAMPLING_FREQ = NUM_POINTS;
const bool USE_MIDDLE = true;

static const double MAX_RADIAN = 3.0 * M_PI;
static const double A = 50.0;
static const double P1 = 0.25 * M_PI;
static const double P2 = 0.5 * M_PI;

double model_function(double input) {
  return A * sin(input) * sin(input + P1) * cos(input + P2);
}

enum CacherType {
  ORIGINAL = 0,
  SMART = 1
};

int main(void) {
  // Cacher hyperparameters for 100000 sampling points:
  const size_t cache_sizes[] = {500, 750, 1000, 2500, 5000, 7500, 10000, 12500,
      15000, 20000, 30000};
  const double smart_cacher_grad_errors[] = {0.0001, 0.00025, 0.0005, 0.00075,
      0.001, 0.0015, 0.002, 0.0025, 0.005};

  std::vector<std::unique_ptr<Cacher<double, double> > > cachers;
  std::vector<CacherType> types;
  for (size_t cs = 0; cs < sizeof(cache_sizes)/sizeof(*cache_sizes); cs++) {
    cachers.push_back(std::make_unique<OriginalCacher>(cache_sizes[cs],
        &model_function, 0, MAX_RADIAN, USE_MIDDLE));
    types.push_back(ORIGINAL);

    size_t num_ges = sizeof(smart_cacher_grad_errors)/
        sizeof(*smart_cacher_grad_errors);
    for (size_t ge = 0; ge < num_ges; ge++) {
      cachers.push_back(std::make_unique<SmartCacher<double, double>>(
          cache_sizes[cs],
          &model_function,
          0.0,
          MAX_RADIAN,
          MAX_RADIAN / INIT_SAMPLING_FREQ,
          smart_cacher_grad_errors[ge],
          &avg_function,
          USE_MIDDLE));
      types.push_back(SMART);
    }
  }

  std::vector<FILE *> error_files;
  std::string suff = USE_MIDDLE ? "_use_middle" : "_use_start";
  error_files.push_back(fopen(("OriginalCacher" + suff + ".txt").c_str(), "w"));
  error_files.push_back(fopen(("SmartCacher" + suff + ".txt").c_str(), "w"));

  for (size_t c = 0; c < cachers.size(); c++) {
    cachers[c]->init();

    std::string filebase = cachers[c]->get_name();
    printf("doing: %s to errors file index: %d\n", filebase.c_str(), types[c]);
    FILE *data_file = fopen((filebase + "_data.txt").c_str(), "w");
    double error = 0.0;
    for (int i = 0; i < NUM_POINTS; ++i) {
      double angle = ((double) i / (double) NUM_POINTS) * MAX_RADIAN;
      double model = model_function(angle);
      double cached = cachers[c]->retrieve(angle);
      fprintf(data_file, "%.10f\t%.10f\t%.10f\n", angle, model, cached);
      error += std::abs(cached - model);
    }
    fclose(data_file);
    printf("%.10f\n", error / NUM_POINTS);
    fprintf(error_files[types[c]], "%lu\t%.20f\n", cachers[c]->get_size(),
        error / NUM_POINTS);
  }

  for (FILE * ef : error_files) {
    fclose(ef);
  }

  return 0;
}
