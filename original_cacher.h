#ifndef ORIGINAL_CACHER_H
#define ORIGINAL_CACHER_H

#include <memory>
#include <string>
#include "cacher.h"

/* OriginalCacher, as described by the report.
 * Uniform samples stored in flat layer in single array.
 */


class OriginalCacher : public Cacher<double, double> {
  size_t _size;
  std::unique_ptr<double[]> _cache;
  bool _use_middle;

public:
  OriginalCacher(size_t, double (*)(double), double, double, bool);
  double retrieve(double) override;
  void init(void) override;
  std::string get_name(void) override;
  size_t get_size(void) override;
};

#endif
