// heFFTe implementation of pfc code

#include <chrono>
#include <climits>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <argparse/argparse.hpp>
#include <heffte.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

const double pi = std::atan(1.0) * 4.0;

struct Simulation {

  unsigned int Lx;
  unsigned int Ly;
  unsigned int Lz;
  double dx;
  double dy;
  double dz;
  double x0;
  double y0;
  double z0;
  double t0;
  double t1;
  double dt;
  unsigned long int max_iters;

  std::vector<double> k2_;             // "Laplace" operator
  std::vector<double> L_;              // Linear part
  std::vector<double> u;               // Field in real space
  std::vector<std::complex<double>> U; // Field in Fourier space
  std::vector<std::complex<double>> N; // Nonlinear part of u in Fourier space

  std::filesystem::path results_dir = ".";

  std::string exit_msg;

  Simulation() {
    set_domain({-64.0, -64.0, -64.0}, {1.0, 1.0, 1.0}, {128, 128, 128});
    set_time(0.0, 100.0, 1.0);
    max_iters = ULONG_MAX;
    exit_msg = "";
  }

  void set_domain(std::array<double, 3> o, std::array<double, 3> h,
                  std::array<int, 3> d) {
    x0 = o[0];
    y0 = o[1];
    z0 = o[2];
    dx = h[0];
    dy = h[1];
    dz = h[2];
    Lx = d[0];
    Ly = d[1];
    Lz = d[2];
  }

  void set_time(double t0_, double t1_, double dt_) {
    t0 = t0_;
    t1 = t1_;
    dt = dt_;
  }

  double get_dt() {
    return dt;
  }

  void set_dt(double dt_) {
    dt = dt_;
  }

  void set_max_iters(unsigned long int nmax) {
    max_iters = nmax;
  }

  void set_results_dir(std::string path) {
    results_dir = path;
  }

  std::filesystem::path get_results_dir() {
    return results_dir;
  }

  virtual bool done(unsigned long int n, double t) {
    if (n > max_iters) {
      exit_msg = "maximum number of iterations (" + std::to_string(max_iters) +
                 ") reached";
      return true;
    }
    if (t >= t1) {
      exit_msg = "simulated succesfully to time " + std::to_string(t1) + ", (" +
                 std::to_string(n) + " iterations)";
      return true;
    }
    return false;
  }

  double k2(double x, double y, double z) {
    auto fx = 2.0 * pi / (dx * Lx);
    auto fy = 2.0 * pi / (dy * Ly);
    auto fz = 2.0 * pi / (dz * Lz);
    auto kx = x < Lx / 2.0 ? x * fx : (x - Lx) * fx;
    auto ky = y < Ly / 2.0 ? y * fy : (y - Ly) * fy;
    auto kz = z < Lz / 2.0 ? z * fz : (z - Lz) * fz;
    return kx * kx + ky * ky + kz * kz;
  }

  unsigned long int resize(unsigned long int size_inbox,
                           unsigned long int size_outbox) {
    unsigned long int size = 0;
    k2_.resize(size_outbox);
    L_.resize(size_outbox);
    u.resize(size_inbox);
    U.resize(size_outbox);
    N.resize(size_outbox);
    size += sizeof(double) * k2_.size();
    size += sizeof(double) * L_.size();
    size += sizeof(double) * u.size();
    size += sizeof(std::complex<double>) * U.size();
    size += sizeof(std::complex<double>) * N.size();
    return size;
  }

  void fill_k2(std::array<int, 3> low, std::array<int, 3> high) {
    unsigned long int idx = 0;
    for (auto z = low[2]; z <= high[2]; z++) {
      for (auto y = low[1]; y <= high[1]; y++) {
        for (auto x = low[0]; x <= high[0]; x++) {
          k2_[idx] = k2(x, y, z);
          idx += 1;
        }
      }
    }
  }

  void fill_L(std::array<int, 3> low, std::array<int, 3> high) {
    unsigned long int idx = 0;
    for (auto k = low[2]; k <= high[2]; k++) {
      for (auto j = low[1]; j <= high[1]; j++) {
        for (auto i = low[0]; i <= high[0]; i++) {
          L_[idx] = L(i, j, k);
          idx += 1;
        }
      }
    }
  }

  void fill_u0(std::array<int, 3> low, std::array<int, 3> high) {
    unsigned long int idx = 0;
    for (auto k = low[2]; k <= high[2]; k++) {
      for (auto j = low[1]; j <= high[1]; j++) {
        for (auto i = low[0]; i <= high[0]; i++) {
          u[idx] = u0(x0 + i * dx, y0 + j * dy, z0 + k * dz);
          idx += 1;
        }
      }
    }
  }

