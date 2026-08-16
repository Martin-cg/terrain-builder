#pragma once

#include "batch_store.h"
#include "bvh.h"
#include "frontier.h"

namespace bvh {

using Storage = BatchStore_<Batch>;

}

namespace frontier {

using RunStorage = BatchStore_<Batch>;

}