#include "pch.hpp"

namespace
{
    bool is_probable_pointer(const uintptr_t pointer)
    {
        return pointer > 0x10000 && pointer < 0x00007FFFFFFF0000;
    }

    struct dynamic_offsets_t
    {
        uintptr_t m_entity_list = offsets::m_dw_entity_list;
        uintptr_t m_global_vars = offsets::m_dw_global_vars;
        uintptr_t m_local_player_controller = offsets::m_dw_local_player_controller;
        uintptr_t m_schema_system = offsets::m_schema_system;
        bool m_initialized = false;
    };

    dynamic_offsets_t& dynamic_offsets()
    {
        static dynamic_offsets_t offsets = {};
        return offsets;
    }

    void load_dynamic_offsets_from_dump()
    {
        auto& runtime_offsets = dynamic_offsets();
        if (runtime_offsets.m_initialized) return;

        runtime_offsets.m_initialized = true;

        // Try to load from a2x dump if available
        std::ifstream file("dump/offsets.json");
        if (!file.is_open()) return;

        nlohmann::json j;
        try { file >> j; }
        catch (...) { return; }

        if (j.contains(CLIENT_DLL))
        {
            const auto& client = j[CLIENT_DLL];
            if (client.contains("dwEntityList")) runtime_offsets.m_entity_list = client["dwEntityList"];
            if (client.contains("dwGlobalVars")) runtime_offsets.m_global_vars = client["dwGlobalVars"];
            if (client.contains("dwLocalPlayerController")) runtime_offsets.m_local_player_controller = client["dwLocalPlayerController"];
        }
        if (j.contains(SCHEMASYSTEM_DLL))
        {
            const auto& schema = j[SCHEMASYSTEM_DLL];
            if (schema.contains("dwSchemaSystem")) runtime_offsets.m_schema_system = schema["dwSchemaSystem"];
        }
    }
}

uintptr_t i::get_entity_list_offset() { load_dynamic_offsets_from_dump(); return dynamic_offsets().m_entity_list; }
uintptr_t i::get_global_vars_offset() { load_dynamic_offsets_from_dump(); return dynamic_offsets().m_global_vars; }
uintptr_t i::get_local_player_controller_offset() { load_dynamic_offsets_from_dump(); return dynamic_offsets().m_local_player_controller; }
uintptr_t i::get_schema_system_offset() { load_dynamic_offsets_from_dump(); return dynamic_offsets().m_schema_system; }

bool i::refresh_global_vars()
{
    LOG_DEBUG("refresh_global_vars started");

    const auto [client_base, client_size] = m_memory->get_module_info(CLIENT_DLL);
    if (!client_base.has_value()) return false;

    // Signature
    const auto pattern = m_memory->find_pattern(CLIENT_DLL, GET_GLOBAL_VARS);
    if (pattern.has_value())
    {
        const auto ptr = m_memory->read_t<uintptr_t>(pattern->rip().as<uintptr_t>());
        if (is_probable_pointer(ptr))
        {
            m_global_vars = reinterpret_cast<c_global_vars*>(ptr);
            LOG_DEBUG("refresh_global_vars resolved via signature at '0x%llX'", static_cast<unsigned long long>(ptr));
            return true;
        }
    }

    // Fallback from dump
    const auto offset = get_global_vars_offset();
    if (offset > 0x10000)
    {
        const auto ptr = m_memory->read_t<uintptr_t>(client_base.value() + offset);
        if (is_probable_pointer(ptr))
        {
            m_global_vars = reinterpret_cast<c_global_vars*>(ptr);
            LOG_DEBUG("refresh_global_vars resolved via dump offset at '0x%llX'", static_cast<unsigned long long>(ptr));
            return true;
        }
    }

    LOG_WARNING("refresh_global_vars failed");
    return false;
}

bool i::setup()
{
    LOG_DEBUG("interfaces::setup started");

    const auto [client_base, _] = m_memory->get_module_info(CLIENT_DLL);
    if (!client_base.has_value()) return false;

    const auto [schema_base, __] = m_memory->get_module_info(SCHEMASYSTEM_DLL);
    if (!schema_base.has_value()) return false;

    LOG_DEBUG("interfaces::setup module bases client='0x%llX' schema='0x%llX'",
        static_cast<unsigned long long>(client_base.value()), static_cast<unsigned long long>(schema_base.value()));

    // Schema System
    m_schema_system = nullptr;
    const auto schema_pattern = m_memory->find_pattern(SCHEMASYSTEM_DLL, GET_SCHEMA_SYSTEM);
    if (schema_pattern.has_value())
    {
        const auto ptr = m_memory->read_t<uintptr_t>(schema_pattern->rip().as<uintptr_t>());
        if (is_probable_pointer(ptr))
            m_schema_system = reinterpret_cast<c_schema_system*>(ptr);
    }
    if (!m_schema_system)
    {
        const auto offset = get_schema_system_offset();
        if (offset)
        {
            const auto ptr = m_memory->read_t<uintptr_t>(schema_base.value() + offset);
            if (is_probable_pointer(ptr))
                m_schema_system = reinterpret_cast<c_schema_system*>(ptr);
        }
    }

    // Global Vars
    refresh_global_vars();

    // Entity System
    m_game_entity_system = nullptr;
    const auto entity_pattern = m_memory->find_pattern(CLIENT_DLL, GET_ENTITY_LIST);
    if (entity_pattern.has_value())
    {
        const auto ptr = m_memory->read_t<uintptr_t>(entity_pattern->rip().as<uintptr_t>());
        if (is_probable_pointer(ptr))
            m_game_entity_system = reinterpret_cast<c_game_entity_system*>(ptr);
    }
    if (!m_game_entity_system)
    {
        const auto offset = get_entity_list_offset();
        if (offset)
        {
            const auto ptr = m_memory->read_t<uintptr_t>(client_base.value() + offset);
            if (is_probable_pointer(ptr))
                m_game_entity_system = reinterpret_cast<c_game_entity_system*>(ptr);
        }
    }

    const bool success = (m_schema_system != nullptr) && (m_game_entity_system != nullptr) && (m_global_vars != nullptr);

    LOG_DEBUG("interfaces::setup completed (success='%u', schema='%u', global='%u', entity='%u')",
        success, m_schema_system ? 1 : 0, m_global_vars ? 1 : 0, m_game_entity_system ? 1 : 0);

    return success;
}