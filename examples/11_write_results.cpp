#include <complex>
#include <mpi.h>
#include <openpfc/openpfc.hpp>
#include <string>
#include <vector>

using namespace pfc;

/**
 * @brief Get type of field as a string.
 *
 */
template <typename T> std::string get_data_type_name() {
  if constexpr (std::is_same_v<T, float>)
    return "Float32";
  else if constexpr (std::is_same_v<T, double>)
    return "Float64";
  else if constexpr (std::is_same_v<T, int>)
    return "Int32";
  else if constexpr (std::is_same_v<T, long>)
    return "Int64";
  else if constexpr (std::is_same_v<T, long long>)
    return "Int64";
  else if constexpr (std::is_same_v<T, unsigned int>)
    return "UInt32";
  else if constexpr (std::is_same_v<T, unsigned long>)
    return "UInt64";
  else if constexpr (std::is_same_v<T, unsigned long long>)
    return "UInt64";
  else if constexpr (std::is_same_v<T, std::complex<double>>)
    return "Float64";
  else if constexpr (std::is_same_v<T, std::complex<float>>)
    return "Float32";
  else
    throw std::runtime_error("Unsupported data type.");
}

/**
 * @brief Interface for writing results to a file.
 *
 */
template <typename T> class IWriter {
private:
  std::array<int, 3> m_global_dimensions; ///< Global dimensions of the field.
  std::array<int, 3> m_local_dimensions;  ///< Local dimensions of the field.
  std::array<int, 3> m_offset;            ///< Offset of the local domain.
  std::array<double, 3> m_origin;         ///< Origin of the local domain.
  std::array<double, 3> m_spacing;        ///< Spacing of the local domain.
  std::string m_uri;                      ///< URI of the file to write to.
  std::string m_field_name = "default";

public:
  /**
   * @brief Set the URI. Depending on implementation, this can be file name, a database URI, etc.
   *
   * @param uri
   */
  virtual void set_uri(const std::string &uri) { m_uri = uri; }

  /**
   * @brief Get the URI.
   *
   * @return const std::string&
   */
  const std::string &get_uri() const { return m_uri; }

  /**
   * @brief Set the domain of the field.
   *
   * @param global_dimensions can be obtained from the world object.
   * @param local_dimensions can be obtained from the field object.
   * @param offset can be obtained from the field object.
   */
  virtual void set_domain(const std::array<int, 3> &global_dimensions, const std::array<int, 3> &local_dimensions,
                          const std::array<int, 3> &offset) {
    m_global_dimensions = global_dimensions;
    m_local_dimensions = local_dimensions;
    m_offset = offset;
  }

  /**
   * @brief Get the global dimensions of the field.
   *
   * @return const std::array<int, 3>&
   */
  const std::array<int, 3> &get_global_dimensions() const { return m_global_dimensions; }

  /**
   * @brief Get the local dimensions of the field.
   *
   * @return const std::array<int, 3>&
   */
  const std::array<int, 3> &get_local_dimensions() const { return m_local_dimensions; }

  /**
   * @brief Get the offset of the local domain.
   *
   * @return const std::array<int, 3>&
   */
  const std::array<int, 3> &get_offset() const { return m_offset; }

  /**
   * @brief Set the origin of the local domain.
   *
   * @param origin
   */
  virtual void set_origin(const std::array<double, 3> &origin) { m_origin = origin; }

  /**
   * @brief Get the origin of the local domain.
   *
   * @return const std::array<double, 3>&
   */
  const std::array<double, 3> &get_origin() const { return m_origin; }

  /**
   * @brief Set the spacing of the local domain.
   *
   * @param spacing
   */
  virtual void set_spacing(const std::array<double, 3> &spacing) { m_spacing = spacing; }

  /**
   * @brief Get the spacing of the local domain.
   *
   * @return const std::array<double, 3>&
   */
  const std::array<double, 3> &get_spacing() const { return m_spacing; }

  /**
   * @brief Set the name of the field.
   *
   * @param field_name
   */
  virtual void set_field_name(const std::string &field_name) { m_field_name = field_name; }

  /**
   * @brief Get the name of the field.
   *
   * @return const std::string&
   */
  const std::string &get_field_name() const { return m_field_name; }

  /**
   * @brief Get the size of the data.
   *
   * @return size_t
   */
  size_t get_data_size() const { return m_global_dimensions[0] * m_global_dimensions[1] * m_global_dimensions[2]; }

  std::string get_data_type() const { return get_data_type_name<T>(); }

  /**
   * @brief Initialize the writer. Depending on the implementation, this could
   * be opening a file, connecting to a database and so on.
   *
   */
  virtual void initialize() {}

  /**
   * @brief Write the results to the file.
   *
   * @param data
   */
  virtual void write(const std::vector<T> &data) = 0;
};

