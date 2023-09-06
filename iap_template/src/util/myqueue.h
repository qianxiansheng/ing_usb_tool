#pragma once

#include <iostream>
#include <deque>
#include <mutex>
#include <condition_variable>

template <typename T>
class BlockingQueue
{
public:
	BlockingQueue(uint32_t capacity = 100);

	BlockingQueue(const BlockingQueue&) = delete;
	BlockingQueue& operator=(const BlockingQueue&) = delete;

	~BlockingQueue();

public:
	void put(const T& x);
	void put(T&& x);
	bool take(T& out);
	bool take(T& out, int timeout);
private:
	uint32_t _capacity;
	std::mutex _mutex;
	std::condition_variable _notEmpty;
	std::condition_variable _notFull;
	std::deque<T> _queue;
};

template <typename T>
BlockingQueue<T>::BlockingQueue(uint32_t capacity)
	: _capacity(capacity)
	, _mutex()
	, _notEmpty()
	, _notFull()
	, _queue()
{}

template <typename T>
BlockingQueue<T>::~BlockingQueue()
{
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_queue.clear();
	}
	_notEmpty.notify_all();
	_notFull.notify_all();
}


template <typename T>
void BlockingQueue<T>::put(const T& x)
{
	std::unique_lock<std::mutex> lock(_mutex);
	while (_queue.size() >= _capacity)
		_notFull.wait(lock);
	_queue.push_back(x);
	_notEmpty.notify_all();
}

template <typename T>
void BlockingQueue<T>::put(T&& x)
{
	std::unique_lock<std::mutex> lock(_mutex);
	while (_queue.size() >= _capacity)
		_notFull.wait(lock);
	_queue.push_back(x);
	_notEmpty.notify_all();
}

template <typename T>
bool BlockingQueue<T>::take(T& out)
{
	std::unique_lock<std::mutex> lock(_mutex);
	while (_queue.empty())
		_notEmpty.wait(lock);
	out = _queue.front();
	_queue.pop_front();
	_notFull.notify_all();
	return true;
}

template <typename T>
bool BlockingQueue<T>::take(T& out, int timeout)
{
	std::unique_lock<std::mutex> lock(_mutex);
	while (_queue.empty())
		if (_notEmpty.wait_for(lock, std::chrono::seconds(timeout)) == std::cv_status::timeout)
			return false;
	out = _queue.front();
	_queue.pop_front();
	_notFull.notify_all();
	return true;
}