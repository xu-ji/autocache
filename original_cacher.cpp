#include "original_cacher.h"

OriginalCacher::OriginalCacher(size_t size, double (*cached_function)(double),
    double start, double end, bool use_middle) :
  Cacher<double, double>(start, end, cached_function), _size(size),
  _use_middle(use_middle) {}

void OriginalCacher::init(void) {
  this->_cache = std::unique_ptr<double[]>(new double[this->_size]);

  double offset = this->_use_middle ? 0.5 : 0.0;
  for (int i = 0; i < this->_size; i++) {
    double res = (*this->_cached_function)((i + offset) *
        ((this->_input_range_end - this->_input_range_start) / this->_size) +
        this->_input_range_start);
    this->_cache[i] = res;
  }
}

double OriginalCacher::retrieve(double input) {
  int ind = (int) ((input - this->_input_range_start) / ((this->_input_range_end
      - this->_input_range_start) / this->_size));
  return this->_cache[ind];
}

std::string OriginalCacher::get_name(void) {
  return "original_cacher_" + std::to_string(this->_size);
}

size_t OriginalCacher::get_size(void) {
  return this->_size;
}
