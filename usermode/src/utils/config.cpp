#include "pch.hpp"

int cfg::setup(config_data_t& config_data)
{
    std::ifstream file("config.json");
    if (!file.is_open())
    {
        std::ofstream example_config("config.json");
        example_config << R"({
    "m_use_localhost": true,
    "m_local_ip": "192.168.x.x",
    "m_public_ip": "x.x.x.x",
    "m_debug": false
})";
        LOG_WARNING("Created example config.json");
        return 1;
    }

    const auto parsed_data = nlohmann::json::parse(file);
    if (parsed_data.empty()) return 2;

    try
    {
        config_data = parsed_data.get<config_data_t>();
    }
    catch (...) { return 3; }

    return 0;
}