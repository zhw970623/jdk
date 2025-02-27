/*
 * Copyright (c) 2003, 2022, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include <string.h>
#include "jvmti.h"
#include "jvmti_common.h"
#include "jvmti_thread.h"

extern "C" {

/* ============================================================================= */

/* scaffold objects */
static jlong timeout = 0;

/* constant names */
#define THREAD_NAME     "TestedThread"

/* ============================================================================= */

/** Agent algorithm. */
static void JNICALL
agentProc(jvmtiEnv *jvmti, JNIEnv *jni, void *arg) {

  LOG("Wait for thread to start\n");
  if (!agent_wait_for_sync(timeout))
    return;

  /* perform testing */
  {
    LOG("Find thread: %s\n", THREAD_NAME);
    jthread tested_thread = find_thread_by_name(jvmti, jni, THREAD_NAME);
    if (tested_thread == NULL) {
      return;
    }
    LOG("  ... found thread: %p\n", (void *) tested_thread);

    LOG("Suspend thread: %p\n", (void *) tested_thread);
    suspend_thread(jvmti, jni, tested_thread);

    LOG("Let thread to run and finish\n");
    if (!agent_resume_sync()) {
      return;
    }

    LOG("Get state vector for thread: %p\n", (void *) tested_thread);
    {
      jint state = get_thread_state(jvmti, jni, tested_thread);
      LOG("  ... got state vector: %s (%d)\n", TranslateState(state), (int) state);

      if ((state & JVMTI_THREAD_STATE_SUSPENDED) == 0) {
        LOG("SuspendThread() does not turn on flag SUSPENDED:\n"
            "#   state: %s (%d)\n", TranslateState(state), (int) state);
        set_agent_fail_status();
      }
    }

    LOG("Resume thread: %p\n", (void *) tested_thread);
    resume_thread(jvmti, jni, tested_thread);

    LOG("Wait for thread to finish\n");
    if (!agent_wait_for_sync(timeout)) {
      return;
    }

    LOG("Delete thread reference\n");
    jni->DeleteGlobalRef(tested_thread);
  }

  LOG("Let debugee to finish\n");
  if (!agent_resume_sync()) {
    return;
  }
}

/* ============================================================================= */

jint Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) {
  jvmtiEnv *jvmti = NULL;

  timeout = 60 * 1000;

  jint res = jvm->GetEnv((void **) &jvmti, JVMTI_VERSION_9);
  if (res != JNI_OK || jvmti == NULL) {
    LOG("Wrong result of a valid call to GetEnv!\n");
    return JNI_ERR;
  }

  /* add specific capabilities for suspending thread */

  jvmtiCapabilities caps;
  memset(&caps, 0, sizeof(caps));
  caps.can_suspend = 1;
  if (jvmti->AddCapabilities(&caps) != JVMTI_ERROR_NONE) {
    return JNI_ERR;
  }


  if (init_agent_data(jvmti, &agent_data) != JVMTI_ERROR_NONE) {
    return JNI_ERR;
  }

  /* register agent proc and arg */
  if (!set_agent_proc(agentProc, NULL)) {
    return JNI_ERR;
  }

  return JNI_OK;
}

}
