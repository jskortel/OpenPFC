#include <catch2/catch_test_macros.hpp>
#include <openpfc/simulator.hpp>

using namespace pfc;

// Define a mock implementation of the Model class for testing
class MockModel : public Model {
public:
  void step(double) override {}
  void initialize(double) override {}
};

// Define a mock implementation of the FieldModifier class for testing
class MockIC : public FieldModifier {
public:
  void apply(Model &m, double) override {
    std::vector<double> &field = m.get_real_field(get_field_name());
    std::fill(field.begin(), field.end(), 1.0);
  }
};

TEST_CASE("Simulator functionality", "[Simulator]") {
  SECTION("Add and apply initial conditions") {
    // Create an instance of the Simulator
    Time time({0.0, 10.0, 1.0}, 1.0);
    MockModel model;
    Simulator simulator(model, time);

    // Add a field called "phi" to the model and fill it with zeros
    size_t N = 1;
    std::vector<double> phi(N);
    std::fill(phi.begin(), phi.end(), 0.0);
    model.add_field("phi", phi);

    // Create a FieldModifier object
    MockIC ic;
    ic.set_field_name("phi");

    // Create unique pointer to the FieldModifier object
    std::unique_ptr<FieldModifier> ptr_ic = std::make_unique<MockIC>(ic);

    // Add initial condition to the simulator
    simulator.add_initial_conditions(std::move(ptr_ic));
    REQUIRE(simulator.get_initial_conditions().size() == 1);

    // Apply initial conditions to the model
    REQUIRE(model.get_real_field("phi")[0] == 0.0);
    simulator.apply_initial_conditions();
    REQUIRE(model.get_real_field("phi")[0] == 1.0);
  }

  SECTION("Add and apply boundary conditions") {
    // Create an instance of the Simulator
    Time time({0.0, 10.0, 1.0}, 1.0);
    MockModel model;
    Simulator simulator(model, time);

    // Add a field called "phi" to the model and fill it with zeros
    size_t N = 1;
    std::vector<double> phi(N);
    std::fill(phi.begin(), phi.end(), 0.0);
    model.add_field("phi", phi);

    // Create a FieldModifier object
    MockIC ic;
    ic.set_field_name("phi");

    // Create unique pointer to the FieldModifier object
    std::unique_ptr<FieldModifier> ptr_ic = std::make_unique<MockIC>(ic);

    // Add boundary condition to the simulator
    simulator.add_boundary_conditions(std::move(ptr_ic));
    REQUIRE(simulator.get_boundary_conditions().size() == 1);

    // Apply boundary conditions to the model
    REQUIRE(model.get_real_field("phi")[0] == 0.0);
    simulator.apply_boundary_conditions();
    REQUIRE(model.get_real_field("phi")[0] == 1.0);
  }
}
