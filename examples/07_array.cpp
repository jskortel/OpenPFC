#include <algorithm>
#include <iostream>
#include <openpfc/utils/array.hpp>

/**
 * \example 07_array.cpp
 *
 * A basic multidimensional array is implemented, to make it easier to work with
 * indices. Implementation makes it possible to give offset for indices. This
 * can help working with discrete fields of data, when the field is decomposed
 * to several different machines using domain decomposition and MPI. Example
 * will follow. Interally, `pfc::utils::array<T,D>` uses `std::vector<T>` to
 * hold data of type T and `pfc::utils::MultiIndex<D>` to make index
 * manipulations. In this example, we introduce two arrays, one with offset like
 * it would be in the case of domain decomposition, and fill in some values
 * based on indices.
 */
int main() {
  int Lx = 16;
  int Ly = 8;

  // "Process 0" contains the first part of array
  pfc::utils::array<double, 2> arr0({Lx / 2, Ly}, {0, 0});
  // "Process 1" contains the second part of array
  pfc::utils::array<double, 2> arr1({Lx / 2, Ly}, {Lx / 2, 0});

  // Accessing arrays:
  arr0[{0, 0}] = 1.0;
  arr1[{8, 0}] = 2.0;
  std::cout << "First item of arr0: " << arr0[{0, 0}] << std::endl;
  std::cout << "First item of arr1: " << arr1[{8, 0}] << std::endl;

  // We can get access to underlying vector with linear index, if needed. As we
  // have offset of 8 in the second array in x-direction, index {8, 0} is
  // actually pointing to first index of the array. Thus, the above is equivalent to:
  std::cout << "First item of arr0: " << arr0.get_data()[0] << std::endl;
  std::cout << "First item of arr1: " << arr1.get_data()[0] << std::endl;

  // In practice, one might want to apply some function f to arrays, where each
  // array contains different part of domain. This can be done with `apply`.
  // First, define some coordinate system, transforming from ij to xy:
  double dx = 1.0;
  double dy = 1.0;
  double x0 = 0.0;
  double y0 = 0.0;

  // Second, define function of i and j:
  auto f = [&](auto indices) {
    const auto [i, j] = indices;
    double x = x0 + i * dx;
    double y = y0 + j * dy;
    return 1.0 + x + y * y;
  };

  // Then we apply:
  arr0.apply(f);
  arr1.apply(f);

  std::cout << "First item of arr0: " << arr0.get_data()[0] << std::endl; // 1.0 + 0.0 + 0.0 * 0.0 = 1
  std::cout << "First item of arr1: " << arr1.get_data()[0] << std::endl; // 1.0 + 8.0 + 0.0 * 0.0 = 9

  // Another way to fill array would be to use algorithms:
  std::transform(arr0.get_index().begin(), arr0.get_index().end(), arr0.get_data().begin(), f);
  std::transform(arr1.get_index().begin(), arr1.get_index().end(), arr1.get_data().begin(), f);

  return 0;
}
