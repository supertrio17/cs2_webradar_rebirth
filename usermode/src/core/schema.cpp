#include "pch.hpp"
#include "schema.hpp"
#include <fstream>
#include <unordered_map>
#include <format>

struct schema_data_t
{
    fnv1a_t m_hashed_field_name = 0;
    uint32_t m_offset = 0;
};

static std::unordered_map<fnv1a_t, uint32_t> m_schema_data = {};
static constexpr std::array<schema_data_t, 41> m_schema_fallback_data = {
    schema_data_t{ fnv1a::hash_const("CBasePlayerController->m_hPawn"), 0x6BC },
    schema_data_t{ fnv1a::hash_const("CBasePlayerController->m_steamID"), 0x780 },
    schema_data_t{ fnv1a::hash_const("CCSPlayerController->m_iCompTeammateColor"), 0x848 },
    schema_data_t{ fnv1a::hash_const("CCSPlayerController->m_pInGameMoneyServices"), 0x808 },
    schema_data_t{ fnv1a::hash_const("CCSPlayerController->m_sSanitizedPlayerName"), 0x860 },
    schema_data_t{ fnv1a::hash_const("CCSPlayerController_InGameMoneyServices->m_iAccount"), 0x40 },
    schema_data_t{ fnv1a::hash_const("CCSPlayer_ItemServices->m_bHasDefuser"), 0x48 },
    schema_data_t{ fnv1a::hash_const("CCSPlayer_ItemServices->m_bHasHelmet"), 0x49 },
    schema_data_t{ fnv1a::hash_const("CCSWeaponBaseVData->m_WeaponType"), 0x520 },
    schema_data_t{ fnv1a::hash_const("CCSWeaponBaseVData->m_szName"), 0x720 },
    schema_data_t{ fnv1a::hash_const("CEntityIdentity->m_designerName"), 0x20 },
    schema_data_t{ fnv1a::hash_const("CEntityIdentity->m_flags"), 0x30 },
    schema_data_t{ fnv1a::hash_const("CEntityInstance->m_pEntity"), 0x10 },
    schema_data_t{ fnv1a::hash_const("CGameSceneNode->m_vecAbsOrigin"), 0xC8 },
    schema_data_t{ fnv1a::hash_const("CGameSceneNode->m_vecOrigin"), 0x80 },
    schema_data_t{ fnv1a::hash_const("CModelState->m_ModelName"), 0xA8 },
    schema_data_t{ fnv1a::hash_const("CPlayer_WeaponServices->m_hActiveWeapon"), 0x60 },
    schema_data_t{ fnv1a::hash_const("CPlayer_WeaponServices->m_hMyWeapons"), 0x48 },
    schema_data_t{ fnv1a::hash_const("CSkeletonInstance->m_modelState"), 0x150 },
    schema_data_t{ fnv1a::hash_const("C_BaseEntity->m_hOwnerEntity"), 0x520 },
    schema_data_t{ fnv1a::hash_const("C_BaseEntity->m_iHealth"), 0x34C },
    schema_data_t{ fnv1a::hash_const("C_BaseEntity->m_iTeamNum"), 0x3EB },
    schema_data_t{ fnv1a::hash_const("C_BaseEntity->m_nSubclassID"), 0x380 },
    schema_data_t{ fnv1a::hash_const("C_BaseEntity->m_pGameSceneNode"), 0x330 },
    schema_data_t{ fnv1a::hash_const("C_BasePlayerPawn->m_pItemServices"), 0x11E8 },
    schema_data_t{ fnv1a::hash_const("C_BasePlayerPawn->m_pWeaponServices"), 0x11E0 },
    schema_data_t{ fnv1a::hash_const("C_CSPlayerPawn->m_ArmorValue"), 0x1C7C },
    schema_data_t{ fnv1a::hash_const("C_CSPlayerPawn->m_angEyeAngles"), 0x3320 },
    schema_data_t{ fnv1a::hash_const("C_CSPlayerPawn->m_bIsScoped"), 0x1C50 },
    schema_data_t{ fnv1a::hash_const("C_CSPlayerPawnBase->m_flFlashOverlayAlpha"), 0x13F4 },
    schema_data_t{ fnv1a::hash_const("C_Inferno->m_bFireIsBurning"), 0x1618 },
    schema_data_t{ fnv1a::hash_const("C_Inferno->m_fireCount"), 0x1958 },
    schema_data_t{ fnv1a::hash_const("C_Inferno->m_firePositions"), 0x1018 },
    schema_data_t{ fnv1a::hash_const("C_Inferno->m_nFireEffectTickBegin"), 0x196C },
    schema_data_t{ fnv1a::hash_const("C_PlantedC4->m_bBeingDefused"), 0x119C },
    schema_data_t{ fnv1a::hash_const("C_PlantedC4->m_bBombDefused"), 0x11B4 },
    schema_data_t{ fnv1a::hash_const("C_PlantedC4->m_bBombTicking"), 0x1160 },
    schema_data_t{ fnv1a::hash_const("C_PlantedC4->m_flC4Blow"), 0x1190 },
    schema_data_t{ fnv1a::hash_const("C_PlantedC4->m_flDefuseCountDown"), 0x11B0 },
    schema_data_t{ fnv1a::hash_const("C_SmokeGrenadeProjectile->m_nSmokeEffectTickBegin"), 0x1250 },
    schema_data_t{ fnv1a::hash_const("C_SmokeGrenadeProjectile->m_vSmokeDetonationPos"), 0x1268 },
};

