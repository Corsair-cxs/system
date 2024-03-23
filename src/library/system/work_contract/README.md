# Work Contracts


A novel lock free/wait free approach to async/parallel task management with superior performance to task queues.

- Lock free
- Wait free
- Scalable architecture supporting infinite number of scheduled contracts _(tasks)_
- Highly deterministic contract _(task)_ scheduling
- Exceptionally fair contract _(task)_ selection 

Documentation: https://buildingcpp.com/work_contract.htm

# Quantifying Performance:
Comparing async task scheduling, descheduling and execution: Work Contracts vs highly performant traditional "task queues."  _(TBB and Moody Camel)_

**Latency is not the same thing as throughput**.  There is a significant difference between being able to perform tasks at a high rate and executing a single task as quickly as possible.  In a lot of environments highly deterministic latency for execution of a single task is far more important than being able to execute as many tasks as is possible within a certain time frame.  For instance, trading engines need to be able to execute infrequent trades as quickly as possible, rather than executing as many trades as possible.

This benchmark is intended to measure both for throughput as well as latency.  The "Low contention" benchmark more closely measures **latency** for infrequent tasks whereas "High contention" more closely measures **throughput**, with the "Medium Contention" benchmark attempting to measure a reasonable combination of the two.  

The "Maximum Content" benchmark more strongly represents the latency involved with queueing and dequeing tasks.  

Across each category the results indicate that the "Work Contract" performs significantly better than traditional "Lockless/Concurrent Task Queue" approach.


# Benchmark:

Benchmark is run on both Intel and AMD processors and measures number of tasks performed per second for increasing number of concurrent threads.

> Notes: Four tests involve ever descreasing task durations.  As the time it takes to complete a task decreases the remaining latency, attributable to scheduling and invoking tasks, becomes a larger component of the overall latency per task.  

**Low Contention: Calculate number of primes up to 1000 using sieve of Eratosthenes**:
> This task is sufficiently complicated which causes the worker threads spend more time executing the task than they do competing for new tasks.  This greatly reduces the contention on the task queue or work contract group.  Yet still, it is clear that the MPMC queues begin to faulter as the number of worker threads increases. 

<img src="https://www.buildingcpp.com/images/AMD_low_contention.png">
<img src="https://www.buildingcpp.com/images/intel_low_contention.png">

**Medium Contention: Calculate number of primes up to 300 using sieve of Eratosthenes**:
> Same as the "low contention" test but with reduced task execution time, thereby increasing the contention on the task queue or work contract group. Again, it is clear that the MPMC queues begin to faulter as the number of worker threads increases. 

<img src="https://www.buildingcpp.com/images/AMD_medium_contention.png">
<img src="https://www.buildingcpp.com/images/intel_medium_contention.png">

**High Contention: Calculate number of primes up to 100 using sieve of Eratosthenes**:
> Same as the "medium contention" test but with a much reduced task execution time, thereby increasing the contention on the task queue or work contract group to nearly maximum. As contention increases all solutions begin to perform worse as threads increase. However, the degradation in performance for work contracts is far less pronounced than it is for MPMC queues.

<img src="https://www.buildingcpp.com/images/AMD_high_contention.png">
<img src="https://www.buildingcpp.com/images/intel_high_contention.png">

**Maximum Contention: Task is empty function which does nothing**:
> By eliminating any work within the task this test creates the maximum contention on the task queue or work contract group. Finally, all solutions are heavily affected by the constant contention with all work removed. However, work contracts consistently out performs the MPMC based solutions. At this point TBB's performance has degraded markedly.

<img src="https://www.buildingcpp.com/images/AMD_maximum_contention.png">
<img src="https://www.buildingcpp.com/images/intel_maximum_contention.png">



