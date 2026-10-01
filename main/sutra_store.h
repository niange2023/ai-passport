#pragma once

#include <stdbool.h>

#include "sutra_model.h"

bool sutra_store_init(sutra_model_t *model);
void sutra_store_request_save(const sutra_model_t *model);
bool sutra_store_has_error(void);
