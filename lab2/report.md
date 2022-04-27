# 实验二 CFS代码分析

## 实验目的

根据提供的源代码分析CFS的代码实现原理，比如包括哪些函数？设置了哪些关键参数？执行流程是怎样的？

## 实验环境

Linux源代码版本：5.12，见外部网站https://github.com/torvalds/linux/tree/v5.12。

## 实验步骤

### CFS算法

完全公平调度器（英语：Completely Fair Scheduler，缩写为CFS），Linux内核的一部分，负责进程调度。参考了康恩·科里瓦斯（Con Kolivas）提出的调度器源代码后，由匈牙利程式员英格·蒙内（Ingo Molnar）所提出。在Linux kernel 2.6.23之后采用，取代先前的O(1)调度器，成为系统预设的调度器。它负责将CPU资源，分配给正在执行中的进程，目标在于最大化程式互动效能与整体CPU的使用率。使用红黑树来实作，算法效率为$O(log(n))$。

### 相关数据结构

在这一部分中我们会对Linux实现CFS调度的关键数据结构进行介绍。以下所有数据结构均在`include/linux/sched.h`文件中实现。

#### struct rq

Linux在每颗CPU执行调度最主要的数据结构，操作系统采用它管理每一颗CPU的各种进程的管理与调度。

```c
struct rq {
	/* runqueue lock: */
	raw_spinlock_t		__lock; //自旋锁，用于保护struct rq操作的原子性，防止其结构被并行程序破坏

    // ...
    
	struct cfs_rq		cfs; // 完全公平调度队列
	struct rt_rq		rt; // 实时调度队列
	struct dl_rq		dl; // dealine调度队列

	// ...
    
	struct task_struct __rcu	*curr; //当前进程
	struct task_struct	*idle; // 空闲进程
	struct task_struct	*stop; // 暂停进程
	
    // ..
    
};
```

可以看到`rq`中含有三个调度队列，分别实现了完全公平调度、实时调度和deadline调度，并且记录了内部的进程信息，根据进程所处的状态将其分类进行管理。

#### struct cfs_rq

Linux在这一部分中实现了CFS调度队列，对它进行管理和数据的修改以实现对程序的完全公平调度。

```c
struct cfs_rq {
	// ...

	u64			exec_clock; //开始执行的时刻
	u64			min_vruntime; //最小虚拟执行时间

    // ...

	struct rb_root_cached	tasks_timeline; // 红黑树缓存节点

	/*
	 * 'curr' points to currently running entity on this cfs_rq.
	 * It is set to NULL otherwise (i.e when none are currently running).
	 */
	struct sched_entity	*curr; //当前调度实体
	struct sched_entity	*next;
	struct sched_entity	*last;
	struct sched_entity	*skip;

	// ...
    
};

```

在`cfs_rq`中记录了调度算法执行的相关信息和管理的数据结构。在队列的管理方面采用红黑树进行运行队列的管理，这是一种自平衡的搜索二叉树数据结构，可以尽可能地帮助缩短搜索目标节点的时间。同时`cfs_rq`也保存了当前调度的相关信息，包括执行时间、执行进程等信息。

#### struct sched_entity

在`cfs_rq`中我们可以看到其调度对象并非直接管理进程的数据结构的`task_struct`，这是为了赋予Linux调度更高的自由度，使其可以实现组调度等功能。取而代之的是`sched_entity`，这是操作系统进行CFS调度最直接的调度对象。

```c
struct sched_entity {
	/* For load-balancing: */
	struct load_weight		load;keyi
	struct rb_node			run_node; // 对应的红黑树节点
	struct list_head		group_node;
	unsigned int			on_rq; // 标记该调度实体是否在已在调度队列中

	u64				exec_start; // 开始执行的时间
	u64				sum_exec_runtime; // 总执行时间
	u64				vruntime; // 虚拟执行时间
	u64				prev_sum_exec_runtime; // 之前虚拟执行时间和

	u64				nr_migrations;

// 这一部分实现了组调度的相关对象
#ifdef CONFIG_FAIR_GROUP_SCHED
	int				depth;
	struct sched_entity		*parent;
	/* rq on which this entity is (to be) queued: */
	struct cfs_rq			*cfs_rq;
	/* rq "owned" by this entity/group: */
	struct cfs_rq			*my_q;
	/* cached value of my_q->h_nr_running */
	unsigned long			runnable_weight;
#endif

	// ...
    
};
```

