#pragma once
/*
*    基于最小堆的  单个线程   定时器
*
  用法与用量

	TimerManager tm;
	tm.AddTimer(1000)->Start(F1,5);
	tm.AddTimer(1500)->Start(F2);
	tm.AddTimer(700)->Start(F3);

	tm.run();    // run on this thread;
	//tm.StartTimerManager();  // run on a new thread
*/

#ifndef _PETER_C_TIMER_
#define _PETER_C_TIMER_

#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <chrono>
#include <memory>


using _timerFun = std::function<void()>;

enum TimerType { ONCE, CIRCLE };

class Timer
{
public:
	TimerType          m_timerType;            // 单次定时 or 循环
	unsigned int       m_interval;             // 间隔
	size_t		       m_heapIndex;            // 容器内序号
	unsigned long long m_expires;              // 到期时间
	_timerFun          m_fun;                  // 执行的函数

	Timer(unsigned int interval, TimerType timeType = CIRCLE)
		: m_heapIndex(-1), m_interval(interval), m_timerType(timeType) {};
	~Timer() {};

	// 支持无限参数，丢弃返回值（void）
	template<class F, class... Args>
	void Start(F&& f, Args&&... args) {
		m_fun = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
	};
};

// 单线程 定时器管理器
class TimerManager
{
	typedef std::shared_ptr<Timer> pTimer;

private:
	int                   m_interval;        // 检测间隔
	std::atomic<bool>     m_stop_flag;       // 停止标志
	std::thread           m_thread;          // 执行线程
	struct HeapEntry                         // 定时器和序号组合成的结构体，用于排序
	{
		unsigned long long time;
		pTimer             timer;
	};
	std::vector<HeapEntry> heap_;            // 容器

public:
	TimerManager(int miniseconds = 100) : m_interval(miniseconds) { m_stop_flag.store(false); };
	~TimerManager() { StopTimerManager(); };

	void StopTimerManager() {
		m_stop_flag.store(true);
		if (m_thread.joinable())
			m_thread.join();
	};

	bool StartTimerManager() {
		StopTimerManager();
		m_stop_flag.store(false);
		m_thread = std::move(std::thread([this] {this->run(); }));
		return true;
	};

	void run() {
		//std::cout << "begin Timer Manager at thread: " << std::this_thread::get_id() << std::endl;
		while (!m_stop_flag) {
			DetectTimers();
			std::this_thread::sleep_for(std::chrono::milliseconds(m_interval));
		}
		//std::cout <<"end Timer Manager thread ID: " << std::this_thread::get_id() << std::endl;
	};

	// 添加一个已有的定时器，外部修改一下 到期时间即可
	void AddTimer(pTimer t) {
		HeapEntry entry = { t->m_expires, t };
		heap_.push_back(entry);
		UpHeap(heap_.size() - 1);
	};

	// 创建一个定时器，无 fun
	pTimer AddTimer(unsigned int interval, TimerType tp = CIRCLE) {
		pTimer ttt = std::make_shared<Timer>(interval, tp);
		ttt->m_heapIndex = heap_.size();
		ttt->m_expires = interval + TimerManager::GetCurrentMillisecs();
		AddTimer(ttt);
		return ttt;
	};

private:
	//  移除定时器
	void RemoveTimer(size_t index) {
		if (!heap_.empty() && index < heap_.size())
		{
			if (index == heap_.size() - 1) // 仅有一个，直接弹出
			{
				heap_.pop_back();
			}
			else
			{
				SwapHeap(index, heap_.size() - 1);   // 与最后一个交换，弹出最后一个，再从中间位置比较，排序
				heap_.pop_back();
				size_t parent = (index - 1) / 2;
				if (index > 0 && heap_[index].time < heap_[parent].time)
					UpHeap(index);
				else
					DownHeap(index);
			}
		}
	};

	void UpHeap(size_t index) { // 二分排序法
	
		size_t parent = (index - 1) / 2;
		while (index > 0 && heap_[index].time < heap_[parent].time)
		{
			SwapHeap(index, parent);
			index = parent;
			parent = (index - 1) / 2;
		}
	}
	void DownHeap(size_t index) {
		size_t child = index * 2 + 1;
		while (child < heap_.size())
		{
			size_t minChild = (child + 1 == heap_.size() || heap_[child].time < heap_[child + 1].time)
				? child : child + 1;
			if (heap_[index].time < heap_[minChild].time)
				break;
			SwapHeap(index, minChild);
			index = minChild;
			child = index * 2 + 1;
		}
	};

	void SwapHeap(size_t index1, size_t index2)	{
		HeapEntry tmp = heap_[index1];
		heap_[index1] = heap_[index2];
		heap_[index2] = tmp;
		heap_[index1].timer->m_heapIndex = index1;
		heap_[index2].timer->m_heapIndex = index2;
	};

	void DetectTimers() {
		unsigned long long now = GetCurrentMillisecs();

		while (!heap_.empty() && heap_[0].time <= now)
		{
			pTimer timer = heap_[0].timer;
			RemoveTimer(0);
			timer->m_fun();
			if (timer->m_timerType == CIRCLE) {
				timer->m_expires = now + timer->m_interval;
				AddTimer(timer);
			}
		}
	};

	unsigned long long GetCurrentMillisecs() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()
			).count();
	};
};

#endif
