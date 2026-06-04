#include "pch.hpp"

bool f::run()
{
    if (!sdk::m_local_controller) return false;

    const auto local_team = sdk::m_local_controller->m_iTeamNum();
    if (local_team == e_team::none || local_team == e_team::spec)
        return false;

    m_data.clear();
    m_data["m_local_team"] = static_cast<int>(local_team);

    get_map();
    get_player_info();
    get_grenades_and_bomb();

    return true;
}

void f::get_map()
{
    std::string map_name = i::m_global_vars ? i::m_global_vars->m_map_name() : "";
    if (map_name.empty() || map_name.find("<empty>") != std::string::npos)
        map_name = "invalid";

    m_data["m_map"] = map_name;

    if (f::features_vars::map_name != map_name) {
        f::bomb::update_bomb_dmg_info(map_name);
        f::features_vars::map_name = map_name;
    }
}

void f::get_player_info()
{
    m_data["m_players"].clear();

    if (!i::m_game_entity_system) return;

    int players_found = 0;

    for (int32_t idx = 1; idx < 2048; ++idx)
    {
        const auto entity = i::m_game_entity_system->get<c_base_entity*>(idx);
        if (!entity) continue;

        auto* controller = reinterpret_cast<c_cs_player_controller*>(entity);
        if (!controller) continue;

        const auto team = controller->m_iTeamNum();
        if (team != e_team::t && team != e_team::ct) continue;

        auto* pawn = controller->get_player_pawn();
        if (!pawn) continue;

        if (f::players::get_data(idx, controller, pawn))
        {
            f::players::get_weapons(pawn);
            f::players::get_active_weapon(pawn);
            m_data["m_players"].push_back(m_player_data);
            players_found++;
        }
    }

    if (config.m_debug)
        LOG_DEBUG("Players found and added: %d", players_found);
}

void f::get_grenades_and_bomb()
{
    if (!i::m_game_entity_system) return;

    m_data["m_grenades"] = nlohmann::json::array();
    m_data["m_thrown_nades"] = nlohmann::json::array();
    m_data["m_dropped_weapons"] = nlohmann::json::array();
    m_data["m_bomb"] = nlohmann::json{};

    for (int32_t idx = 1; idx < 2048; ++idx)
    {
        const auto entity = i::m_game_entity_system->get<c_base_entity*>(idx);
        if (!entity) continue;

        const std::string class_name = entity->get_schema_class_name();
        if (class_name.empty()) continue;

        if (class_name.find("PlantedC4") != std::string::npos)
        {
            f::bomb::get_planted_bomb(reinterpret_cast<c_planted_c4*>(entity));
        }
        else if (class_name.find("C4") != std::string::npos)
        {
            f::bomb::get_carried_bomb(entity);
        }
        else if (class_name.find("SmokeGrenadeProjectile") != std::string::npos)
        {
            auto* smoke = reinterpret_cast<c_smoke_grenade*>(entity);
            if (f::grenades::get_smoke(smoke))
                m_data["m_grenades"].push_back(m_grenade_data);
        }
        else if (class_name.find("Inferno") != std::string::npos)
        {
            auto* molo = reinterpret_cast<c_molo_grenade*>(entity);
            if (f::grenades::get_molo(molo))
                m_data["m_grenades"].push_back(m_grenade_data);
        }
        else if (class_name.find("GrenadeProjectile") != std::string::npos)
        {
            auto* nade = reinterpret_cast<c_base_grenade*>(entity);
            if (f::grenades::get_thrown(nade))
                m_data["m_thrown_nades"].push_back(m_grenade_thrown_data);
        }
        else if (class_name.find("weapon_") != std::string::npos)
        {
            if (f::dropped_weapons::get_weapon(entity))
                m_data["m_dropped_weapons"].push_back(m_dropped_weapon_data);
        }
    }

    if (config.m_debug)
        LOG_DEBUG("Features scan: grenades=%d, thrown=%d, dropped=%d",
            m_data["m_grenades"].size(), m_data["m_thrown_nades"].size(), m_data["m_dropped_weapons"].size());
}