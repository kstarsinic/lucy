/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define C_CFISH_STRING
#define CFISH_USE_SHORT_NAMES
#define CHY_USE_SHORT_NAMES

#include "charmony.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "Clownfish/VTable.h"
#include "Clownfish/String.h"

#include "Clownfish/Err.h"
#include "Clownfish/Util/Memory.h"
#include "Clownfish/Util/StringHelper.h"

String*
Str_new(size_t size) {
    String *self = (String*)VTable_Make_Obj(STRING);
    return Str_init(self, size);
}

String*
Str_init(String *self, size_t size) {
    self->ptr  = (char*)MALLOCATE(size + 1);
    *self->ptr = '\0'; // Empty string.
    self->size = 0;
    self->cap  = size + 1;
    return self;
}

void
Str_Destroy_IMP(String *self) {
    FREEMEM(self->ptr);
    SUPER_DESTROY(self, STRING);
}

