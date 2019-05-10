#include "job_system.h"
#include "engine/array.h"
#include "engine/engine.h"
#include "engine/fibers.h"
#include "engine/iallocator.h"
#include "engine/log.h"
#include "engine/math_utils.h"
#include "engine/mt/atomic.h"
#include "engine/mt/lock_free_fixed_queue.h"
#include "engine/mt/sync.h"
#include "engine/mt/task.h"
#include "engine/mt/thread.h"
#include "engine/profiler.h"

namespace Malmy
{

	namespace JobSystem
	{

		enum {
			HANDLE_ID_MASK = 0xffFF,
			HANDLE_GENERATION_MASK = 0xffFF0000
		};

		struct Job
		{
			void(*task)(void*) = nullptr;
			void* data = nullptr;
			SignalHandle dec_on_finish;
		};

		struct Signal {
			volatile int value;
			u32 generation;
			Job next_job;
			SignalHandle sibling;
		};

		struct FiberDecl
		{
			int idx;
			Fiber::Handle fiber;
			Job current_job;
			bool job_finished;
			struct WorkerTask* worker_task;
		};

		struct System
		{
			System(IAllocator& allocator)
				: m_allocator(allocator)
				, m_workers(allocator)
				, m_job_queue(allocator)
				, m_ready_fibers(allocator)
				, m_signals_pool(allocator)
				, m_work_signal(true)
				, m_event_outside_job(true)
				, m_free_queue(allocator)
				, m_free_fibers(allocator)
				, m_sync(false)
			{
				m_signals_pool.resize(4096);
				m_free_queue.resize(4096);
				m_event_outside_job.trigger();
				m_work_signal.reset();
				for (int i = 0; i < 4096; ++i) {
					m_free_queue[i] = i;
					m_signals_pool[i].sibling = JobSystem::INVALID_HANDLE;
					m_signals_pool[i].generation = 0;
				}
			}

			MT::SpinMutex m_sync;
			MT::Event m_event_outside_job;
			MT::Event m_work_signal;
			Array<MT::Task*> m_workers;
			Array<Job> m_job_queue;
			Array<Signal> m_signals_pool;
			FiberDecl m_fiber_pool[512];
			Array<FiberDecl*> m_free_fibers;
			Array<FiberDecl*> m_ready_fibers;
			IAllocator& m_allocator;
			Array<u32> m_free_queue;
		};

		static System* g_system = nullptr;

		static MALMY_FORCE_INLINE FiberDecl* getReadyFiber(System& system)
		{
			MT::SpinLock lock(system.m_sync);

			if (system.m_ready_fibers.empty()) return nullptr;
			FiberDecl* fiber = system.m_ready_fibers.back();
			system.m_ready_fibers.pop();
			return fiber;
		}

		static MALMY_FORCE_INLINE Job getReadyJob(System& system)
		{
			MT::SpinLock lock(system.m_sync);

			if (system.m_job_queue.empty()) return { nullptr, nullptr };

			Job job = system.m_job_queue.back();
			system.m_job_queue.pop();
			if (system.m_job_queue.empty()) system.m_work_signal.reset();
			return job;
		}

		static thread_local MT::Task* g_worker = nullptr;

		struct WorkerTask : MT::Task
		{

			WorkerTask(System& system)
				: Task(system.m_allocator)
				, m_system(system)
			{
				//
			}

			static FiberDecl& getFreeFiber()
			{
				MT::SpinLock lock(g_system->m_sync);

				ASSERT(!g_system->m_free_fibers.empty());
				FiberDecl* decl = g_system->m_free_fibers.back();
				g_system->m_free_fibers.pop();

				return *decl;
			}

			static void handleSwitch(FiberDecl& fiber)
			{
				if (fiber.job_finished) {
					g_system->m_free_fibers.push(&fiber);
				}
				g_system->m_sync.unlock();
			}

			int task() override
			{
				g_worker = this;
				Fiber::initThread(manage, &m_primary_fiber);
				return 0;
			}

			static void __stdcall manage(void* data)

