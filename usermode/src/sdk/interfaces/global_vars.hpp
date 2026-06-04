#pragma once

class c_global_vars
{
public:
	SCHEMA_ADD_OFFSET(float, m_curtime, 0x30);

	std::string m_map_name()
	{
		static constexpr std::array<uint32_t, 5> map_name_offsets = { 0x188, 0x180, 0x190, 0x198, 0x1A0 };
		for (const auto& offset : map_name_offsets)
		{
			const auto map_name_ptr = m_memory->read_t<uintptr_t>(reinterpret_cast<uintptr_t>(this) + offset);
			if (!map_name_ptr)
				continue;

			const auto map_name = m_memory->read_t<std::string>(map_name_ptr);
			if (map_name.empty() || map_name.find("<empty>") != std::string::npos)
				continue;

			return map_name;
		}

		return {};
	}
};