在`sched_entity`中，首先存储了调度实体对应的红黑树节点和执行时间的有关信息。除此之外可以看见其也实现了组调度的相关数据结构，在编译时可供选择。

#### struct task_struct

`task_struct`是Linux中对进程和线程进行描述的数据结构。在这其中保存了每个进程或线程用于CFS调度的调度实体，同时也保存了相关调度的数据统计信息。

```c
struct task_struct {
#ifdef CONFIG_THREAD_INFO_IN_TASK

    // ...
    
	randomized_struct_fields_start

	void				*stack; // 栈空间
	refcount_t			usage;
	/* Per task flags (PF_*), defined further below: */
	unsigned int			flags;
	unsigned int			ptrace;

	// ...
    
	int				on_rq; // 标记是否在调度队列中

	int				prio;
	int				static_prio;
	int				normal_prio;
	unsigned int			rt_priority; // 实时调度优先度

	struct sched_entity		se; // cfs调度实体
	struct sched_rt_entity		rt; // 实时调度实体
	struct sched_dl_entity		dl; // deadline调度实体
	const struct sched_class	*sched_class; // 该task_struct采用的调度器

	// ...

	struct sched_statistics         stats; // 对调度数据的统计

	// ...
	struct mm_struct		*mm; // 内存管理数据结构
	struct mm_struct		*active_mm;

    // ...
    
};
```

### Linux调度器

#### 调度器类

Linux为所有的调度器设置了一个结构进行描述。

```c
struct sched_class {

#ifdef CONFIG_UCLAMP_TASK
	int uclamp_enabled;
#endif

	void (*enqueue_task) (struct rq *rq, struct task_struct *p, int flags); //进程入队
	void (*dequeue_task) (struct rq *rq, struct task_struct *p, int flags); //进程出队
	void (*yield_task)   (struct rq *rq); // 进程放弃当前CPU控制权
	bool (*yield_to_task)(struct rq *rq, struct task_struct *p); // 进程放弃当前CPU控制权并将其交给指定进程

	void (*check_preempt_curr)(struct rq *rq, struct task_struct *p, int flags); // 检测是否能抢占当前进程

	struct task_struct *(*pick_next_task)(struct rq *rq); // 选择下一个进程

	void (*put_prev_task)(struct rq *rq, struct task_struct *p); // 设置上一进程
	void (*set_next_task)(struct rq *rq, struct task_struct *p, bool first); // 设置下一进程

	// ...

	void (*task_tick)(struct rq *rq, struct task_struct *p, int queued);
	void (*task_fork)(struct task_struct *p);
	void (*task_dead)(struct task_struct *p);

	/*
	 * The switched_from() call is allowed to drop rq->lock, therefore we
	 * cannot assume the switched_from/switched_to pair is serialized by
	 * rq->lock. They are however serialized by p->pi_lock.
	 */
	void (*switched_from)(struct rq *this_rq, struct task_struct *task); // 进程切换相关
	void (*switched_to)  (struct rq *this_rq, struct task_struct *task); // 进程切换相关
	void (*prio_changed) (struct rq *this_rq, struct task_struct *task,
			      int oldprio); // 修改进程优先级

	unsigned int (*get_rr_interval)(struct rq *rq,
					struct task_struct *task);

	void (*update_curr)(struct rq *rq); // 更新当前进程数据

#define TASK_SET_GROUP		0
#define TASK_MOVE_GROUP		1

#ifdef CONFIG_FAIR_GROUP_SCHED
	void (*task_change_group)(struct task_struct *p, int type);
#endif
};
```

`struct sched_class`主要实现了抽象的调度器结构，其中的成员变量主要是调度相关的函数，可以通过对相关函数指针的修改指向不同的函数以实现不同的调度方法。

CFS调度器对象在宏中实现

```c
DEFINE_SCHED_CLASS(fair) = {

	.enqueue_task		= enqueue_task_fair,
	.dequeue_task		= dequeue_task_fair,
	.yield_task		= yield_task_fair,
	.yield_to_task		= yield_to_task_fair,

	.check_preempt_curr	= check_preempt_wakeup,

	.pick_next_task		= __pick_next_task_fair,
	.put_prev_task		= put_prev_task_fair,
	.set_next_task          = set_next_task_fair,

#ifdef CONFIG_SMP
	.balance		= balance_fair,
	.pick_task		= pick_task_fair,
	.select_task_rq		= select_task_rq_fair,
	.migrate_task_rq	= migrate_task_rq_fair,

	.rq_online		= rq_online_fair,
	.rq_offline		= rq_offline_fair,

	.task_dead		= task_dead_fair,
	.set_cpus_allowed	= set_cpus_allowed_common,
#endif

	.task_tick		= task_tick_fair,
	.task_fork		= task_fork_fair,

	.prio_changed		= prio_changed_fair,
	.switched_from		= switched_from_fair,
	.switched_to		= switched_to_fair,

	.get_rr_interval	= get_rr_interval_fair,

	.update_curr		= update_curr_fair,

#ifdef CONFIG_FAIR_GROUP_SCHED
	.task_change_group	= task_change_group_fair,
#endif

#ifdef CONFIG_UCLAMP_TASK
	.uclamp_enabled		= 1,
#endif
};
```

