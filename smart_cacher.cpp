#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include "cacher.h"

static const bool DEBUG = true;

/* SmartCacher: cacher with automatically determined variable sampling
 * frequencies/areas and a dual-layer structure.
 *
 * Assumes input_type and output_type are arithmetic, although this could be
 * changed by overloading the arithmetic operators.
 */

template<typename input_t, typename output_t>
class SmartCacher : public Cacher<input_t, output_t> {
  // Passed in parameters
  input_t _granularity;
  output_t _max_grad_error;
  size_t _est_max_cache_size;
  output_t (*_avg_function)(std::shared_ptr<std::vector<output_t> > const &);
  bool _use_middle;

  // Pointer to array with k elements, where k = number of buckets
  // Each element is a pointer to an array containing cached values
  std::unique_ptr<std::unique_ptr<output_t[]>[]> _cache;

  // Number of buckets - automatically determined during init()
  size_t _num_buckets;

  // Number of entries in each bucket
  std::unique_ptr<size_t[]> _bucket_sizes;

  // Size of minibuckets for each bucket
  std::unique_ptr<input_t[]> _bucket_granularities;

  // Bucket i's walls are specified at [2 * i, 2 * i + 1]
  std::unique_ptr<input_t[]> _bucket_walls;

  // Retrievable range
  input_t _fst_x_value;
  input_t _last_x_value;

  // Actual number of minibuckets (cache size)
  size_t _actual_cache_size;

  static bool belongs_in_bucket(std::shared_ptr<std::vector<output_t> > const &,
    output_t (*)(std::shared_ptr<std::vector<output_t> > const &),
    output_t,
    output_t);

public:
  SmartCacher(size_t, output_t (*)(input_t), input_t,
    input_t, input_t, output_t,
    output_t (*)(std::shared_ptr<std::vector<output_t> > const &),
    bool use_middle);
  output_t retrieve(input_t) override;
  void init(void) override;
  std::string get_name(void) override;
  size_t get_size(void) override;
};

template<typename input_t, typename output_t>
SmartCacher<input_t, output_t>::SmartCacher(size_t est_max_cache_size,
     output_t (*cached_function)(input_t),
     input_t fst_input,
     input_t last_input,
     input_t granularity,
     output_t max_grad_error,
     output_t (*avg_function)(std::shared_ptr<std::vector<output_t> > const &),
     bool use_middle) :
  Cacher<input_t, output_t>(fst_input, last_input, cached_function),
  _granularity(granularity), _max_grad_error(max_grad_error),
  _est_max_cache_size(est_max_cache_size), _avg_function(avg_function),
  _use_middle(use_middle) {}

