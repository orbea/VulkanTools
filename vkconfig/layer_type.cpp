/*
 * Copyright (c) 2020 Valve Corporation
 * Copyright (c) 2020 LunarG, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * The vkConfig2 program monitors and adjusts the Vulkan configuration
 * environment. These settings are wrapped in this class, which serves
 * as the "model" of the system.
 *
 * Authors:
 * - Richard S. Wright Jr. <richard@lunarg.com>
 * - Christophe Riccio <christophe@lunarg.com>
 */

#include "layer_type.h"

#include "util.h"

#include <cassert>

const char* GetLayerTypeLabel(LayerType type) {
    assert(type >= LAYER_TYPE_FIRST && type <= LAYER_TYPE_LAST);

    const char* table[] = {
        "Explicit",         // LAYER_TYPE_EXPLICIT
        "Implicit",         // LAYER_TYPE_IMPLICIT
        "Custom Path"       // LAYER_TYPE_CUSTOM
    };
    static_assert(countof(table) == LAYER_TYPE_COUNT, "The tranlation table size doesn't match the enum number of elements");
    
    return table[type];
}

