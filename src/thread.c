/* Copyright 2017 IBM Corp.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * 	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <libpdbg.h>

#include "main.h"
#include "optcmd.h"
#include "path.h"

static int print_thread_status(struct pdbg_target *target, uint32_t index, uint64_t *arg, uint64_t *valid)
{
	struct thread_state *status = (struct thread_state *) arg;

	status[index] = thread_status(target);
	valid[index] = true;
	return 1;
}

static int print_core_thread_status(struct pdbg_target *core_target, uint32_t index, uint64_t *maxindex, uint64_t *unused1)
{
	struct thread_state status[8];
	uint64_t valid[8] = {0};
	int i, rc;

	printf("c%02d:  ", index);

	/* TODO: This cast is gross. Need to rewrite for_each_child_target as an iterator. */
	rc = for_each_child_target("thread", core_target, print_thread_status, (uint64_t *) &status[0], &valid[0]);
	for (i = 0; i <= *maxindex; i++) {
		if (!valid[i]) {
			printf("    ");
			continue;
		}

		if (status[i].active)
			printf("A");
		else
			printf(".");

		switch (status[i].sleep_state) {
		case PDBG_THREAD_STATE_DOZE:
			printf("D");
			break;

		case PDBG_THREAD_STATE_NAP:
			printf("N");
			break;

		case PDBG_THREAD_STATE_SLEEP:
			printf("Z");
			break;

		case PDBG_THREAD_STATE_STOP:
			printf("S");
			break;

		default:
			printf(".");
			break;
		}

		if (status[i].quiesced)
			printf("Q");
		else
			printf(".");
		printf(" ");

	}
	printf("\n");

	return rc;
}

static bool is_real_address(struct thread_regs *regs, uint64_t addr)
{
	return true;
	if ((addr & 0xf000000000000000ULL) == 0xc000000000000000ULL)
		return true;
	return false;
}

static int load8(struct pdbg_target *target, uint64_t addr, uint64_t *value)
{
	if (adu_getmem(target, addr, (uint8_t *)value, 8)) {
		pdbg_log(PDBG_ERROR, "Unable to read memory address=%016" PRIx64 ".\n", addr);
		return 0;
	}

	return 1;
}

uint64_t flip_endian(uint64_t v)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	return be64toh(v);
#else
	return le64toh(v);
#endif
}

static int dump_stack(struct thread_regs *regs)
{
	struct pdbg_target *target;
	uint64_t next_sp = regs->gprs[1];
	uint64_t pc;
	bool finished = false;

	pdbg_for_each_class_target("adu", target) {
		if (pdbg_target_probe(target) != PDBG_TARGET_ENABLED)
			continue;
		break;
	}

	printf("STACK:           SP                NIA\n");
	if (!target)
		pdbg_log(PDBG_ERROR, "Unable to read memory (no ADU found)\n");

	if (!(next_sp && is_real_address(regs, next_sp))) {
		printf("SP:0x%016" PRIx64 " does not appear to be a stack\n", next_sp);
		return 0;
	}

	while (!finished) {
		uint64_t sp = next_sp;
		uint64_t tmp, tmp2;
		bool flip = false;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		bool be = false;
#else
		bool be = true;
#endif

		if (!is_real_address(regs, sp))
			break;

		if (!load8(target, sp, &tmp))
			return 1;
		if (!load8(target, sp + 16, &pc))
			return 1;

		tmp2 = flip_endian(tmp);

		if (!tmp) {
			finished = true;
			goto no_flip;
		}

		/*
		 * Basic endian detection.
		 * Stack grows down, so as we unwind it we expect to see
		 * increasing addresses without huge jumps.  The stack may
		 * switch endian-ness across frames in some cases (e.g., LE
		 * kernel calling BE OPAL).
		 */

		/* Check for OPAL stack -> Linux stack */
		if ((sp >= 0x30000000UL && sp < 0x40000000UL) &&
				!(tmp >= 0x30000000UL && tmp < 0x40000000UL)) {
			if (tmp >> 60 == 0xc)
				goto no_flip;
			if (tmp2 >> 60 == 0xc)
				goto do_flip;
		}

		/* Check for Linux -> userspace */
		if ((sp >> 60 == 0xc) && !(tmp >> 60 == 0xc)) {
			finished = true; /* Don't decode userspace */
			if (tmp >> 60 == 0)
				goto no_flip;
			if (tmp2 >> 60 == 0)
				goto do_flip;
		}

		/* Otherwise try to ensure sane stack */
		if (tmp < sp || (tmp - sp > 0xffffffffUL)) {
			if (tmp2 < sp || (tmp2 - sp > 0xffffffffUL)) {
				finished = true;
				goto no_flip;
			}
do_flip:
			next_sp = tmp2;
			flip = true;
			be = !be;
		} else {
no_flip:
			next_sp = tmp;
		}

		if (flip)
			pc = flip_endian(pc);

		printf(" 0x%016" PRIx64 " 0x%016" PRIx64 " (%s)\n",
			sp, pc, be ? "big-endian" : "little-endian");
	}
	printf(" 0x%016" PRIx64 "\n", next_sp);

	return 0;
}

