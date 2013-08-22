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

#include <string.h>
#include <stdio.h>

#define CHY_USE_SHORT_NAMES
#define CFISH_USE_SHORT_NAMES
#define TESTCFISH_USE_SHORT_NAMES

#include "charmony.h"

#include "Clownfish/Test/TestString.h"

#include "Clownfish/String.h"
#include "Clownfish/VTable.h"
#include "Clownfish/TestHarness/TestBatchRunner.h"

TestString*
TestStr_new() {
    return (TestString*)VTable_Make_Obj(TESTSTRING);
}

void
TestStr_Run_IMP(TestString *self, TestBatchRunner *runner) {
    TestBatchRunner_Plan(runner, (TestBatch*)self, 1);
    PASS(runner, "Dummy test");
}