			{
				WorkerTask* that = (WorkerTask*)g_worker;
				that->m_finished = false;
				while (!that->m_finished)
				{
					FiberDecl* fiber = getReadyFiber(*g_system);
					if (fiber) {
						fiber->worker_task = that;
						that->m_current_fiber = fiber;
						fiber->job_finished = false;

						g_system->m_sync.lock();
						Fiber::switchTo(&that->m_primary_fiber, fiber->fiber);

						that->m_current_fiber = nullptr;
						handleSwitch(*fiber);
						continue;
					}

					Job job = getReadyJob(*g_system);
					if (job.task) {
						FiberDecl& fiber_decl = getFreeFiber();
						fiber_decl.worker_task = that;
						fiber_decl.current_job = job;
						fiber_decl.job_finished = false;
						that->m_current_fiber = &fiber_decl;

						g_system->m_sync.lock();
						Fiber::switchTo(&that->m_primary_fiber, fiber_decl.fiber);
						that->m_current_fiber = nullptr;
						handleSwitch(fiber_decl);
					}
					else
					{
						PROFILE_BLOCK("wait");
						g_system->m_work_signal.waitTimeout(1);
					}
				}
			}

			bool m_finished = false;
			FiberDecl* m_current_fiber = nullptr;
			Fiber::Handle m_primary_fiber;
			System& m_system;
		};

		static MALMY_FORCE_INLINE SignalHandle allocateSignal()
		{
			ASSERT(!g_system->m_free_queue.empty());

			const u32 handle = g_system->m_free_queue.back();
			Signal& w = g_system->m_signals_pool[handle & HANDLE_ID_MASK];
			w.value = 1;
			w.sibling = JobSystem::INVALID_HANDLE;
			w.next_job.task = nullptr;
			g_system->m_free_queue.pop();

			return handle & HANDLE_ID_MASK | w.generation;
		}

		void trigger(SignalHandle handle)
		{
			ASSERT((handle & HANDLE_ID_MASK) < 4096);

			MT::SpinLock lock(g_system->m_sync);

			Signal& counter = g_system->m_signals_pool[handle & HANDLE_ID_MASK];
			--counter.value;
			if (counter.value > 0) return;

			bool any_new_job = false;
			SignalHandle iter = handle;
			while (isValid(iter)) {
				Signal& signal = g_system->m_signals_pool[iter & HANDLE_ID_MASK];
				if (signal.next_job.task) {
					g_system->m_job_queue.push(signal.next_job);
					any_new_job = true;
				}
				signal.generation = (((signal.generation >> 16) + 1) & 0xffFF) << 16;
				g_system->m_free_queue.push(iter & HANDLE_ID_MASK | signal.generation);
				signal.next_job.task = nullptr;
				iter = signal.sibling;
			}
			if (any_new_job) g_system->m_work_signal.trigger();
		}

		static MALMY_FORCE_INLINE bool isSignalZero(SignalHandle handle, bool lock)
		{
			if (!isValid(handle)) return true;

			const u32 gen = handle & HANDLE_GENERATION_MASK;
			const u32 id = handle & HANDLE_ID_MASK;

			if (lock) g_system->m_sync.lock();
			Signal& counter = g_system->m_signals_pool[id];
			bool is_zero = counter.generation != gen || counter.value == 0;
			if (lock) g_system->m_sync.unlock();
			return is_zero;
		}

		static MALMY_FORCE_INLINE void runInternal(void* data
			, void(*task)(void*)
			, SignalHandle precondition
			, bool lock
			, SignalHandle* on_finish)
		{
			Job j;
			j.data = data;
			j.task = task;

			if (lock) g_system->m_sync.lock();
			j.dec_on_finish = [&]() -> SignalHandle {
				if (!on_finish) return INVALID_HANDLE;
				if (isValid(*on_finish) && !isSignalZero(*on_finish, false)) {
					++g_system->m_signals_pool[*on_finish & HANDLE_ID_MASK].value;
					return *on_finish;
				}
				return allocateSignal();
			}();
			if (on_finish) *on_finish = j.dec_on_finish;

			if (!isValid(precondition) || isSignalZero(precondition, false)) {
				g_system->m_job_queue.push(j);
				g_system->m_work_signal.trigger();
			}
			else {
				Signal& counter = g_system->m_signals_pool[precondition & HANDLE_ID_MASK];
				if (counter.next_job.task) {
					const SignalHandle ch = allocateSignal();
					Signal& c = g_system->m_signals_pool[ch & HANDLE_ID_MASK];
					c.next_job = j;
					c.sibling = counter.sibling;
					counter.sibling = ch;
				}
				else {
					counter.next_job = j;
				}
			}

			if (lock) g_system->m_sync.unlock();
		}