namespace
{
    bool add_schema_entry(const fnv1a_t hash, const uint32_t offset, const bool overwrite = false)
    {
        if (!offset) return false;
        const auto it = m_schema_data.find(hash);
        if (it != m_schema_data.end())
        {
            if (overwrite) it->second = offset;
            return false;
        }
        m_schema_data.emplace(hash, offset);
        return true;
    }

    bool add_schema_entry(const schema_data_t& data, const bool overwrite = false)
    {
        return add_schema_entry(data.m_hashed_field_name, data.m_offset, overwrite);
    }

    std::optional<std::filesystem::path> get_executable_directory()
    {
        std::array<char, MAX_PATH> path_buffer = {};
        const auto path_size = GetModuleFileNameA(nullptr, path_buffer.data(), static_cast<DWORD>(path_buffer.size()));
        if (!path_size || path_size == path_buffer.size()) return {};
        std::filesystem::path executable_path(path_buffer.data());
        return executable_path.parent_path();
    }

    std::vector<std::filesystem::path> get_possible_dump_directories()
    {
        std::vector<std::filesystem::path> dump_directories = {};
        std::set<std::filesystem::path> unique_dump_directories = {};
        auto add_dump_dir = [&](const std::filesystem::path& base_path)
            {
                if (base_path.empty()) return;
                for (uint32_t idx = 0; idx < 4; ++idx)
                {
                    auto current = base_path;
                    for (uint32_t up = 0; up < idx; ++up)
                        current = current.parent_path();
                    if (current.empty()) continue;
                    const auto dump_path = current / "dump";
                    if (!unique_dump_directories.emplace(dump_path).second) continue;
                    dump_directories.emplace_back(dump_path);
                }
            };
        add_dump_dir(std::filesystem::current_path());
        if (const auto executable_directory = get_executable_directory(); executable_directory.has_value())
            add_dump_dir(executable_directory.value());
        return dump_directories;
    }

    std::optional<std::filesystem::path> find_dump_file(const std::string_view& file_name)
    {
        const auto dump_directories = get_possible_dump_directories();
        for (const auto& dump_directory : dump_directories)
        {
            const auto file_path = dump_directory / file_name;
            if (std::filesystem::exists(file_path))
                return file_path;
        }
        return {};
    }

    uint32_t load_offsets_from_dump_file(const std::filesystem::path& file_path, const std::string_view& module_name)
    {
        nlohmann::json json_dump = {};
        std::ifstream file(file_path);
        if (!file.is_open()) return 0;

        try { file >> json_dump; }
        catch (...) { return 0; }

        const std::string module_key(module_name);
        if (!json_dump.contains(module_key)) return 0;

        const auto& module = json_dump[module_key];
        if (!module.contains("classes") || !module["classes"].is_object()) return 0;

        uint32_t loaded = 0;
        for (const auto& [class_name, class_data] : module["classes"].items())
        {
            if (!class_data.contains("fields") || !class_data["fields"].is_object()) continue;

            for (const auto& [field_name, offset_data] : class_data["fields"].items())
            {
                if (!offset_data.is_number_integer() && !offset_data.is_number_unsigned()) continue;

                const auto field_offset = static_cast<uint32_t>(offset_data.get<uint64_t>());
                if (!field_offset) continue;

                const auto field_path = std::format("{}->{}", class_name, field_name);
                if (add_schema_entry(fnv1a::hash(field_path), field_offset))
                    loaded++;
            }
        }
        return loaded;
    }

    uint32_t load_offsets_from_dump_files()
    {
        uint32_t loaded = 0;
        if (const auto client_dump = find_dump_file("client_dll.json"); client_dump.has_value())
            loaded += load_offsets_from_dump_file(client_dump.value(), "client.dll");

        if (const auto engine_dump = find_dump_file("engine2_dll.json"); engine_dump.has_value())
            loaded += load_offsets_from_dump_file(engine_dump.value(), "engine2.dll");

        return loaded;
    }
}

bool schema::setup()
{
    LOG_DEBUG("schema::setup started");
    m_schema_data.clear();

    uint32_t dump_loaded = load_offsets_from_dump_files();

    if (dump_loaded > 1000)
        LOG_INFO("loaded '%d' schema offsets from a2x dump", dump_loaded);
    else
        LOG_WARNING("dump only provided '%d' offsets - make sure client_dll.json exists in dump folder", dump_loaded);

    uint32_t fallback_added = 0;
    for (const auto& fallback_entry : m_schema_fallback_data)
    {
        if (add_schema_entry(fallback_entry))
            fallback_added++;
    }

    LOG_INFO("loaded '%d' schema offsets total ('%d' dumped + '%d' fallbacks)",
        static_cast<int32_t>(m_schema_data.size()), dump_loaded, fallback_added);

    if (m_schema_data.empty())
        LOG_ERROR("failed to load any schema offsets!");

    return !m_schema_data.empty();
}

uint32_t schema::get_offset(const fnv1a_t hashed_field_name)
{
    const auto it = m_schema_data.find(hashed_field_name);
    if (it != m_schema_data.end())
        return it->second;

    // Only log in debug to reduce spam
#ifdef _DEBUG
    LOG_ERROR("failed to find offset for hash '%llu'", static_cast<unsigned long long>(hashed_field_name));
#endif
    return 0;
}