  virtual void calculate_nonlinear_part() {
    for (unsigned long int i = 0; i < u.size(); i++) {
      u[i] = f(u[i]);
    }
  }

  void integrate() {
    for (auto i = 0; i < U.size(); i++) {
      U[i] = 1.0 / (1.0 - dt * L_[i]) * (U[i] - k2_[i] * dt * N[i]);
    };
  }

  virtual void finalize_step(unsigned long int n, double t) {
  }

  virtual void finalize_master_step(unsigned long int n, double t) {
    if (ceil(t / t1 * 100.0) != ceil((t - dt) / t1 * 100.0)) {
      std::cout << "n = " << n << ", t = " << t << ", dt = " << dt << ", "
                << ceil(t / t1 * 100.0) << " percent done" << std::endl;
    }
  }

  virtual void tune_dt(unsigned long int n, double t) {
  }

  // in practice we are interested of replacing the things below with our
  // owns...

  virtual bool writeat(unsigned long int n, double t) {
    return true;
  }

  virtual std::filesystem::path get_result_file_name(unsigned long int n,
                                                     double t) {
    std::filesystem::path filename = "u" + std::to_string(n) + ".bin";
    return get_results_dir() / filename;
  }

  virtual double u0(double x, double y, double z) {
    return exp(-x * x / Lx) * exp(-y * y / Ly) * exp(-z * z / Lz);
  }

  virtual double L(double x, double y, double z) {
    return -k2(x, y, z);
  }

  virtual double f(double u) {
    return 0.0;
  }
};

struct Diffusion : Simulation {

  const double a = 1.0; // diffusion constant

  virtual double L(double x, double y, double z) {
    return -a * k2(x, y, z);
  }
};

struct BasicPFC : Simulation {

  const std::string description = "A basic phase field crystal model";
  const double Bx = 1.3;
  const double Bl = 1.0;
  const double p2 = -1.0 / 2.0;
  const double p3 = 1.0 / 3.0;

  double L(double x, double y, double z) {
    auto k2i = k2(x, y, z);
    auto k4i = pow(k2i, 2);
    auto C = -Bx * (-2.0 * k2i + k4i);
    return -k2i * (Bl - C);
  }

  double f(double u) {
    return p2 * u * u + p3 * u * u * u;
  }

  double u0(double x, double y, double z) {
    const double A = 1.0;
    const double n_os = -0.04;
    const double n_ol = -0.05;
    auto R = 20.0;
    if (x * x + y * y + z * z > R * R) {
      return n_ol;
    }
    double cx = cos(x) * dx;
    double cy = cos(y) * dy;
    double cz = cos(z) * dz;
    return n_os + A * (cx * cy + cy * cz + cz * cx);
  }

  void tune_dt(unsigned long int n, double t) {
    return;
    int nmax = max_iters;
    double tmax = t1;
    double dt0 = 1.0;
    double tau = 3.0;
    double tnext = dt0 * (n + 1.0) +
                   pow(1.0 * (n + 1.0) / nmax, tau) * (t1 - dt0 * (n + 1.0));
    set_dt(std::max(dt0, tnext - t));
  }

  bool writeat(unsigned long int n, double t) {
    return true;
  }
};

void MPI_Write_Data(std::string filename, MPI_Datatype &filetype,
                    std::vector<double> &u) {
  MPI_File fh;
  MPI_File_open(MPI_COMM_WORLD, filename.c_str(),
                MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &fh);
  MPI_Offset filesize = 0;
  const unsigned int disp = 0;
  MPI_File_set_size(fh, filesize); // force overwriting existing data
  MPI_File_set_view(fh, disp, MPI_DOUBLE, filetype, "native", MPI_INFO_NULL);
  MPI_File_write_all(fh, u.data(), u.size(), MPI_DOUBLE, MPI_STATUS_IGNORE);
  MPI_File_close(&fh);
}

