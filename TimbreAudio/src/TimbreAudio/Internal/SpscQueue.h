#pragma once
#include <algorithm>
#include <atomic>
#include <bit>
#include <cstddef>
#include <vector>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324) // structure was padded due to alignment specifier
#endif

namespace Timbre::Internal
{
	// Bounded lock-free queue for one producer and one consumer thread. Pushed items become visible
	// to the consumer on Publish(), which lets the producer hand over a batch atomically.
	template<typename T>
	class SpscQueue
	{
	public:
		explicit SpscQueue(size_t capacity)
			: m_Items(std::bit_ceil(std::max<size_t>(capacity, 2))), m_Mask(m_Items.size() - 1)
		{
		}

		size_t Capacity() const { return m_Items.size(); }

		// Producer side

		bool Push(const T& item)
		{
			if (m_Staged - m_Head.load(std::memory_order_acquire) >= m_Items.size())
				return false;
			m_Items[m_Staged & m_Mask] = item;
			m_Staged++;
			return true;
		}

		void Publish() { m_Tail.store(m_Staged, std::memory_order_release); }

		bool PushAndPublish(const T& item)
		{
			if (!Push(item))
				return false;
			Publish();
			return true;
		}

		// Consumer side

		size_t Size() const { return m_Tail.load(std::memory_order_acquire) - m_Head.load(std::memory_order_relaxed); }

		// Only valid for offset < Size().
		const T& Peek(size_t offset) const { return m_Items[(m_Head.load(std::memory_order_relaxed) + offset) & m_Mask]; }

		void Pop() { m_Head.store(m_Head.load(std::memory_order_relaxed) + 1, std::memory_order_release); }

		template<typename Function>
		size_t ConsumeAll(Function&& function)
		{
			const size_t head = m_Head.load(std::memory_order_relaxed);
			const size_t tail = m_Tail.load(std::memory_order_acquire);
			for (size_t i = head; i != tail; i++)
				function(m_Items[i & m_Mask]);
			m_Head.store(tail, std::memory_order_release);
			return tail - head;
		}

	private:
		std::vector<T> m_Items;
		size_t m_Mask;
		alignas(64) std::atomic<size_t> m_Head = 0;
		alignas(64) std::atomic<size_t> m_Tail = 0;
		alignas(64) size_t m_Staged = 0;
	};
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
