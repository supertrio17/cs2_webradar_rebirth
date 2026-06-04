#include "pch.hpp"

void sdk::update()
{
    sdk::m_local_controller = c_cs_player_controller::get_local_player_controller();

    if (!sdk::m_local_controller)
    {
        if (config.m_debug)
            LOG_DEBUG("sdk::update - local controller is null");
    }
    else
    {
        const auto team = sdk::m_local_controller->m_iTeamNum();
        if (config.m_debug)
            LOG_DEBUG("sdk::update - local team = %d", (int)team);
    }
}