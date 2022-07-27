#include <iostream>
#include <pfc/model.hpp>
#include <pfc/world.hpp>

/*
This is a "low level" implementation of simple diffusion model. No simulator is
defined: just the model and then manual stepping of it. Also, initial condition
is defined inside model.

Like can be seen, it's enough to derive model from base class Model and
implement three class methods:
1) void initialize(double dt) is called only one time and used to initialize all
   necessary for simulation
2) void step(double dt) is called to step model one time increment
3) std::vector<double> &get_field() is returning the (reference) to field
   variable which can then be processed during steps (writing to disk etc...)
*/

using namespace std;
using namespace pfc;

class Diffusion : public Model {
  using Model::Model;

private:
  vector<double> opL, psi;
  vector<complex<double>> psi_F;
  const bool verbose = false;
  int m_midpoint_idx = -1;

public:
  void initialize(double dt) override {
    if (master) cout << "Allocate space" << endl;
    // The main variable and it's fourier transform
    psi.resize(size_inbox());
    psi_F.resize(size_outbox());
    /*
    Linear operator L

    Because we are doing FFT between real and complex using symmetry,
    it's enough to define only a half of the operator. Thus, the operator size
    matches with the outbox.
    */
    opL.resize(size_outbox());

    /*
    World is defining the global dimensions of the problem as well as origo and
    chosen discretization parameters.
    */
    World w = get_world();
    if (master) cout << "World: " << w << endl;

    /*
    Limits for this particular MPI rank.
    */
    Vec3<int> i_low = get_inbox_low();
    Vec3<int> i_high = get_inbox_high();
    Vec3<int> o_low = get_outbox_low();
    Vec3<int> o_high = get_outbox_high();

    if (master) cout << "Create initial condition" << endl;
    int idx = 0;
    double D = 1.0;
    for (int k = i_low[2]; k <= i_high[2]; k++) {
      for (int j = i_low[1]; j <= i_high[1]; j++) {
        for (int i = i_low[0]; i <= i_high[0]; i++) {
          double x = w.x0 + i * w.dx;
          double y = w.y0 + j * w.dy;
          double z = w.z0 + k * w.dz;
          // Could also be defined
          // size_t idx = k*Lx*Ly + j*Ly + i
          psi[idx] = exp(-(x * x + y * y + z * z) / (4.0 * D));
          if (abs(x) < 1.0e-9 && abs(y) < 1.0e-9 && abs(z) < 1.0e-9) {
            cout << "Found midpoint from index " << idx << endl;
            m_midpoint_idx = idx;
          }
          idx += 1;
        }
      }
    }

    if (master && verbose)
      for (int i = 0, N = psi.size(); i < N; i++) {
        cout << "psi[" << i << "] = " << psi[i] << endl;
      }

    if (master) cout << "Prepare operators" << endl;
    idx = 0;
    const double pi = std::atan(1.0) * 4.0;
    const double fx = 2.0 * pi / (w.dx * w.Lx);
    const double fy = 2.0 * pi / (w.dy * w.Ly);
    const double fz = 2.0 * pi / (w.dz * w.Lz);
    for (int k = o_low[2]; k <= o_high[2]; k++) {
      for (int j = o_low[1]; j <= o_high[1]; j++) {
        for (int i = o_low[0]; i <= o_high[0]; i++) {
          // laplacian operator -k^2
          const double ki = (i <= w.Lx / 2) ? i * fx : (i - w.Lx) * fx;
          const double kj = (j <= w.Ly / 2) ? j * fy : (j - w.Ly) * fy;
          const double kk = (k <= w.Lz / 2) ? k * fz : (k - w.Lz) * fz;
          const double kLap = -(ki * ki + kj * kj + kk * kk);
          if (master && verbose)
            cout << "idx = " << idx << ", ki = " << ki << ", kj " << kj
                 << ", kk = " << kk << ", kLap = " << kLap << endl;
          opL[idx++] = 1.0 / (1.0 - dt * kLap);
        }
      }
    }

    if (master && verbose)
      for (int i = 0, N = opL.size(); i < N; i++) {
        cout << "opL[" << i << "] = " << opL[i] << endl;
      }
  }

  void step(double) override {
    fft_r2c(psi, psi_F);
    for (int k = 0, N = psi_F.size(); k < N; k++) {
      psi_F[k] = opL[k] * psi_F[k];
    }
    fft_c2r(psi_F, psi);
  }

  vector<double> &get_field() override { return psi; }

  int get_midpoint_idx() const { return m_midpoint_idx; }
};