class VtkHeader {
private:
  std::string m_name;                  ///< Name of the field.
  std::string m_data_type;             ///< Data type of the field.
  unsigned short int m_data_type_size; ///< Size of the data type.
  std::array<int, 3> m_whole_extent;   ///< Whole extent of the field.
  std::array<double, 3> m_origin;      ///< Origin of the field.
  std::array<double, 3> m_spacing;     ///< Spacing of the field.
  std::array<int, 3> m_piece_extent;   ///< Extent of the piece.
  size_t m_header_size = 1024;         ///< Size of the header.

public:
  /**
   * @brief Set the name of the field.
   *
   * @param name
   */
  void set_name(const std::string &name) { m_name = name; }

  /**
   * @brief Set the data type of the field.
   *
   * @param data_type
   */
  void set_data_type(const std::string &data_type) { m_data_type = data_type; }

  /**
   * @brief Set the data type size of the field.
   *
   * @param data_type_size
   */
  void set_data_type_size(unsigned short int data_type_size) { m_data_type_size = data_type_size; }

  /**
   * @brief Set the whole extent of the field.
   *
   * @param whole_extent
   */
  void set_whole_extent(const std::array<int, 3> &whole_extent) { m_whole_extent = whole_extent; }

  /**
   * @brief Set the origin of the field.
   *
   * @param origin
   */
  void set_origin(const std::array<double, 3> &origin) { m_origin = origin; }

  /**
   * @brief Set the spacing of the field.
   *
   * @param spacing
   */
  void set_spacing(const std::array<double, 3> &spacing) { m_spacing = spacing; }

  /**
   * @brief Set the extent of the piece.
   *
   * @param piece_extent
   */
  void set_piece_extent(const std::array<int, 3> &piece_extent) { m_piece_extent = piece_extent; }

  /**
   * @brief Get the data type of the field.
   *
   */
  std::string get_data_type() const { return m_data_type; }

  /**
   * @brief Get the size of the header.
   *
   */
  size_t get_header_size() const { return m_header_size; }

  /**
   * @brief Get the whole extend as a string in a format x_low x_high y_low y_high z_low z_high.
   *
   */
  std::string get_whole_extent() const {
    std::stringstream ss;
    ss << "0 " << m_whole_extent[0] - 1 << " 0 " << m_whole_extent[1] - 1 << " 0 " << m_whole_extent[2] - 1;
    return ss.str();
  }

  /**
   * @brief Get the origin as a string in a format x y z.
   *
   */
  std::string get_origin() const {
    std::stringstream ss;
    ss << m_origin[0] << " " << m_origin[1] << " " << m_origin[2];
    return ss.str();
  }

  /**
   * @brief Get the spacing as a string in a format x y z.
   *
   */
  std::string get_spacing() const {
    std::stringstream ss;
    ss << m_spacing[0] << " " << m_spacing[1] << " " << m_spacing[2];
    return ss.str();
  }

  /**
   * @brief Get field name.
   *
   * @return const std::string&
   */
  const std::string &get_field_name() const { return m_name; }

  /**
   * @brief Get the data size
   *
   * @return size_t
   */
  size_t get_data_size() const {
    size_t data_size = m_data_type_size;
    for (int i : m_whole_extent) data_size *= i;
    return data_size;
  }

