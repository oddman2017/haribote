/* マルチタスク関係 */

#include "bootpack.h"

struct TASKCTL *taskctl;
struct TIMER *task_timer;

struct TASK *task_init(struct MEMMAN *memman)
{
	int i;
	struct TASK *task;
	struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *) ADR_GDT;
	taskctl = (struct TASKCTL *) memman_alloc_4k(memman, sizeof (struct TASKCTL));
	for (i = 0; i < MAX_TASKS; i++) {
		taskctl->tasks0[i].flags = 0;
		taskctl->tasks0[i].sel = (TASK_GDT0 + i) * 8;
		set_segmdesc(gdt + TASK_GDT0 + i, 103, (int) &taskctl->tasks0[i].tss, AR_TSS32);
	}
	task = task_alloc();
	task->flags = 2; /* 動作中マーク */
	task->priority = 2; /* 0.02秒 */
	taskctl->running = 1;
	taskctl->now = 0;
	taskctl->tasks[0] = task;
	load_tr(task->sel);
	task_timer = timer_alloc();
	timer_settime(task_timer, task->priority);
	return task;
}

struct TASK *task_alloc(void)
{
	int i;
	struct TASK *task;
	for (i = 0; i < MAX_TASKS; i++) {
		if (taskctl->tasks0[i].flags == 0) {
			task = &taskctl->tasks0[i];
			task->flags = 1; /* 使用中マーク */
			task->tss.eflags = 0x00000202; /* IF = 1; */
			task->tss.eax = 0; /* とりあえず0にしておくことにする */
			task->tss.ecx = 0;
			task->tss.edx = 0;
			task->tss.ebx = 0;
			task->tss.ebp = 0;
			task->tss.esi = 0;
			task->tss.edi = 0;
			task->tss.es = 0;
			task->tss.ds = 0;
			task->tss.fs = 0;
			task->tss.gs = 0;
			task->tss.ldtr = 0;
			task->tss.iomap = 0x40000000;
			return task;
		}
	}
	return 0; /* もう全部使用中 */
}

void task_run(struct TASK *task, int priority)
{
	if (priority > 0) {
		task->priority = priority;
	}
	if (task->flags != 2) {
		task->flags = 2; /* 動作中マーク */
		taskctl->tasks[taskctl->running] = task;
		taskctl->running++;
	}
	return;
}

void task_switch(void)
{
	struct TASK *task;
	taskctl->now++;
	if (taskctl->now == taskctl->running) {
		taskctl->now = 0;
	}
	task = taskctl->tasks[taskctl->now];
	timer_settime(task_timer, task->priority);
	if (taskctl->running >= 2) {
		farjmp(0, task->sel);
	}
	return;
}

void task_sleep(struct TASK *task)
{
	int i;
	char ts = 0;
	if (task->flags == 2) {		/* 指定タスクがもし起きていたら */
		if (task == taskctl->tasks[taskctl->now]) {
			ts = 1; /* 自分自身を寝かせるので、あとでタスクスイッチする */
		}
		/* taskがどこにいるかを探す */
		for (i = 0; i < taskctl->running; i++) {
			if (taskctl->tasks[i] == task) {
				/* ここにいた */
				break;
			}
		}
		taskctl->running--;
		if (i < taskctl->now) {
			taskctl->now--; /* ずれるので、これもあわせておく */
		}
		/* ずらし */
		for (; i < taskctl->running; i++) {
			taskctl->tasks[i] = taskctl->tasks[i + 1];
		}
		task->flags = 1; /* 動作していない状態 */
		if (ts != 0) {
			/* タスクスイッチする */
			if (taskctl->now >= taskctl->running) {
				/* nowがおかしな値になっていたら、修正する */
				taskctl->now = 0;
			}
			farjmp(0, taskctl->tasks[taskctl->now]->sel);
		}
	}
	return;
}