void run() {
  int Lx = 64;
  int Ly = Lx;
  int Lz = Lx;
  double dx = 2.0 * constants::pi / 8.0;
  double dy = dx;
  double dz = dx;
  double x0 = -0.5 * Lx * dx;
  double y0 = -0.5 * Ly * dy;
  double z0 = -0.5 * Lz * dz;
  double t = 0.0;
  double t_stop = 0.5874010519681994;
  double dt = (t_stop - t) / 42;
  int n = 0;
  Diffusion D({Lx, Ly, Lz}, {x0, y0, z0}, {dx, dy, dz});
  D.initialize(dt);
  vector<double> &psi = D.get_field();
  // int idx = 32 * 64 * 64 + 32 * 64 + 32;
  int idx = D.get_midpoint_idx();
  if (idx != -1)
    cout << "n = " << n << ", t = " << t << ", psi[" << idx
         << "] = " << psi[idx] << endl;
  while (t <= t_stop) {
    t += dt;
    n += 1;
    D.step(dt);
    if (idx != -1)
      cout << "n = " << n << ", t = " << t << ", psi[" << idx
           << "] = " << psi[idx] << endl;
  }
}

int main(int argc, char *argv[]) {
  cout << std::fixed;
  cout.precision(12);
  MPI_Init(&argc, &argv);
  run();
  MPI_Finalize();
  return 0;
}

/*
Output:

Allocate space
World: (Lx = 64, Ly = 64, Lz = 64, x0 = -25.132741228718, y0 = -25.132741228718,
z0 = -25.132741228718, dx = 0.785398163397, dy = 0.785398163397, dz =
0.785398163397)
Create initial condition
Found midpoint from index 133152
Prepare operators
n = 0, t = 0.000000000000, psi[133152] = 1.000000000000
n = 1, t = 0.013985739333, psi[133152] = 0.979721090279
n = 2, t = 0.027971478665, psi[133152] = 0.960110027682
n = 3, t = 0.041957217998, psi[133152] = 0.941136780128
n = 4, t = 0.055942957330, psi[133152] = 0.922773010503
n = 5, t = 0.069928696663, psi[133152] = 0.904991962443
n = 6, t = 0.083914435995, psi[133152] = 0.887768354997
n = 7, t = 0.097900175328, psi[133152] = 0.871078285414
n = 8, t = 0.111885914661, psi[133152] = 0.854899139328
n = 9, t = 0.125871653993, psi[133152] = 0.839209507718
n = 10, t = 0.139857393326, psi[133152] = 0.823989110058
n = 11, t = 0.153843132658, psi[133152] = 0.809218723138
n = 12, t = 0.167828871991, psi[133152] = 0.794880115081
n = 13, t = 0.181814611323, psi[133152] = 0.780955984136
n = 14, t = 0.195800350656, psi[133152] = 0.767429901836
n = 15, t = 0.209786089989, psi[133152] = 0.754286260194
n = 16, t = 0.223771829321, psi[133152] = 0.741510222594
n = 17, t = 0.237757568654, psi[133152] = 0.729087678086
n = 18, t = 0.251743307986, psi[133152] = 0.717005198834
n = 19, t = 0.265729047319, psi[133152] = 0.705250000442
n = 20, t = 0.279714786652, psi[133152] = 0.693809904969
n = 21, t = 0.293700525984, psi[133152] = 0.682673306399
n = 22, t = 0.307686265317, psi[133152] = 0.671829138405
n = 23, t = 0.321672004649, psi[133152] = 0.661266844212
n = 24, t = 0.335657743982, psi[133152] = 0.650976348422
n = 25, t = 0.349643483314, psi[133152] = 0.640948030646
n = 26, t = 0.363629222647, psi[133152] = 0.631172700815
n = 27, t = 0.377614961980, psi[133152] = 0.621641576044
n = 28, t = 0.391600701312, psi[133152] = 0.612346258953
n = 29, t = 0.405586440645, psi[133152] = 0.603278717317
n = 30, t = 0.419572179977, psi[133152] = 0.594431264972
n = 31, t = 0.433557919310, psi[133152] = 0.585796543882
n = 32, t = 0.447543658642, psi[133152] = 0.577367507288
n = 33, t = 0.461529397975, psi[133152] = 0.569137403861
n = 34, t = 0.475515137308, psi[133152] = 0.561099762795
n = 35, t = 0.489500876640, psi[133152] = 0.553248379778
n = 36, t = 0.503486615973, psi[133152] = 0.545577303777
n = 37, t = 0.517472355305, psi[133152] = 0.538080824588
n = 38, t = 0.531458094638, psi[133152] = 0.530753461098
n = 39, t = 0.545443833970, psi[133152] = 0.523589950209
n = 40, t = 0.559429573303, psi[133152] = 0.516585236400
n = 41, t = 0.573415312636, psi[133152] = 0.509734461852
n = 42, t = 0.587401051968, psi[133152] = 0.503032957135
*/