  /**
   * @brief Write the header to the file. It should be called only by the rank 0. Typical output is:
   *
   *    <?xml version="1.0" encoding="utf-8"?>
   *    <VTKFile type="ImageData" version="1.0" byte_order="LittleEndian" header_type="UInt64">
   *      <ImageData WholeExtent="0 3 0 2 0 1" Origin="1 1 1" Spacing="1 1 1">
   *        <Piece Extent="0 3 0 2 0 1">
   *          <PointData>
   *            <DataArray type="Float64" Name="density" NumberOfComponents="1" format="appended" offset="0"/>
   *          </PointData>
   *        </Piece>
   *      </ImageData>
   *      <AppendedData encoding="raw">
   *      _[DATA]_
   *      </AppendedData>
   *    </VTKFile>
   *
   * Description of _[DATA]_: The data is written in binary format. The first byte is `_` and then the first 8
   * bytes are the size of the data in bytes (size_t). Then comes the data. The idea of this implementation is that
   * there is a fixed offset (1024 bytes) reserved for the header and the data is written after that.
   */
  void write(std::string &filename) {
    // Open the file in binary mode
    std::fstream file(filename, std::ios::in | std::ios::out | std::ios::binary);

    if (!file) {
      std::cerr << "Failed to open the file: " << filename << std::endl;
      return;
    }

    file.seekp(0);
    file << R"(<?xml version="1.0" encoding="utf-8"?>)" << std::endl;
    file << R"(<VTKFile type="ImageData" version="1.0" byte_order="LittleEndian" header_type="UInt64">)" << std::endl;
    file << R"(  <ImageData WholeExtent=")" << get_whole_extent() << R"(" Origin=")" << get_origin() << R"("
      Spacing=")"
         << get_spacing() << R"(">)" << std::endl;
    file << R"(    <Piece Extent=")" << get_whole_extent() << R"(">)" << std::endl;
    file << R"(      <PointData>)" << std::endl;
    file << R"(        <DataArray type=")" << get_data_type() << R"(" Name=")" << get_field_name()
         << R"(" NumberOfComponents="1" format="appended" offset="0"/>)" << std::endl;
    file << R"(      </PointData>)" << std::endl;
    file << R"(    </Piece>)" << std::endl;
    file << R"(  </ImageData>)" << std::endl;
    file << R"(  <AppendedData encoding="raw">)" << std::endl;
    // The first byte is `_` and then the first 8 bytes are the size of the data in bytes (size_t)
    size_t pos = get_header_size() - sizeof(char) - sizeof(size_t);
    file.seekp(pos);
    file << R"(_)";
    size_t data_size = get_data_size();
    file.write(reinterpret_cast<const char *>(&data_size), sizeof(size_t));

    // Then comes data: this is already written by other processes, so we just go to the end of the file.
    file.seekp(0, std::ios::end);
    file << R"(  </AppendedData>)" << std::endl;
    file << R"(</VTKFile>)" << std::endl;
    file.close();
  }
};

/**
 * @brief Writes results to a VTK file. This implementation is using `MPI_File_write_all`.
 *
 */
