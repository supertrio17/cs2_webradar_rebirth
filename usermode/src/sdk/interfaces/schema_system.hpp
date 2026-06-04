#pragma once

namespace schema_detail
{
    inline bool is_probable_pointer(const uintptr_t pointer)
    {
        return pointer > 0x10000 && pointer < 0x00007FFFFFFF0000;
    }

    inline bool is_printable_ascii(const std::string& text)
    {
        if (text.empty())
            return false;

        return std::ranges::all_of(text, [](const unsigned char character)
        {
            return character >= 32 && character <= 126;
        });
    }

    inline bool is_valid_module_name(const std::string& module_name)
    {
        if (module_name.empty() || module_name.size() >= 96 || !is_printable_ascii(module_name))
            return false;

        if (module_name.find(".dll") != std::string::npos)
            return true;

        return module_name.find("client") != std::string::npos ||
            module_name.find("engine") != std::string::npos ||
            module_name.find("schema") != std::string::npos;
    }
}

class c_schema_class_field_data
{
public:
    SCHEMA_ADD_STRING_OFFSET(m_name, 0x00);
    SCHEMA_ADD_OFFSET(uint16_t, m_single_inheritance_offset, 0x10);
    uint8_t m_pad0[0x20];
};
static_assert(sizeof(c_schema_class_field_data) == 0x20, "wrong size on c_schema_class_field_data");

class c_schema_type_declared_class
{
public:
    SCHEMA_ADD_STRING_OFFSET(m_binary_name, 0x08);

    std::pair<uint16_t, c_schema_class_field_data*> get_fields() const
    {
        static uint32_t count_offset = 0x1C;
        static uint32_t fields_offset = 0x28;

        auto field_count = m_memory->read_t<uint16_t>(reinterpret_cast<uintptr_t>(this) + count_offset);
        auto field_data = m_memory->read_t<c_schema_class_field_data*>(reinterpret_cast<uintptr_t>(this) + fields_offset);
        if (field_count > 0 && field_count < 4096 && schema_detail::is_probable_pointer(reinterpret_cast<uintptr_t>(field_data)))
            return { field_count, field_data };

        static constexpr std::array<std::pair<uint32_t, uint32_t>, 5> candidate_offsets = {
            std::pair{0x1C, 0x28},
            std::pair{0x20, 0x30},
            std::pair{0x18, 0x28},
            std::pair{0x24, 0x30},
            std::pair{0x20, 0x28}
        };

        for (const auto& [candidate_count_offset, candidate_fields_offset] : candidate_offsets)
        {
            field_count = m_memory->read_t<uint16_t>(reinterpret_cast<uintptr_t>(this) + candidate_count_offset);
            field_data = m_memory->read_t<c_schema_class_field_data*>(reinterpret_cast<uintptr_t>(this) + candidate_fields_offset);

            if (field_count <= 0 || field_count >= 4096 || !schema_detail::is_probable_pointer(reinterpret_cast<uintptr_t>(field_data)))
                continue;

            const auto first_field_name = field_data->m_name();
            if (first_field_name.empty())
                continue;

            count_offset = candidate_count_offset;
            fields_offset = candidate_fields_offset;
            return { field_count, field_data };
        }

        return {};
    }
};

class c_schema_system_type_scope
{
public:
    std::string m_module_name() const
    {
        static uint32_t module_name_offset = 0x08;

        auto read_module_name = [this](const uint32_t offset) -> std::string
        {
            const auto name_ptr = m_memory->read_t<uintptr_t>(reinterpret_cast<uintptr_t>(this) + offset);
            if (schema_detail::is_probable_pointer(name_ptr))
            {
                auto name = m_memory->read_t<std::string>(name_ptr);
                if (schema_detail::is_valid_module_name(name))
                    return name;
            }

            auto name = m_memory->read_t<std::string>(reinterpret_cast<uintptr_t>(this) + offset);
            if (schema_detail::is_valid_module_name(name))
                return name;

            return {};
        };

        auto module_name = read_module_name(module_name_offset);
        if (!module_name.empty())
            return module_name;

        static constexpr std::array<uint32_t, 12> module_name_candidates = {
            0x08, 0x10, 0x18, 0x20, 0x28, 0x30,
            0x38, 0x40, 0x48, 0x50, 0x58, 0x60
        };

        for (const auto candidate_offset : module_name_candidates)
        {
            module_name = read_module_name(candidate_offset);
            if (module_name.empty())
                continue;

            if (module_name_offset != candidate_offset)
                LOG_DEBUG("resolved schema type scope module name offset to '0x%X'", candidate_offset);

            module_name_offset = candidate_offset;
            return module_name;
        }

        return {};
    }

