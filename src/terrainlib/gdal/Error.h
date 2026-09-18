#pragma once

#include "error/Error.h"
#include "gdal/CplError.h"
#include "gdal/OgrError.h"

namespace gdal {

// An error from a GDAL or an OGR call.
using Error = alp::Error<CplError, OgrError>;

} // namespace gdal
