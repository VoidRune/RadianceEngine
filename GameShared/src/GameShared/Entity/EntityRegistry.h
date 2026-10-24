#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace Game
{
	struct Entity
	{
		uint32_t Index = UINT32_MAX;
		uint32_t Generation = 0;

		bool IsValid() const { return Index != UINT32_MAX; }
		bool operator==(const Entity&) const = default;
	};

	// Each<T> iterates backwards, so destroying the current entity (or adding T) inside it is safe.
	class EntityRegistry
	{
	public:
		Entity Create()
		{
			uint32_t index;
			if (!m_FreeIndices.empty())
			{
				index = m_FreeIndices.back();
				m_FreeIndices.pop_back();
			}
			else
			{
				index = uint32_t(m_Generations.size());
				m_Generations.push_back(0);
				m_Alive.push_back(false);
			}
			m_Alive[index] = true;
			m_AliveCount++;
			return { index, m_Generations[index] };
		}

		void Destroy(Entity entity)
		{
			if (!IsAlive(entity))
				return;
			for (const std::unique_ptr<PoolBase>& pool : m_Pools)
				if (pool)
					pool->Remove(entity.Index);
			m_Alive[entity.Index] = false;
			m_Generations[entity.Index]++;
			m_FreeIndices.push_back(entity.Index);
			m_AliveCount--;
		}

		bool IsAlive(Entity entity) const
		{
			return entity.Index < m_Alive.size() && m_Alive[entity.Index] && m_Generations[entity.Index] == entity.Generation;
		}

		size_t GetAliveCount() const { return m_AliveCount; }

		template<typename T, typename... Args>
		T& Add(Entity entity, Args&&... args)
		{
			Pool<T>& pool = GetPool<T>();
			if (T* existing = pool.Find(entity.Index))
			{
				*existing = T{ std::forward<Args>(args)... };
				return *existing;
			}
			return pool.Insert(entity.Index, T{ std::forward<Args>(args)... });
		}

		template<typename T>
		void Remove(Entity entity)
		{
			if (Pool<T>* pool = FindPool<T>(); pool && IsAlive(entity))
				pool->Remove(entity.Index);
		}

		template<typename T>
		T* TryGet(Entity entity)
		{
			Pool<T>* pool = FindPool<T>();
			return pool && IsAlive(entity) ? pool->Find(entity.Index) : nullptr;
		}

		template<typename T>
		const T* TryGet(Entity entity) const { return const_cast<EntityRegistry*>(this)->TryGet<T>(entity); }

		template<typename T>
		bool Has(Entity entity) const { return TryGet<T>(entity) != nullptr; }

		template<typename T, typename Function>
		void Each(Function&& function)
		{
			Pool<T>* pool = FindPool<T>();
			if (!pool)
				return;
			for (size_t i = pool->Entities.size(); i-- > 0;)
			{
				if (i >= pool->Entities.size())
					continue;
				const uint32_t index = pool->Entities[i];
				function(Entity{ index, m_Generations[index] }, pool->Components[i]);
			}
		}

	private:
		struct PoolBase
		{
			virtual ~PoolBase() = default;
			virtual void Remove(uint32_t index) = 0;
		};

		template<typename T>
		struct Pool : PoolBase
		{
			std::vector<uint32_t> Sparse;
			std::vector<uint32_t> Entities;
			std::vector<T> Components;

			T* Find(uint32_t index)
			{
				return index < Sparse.size() && Sparse[index] != 0 ? &Components[Sparse[index] - 1] : nullptr;
			}

			T& Insert(uint32_t index, T&& value)
			{
				if (index >= Sparse.size())
					Sparse.resize(size_t(index) + 1, 0);
				Entities.push_back(index);
				Components.push_back(std::move(value));
				Sparse[index] = uint32_t(Entities.size());
				return Components.back();
			}

			void Remove(uint32_t index) override
			{
				if (index >= Sparse.size() || Sparse[index] == 0)
					return;
				const uint32_t dense = Sparse[index] - 1;
				const uint32_t last = uint32_t(Entities.size() - 1);
				if (dense != last)
				{
					Entities[dense] = Entities[last];
					Components[dense] = std::move(Components[last]);
					Sparse[Entities[dense]] = dense + 1;
				}
				Entities.pop_back();
				Components.pop_back();
				Sparse[index] = 0;
			}
		};

		static uint32_t NextTypeId()
		{
			static std::atomic<uint32_t> counter = 0;
			return counter.fetch_add(1);
		}

		template<typename T>
		static uint32_t TypeId()
		{
			static const uint32_t id = NextTypeId();
			return id;
		}

		template<typename T>
		Pool<T>* FindPool()
		{
			const uint32_t id = TypeId<T>();
			return id < m_Pools.size() ? static_cast<Pool<T>*>(m_Pools[id].get()) : nullptr;
		}

		template<typename T>
		Pool<T>& GetPool()
		{
			const uint32_t id = TypeId<T>();
			if (id >= m_Pools.size())
				m_Pools.resize(size_t(id) + 1);
			if (!m_Pools[id])
				m_Pools[id] = std::make_unique<Pool<T>>();
			return static_cast<Pool<T>&>(*m_Pools[id]);
		}

		std::vector<uint32_t> m_Generations;
		std::vector<bool> m_Alive;
		std::vector<uint32_t> m_FreeIndices;
		std::vector<std::unique_ptr<PoolBase>> m_Pools;
		size_t m_AliveCount = 0;
	};
}
