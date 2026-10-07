#pragma once
#include "GameShared/Core/Math.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace Game
{
	template<typename T>
	concept Trivial = std::is_arithmetic_v<T> || std::is_enum_v<T>;

	class ByteWriter
	{
	public:
		template<Trivial T>
		void Write(T value)
		{
			const size_t offset = m_Data.size();
			m_Data.resize(offset + sizeof(T));
			std::memcpy(m_Data.data() + offset, &value, sizeof(T));
		}

		void Write(Vec2 value)
		{
			Write(value.x);
			Write(value.y);
		}

		void Write(std::string_view text)
		{
			const uint16_t size = uint16_t(std::min<size_t>(text.size(), UINT16_MAX));
			Write(size);
			m_Data.insert(m_Data.end(), text.begin(), text.begin() + size);
		}

		const std::vector<uint8_t>& GetData() const { return m_Data; }
		std::vector<uint8_t> Take() { return std::move(m_Data); }

	private:
		std::vector<uint8_t> m_Data;
	};

	class ByteReader
	{
	public:
		explicit ByteReader(std::span<const uint8_t> data) : m_Data(data) {}

		template<Trivial T>
		T Read()
		{
			T value{};
			if (m_Failed || m_Offset + sizeof(T) > m_Data.size())
			{
				m_Failed = true;
				return value;
			}
			std::memcpy(&value, m_Data.data() + m_Offset, sizeof(T));
			m_Offset += sizeof(T);
			return value;
		}

		Vec2 ReadVec2()
		{
			const float x = Read<float>();
			const float y = Read<float>();
			return { x, y };
		}

		std::string ReadString()
		{
			const uint16_t size = Read<uint16_t>();
			if (m_Failed || m_Offset + size > m_Data.size())
			{
				m_Failed = true;
				return {};
			}
			std::string text(reinterpret_cast<const char*>(m_Data.data() + m_Offset), size);
			m_Offset += size;
			return text;
		}

		bool Failed() const { return m_Failed; }
		bool AtEnd() const { return m_Offset == m_Data.size(); }

	private:
		std::span<const uint8_t> m_Data;
		size_t m_Offset = 0;
		bool m_Failed = false;
	};
}