    c_utl_ts_hash<c_schema_type_declared_class*, 256, uint32_t> m_hash_classes() const
    {
        static uint32_t hash_classes_offset = 0x540;

        auto read_hash_classes = [this](const uint32_t offset) -> std::optional<c_utl_ts_hash<c_schema_type_declared_class*, 256, uint32_t>>
        {
            const auto hash_classes = m_memory->read_t<c_utl_ts_hash<c_schema_type_declared_class*, 256, uint32_t>>(reinterpret_cast<uintptr_t>(this) + offset);
            const auto size = hash_classes.size();
            if (size > 0 && size < 65536)
                return hash_classes;

            return std::nullopt;
        };

        if (const auto hash_classes = read_hash_classes(hash_classes_offset); hash_classes.has_value())
            return hash_classes.value();

        static constexpr std::array<uint32_t, 8> hash_classes_candidates = { 0x540, 0x558, 0x570, 0x588, 0x5A0, 0x5B8, 0x5D0, 0x5E8 };
        for (const auto candidate_offset : hash_classes_candidates)
        {
            if (candidate_offset == hash_classes_offset)
                continue;

            const auto hash_classes = read_hash_classes(candidate_offset);
            if (!hash_classes.has_value())
                continue;

            hash_classes_offset = candidate_offset;
            return hash_classes.value();
        }

        return {};
    }
};

