#ifndef CACHER_H
#define CACHER_H

/* Abstract class for our cachers.
 */

template<typename input_t, typename output_t>
class Cacher {
protected:
  // Allowed retrievable range.
  input_t _input_range_start;
  input_t _input_range_end;

  // Function we are caching.
  output_t (*_cached_function)(input_t);

  Cacher(input_t start, input_t end, output_t (*cached_function)(input_t)) :
    _input_range_start(start), _input_range_end(end),
    _cached_function(cached_function) {};

public:
  // Initialise the cache. Called before lookups.
  virtual void init(void) = 0;

  // Lookup a key in the cache.
  virtual output_t retrieve(input_t) = 0;

  // Get the name of the cache.
  virtual std::string get_name(void) = 0;

  // Get the size of the cache.
  virtual size_t get_size(void) = 0;

  virtual ~Cacher() {};
};

#endif
