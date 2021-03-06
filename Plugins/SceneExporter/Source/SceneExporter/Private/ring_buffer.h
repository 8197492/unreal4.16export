#pragma once

#include <atomic>

#ifndef VTD_QUEUE_MASK
#	define VTD_QUEUE_MASK (0x7FF)
#endif

namespace vtd
{
	template <class _Ty, _Ty _Default, size_t _Mask = VTD_QUEUE_MASK>
	class ring_buffer
	{
	public:
		typedef _Ty value_type;
		typedef value_type* pointer;
		typedef uint64_t size_type;

		ring_buffer() noexcept
		{
#if defined(DEBUG) | defined(_DEBUG)
			_Size.store(0);
#endif
			_Cursor.store(0);
			_Head.store(0);
			_Tail.store(0);
		}

		~ring_buffer() noexcept = default;

		void push(value_type _Val) noexcept
		{
#if defined(DEBUG) | defined(_DEBUG)
			assert(_Size.fetch_add(1, std::memory_order_acquire) < _Max);
#endif
			size_type cur = _Cursor.fetch_add(1, std::memory_order_acquire);
			buffer[cur & _Mask] = std::move(_Val);
			_Head.fetch_add(1, std::memory_order_release);
		}

		value_type pop() noexcept
		{
			size_type h, t;
			do
			{
				h = _Head.load(std::memory_order_relaxed);
				t = _Tail.load(std::memory_order_relaxed);
				if (t >= h)
				{
					return _Default;
				}
			} while (!_Tail.compare_exchange_weak(t, t + 1, std::memory_order_relaxed));
#if defined(DEBUG) | defined(_DEBUG)
			auto res = std::move(buffer[t & _Mask]);
			assert(_Size.fetch_sub(1, std::memory_order_release) > 0);
			return std::move(res);
#else
			return std::move(buffer[t & _Mask]);
#endif
		}

		size_type size() noexcept
		{
			size_type h = _Head.load(std::memory_order_relaxed);
			size_type t = _Tail.load(std::memory_order_relaxed);
			return h - t;
		}

	private:
		static constexpr size_type _Max = _Mask + 1;

		ring_buffer(const ring_buffer&) = delete;
		ring_buffer(ring_buffer&&) = delete;
		ring_buffer& operator = (const ring_buffer&) = delete;

#if defined(DEBUG) | defined(_DEBUG)
		std::atomic<size_type> _Size;
#endif
		std::atomic<size_type> _Cursor;
		std::atomic<size_type> _Head;
		std::atomic<size_type> _Tail;
		value_type buffer[_Max];

	};

}