class c_schema_system
{
public:
    std::vector<c_schema_system_type_scope*> get_type_scopes(const bool require_named_scope = true, const bool enable_debug_logging = true) const
    {
        static uint32_t type_scope_first_offset = 0x190;
        static uint32_t type_scope_second_offset = 0x198;
        static bool type_scope_data_at_first_offset = false;

        uint32_t rejected_candidates = 0;
        uint32_t logged_rejections = 0;
        static constexpr uint32_t max_rejection_logs = 32;

        auto read_type_scopes = [this, &rejected_candidates, &logged_rejections](const uint32_t first_offset, const uint32_t second_offset, const bool data_at_first_offset, const bool require_named_scope, const bool emit_debug_logs) -> std::vector<c_schema_system_type_scope*>
        {
            const auto log_rejection = [&](const char* format, auto... args)
            {
                if (!emit_debug_logs)
                    return;

                rejected_candidates++;
                if (logged_rejections >= max_rejection_logs)
                    return;

                LOG_DEBUG(format, args...);
                logged_rejections++;
            };

            const auto base = reinterpret_cast<uintptr_t>(this);
            const auto data_offset = data_at_first_offset ? first_offset : second_offset;
            const auto size_offset = data_at_first_offset ? second_offset : first_offset;

            const auto data = m_memory->read_t<uintptr_t>(base + data_offset);
            if (!schema_detail::is_probable_pointer(data))
            {
                log_rejection("rejected type scope candidate first='0x%X' second='0x%X' order='%s' (invalid data ptr '0x%llX')", first_offset, second_offset, data_at_first_offset ? "data,size" : "size,data", static_cast<unsigned long long>(data));
                return {};
            }

            static constexpr std::array<uint32_t, 7> size_offset_deltas = { 0x0, 0x8, 0x10, 0x18, 0x20, 0x28, 0x30 };

            for (const auto size_delta : size_offset_deltas)
            {
                const auto candidate_size_offset = size_offset + size_delta;
                const auto size = m_memory->read_t<uint32_t>(base + candidate_size_offset);
                if (!size || size > 2048)
                    continue;

                std::vector<c_schema_system_type_scope*> type_scopes(size);
                m_memory->read_t(data, type_scopes.data(), size * sizeof(uintptr_t));

                std::vector<c_schema_system_type_scope*> valid_type_scopes = {};
                valid_type_scopes.reserve(type_scopes.size());

                for (const auto& type_scope : type_scopes)
                {
                    if (schema_detail::is_probable_pointer(reinterpret_cast<uintptr_t>(type_scope)))
                        valid_type_scopes.push_back(type_scope);
                }

                if (valid_type_scopes.empty())
                    continue;

                const auto minimum_expected_valid_scopes = std::min<uint32_t>(size, 4);
                if (valid_type_scopes.size() < minimum_expected_valid_scopes)
                    continue;

                size_t named_scope_count = 0;
                const auto max_to_check = std::min<size_t>(valid_type_scopes.size(), 32);
                for (size_t idx = 0; idx < max_to_check; ++idx)
                {
                    if (!valid_type_scopes[idx]->m_module_name().empty())
                        named_scope_count++;
                }

                if (require_named_scope && !named_scope_count)
                    continue;

                if (emit_debug_logs)
                    LOG_DEBUG("accepted type scope candidate first='0x%X' second='0x%X' order='%s' (size_offset='0x%X', size='%u', valid='%u', named='%u')", first_offset, second_offset, data_at_first_offset ? "data,size" : "size,data", candidate_size_offset, size, static_cast<uint32_t>(valid_type_scopes.size()), static_cast<uint32_t>(named_scope_count));

                return valid_type_scopes;
            }

            const auto original_size = m_memory->read_t<uint32_t>(base + size_offset);
            log_rejection("rejected type scope candidate first='0x%X' second='0x%X' order='%s' (invalid size '%u')", first_offset, second_offset, data_at_first_offset ? "data,size" : "size,data", original_size);
            return {};
        };

        struct type_scope_layout_candidate_t
        {
            uint32_t m_first_offset = 0;
            uint32_t m_second_offset = 0;
            bool m_data_at_first_offset = false;
        };

        auto resolve_type_scopes = [&](const bool emit_debug_logs) -> std::vector<c_schema_system_type_scope*>
        {
            if (const auto scopes = read_type_scopes(type_scope_first_offset, type_scope_second_offset, type_scope_data_at_first_offset, require_named_scope, emit_debug_logs); !scopes.empty())
                return scopes;

            static constexpr std::array<type_scope_layout_candidate_t, 12> known_layouts = {
                type_scope_layout_candidate_t{0x190, 0x198, false},
                type_scope_layout_candidate_t{0x190, 0x198, true},
                type_scope_layout_candidate_t{0x188, 0x190, false},
                type_scope_layout_candidate_t{0x188, 0x190, true},
                type_scope_layout_candidate_t{0x1A0, 0x1A8, false},
                type_scope_layout_candidate_t{0x1A0, 0x1A8, true},
                type_scope_layout_candidate_t{0x180, 0x188, false},
                type_scope_layout_candidate_t{0x180, 0x188, true},
                type_scope_layout_candidate_t{0x198, 0x1A0, false},
                type_scope_layout_candidate_t{0x198, 0x1A0, true},
                type_scope_layout_candidate_t{0x190, 0x1A0, true},
                type_scope_layout_candidate_t{0x188, 0x198, true}
            };

            for (const auto& candidate : known_layouts)
            {
                if (candidate.m_first_offset == type_scope_first_offset && candidate.m_second_offset == type_scope_second_offset && candidate.m_data_at_first_offset == type_scope_data_at_first_offset)
                    continue;

                const auto scopes = read_type_scopes(candidate.m_first_offset, candidate.m_second_offset, candidate.m_data_at_first_offset, require_named_scope, emit_debug_logs);
                if (scopes.empty())
                    continue;

                type_scope_first_offset = candidate.m_first_offset;
                type_scope_second_offset = candidate.m_second_offset;
                type_scope_data_at_first_offset = candidate.m_data_at_first_offset;

                LOG_INFO("resolved dynamic type scope layout offsets first='0x%X' second='0x%X' order='%s'", type_scope_first_offset, type_scope_second_offset, type_scope_data_at_first_offset ? "data,size" : "size,data");
                return scopes;
            }

            for (uint32_t offset = 0x140; offset <= 0x360; offset += 0x8)
            {
                for (const bool candidate_data_at_first_offset : { false, true })
                {
                    const auto scopes = read_type_scopes(offset, offset + 0x8, candidate_data_at_first_offset, require_named_scope, emit_debug_logs);
                    if (scopes.empty())
                        continue;

                    type_scope_first_offset = offset;
                    type_scope_second_offset = offset + 0x8;
                    type_scope_data_at_first_offset = candidate_data_at_first_offset;

                    LOG_INFO("resolved scanned type scope layout offsets first='0x%X' second='0x%X' order='%s'", type_scope_first_offset, type_scope_second_offset, type_scope_data_at_first_offset ? "data,size" : "size,data");
                    return scopes;
                }
            }

            return {};
        };

        if (const auto scopes = resolve_type_scopes(false); !scopes.empty())
            return scopes;

        if (enable_debug_logging)
        {
            rejected_candidates = 0;
            logged_rejections = 0;

            LOG_WARNING("failed to resolve type scopes (cached first='0x%X' second='0x%X' order='%s', require_named_scope='%u')", type_scope_first_offset, type_scope_second_offset, type_scope_data_at_first_offset ? "data,size" : "size,data", require_named_scope ? 1 : 0);

            auto scopes = resolve_type_scopes(true);

            if (rejected_candidates > logged_rejections)
                LOG_DEBUG("suppressed '%u' additional rejected type scope candidates", rejected_candidates - logged_rejections);

            return scopes;
        }

        return {};
    }

    class c_schema_system_type_scope* find_type_scope_for_module(const std::string_view& name) const
    {
        const auto type_scopes = get_type_scopes(true, false);
        if (type_scopes.empty())
            return {};

        for (const auto& type_scope : type_scopes)
        {
            const auto module_name = type_scope->m_module_name();
            if (module_name.empty() || module_name.find(name) == std::string::npos)
                continue;

            return type_scope;
        }

        return {};
    }
};
