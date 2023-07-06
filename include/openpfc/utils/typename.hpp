#ifndef PFC_TYPENAME_HPP
#define PFC_TYPENAME_HPP

#include <string>
#include <type_traits>
#include <typeinfo>

namespace pfc {

// Type trait to retrieve the human-readable type name
template <typename T> struct TypeName {
  static std::string get() { return typeid(T).name(); }
};

// Specialization for int
template <> struct TypeName<int> {
  static std::string get() { return "int"; }
};

// Specialization for float
template <> struct TypeName<float> {
  static std::string get() { return "float"; }
};

// Specialization for double
template <> struct TypeName<double> {
  static std::string get() { return "double"; }
};

} // namespace pfc

#endif
