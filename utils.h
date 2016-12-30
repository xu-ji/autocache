#include <math.h>

template <size_t N>
double average(const double (&array)[N]) {
  double result = 0.0;
  for (int i = 0; i < N; i++) {
  result += array[i];
  }
  return result / N;
}

double avg_function(std::shared_ptr<std::vector<double> > const &values) {
  double res = 0.0;
  for (double const & v : *values) {
    res += v;
  }
  return res / values->size();
}

