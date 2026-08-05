
#pragma once

#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "s2geography/geoarrow-geography.h"
#include "s2geography/operation.h"
#include "s2geography/sedona_udf/sedona_udf_internal.h"

namespace s2geography {

namespace internal {

/// \brief Output shim that stashes a scalar value for an Operation
///
/// This is the duck-typed equivalent of the ArrowOutputBuilder used by the
/// SedonaUDF kernels: it implements exactly the members an Exec uses to
/// communicate a scalar result and writes them into an Operation instead of
/// into an Arrow array.
template <typename T>
struct StashedScalarOutput {
  void Append(T value) { *out_ = value; }
  void AppendNull() { *has_result_ = false; }

  T* out_;
  bool* has_result_;
};

using StashedDoubleOutput = StashedScalarOutput<double>;
using StashedIntOutput = StashedScalarOutput<int64_t>;

/// \brief Output shim that stashes a list of integers for an Operation
///
/// This is the duck-typed equivalent of the ListOutputBuilder used by the
/// SedonaUDF kernels. Values are appended to the child builder and Append()
/// closes the (single) list element.
struct StashedIntListOutput {
  /// \brief Duck-typed equivalent of the list's child builder
  struct Items {
    void Append(int64_t value) { out_->push_back(value); }

    std::vector<int64_t>* out_;
  };

  Items& items() { return items_; }
  void Append() {}
  void AppendNull() { *has_result_ = false; }

  Items items_;
  bool* has_result_;
};

/// \brief Adapt a geography argument to the input type an Exec expects
///
/// Most Execs consume a GeoArrowGeography directly; the ones that consume
/// planar input (e.g., st_tessellategeog) declare a GeoArrowGeometryInputView
/// and consume the underlying GeoArrowGeometryView instead. Every Exec with
/// two geography arguments consumes geographies directly, so only the
/// single-geography Operations below apply this adaptation.
template <typename ArgT>
typename ArgT::c_type OperationArg(const GeoArrowGeography& arg) {
  if constexpr (std::is_same_v<typename ArgT::c_type,
                               const GeoArrowGeography&>) {
    return arg;
  } else {
    return arg.geom();
  }
}

/// \brief Base for the Operations wrapping an Exec that writes a scalar
///
/// The value is stashed into this Operation's own double or integer result
/// according to T. Subclasses add exactly one Exec* override, which forwards
/// its arguments to Run().
template <typename Exec, typename T, Operation::OutputType kOutputType>
class ScalarOperation : public Operation {
 public:
  explicit ScalarOperation(std::string name)
      : name_(std::move(name)), out_{ResultSlot(), &has_result_} {}

  const std::string& name() const override { return name_; }

  OutputType output_type() const override { return kOutputType; }

 protected:
  /// \brief Run the Exec with the result defaulting to non-null
  ///
  /// The shim's AppendNull() is what clears has_result_ for the Execs that
  /// return null for a non-null input (e.g., st_distance of an EMPTY).
  template <typename... Args>
  void Run(Args&&... args) {
    has_result_ = true;
    exec_.Exec(std::forward<Args>(args)..., &out_);
  }

 private:
  /// \brief The Operation result member that a value of type T is stashed into
  T* ResultSlot() {
    if constexpr (std::is_same_v<T, double>) {
      return &double_result_;
    } else {
      return &int_result_;
    }
  }

  std::string name_;
  StashedScalarOutput<T> out_;
  Exec exec_;
};

/// \brief Base for the Operations wrapping an Exec that builds a geography
///
/// Subclasses add exactly one Exec* override, which forwards its arguments to
/// Run().
template <typename Exec>
class GeographyOperation : public Operation {
 public:
  explicit GeographyOperation(std::string name) : name_(std::move(name)) {}

  const std::string& name() const override { return name_; }

  OutputType output_type() const override {
    return Operation::OutputType::kGeography;
  }

  const struct GeoArrowGeometry* GetGeography() const override {
    return out_.geometry();
  }

 protected:
  /// \brief Rewind the builder, run the Exec, and pick up the null channel
  template <typename... Args>
  void Run(Args&&... args) {
    out_.Rewind();
    exec_.Exec(std::forward<Args>(args)..., &out_);
    has_result_ = !out_.is_null();
  }

 private:
  std::string name_;
  sedona_udf::GeoArrowScalarOutputBuilder out_;
  Exec exec_;
};

/// \brief Operation for an Exec with one geography argument returning a scalar
template <typename Exec, typename T, Operation::OutputType kOutputType>
class UnaryScalarOperation : public ScalarOperation<Exec, T, kOutputType> {
 public:
  using ScalarOperation<Exec, T, kOutputType>::ScalarOperation;

  void ExecGeog(const GeoArrowGeography& arg0) override {
    this->Run(OperationArg<typename Exec::arg0_t>(arg0));
  }
};

/// \brief Operation for an Exec with two geography arguments returning a
/// scalar
template <typename Exec, typename T, Operation::OutputType kOutputType>
class BinaryScalarOperation : public ScalarOperation<Exec, T, kOutputType> {
 public:
  using ScalarOperation<Exec, T, kOutputType>::ScalarOperation;

  void ExecGeogGeog(const GeoArrowGeography& arg0,
                    const GeoArrowGeography& arg1) override {
    this->Run(arg0, arg1);
  }
};

template <typename Exec>
using UnaryDoubleOperation =
    UnaryScalarOperation<Exec, double, Operation::OutputType::kDouble>;

template <typename Exec>
using UnaryIntOperation =
    UnaryScalarOperation<Exec, int64_t, Operation::OutputType::kInt>;

template <typename Exec>
using BinaryDoubleOperation =
    BinaryScalarOperation<Exec, double, Operation::OutputType::kDouble>;

/// \brief Operation for an Exec with one geography argument returning a
/// geography
template <typename Exec>
class UnaryGeographyOperation : public GeographyOperation<Exec> {
 public:
  using GeographyOperation<Exec>::GeographyOperation;

  void ExecGeog(const GeoArrowGeography& arg0) override {
    this->Run(OperationArg<typename Exec::arg0_t>(arg0));
  }
};

/// \brief Operation for an Exec with two geography arguments returning a
/// geography
template <typename Exec>
class BinaryGeographyOperation : public GeographyOperation<Exec> {
 public:
  using GeographyOperation<Exec>::GeographyOperation;

  void ExecGeogGeog(const GeoArrowGeography& arg0,
                    const GeoArrowGeography& arg1) override {
    this->Run(arg0, arg1);
  }
};

/// \brief Operation for an Exec with a geography and a double argument
/// returning a geography
template <typename Exec>
class GeographyDoubleOperation : public GeographyOperation<Exec> {
 public:
  using GeographyOperation<Exec>::GeographyOperation;

  void ExecGeogDouble(const GeoArrowGeography& arg0, double arg1) override {
    this->Run(OperationArg<typename Exec::arg0_t>(arg0), arg1);
  }
};

}  // namespace internal

}  // namespace s2geography
