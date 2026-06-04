#include "pch.hpp"

const c_base_handle c_entity_instance::get_ref_e_handle()
{
    const auto entity = m_pEntity();
    if (!entity) return {};
    return c_base_handle(entity->get_entry_idx(), entity->get_serial_number() - (entity->m_flags() & 1));
}

const std::string c_entity_instance::get_schema_class_name()
{
    const auto entity = m_pEntity();
    if (!entity) return {};

    const auto class_info = entity->m_pClassInfo();
    if (!class_info) return {};

    uintptr_t class_info_addr = 0;
    memcpy(&class_info_addr, &class_info, sizeof(uintptr_t));

    for (int i = 0; i < 12; ++i)
    {
        uintptr_t name_ptr = m_memory->read_t<uintptr_t>(class_info_addr + 0x08 + (static_cast<uint64_t>(i) * 8));
        if (name_ptr == 0) continue;

        std::string name = m_memory->read_t<std::string>(name_ptr);

        if (name.find("CCSPlayerController") != std::string::npos)
            return name;
    }

    return {};
}

const std::string c_cs_player_pawn::get_model_name()
{
    const uint32_t model_state_offset = schema::get_offset(fnv1a::hash_const("CSkeletonInstance->m_modelState"));
    const uint32_t model_name_offset = schema::get_offset(fnv1a::hash_const("CModelState->m_ModelName"));

    if (!model_state_offset || !model_name_offset)
        return "unknown";

    const auto game_scene_node = m_pGameSceneNode();
    if (!game_scene_node) return "unknown";

    const auto model_state = m_memory->read_t<uintptr_t>(reinterpret_cast<uintptr_t>(game_scene_node) + model_state_offset);
    if (!model_state) return "unknown";

    const auto model_name_ptr = m_memory->read_t<uintptr_t>(model_state + model_name_offset);
    if (!model_name_ptr) return "unknown";

    const auto model_path = m_memory->read_t<std::string>(model_name_ptr);
    if (model_path.empty()) return "unknown";

    size_t last_slash = model_path.rfind("/");
    size_t last_dot = model_path.rfind(".");
    if (last_slash == std::string::npos || last_dot == std::string::npos)
        return "unknown";

    return model_path.substr(last_slash + 1, last_dot - last_slash - 1);
}

c_cs_player_controller* c_cs_player_controller::get_local_player_controller()
{
    static const auto local_player_controller_address = []
        {
            const auto [client_base, client_size] = m_memory->get_module_info(CLIENT_DLL);
            if (!client_base.has_value()) return static_cast<uintptr_t>(0);

            const auto local_player_pattern = m_memory->find_pattern(CLIENT_DLL, GET_LOCAL_PLAYER_CONTROLLER);
            if (local_player_pattern.has_value())
                return local_player_pattern->rip().as<uintptr_t>();

            const uint32_t offset = i::get_local_player_controller_offset();
            if (offset > 0x10000)
                return client_base.value() + offset;

            return static_cast<uintptr_t>(0);
        }();

    if (!local_player_controller_address) return nullptr;
    return m_memory->read_t<c_cs_player_controller*>(local_player_controller_address);
}

c_cs_player_pawn* c_cs_player_controller::get_player_pawn()
{
    const auto& handle = m_hPawn();
    return i::m_game_entity_system->get<c_cs_player_pawn*>(handle);
}

// FIXED: Proper conversion from e_colors to int
const int c_cs_player_controller::get_color()
{
    e_colors raw_color = m_iCompTeammateColor();
    if (raw_color == static_cast<e_colors>(-1))
        return 0;
    return static_cast<int>(raw_color);
}

const f_vector& c_cs_player_controller::get_vec_origin()
{
    const auto pawn = get_player_pawn();
    if (!pawn) return {};

    return pawn->get_scene_origin();
}

const f_vector& c_base_entity::get_scene_origin()
{
    const auto game_scene_node = m_pGameSceneNode();
    if (!game_scene_node) return {};

    const uint32_t abs_origin_offset = schema::get_offset(fnv1a::hash_const("CGameSceneNode->m_vecAbsOrigin"));
    if (!abs_origin_offset) return {};

    return m_memory->read_t<f_vector>(reinterpret_cast<uintptr_t>(game_scene_node) + abs_origin_offset);
}

c_base_player_weapon* c_base_player_weapon::get(const int32_t idx)
{
    const auto handle = m_memory->read_t<int32_t>(reinterpret_cast<uintptr_t>(this) + idx * 0x4);
    if (handle == -1) return nullptr;

    return i::m_game_entity_system->get<c_base_player_weapon*>(handle & ENT_ENTRY_MASK);
}