其中宏定义如下

```c
#define DEFINE_SCHED_CLASS(name) \
const struct sched_class name##_sched_class \
	__aligned(__alignof__(struct sched_class)) \
	__section("__" #name "_sched_class")
```

可以看见以上代码创建了一个名为`fair_sched_class`的调度器对象，并为其中会调用的方法进行了赋值。这样就通过C语言实现了部分面向对象编程的功能。同样，对于其他的方法也实现了对应的调度器，比如`rt_sched_class`，`dl_sched_class`用于规定其他调度器的对应调度方法。

### 调度函数

在这一部分中我们将对CFS的部分调度函数的实现过程进行讨论。

#### enqueue_task_fair

`enqueue_task_fair`函数实现了向调度队列添加指定进程的功能。主要部分如下

```c
if (se->on_rq)
    break;
cfs_rq = cfs_rq_of(se);
enqueue_entity(cfs_rq, se, flags); // 调度实体在此处加入队列

cfs_rq->h_nr_running++;
cfs_rq->idle_h_nr_running += idle_h_nr_running;

if (cfs_rq_is_idle(cfs_rq))
    idle_h_nr_running = 1;

/* end evaluation on encountering a throttled cfs_rq */
if (cfs_rq_throttled(cfs_rq))
    goto enqueue_throttle;

flags = ENQUEUE_WAKEUP;
```

其中`enqueue_entity`主要依赖`__enqueue_entity`函数工作，其如下

```c
static void __enqueue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	rb_add_cached(&se->run_node, &cfs_rq->tasks_timeline, __entity_less);
}
```

可以看见在该函数中，调度器将调度实体对应的红黑树节点加入队列管理的红黑树中。除此之外，`enqueue_entity`依靠`place_entity`设置初始的虚拟运行时间，通过当前的虚拟执行时间和最小虚拟时间重置虚拟执行时间。

同理，`dequeue_task_fair`也主要依赖`__dequeue_entity`实现，通过这个函数将对应的红黑树节点移出队列，并设置虚拟运行时间和修改相应的标志位。

#### __pick_next_task_fair

`__pick_next_task_fair`主要依赖`pick_next_task_fair`函数实现。

`pick_next_task_fair`函数主要工作在这几行

```c
if (prev)
    put_prev_task(rq, prev);
// ...
se = pick_next_entity(cfs_rq, NULL);
set_next_entity(cfs_rq, se);
```

首先调度器先把当前进程设置为上一进程，这一详细过程可见后续的`put_prev_entity_fair`介绍。

其次它在`cfs_rq`中选出新的调度实体，并将其设置为下一个可执行的调度实体，分别具体实现如后。

```c
static struct sched_entity *
pick_next_entity(struct cfs_rq *cfs_rq, struct sched_entity *curr)
{
	struct sched_entity *left = __pick_first_entity(cfs_rq); // 选出最左节点代表的进程
	struct sched_entity *se;

	/*
	 * If curr is set we have to see if its left of the leftmost entity
	 * still in the tree, provided there was anything in the tree at all.
	 */
	if (!left || (curr && entity_before(curr, left)))
		left = curr; // 如果选取进程失败或者进程虚拟运行时间小于当前进程，则选择当前进程

	se = left; /* ideally we run the leftmost entity */

	/*
	 * Avoid running the skip buddy, if running something else can
	 * be done without getting too unfair.
	 */
    // 如果当前进程需要跳过，那么重新选择新的进程
	if (cfs_rq->skip && cfs_rq->skip == se) {
		struct sched_entity *second;

		if (se == curr) {
			second = __pick_first_entity(cfs_rq); // 如果选择的进程就是当前进程，那么选择队列第一个进程
		} else {
			second = __pick_next_entity(se); //否则选择下一个进程
			if (!second || (curr && entity_before(curr, second))) 
				second = curr; // 如果选取进程失败或者进程虚拟运行时间小于当前进程，则选择当前进程
		}

		if (second && wakeup_preempt_entity(second, left) < 1)
			se = second; // 如果选取进程可抢占最左节点代表的进程，则选择选取的进程作为下一个调度的进程
	}

	if (cfs_rq->next && wakeup_preempt_entity(cfs_rq->next, left) < 1) {
		/*
		 * Someone really wants this to run. If it's not unfair, run it.
		 */
		se = cfs_rq->next;
	} else if (cfs_rq->last && wakeup_preempt_entity(cfs_rq->last, left) < 1) {
		/*
		 * Prefer last buddy, try to return the CPU to a preempted task.
		 */
		se = cfs_rq->last;
	}

	return se;
}
```

