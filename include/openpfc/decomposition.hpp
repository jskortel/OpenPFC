#ifndef PFC_DECOMPOSITION_HPP
#define PFC_DECOMPOSITION_HPP

#include "world.hpp"
#include <heffte.h>
#include <iostream>
#include <mpi.h>

namespace pfc {

/**
 * @brief Class representing the domain decomposition for parallel Fast Fourier
 * Transform.
 *
 * The Decomposition class defines the domain decomposition for parallel FFT. It
 * splits the domain into multiple parts based on the number of processors and
 * the grid setup. It provides information about the local domain and
 * communication patterns.
 */
class Decomposition {
private:
  const World &m_world;                                            ///< Reference to the World object.
  const int id, tot;                                               ///< Processor ID and total number of processors.
  const int Lx_c, Ly_c, Lz_c;                                      ///< Dimensions of the complex domain.
  const heffte::box3d<int> real_indexes, complex_indexes;          ///< Index ranges for real and complex domains.
  const std::array<int, 3> proc_grid;                              ///< Processor grid dimensions.
  const std::vector<heffte::box3d<int>> real_boxes, complex_boxes; ///< Local domain boxes.

  /**
   * @brief Get the rank of the current MPI communicator.
   * @param comm The MPI communicator.
   * @return The rank of the current process.
   */
  int get_comm_rank(MPI_Comm comm) {
    int rank;
    MPI_Comm_rank(comm, &rank);
    return rank;
  }

  /**
   * @brief Get the size of the current MPI communicator.
   * @param comm The MPI communicator.
   * @return The size of the communicator (total number of processes).
   */
  int get_comm_size(MPI_Comm comm) {
    int size;
    MPI_Comm_size(comm, &size);
    return size;
  }

public:
  const heffte::box3d<int> inbox, outbox; ///< Local communication boxes.
  const int r2c_direction = 0;            ///< Real-to-complex symmetry direction.

  // clang-format off
  /**
   * @brief Construct a new Decomposition object.
   * @param world Reference to the World object.
   * @param id The ID of the current process.
   * @param tot The total number of processes.
   */
  Decomposition(const World &world, int id, int tot)
      : m_world(world),
        id(id),
        tot(tot),
        Lx_c(floor(m_world.Lx / 2) + 1),
        Ly_c(m_world.Ly),
        Lz_c(m_world.Lz),
        real_indexes(m_world),
        complex_indexes({0, 0, 0}, {Lx_c - 1, Ly_c - 1, Lz_c - 1}),
        proc_grid(heffte::proc_setup_min_surface(real_indexes, tot)),
        real_boxes(heffte::split_world(real_indexes, proc_grid)),
        complex_boxes(heffte::split_world(complex_indexes, proc_grid)),
        inbox(real_boxes[id]),
        outbox(complex_boxes[id]) {
    assert(real_indexes.r2c(r2c_direction) == complex_indexes);
  };
  // clang-format on

  /**
   * @brief Construct a new Decomposition object using MPI communicator.
   * @param world Reference to the World object.
   * @param comm The MPI communicator (default: MPI_COMM_WORLD).
   */
  Decomposition(const World &world, MPI_Comm comm = MPI_COMM_WORLD)
      : Decomposition(world, get_comm_rank(comm), get_comm_size(comm)) {}

  /**
   * @brief Deleted copy constructor.
   */
  Decomposition(const Decomposition &) = delete;

  /**
   * @brief Deleted copy assignment operator.
   */
  Decomposition &operator=(const Decomposition &) = delete;

  /**
   * @brief Get the reference to the World object.
   * @return Reference to the World object.
   */
  const World &get_world() const { return m_world; }

  /**
   * @brief Get the ID of the current process.
   * @return The ID of the current process.
   */
  int get_id() const { return id; }

  friend std::ostream &operator<<(std::ostream &os, const Decomposition &d) {
    const World &w = d.get_world();
    os << "***** DOMAIN DECOMPOSITION STATUS *****\n";
    os << "Real-to-complex symmetry is used (r2c direction = " << (char)('x' + d.r2c_direction) << ")\n";
    os << "Domain is split into " << d.tot << " parts ";
    os << "(minimum surface processor grid: [" << d.proc_grid[0] << ", " << d.proc_grid[1] << ", " << d.proc_grid[2]
       << "])\n";
    os << "Domain in real space: [" << w.Lx << ", " << w.Ly << ", " << w.Lz << "] (" << d.real_indexes.count()
       << " indexes)\n";
    os << "Domain in complex space: [" << d.Lx_c << ", " << d.Ly_c << ", " << d.Lz_c << "] ("
       << d.complex_indexes.count() << " indexes)\n";
    for (int i = 0; i < d.tot; i++) {
      const auto &in = d.real_boxes[i];
      const auto &out = d.complex_boxes[i];
      os << "Domain " << i + 1 << "/" << d.tot << ": ";
      os << "[" << in.low[0] << ", " << in.low[1] << ", " << in.low[2] << "] x ";
      os << "[" << in.high[0] << ", " << in.high[1] << ", " << in.high[2] << "] (" << in.count() << " indexes) => ";
      os << "[" << out.low[0] << ", " << out.low[1] << ", " << out.low[2] << "] x ";
      os << "[" << out.high[0] << ", " << out.high[1] << ", " << out.high[2] << "] (" << out.count() << " indexes)\n";
    }
    return os;
  }
};
} // namespace pfc

#endif