void MPI_Solve(Simulation *s) {

  // unpack simulation settings
  const int Lx = s->Lx;
  const int Ly = s->Ly;
  const int Lz = s->Lz;
  const double dx = s->dx;
  const double dy = s->dy;
  const double dz = s->dz;
  const double x0 = s->x0;
  const double y0 = s->y0;
  const double z0 = s->z0;
  const double t0 = s->t0;
  const double t1 = s->t1;
  const int max_iters = s->max_iters;

  MPI_Comm comm = MPI_COMM_WORLD;

  int me; // this process rank within the comm
  MPI_Comm_rank(comm, &me);

  int num_ranks; // total number of ranks in the comm
  MPI_Comm_size(comm, &num_ranks);

  /*
  If the input of an FFT transform consists of all real numbers,
   the output comes in conjugate pairs which can be exploited to reduce
   both the floating point operations and MPI communications.
   Given a global set of indexes, HeFFTe can compute the corresponding DFT
   and exploit the real-to-complex symmetry by selecting a dimension
   and reducing the indexes by roughly half (the exact formula is floor(n / 2)
  + 1).
   */
  const int Lx_c = floor(Lx / 2) + 1;
  // the dimension where the data will shrink
  const int r2c_direction = 0;
  // define real doman
  heffte::box3d<> real_indexes({0, 0, 0}, {Lx - 1, Ly - 1, Lz - 1});
  // define complex domain
  heffte::box3d<> complex_indexes({0, 0, 0}, {Lx_c - 1, Ly - 1, Lz - 1});

  // check if the complex indexes have correct dimension
  assert(real_indexes.r2c(r2c_direction) == complex_indexes);

  // report the indexes
  if (me == 0) {
    std::cout << "Number of ranks: " << num_ranks << std::endl;
    std::cout << "Domain size: " << Lx << " x " << Ly << " x " << Lz
              << std::endl;
    std::cout << "The global input contains " << real_indexes.count()
              << " real indexes." << std::endl;
    std::cout << "The global output contains " << complex_indexes.count()
              << " complex indexes." << std::endl;
  }

  // create a processor grid with minimum surface (measured in number of
  // indexes)
  auto proc_grid = heffte::proc_setup_min_surface(real_indexes, num_ranks);
  if (me == 0) {
    std::cout << "Minimum surface processor grid: [" << proc_grid[0] << ", "
              << proc_grid[1] << ", " << proc_grid[2] << "]" << std::endl;
  }

  // split all indexes across the processor grid, defines a set of boxes
  auto real_boxes = heffte::split_world(real_indexes, proc_grid);
  auto complex_boxes = heffte::split_world(complex_indexes, proc_grid);

  // pick the box corresponding to this rank
  heffte::box3d<> const inbox = real_boxes[me];
  heffte::box3d<> const outbox = complex_boxes[me];

  // define the heffte class and the input and output geometry
  heffte::fft3d_r2c<heffte::backend::fftw> fft(inbox, outbox, r2c_direction,
                                               comm);

  // vectors with the correct sizes to store the input and output data
  // taking the size of the input and output boxes
  std::cout << "Rank " << me << " input box: " << fft.size_inbox()
            << " indexes, indices x = [" << inbox.low[0] << ", "
            << inbox.high[0] << "], y = [" << inbox.low[1] << ", "
            << inbox.high[1] << "], "
            << "z = [" << inbox.low[2] << ", " << inbox.high[2]
            << "], outbox box: " << fft.size_outbox()
            << " indexes, indices x = [" << outbox.low[0] << ", "
            << outbox.high[0] << "], y = [" << outbox.low[1] << ", "
            << outbox.high[1] << "], "
            << "z = [" << outbox.low[2] << ", " << outbox.high[2] << "]"
            << std::endl;

  // Create and commit new data type
  MPI_Datatype filetype;
  const int size_array[] = {Lx, Ly, Lz};
  const int subsize_array[] = {inbox.high[0] - inbox.low[0] + 1,
                               inbox.high[1] - inbox.low[1] + 1,
                               inbox.high[2] - inbox.low[2] + 1};
  const int start_array[] = {inbox.low[0], inbox.low[1], inbox.low[2]};
  MPI_Type_create_subarray(3, size_array, subsize_array, start_array,
                           MPI_ORDER_FORTRAN, MPI_DOUBLE, &filetype);
  MPI_Type_commit(&filetype);

  if (me == 0) {
    std::cout << "Resize arrays... ";
  }
  auto size = s->resize(fft.size_inbox(), fft.size_outbox());
  std::vector<std::complex<double>> workspace(fft.size_workspace());
  size += sizeof(std::complex<double>) * workspace.size();
  if (me == 0) {
    double GB = 1.0 / (1024.0 * 1024.0 * 1024.0);
    std::cout << size * GB << " GB allocated" << std::endl;
  }

  if (me == 0) {
    std::cout << "Generate Laplace operator k2" << std::endl;
  }
  s->fill_k2(outbox.low, outbox.high);

  if (me == 0) {
    std::cout << "Generate linear operator L" << std::endl;
  }
  s->fill_L(outbox.low, outbox.high);

  if (me == 0) {
    std::cout << "Generate initial condition u0" << std::endl;
  }
  s->fill_u0(inbox.low, inbox.high);

  if (me == 0) {
    std::cout << "Starting simulation" << std::endl;
  }

  unsigned long int n = 0;
  double t = t0;
  double *u = s->u.data();
  std::complex<double> *U = s->U.data();
  std::complex<double> *N = s->N.data();

  if (s->writeat(n, t)) {
    MPI_Write_Data(s->get_result_file_name(n, t), filetype, s->u);
  }

  auto start = std::chrono::high_resolution_clock::now();
  while (!s->done(n, t)) {
    s->tune_dt(n, t);
    n += 1;
    t += s->get_dt();

    // FFT for linear part, U = fft(u),  O(n log n)
    auto t0 = std::chrono::high_resolution_clock::now();
    fft.forward(u, U, workspace.data());

    // calculate nonlinear part, u = f(u), O(n) (store in-place)
    auto t1 = std::chrono::high_resolution_clock::now();
    s->calculate_nonlinear_part();

    // FFT for nonlinear part, N = fft(u), O(n log n)
    auto t2 = std::chrono::high_resolution_clock::now();
    fft.forward(u, N, workspace.data());

    // Semi-implicit time integration U = 1 / (1 - dt * L) * (U - k2 * dt * N),
    // O(n)
    auto t3 = std::chrono::high_resolution_clock::now();
    s->integrate();

    // Back to real space, u = fft^-1(U), O(n log n)
    auto t4 = std::chrono::high_resolution_clock::now();
    fft.backward(U, u, workspace.data(), heffte::scale::full);

    auto t5 = std::chrono::high_resolution_clock::now();
    if (s->writeat(n, t)) { // O(?)
      MPI_Write_Data(s->get_result_file_name(n, t), filetype, s->u);
    }

    auto t6 = std::chrono::high_resolution_clock::now();
    s->finalize_step(n, t);
    if (me == 0) {
      s->finalize_master_step(n, t);
    }

    if (me == 0) {
      std::cout << "Iteration " << n << " (time " << t << ") summary: ";
      auto dt1 = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
      auto dt2 = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
      auto dt3 = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2);
      auto dt4 = std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3);
      auto dt5 = std::chrono::duration_cast<std::chrono::milliseconds>(t5 - t4);
      auto dt6 = std::chrono::duration_cast<std::chrono::milliseconds>(t6 - t5);
      auto dt7 = std::chrono::duration_cast<std::chrono::milliseconds>(t6 - t0);
      std::cout << "U=fft(u) " << dt1.count() << " ms, ";
      std::cout << "n=f(u) " << dt2.count() << " ms, ";
      std::cout << "N=fft(n) " << dt3.count() << " ms, ";
      std::cout << "U=L(U, N) " << dt4.count() << " ms, ";
      std::cout << "u=FFT^-1(U) " << dt5.count() << " ms, ";
      std::cout << "W(u) " << dt6.count() << " ms, ";
      std::cout << "T " << dt7.count() << std::endl;
    }
  }
  auto stop = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
  if (me == 0) {
    std::cout << n << " iterations in " << duration.count() / 1000.0
              << " seconds (" << duration.count() / n << " ms / iteration)"
              << std::endl;
  }

  if (me == 0) {
    std::cout << "Simulation done. Exit message: " + s->exit_msg << std::endl;
  }
}

