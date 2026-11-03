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

		void WriteVarint(uint32_t value)
		{
			while (value >= 0x80u)
			{
				m_Data.push_back(uint8_t(value | 0x80u));
				value >>= 7;
			}
			m_Data.push_back(uint8_t(value));
		}

		void WriteSigned(int32_t value)
		{
			WriteVarint((uint32_t(value) << 1) ^ uint32_t(value >> 31));
		}

		void WriteBytes(std::span<const uint8_t> bytes)
		{
			m_Data.insert(m_Data.end(), bytes.begin(), bytes.end());
		}

		size_t GetSize() const { return m_Data.size(); }
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

		uint32_t ReadVarint()
		{
			uint32_t value = 0;
			for (uint32_t shift = 0; shift < 35; shift += 7)
			{
				const uint8_t byte = Read<uint8_t>();
				value |= uint32_t(byte & 0x7Fu) << shift;
				if ((byte & 0x80u) == 0 || m_Failed)
					return value;
			}
			m_Failed = true;
			return 0;
		}

		int32_t ReadSigned()
		{
			const uint32_t value = ReadVarint();
			return int32_t(value >> 1) ^ -int32_t(value & 1u);
		}

		std::span<const uint8_t> ReadBytes(size_t size)
		{
			if (m_Failed || m_Offset + size > m_Data.size())
			{
				m_Failed = true;
				return {};
			}
			const std::span<const uint8_t> bytes = m_Data.subspan(m_Offset, size);
			m_Offset += size;
			return bytes;
		}

		bool Failed() const { return m_Failed; }
		bool AtEnd() const { return m_Offset == m_Data.size(); }

	private:
		std::span<const uint8_t> m_Data;
		size_t m_Offset = 0;
		bool m_Failed = false;
	};
}
