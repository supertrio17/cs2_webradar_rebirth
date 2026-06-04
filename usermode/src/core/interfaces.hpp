#pragma once

class c_schema_system;
class c_game_entity_system;
class c_global_vars;

namespace i
{
    bool setup();
    bool refresh_global_vars();

    uintptr_t get_entity_list_offset();
    uintptr_t get_global_vars_offset();
    uintptr_t get_local_player_controller_offset();
    uintptr_t get_schema_system_offset();

    inline c_schema_system* m_schema_system = nullptr;
    inline c_game_entity_system* m_game_entity_system = nullptr;
    inline c_global_vars* m_global_vars = nullptr;
}