int main(int argc, char *argv[]) {

  argparse::ArgumentParser program("diffusion");

  program.add_argument("--verbose")
      .help("increase output verbosity")
      .default_value(true)
      .implicit_value(true);

  program.add_argument("--Lx")
      .help("Number of grid points in x direction")
      .scan<'i', int>()
      .default_value(512);

  program.add_argument("--Ly")
      .help("Number of grid points in y direction")
      .scan<'i', int>()
      .default_value(512);

  program.add_argument("--Lz")
      .help("Number of grid points in z direction")
      .scan<'i', int>()
      .default_value(512);

  program.add_argument("--results-dir")
      .help("Where to write results")
      .default_value("./results");

  try {
    program.parse_args(argc, argv);
  } catch (const std::runtime_error &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    std::exit(1);
  }

  Simulation *s = new BasicPFC();
  auto Lx = program.get<int>("--Lx");
  auto Ly = program.get<int>("--Ly");
  auto Lz = program.get<int>("--Lz");
  double dx = 2.0 * pi / 8.0;
  double dy = dx;
  double dz = dx;
  double x0 = -0.5 * Lx * dx;
  double y0 = -0.5 * Ly * dy;
  double z0 = -0.5 * Lz * dz;
  s->set_domain({x0, y0, z0}, {dx, dx, dx}, {Lx, Ly, Lz});
  s->set_time(0.0, 10.0, 1.0);
  s->set_max_iters(10);
  s->set_results_dir(program.get<std::string>("--results-dir"));
  MPI_Init(&argc, &argv);
  MPI_Solve(s);
  MPI_Finalize();

  return 0;
}
