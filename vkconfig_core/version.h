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
 * Authors:
 * - Richard S. Wright Jr.
 * - Christophe Riccio
 */

#pragma once

#include <cstddef>
#include <string>

static const bool SUPPORT_VKCONFIG_2_0_1 = true;

class Version {
   public:
    static const Version VKCONFIG;
    static const Version VKHEADER;

    Version(uint32_t version_complete) : _data(version_complete) {}
    Version(uint32_t version_major, uint32_t version_minor, uint32_t version_patch);
    Version(const char *version);

    std::string str() const;

    bool operator!=(const Version &other_version) const { return !(*this == other_version); }

    bool operator==(const Version &other_version) const { return _data == other_version._data; }

    bool operator<(const Version &other_version) const { return _data < other_version._data; }

    bool operator>=(const Version &other_version) const { return !(*this < other_version); }

    bool operator>(const Version &other_version) const { return _data > other_version._data; }

    bool operator<=(const Version &other_version) const { return !(*this > other_version); }

    uint32_t GetMajor() const;
    uint32_t GetMinor() const;
    uint32_t GetPatch() const;

   private:
    const uint32_t _data;
};