template <typename T> class VtkWriter : public IWriter<T> {

private:
  MPI_Datatype m_filetype;   // MPI datatype for the file.
  MPI_Comm m_comm;           // MPI communicator.
  size_t header_size = 1024; // Offset in bytes.

public:
  /**
   * @brief Construct a new VtkWriter object
   *
   */
  VtkWriter(MPI_Comm comm = MPI_COMM_WORLD) : m_comm(comm) {}

  MPI_Datatype get_filetype() const { return m_filetype; }

  MPI_Datatype get_mpi_datatype() const {
    if constexpr (std::is_same_v<T, float>)
      return MPI_FLOAT;
    else if constexpr (std::is_same_v<T, double>)
      return MPI_DOUBLE;
    else if constexpr (std::is_same_v<T, int>)
      return MPI_INT;
    else if constexpr (std::is_same_v<T, long>)
      return MPI_LONG;
    else if constexpr (std::is_same_v<T, long long>)
      return MPI_LONG_LONG;
    else if constexpr (std::is_same_v<T, unsigned int>)
      return MPI_UNSIGNED;
    else if constexpr (std::is_same_v<T, unsigned long>)
      return MPI_UNSIGNED_LONG;
    else if constexpr (std::is_same_v<T, unsigned long long>)
      return MPI_UNSIGNED_LONG_LONG;
    else if constexpr (std::is_same_v<T, std::complex<double>>)
      return MPI_DOUBLE_COMPLEX;
    else if constexpr (std::is_same_v<T, std::complex<float>>)
      return MPI_COMPLEX;
    else
      throw std::runtime_error("Unsupported data type.");
  }

  /**
   * @brief Check if the rank is 0.
   *
   * @return true
   * @return false
   */
  bool is_rank0() const {
    int rank;
    MPI_Comm_rank(m_comm, &rank);
    return rank == 0;
  }

  /**
   * @brief Initialize the writer. This will create the MPI datatype for the file and commit it.
   *
   */
  void initialize() override {
    int ndims = 3;
    const int *size_array = IWriter<T>::get_global_dimensions().data();
    const int *subsize_array = IWriter<T>::get_local_dimensions().data();
    const int *start_array = IWriter<T>::get_offset().data();
    int order = MPI_ORDER_FORTRAN;
    MPI_Datatype oldtype = get_mpi_datatype();
    MPI_Datatype *newtype = &m_filetype;
    MPI_Type_create_subarray(ndims, size_array, subsize_array, start_array, order, oldtype, newtype);
    MPI_Type_commit(&m_filetype);
  }

  /**
   * @brief Write the results to the file.
   *
   * @param data
   */
  void write(const std::vector<T> &data) override {
    MPI_File fh;
    MPI_Status status;
    int amode = MPI_MODE_CREATE | MPI_MODE_WRONLY;
    MPI_File_open(m_comm, IWriter<T>::get_uri().c_str(), amode, MPI_INFO_NULL, &fh);
    MPI_File_set_size(fh, 0);
    MPI_File_set_view(fh, header_size, get_mpi_datatype(), get_filetype(), "native", MPI_INFO_NULL);
    MPI_File_write_all(fh, data.data(), data.size(), get_mpi_datatype(), &status);
    if (status.MPI_ERROR != MPI_SUCCESS) {
      std::cerr << "Error writing to file: " << status.MPI_ERROR << std::endl;
      // MPI_Abort(m_comm, status.MPI_ERROR);
    }
    MPI_File_close(&fh);
    // Wait for all processes to finish writing, after that write header
    MPI_Barrier(m_comm);
    if (is_rank0()) {
      VtkHeader header;
      header.set_name(IWriter<T>::get_field_name());
      header.set_data_type(IWriter<T>::get_data_type());
      header.set_data_type_size(sizeof(T));
      header.set_whole_extent(IWriter<T>::get_global_dimensions());
      header.set_origin(IWriter<T>::get_origin());
      header.set_spacing(IWriter<T>::get_spacing());
      header.set_piece_extent(IWriter<T>::get_global_dimensions());
      std::string filename = IWriter<T>::get_uri();
      header.write(filename);
    }
  }
};

// In this example, we will write the results of a simulation to a file.
int main(int argc, char **argv) {
  MPI_Worker worker(argc, argv);
  World world({4, 3, 2});
  Decomposition decomp(world);
  DiscreteField<double, 3> field(decomp);

  std::vector<double> arr(2 * 3 * 4);
  for (unsigned int i = 0; i < arr.size(); i++) arr[i] = static_cast<double>(i);
  field.set_data(std::move(arr));

  VtkWriter<double> writer;
  writer.set_uri("results.vti");
  writer.set_field_name("density");
  writer.set_domain(world.get_size(), field.get_size(), field.get_offset());
  writer.set_origin(world.get_origin());
  writer.set_spacing(world.get_discretization());
  std::cout << "Writing results to file: " << writer.get_uri() << "\n";
  writer.initialize();
  writer.write(field.get_array().get_data());
  MPI_Barrier(MPI_COMM_WORLD);
  return 0;
}

/*
TEST_CASE("VtkWriter", "[VtkWriter]") {
  World world({8, 2, 2});
  Decomposition decomp(world);
  DiscreteField<double, 3> field(decomp);
  field.apply([](auto x, auto y, auto z) { return x + y + z; });
  VtkWriter<double> writer;
  writer.set_uri("results.vtk");
  writer.set_domain(world.get_size(), field.get_size(), field.get_offset());
  writer.write(field.get_array().get_data());
  std::string expectedOutput = R"EXPECTED(<?xml version="1.0" encoding="utf-8"?>
<VTKFile type="ImageData" version="1.0" byte_order="LittleEndian" header_type="UInt64">
  <ImageData WholeExtent="0 3 0 2 0 1" Origin="1 1 1" Spacing="1 1 1">
    <Piece Extent="0 3 0 2 0 1">
      <PointData>
        <DataArray type="Float64" Name="density" NumberOfComponents="1" format="appended" offset="0"/>
      </PointData>
    </Piece>
  </ImageData>
  <AppendedData encoding="raw">
</VTKFile>)EXPECTED";
  REQUIRE(true);
}
*/