template<typename input_t, typename output_t>
void SmartCacher<input_t, output_t>::init(void){
  // First part: work out the buckets; i.e which segments of input should
  // have the same sampling granularity

  // Temporary storage for our buckets
  // (we do not know how many there will be in advance)
  std::vector<std::shared_ptr<std::vector<output_t> > > buckets;
  std::vector<input_t> est_bucket_walls;

  // Heap allocated as will probably be large. Deallocated when function ends
  std::unique_ptr<std::vector<input_t> > stored_ys(new std::vector<input_t>);

  stored_ys->push_back((*this->_cached_function)(this->_input_range_start));
  input_t curr_x = this->_input_range_start + this->_granularity;
  std::unique_ptr<std::vector<output_t> > fst_gradient(
      new std::vector<output_t>);
  buckets.push_back(std::move(fst_gradient));
  est_bucket_walls.push_back(this->_input_range_start);

  this->_fst_x_value = this->_input_range_start;
  while (curr_x <= this->_input_range_end) {
    output_t curr_y = (*this->_cached_function)(curr_x);
    output_t grad = curr_y - stored_ys->back();
    std::shared_ptr<std::vector<output_t> > curr_bucket = buckets.back();
    if (belongs_in_bucket(curr_bucket, this->_avg_function, grad,
        this->_max_grad_error)) {
      curr_bucket->push_back(grad);
    } else {
      std::shared_ptr<std::vector<output_t> > next_gradient(
          new std::vector<output_t>);
      next_gradient->push_back(grad);
      buckets.push_back(std::move(next_gradient));
      est_bucket_walls.push_back(curr_x);
    }
    stored_ys->push_back(curr_y);
    curr_x += this->_granularity;
  }
  // Size of bucket_walls is num_buckets + 1
  est_bucket_walls.push_back(this->_input_range_end);
  this->_last_x_value = this->_input_range_end;

  if (DEBUG) {
    FILE *buckets_file = fopen("buckets.txt", "w");
    for (input_t wall : est_bucket_walls) {
      fprintf(buckets_file, "%.10f\t%.10f\n", wall,
          (*this->_cached_function)(wall));
    }
    fclose(buckets_file);
    printf("buckets: %lu, ", buckets.size());
  }
  this->_num_buckets = buckets.size();

  // Second part: build buckets (within each bucket the increments are the same)

  // Compute sum of all abs average gradients
  std::vector<output_t> abs_dy_dx_per_bucket;
  output_t sum_of_dy_dxs = 0.0;
  for (size_t i = 0; i < this->_num_buckets; i++) {
    double abs_dx = std::abs(est_bucket_walls[i + 1] - est_bucket_walls[i]);
    double abs_dy = std::abs((*this->_cached_function)(est_bucket_walls[i]) -
        (*this->_cached_function)(est_bucket_walls[i + 1]));
    double weight = abs_dy / abs_dx;
    abs_dy_dx_per_bucket.push_back(weight);
    sum_of_dy_dxs += (weight);
  }

  this->_cache = std::unique_ptr<std::unique_ptr<output_t[]>[]>(
      new std::unique_ptr<output_t[]>[this->_num_buckets]);
  this->_bucket_sizes = std::unique_ptr<size_t[]>(
      new size_t[this->_num_buckets]);
  this->_bucket_granularities = std::unique_ptr<input_t[]>(
      new input_t[this->_num_buckets]);
  this->_bucket_walls = std::unique_ptr<input_t[]>(
      new input_t[2 * this->_num_buckets]);
  this->_actual_cache_size = 0;

  for (size_t bucket_ind = 0; bucket_ind < this->_num_buckets; bucket_ind++) {
    size_t safety = 2;
    size_t min_bucket_size = 2;
    double raw_est = (this->_est_max_cache_size *
        (abs_dy_dx_per_bucket[bucket_ind] / sum_of_dy_dxs)) +
        (this->_est_max_cache_size *
            (est_bucket_walls[bucket_ind + 1] - est_bucket_walls[bucket_ind]) /
            (this->_last_x_value - this->_fst_x_value));
    raw_est = raw_est / 2.0;
    size_t est_num_elems_for_bucket = std::max(min_bucket_size,
        static_cast<size_t>(raw_est));
    std::unique_ptr<output_t[]> bucket(
        new output_t[est_num_elems_for_bucket + safety]);

    input_t fst_bucket_elem = est_bucket_walls[bucket_ind];
    input_t last_bucket_elem = est_bucket_walls[bucket_ind + 1];
    input_t bucket_granularity = (last_bucket_elem - fst_bucket_elem) /
        static_cast<input_t>(est_num_elems_for_bucket);
    assert(bucket_granularity > 0.0);

    input_t curr_key = fst_bucket_elem;
    this->_bucket_walls[bucket_ind * 2] = fst_bucket_elem;
    size_t actual_bucket_elems = 0;

    input_t offset = this->_use_middle ? 0.5 : 0.0;
    while (curr_key <= last_bucket_elem) {
      output_t curr_val =
          (*this->_cached_function)(curr_key + offset * bucket_granularity);
      bucket[actual_bucket_elems++] = curr_val;
      curr_key += bucket_granularity;
    }

    // If we haven't actually included last_bucket_elem, extend past it
    // It's better to have overlaps in our bucket ranges than holes
    if (curr_key > last_bucket_elem) {
      output_t curr_val = (*this->_cached_function)(curr_key);
      bucket[actual_bucket_elems++] = curr_val;
    }

    this->_actual_cache_size += actual_bucket_elems;
    this->_bucket_walls[bucket_ind * 2 + 1] = curr_key;
    this->_cache[bucket_ind] = std::move(bucket);
    this->_bucket_sizes[bucket_ind] = actual_bucket_elems;
    this->_bucket_granularities[bucket_ind] = bucket_granularity;
  }

  if (DEBUG) {
    printf("total minibuckets %lu \n", this->_actual_cache_size);
    // print resolution per buckets
    FILE *buckets_gran_file = fopen("buckets_gran.txt", "w");
    for (int i = 0; i < this->_num_buckets; i++) {
      fprintf(buckets_gran_file, "%.10f\t%.10f\n", this->_bucket_walls[i * 2],
          this->_bucket_granularities[i]);
    }
    fclose(buckets_gran_file);
  }
}

template<typename input_t, typename output_t>
bool SmartCacher<input_t, output_t>::belongs_in_bucket(
    std::shared_ptr<std::vector<output_t> > const &bucket,
    output_t (*avg_function)(std::shared_ptr<std::vector<output_t> > const &),
    output_t grad,
    output_t max_error) {
  if (bucket->empty()) {
    return true;
  }
  output_t bucket_avg = (*avg_function)(bucket);
  return std::abs(grad - bucket_avg) < max_error;
}

template<typename input_t, typename output_t>
output_t SmartCacher<input_t, output_t>::retrieve(input_t key) {
  // Do binary search over buckets
  size_t leftmost_bucket = 0;
  size_t rightmost_bucket = this->_num_buckets - 1;
  size_t found_bucket;
  bool found = false;
  input_t smallest;
  input_t biggest;
  while (!found) {
    size_t mid_bucket = leftmost_bucket +
        ((rightmost_bucket - leftmost_bucket) / 2);
    smallest = this->_bucket_walls[2 * mid_bucket];
    biggest = this->_bucket_walls[2 * mid_bucket + 1];
    if (smallest <= key &&
        biggest >= key) {
      found = true;
      found_bucket = mid_bucket;
    } else {
      if (key > biggest) {
        leftmost_bucket = mid_bucket + 1;
      } else {
        rightmost_bucket = mid_bucket - 1;
      }
    }
  }

  // Directly index into minibucket
  return this->_cache[found_bucket][static_cast<size_t>((key - smallest) /
      this->_bucket_granularities[found_bucket])];
}

template<typename input_t, typename output_t>
std::string SmartCacher<input_t, output_t>::get_name(void) {
  return "smart_cacher_" + std::to_string(this->_est_max_cache_size) + "_" +
      std::to_string(this->_max_grad_error);
}

template<typename input_t, typename output_t>
size_t SmartCacher<input_t, output_t>::get_size(void) {
  return this->_actual_cache_size;
}