static int get_thread_max_index(struct pdbg_target *target, uint32_t index, uint64_t *maxindex, uint64_t *unused)
{
	if (index > *maxindex)
		*maxindex = index;
	return 1;
}

static int get_core_max_threads(struct pdbg_target *core_target, uint32_t index, uint64_t *maxindex, uint64_t *unused1)
{
	return for_each_child_target("thread", core_target, get_thread_max_index, maxindex, NULL);
}

static int print_proc_thread_status(struct pdbg_target *pib_target, uint32_t index, uint64_t *unused, uint64_t *unused1)
{
	int i;
	uint64_t maxindex = 0;

	for_each_child_target("core", pib_target, get_core_max_threads, &maxindex, NULL);

	printf("\np%01dt:", index);
	for (i = 0; i <= maxindex; i++)
		printf("   %d", i);
	printf("\n");

	return for_each_child_target("core", pib_target, print_core_thread_status, &maxindex, NULL);
};

static int state_thread(struct pdbg_target *thread_target, uint32_t index, uint64_t *i_doBacktrace, uint64_t *unused)
{
	struct thread_regs regs;
	bool do_backtrace = (bool) i_doBacktrace;

	if (ram_state_thread(thread_target, &regs))
		return 0;

	if (do_backtrace)
		dump_stack(&regs);

	return 1;
}

static int thread_start(void)
{
	struct pdbg_target *target;
	int count = 0;

	for_each_path_target_class("thread", target) {
		if (pdbg_target_status(target) != PDBG_TARGET_ENABLED)
			continue;

		ram_start_thread(target);
		count++;
	}

	return count;
}
OPTCMD_DEFINE_CMD(start, thread_start);

static int thread_step(uint64_t steps)
{
	struct pdbg_target *target;
	int count = 0;

	for_each_path_target_class("thread", target) {
		if (pdbg_target_status(target) != PDBG_TARGET_ENABLED)
			continue;

		ram_step_thread(target, (int)steps);
		count++;
	}

	return count;
}
OPTCMD_DEFINE_CMD_WITH_ARGS(step, thread_step, (DATA));

static int thread_stop(void)
{
	struct pdbg_target *target;
	int count = 0;

	for_each_path_target_class("thread", target) {
		if (pdbg_target_status(target) != PDBG_TARGET_ENABLED)
			continue;

		ram_stop_thread(target);
		count++;
	}

	return count;
}
OPTCMD_DEFINE_CMD(stop, thread_stop);

static int thread_status_print(void)
{
	return for_each_target("pib", print_proc_thread_status, NULL, NULL);
}
OPTCMD_DEFINE_CMD(threadstatus, thread_status_print);

static int thread_sreset(void)
{
	struct pdbg_target *target;
	int count = 0;

	for_each_path_target_class("thread", target) {
		if (pdbg_target_status(target) != PDBG_TARGET_ENABLED)
			continue;

		ram_sreset_thread(target);
		count++;
	}

	return count;
}
OPTCMD_DEFINE_CMD(sreset, thread_sreset);


struct reg_flags {
	bool do_backtrace;
};

#define REG_BACKTRACE_FLAG ("--backtrace", do_backtrace, parse_flag_noarg, false)

static int thread_state(struct reg_flags flags)
{
	int err;

	err = for_each_target("thread", state_thread,
			(uint64_t *)flags.do_backtrace, NULL);

	for_each_target_release("thread");

	return err;
}
OPTCMD_DEFINE_CMD_ONLY_FLAGS(regs, thread_state, reg_flags, (REG_BACKTRACE_FLAG));