		void run(void* data, void(*task)(void*), SignalHandle* on_finished, SignalHandle precondition)
		{
			runInternal(data, task, precondition, true, on_finished);
		}

		static void __stdcall fiberProc(void* data)
		{
			g_system->m_sync.unlock();

			FiberDecl* fiber_decl = (FiberDecl*)data;
			for (;;)
			{
				Job job = fiber_decl->current_job;
				job.task(job.data);
				if (isValid(job.dec_on_finish)) trigger(job.dec_on_finish);
				fiber_decl->job_finished = true;

				g_system->m_sync.lock();
				Fiber::switchTo(&fiber_decl->fiber, fiber_decl->worker_task->m_primary_fiber);
				g_system->m_sync.unlock();
			}
		}

		bool init(IAllocator& allocator)
		{
			ASSERT(!g_system);

			g_system = MALMY_NEW(allocator, System)(allocator);
			g_system->m_work_signal.reset();

			int count = Math::maximum(1, int(MT::getCPUsCount() - 0));
			for (int i = 0; i < count; ++i) {
				WorkerTask* task = MALMY_NEW(allocator, WorkerTask)(*g_system);
				if (task->create("Job system worker")) {
					g_system->m_workers.push(task);
					task->setAffinityMask((u64)1 << i);
				}
				else {
					g_log_error.log("Engine") << "Job system worker failed to initialize.";
					MALMY_DELETE(allocator, task);
				}
			}

			g_system->m_free_fibers.reserve(lengthOf(g_system->m_fiber_pool));
			for (FiberDecl& fiber : g_system->m_fiber_pool) {
				g_system->m_free_fibers.push(&fiber);
			}

			const int fiber_num = lengthOf(g_system->m_fiber_pool);
			for (int i = 0; i < fiber_num; ++i) {
				FiberDecl& decl = g_system->m_fiber_pool[i];
				decl.fiber = Fiber::create(64 * 1024, fiberProc, &g_system->m_fiber_pool[i]);
				decl.idx = i;
				decl.worker_task = nullptr;
			}

			return !g_system->m_workers.empty();
		}

		void shutdown()
		{
			if (!g_system) return;

			IAllocator& allocator = g_system->m_allocator;
			for (MT::Task* task : g_system->m_workers)
			{
				WorkerTask* wt = (WorkerTask*)task;
				wt->m_finished = true;
			}

			for (MT::Task* task : g_system->m_workers)
			{
				while (!task->isFinished()) g_system->m_work_signal.trigger();
				task->destroy();
				MALMY_DELETE(allocator, task);
			}

			for (FiberDecl& fiber : g_system->m_fiber_pool)
			{
				Fiber::destroy(fiber.fiber);
			}

			MALMY_DELETE(allocator, g_system);
			g_system = nullptr;
		}

		void wait(SignalHandle handle)
		{
			g_system->m_sync.lock();
			if (isSignalZero(handle, false)) {
				g_system->m_sync.unlock();
				return;
			}

			if (g_worker) {
				PROFILE_BLOCK("waiting");
				FiberDecl* fiber_decl = ((WorkerTask*)g_worker)->m_current_fiber;

				runInternal(fiber_decl, [](void* data) {
					MT::SpinLock lock(g_system->m_sync);
					g_system->m_ready_fibers.push((FiberDecl*)data);
				}, handle, false, nullptr);
				fiber_decl->job_finished = false;
				Fiber::switchTo(&fiber_decl->fiber, fiber_decl->worker_task->m_primary_fiber);

				ASSERT(isSignalZero(handle, false));
				g_system->m_sync.unlock();
			}
			else
			{
				PROFILE_BLOCK("not a job waiting");

				g_system->m_event_outside_job.reset();

				runInternal(nullptr, [](void* data) {
					g_system->m_event_outside_job.trigger();
				}, handle, false, nullptr);

				g_system->m_sync.unlock();

				MT::yield();
				while (!isSignalZero(handle, true)) {
					g_system->m_event_outside_job.waitTimeout(1);
				}
			}
		}

	} // namespace JobSystem

} // namespace Malmy