选取好进程调度实体后，调度器通过`set_next_entity`将其设置为下一个运行进程，其核心工作部分在于

```c
/* 'current' is not kept within the tree. */
if (se->on_rq) {
    /*
    * Any task has to be enqueued before it get to execute on
    * a CPU. So account for the time it spent waiting on the
    * runqueue.
    */
    update_stats_wait_end_fair(cfs_rq, se);
    __dequeue_entity(cfs_rq, se); // 把选中的调度节点移出调度队列
    update_load_avg(cfs_rq, se, UPDATE_TG);
}

update_stats_curr_start(cfs_rq, se); // 更新调度实体开始计时
cfs_rq->curr = se; // 设置当前进程为选取的进程

// ...

se->prev_sum_exec_runtime = se->sum_exec_runtime; // 更新进程的虚拟运行时间
```

这样我们便完成了调度对象的选取和切换。

#### put_prev_task_fair

`put_prev_task_fair`的作用是将当前进程加入到调度队列中，其主要由`put_prev_entity`实现。

`put_prev_entity`主要由以下部分完成工作。

```c
if (prev->on_rq) {
    update_stats_wait_start_fair(cfs_rq, prev); 
    /* Put 'current' back into the tree. */
    __enqueue_entity(cfs_rq, prev); // 将当前进程加入调度队列中
    /* in !on_rq case, update occurred at dequeue */
    update_load_avg(cfs_rq, prev, 0); 
}
cfs_rq->curr = NULL;
```

该函数将指定进程加入调度队列中，并把当前的运行进程置空。

#### set_next_task_fair

`set_next_task_fair`的作用是设置下一运行进程，主要由`set_next_entity`实现，其已在上方介绍过。可见`put_prev_task_fair`和`set_next_task_fair`共同实现了在`cfs_rq`中切换当前运行进程的工作。

#### task_fork_fair

`task_fork_fair`用于处理在`fork`新进程时为子进程设置虚拟运行时间的情况，内容如下

```c
static void task_fork_fair(struct task_struct *p)
{
	struct cfs_rq *cfs_rq;
	struct sched_entity *se = &p->se, *curr;
	struct rq *rq = this_rq();
	struct rq_flags rf;

	rq_lock(rq, &rf); // 上锁
	update_rq_clock(rq); // 更新时钟

	cfs_rq = task_cfs_rq(current);
	curr = cfs_rq->curr;
	if (curr) {
		update_curr(cfs_rq);
		se->vruntime = curr->vruntime; // 如果存在当前运行的进程，那么将子进程的虚拟运行时间初始化为当前运行进程的虚拟运行时间
	}
	place_entity(cfs_rq, se, 1); // 结合cfs_rq的最小虚拟运行时间和se目前的虚拟运行时间，为其重新设置虚拟运行时间

    // 如果设置必须优先运行子进程，那么在父进程的虚拟运行时间小于子进程的情况下交换双方的虚拟运行时间并重新调度保证子进程的优先运行
	if (sysctl_sched_child_runs_first && curr && entity_before(curr, se)) {
		/*
		 * Upon rescheduling, sched_class::put_prev_task() will place
		 * 'current' within the tree based on its new key value.
		 */
		swap(curr->vruntime, se->vruntime);
		resched_curr(rq);
	}

	se->vruntime -= cfs_rq->min_vruntime; // 当前虚拟运行时间自减最小运行时间
	rq_unlock(rq, &rf); // 解锁
}
```

#### update_curr_fair

`update_curr_fair`用于更新当前调度实体的相关状态，在`update_curr`函数中实现，具体如下

