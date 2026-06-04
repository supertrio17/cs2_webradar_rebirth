#pragma once

struct config_data_t
{
    bool m_use_localhost = true;
    std::string m_local_ip = "192.168.x.x";
    std::string m_public_ip = "x.x.x.x";
    bool m_debug = false;   // Default OFF

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(config_data_t, m_use_localhost, m_local_ip, m_public_ip, m_debug)
};

namespace cfg
{
    int setup(config_data_t& config_data);
}