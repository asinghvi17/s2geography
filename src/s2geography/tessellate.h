#pragma once

#include <memory>

#include "s2geography/operation.h"
#include "s2geography/sedona_udf/sedona_extension.h"

namespace s2geography {

std::unique_ptr<Operation> TessellateGeog();
std::unique_ptr<Operation> TessellateGeom();
std::unique_ptr<Operation> Segmentize();

namespace sedona_udf {

/// \brief Kernel to convert planar geometry to geography (spherical)
void TessellateToGeog(struct SedonaCScalarKernel* out);

/// \brief Kernel to convert geography (spherical) to planar geometry
void TessellateToGeom(struct SedonaCScalarKernel* out);

/// \brief Kernel to segmentize geography along spherical edges
void Segmentize(struct SedonaCScalarKernel* out);

}  // namespace sedona_udf

}  // namespace s2geography