```c
static void update_curr(struct cfs_rq *cfs_rq)
{
	struct sched_entity *curr = cfs_rq->curr;
	u64 now = rq_clock_task(rq_of(cfs_rq));
	u64 delta_exec;

	if (unlikely(!curr))
		return;

    // Caculate the execution time.
	delta_exec = now - curr->exec_start; // 计算当前调度实体的运行时间
	if (unlikely((s64)delta_exec <= 0))
		return;

    // Update the latest start time.
	curr->exec_start = now; // 重置当前进程开始运行的时间

    // ...

	curr->sum_exec_runtime += delta_exec; // 重新计算当前调度实体运行时间的总长
    
    // ...

    // Adjust the virtual running time.
	curr->vruntime += calc_delta_fair(delta_exec, curr); // 增加当前调度实体的虚拟运行时间
	update_min_vruntime(cfs_rq); // 更新调度队列的最小虚拟运行时间

    // ...
}
```

其中我们关注`calc_delta_fair`，这关系虚拟运行时间的计算方式，其主要由`__calc_delta_fair`实现

```c
static u64 __calc_delta(u64 delta_exec, unsigned long weight, struct load_weight *lw)
{
	u64 fact = scale_load_down(weight);
	u32 fact_hi = (u32)(fact >> 32);
	int shift = WMULT_SHIFT;
	int fs;

	__update_inv_weight(lw);

	if (unlikely(fact_hi)) {
		fs = fls(fact_hi);
		shift -= fs;
		fact >>= fs;
	}

	fact = mul_u32_u32(fact, lw->inv_weight);

	fact_hi = (u32)(fact >> 32);
	if (fact_hi) {
		fs = fls(fact_hi);
		shift -= fs;
		fact >>= fs;
	}

	return mul_u64_u32_shr(delta_exec, fact, shift);
}
```

可以得到虚拟运行时间的计算公式为

$$vruntime=\Delta_{exec}\times\frac{weight}{lw.weight}$$

或者

$$vruntime=(\Delta_{exec}\times weight\times lw.inv\_weight)>>32$$

### 进程创建

Linux进程的创建主要依赖`copy_process`函数对父进程的相关进程内容进行复制，在这其中也实现了对CFS调度相关内容和数据结构的初始化。主要在其中进行了两个函数的调用。

首先是`sched_fork`函数，在该函数中实现了对进程调度方法的指定，内容如下

```c
if (dl_prio(p->prio))
	return -EAGAIN;
else if (rt_prio(p->prio))
	p->sched_class = &rt_sched_class;
else
	p->sched_class = &fair_sched_class;
```

可见只有在并未设置`prio`属性的情况下进程才会被设置为CFS调度。

其次是`sched_cgroup_fork`函数，在这其中主要实现了对新进程设置初始的虚拟运行时间，即调用上方所提到的`task_fork_fair`

```c
if (p->sched_class->task_fork)
	p->sched_class->task_fork(p);
```

在此处被设置为CFS调度的进程们对进程的虚拟运行时间进行了初始化并决定是否进行进程的重新调度。

### 进程调度

Linux的进程调度主要在`schedule`函数中实现，其主要内容如下

```c
asmlinkage __visible void __sched schedule(void)
{
	struct task_struct *tsk = current;

	sched_submit_work(tsk); // 提交当前进程的工作以避免死锁的产生
	do {
		preempt_disable(); // 关闭抢占
		__schedule(SM_NONE); // 进行调度
		sched_preempt_enable_no_resched(); // 如果不需要重新调度则开启抢占
	} while (need_resched()); // 重复以上过程直至无需调度
	sched_update_worker(tsk); // 更新进程管理相关数据结构
}
EXPORT_SYMBOL(schedule);
```

而`__schedule`函数主要工作在这一部分

```c
next = pick_next_task(rq, prev, &rf); // 选择下一执行进程
clear_tsk_need_resched(prev); // 清理需要调度的标志位
clear_preempt_need_resched(); // 清理允许不需要重新调度时开启抢占的标志位
```

而`pick_next_task`的主要工作由在上一部分中介绍的`pick_next_task_fair`函数实现，它选择了新的调度实体并将原先的调度实体重新加入运行队列中。在其他的部分中`__schedule`函数同时进行了相关统计数据的计算并且完成了进程的上下文切换的工作，真正实现了进程运行的转换。

### 其他部分

在相关的函数中，Linux也实现了SMP、NUMA以及组调度相关的机制，由于与CFS的调度流程并无太大关联此处不再赘述。