#pragma once

/* current build of cs2_webradar */
#define CS2_WEBRADAR_VERSION "v1.3.1"

/* game modules */
#define CLIENT_DLL "client.dll"
#define ENGINE2_DLL "engine2.dll"
#define SCHEMASYSTEM_DLL "schemasystem.dll"

/* game signatures */
#define GET_SCHEMA_SYSTEM "48 89 05 ? ? ? ? 4C 8D 0D ? ? ? ? 33 C0 48 C7 05 ? ? ? ? ? ? ? ? 89 05"
#define GET_ENTITY_LIST "48 8B 1D ? ? ? ? 48 89 1D ? ? ? ? 4C 63 B3"
#define GET_GLOBAL_VARS "48 89 15 ? ? ? ? 48 89 42"
#define GET_LOCAL_PLAYER_CONTROLLER "48 8B 05 ? ? ? ? 41 89 BE"

/* cs2-dumper offsets from dump folder (2026-05-29) */
namespace offsets
{
    inline constexpr uintptr_t m_dw_entity_list = 0x24E5590;
    inline constexpr uintptr_t m_dw_global_vars = 0x205F6D0;
    inline constexpr uintptr_t m_dw_local_player_controller = 0x231E700;
    inline constexpr uintptr_t m_schema_system = 0x76800;
}

namespace log_detail
{
    template<typename... t_args>
    inline void write(const char* level, const char* format, t_args... args)
    {
        FILE* log = fopen("WR_Log.txt", "a+");
        if (!log)
            return;

        fprintf(log, "[%s] ", level);
        fprintf(log, format, args...);
        fprintf(log, "\n");
        fclose(log);
    }

    template<typename... t_args>
    inline void write_error(const char* file, const int line, const char* format, t_args... args)
    {
        const auto filename = std::filesystem::path(file).filename().string();

        FILE* log = fopen("WR_Log.txt", "a+");
        if (!log)
            return;

        fprintf(log, "[ERROR] [%s:%d] ", filename.c_str(), line);
        fprintf(log, format, args...);
        fprintf(log, "\n");
        fclose(log);
    }

    inline void clear()
    {
        FILE* log = fopen("WR_Log.txt", "w+");
        if (log)
            fclose(log);
    }
}

#define LOG_INFO(...) log_detail::write("INFO", __VA_ARGS__)
#define LOG_DEBUG(...) log_detail::write("DEBUG", __VA_ARGS__)
#define LOG_WARNING(...) log_detail::write("WARNING", __VA_ARGS__)
#define LOG_ERROR(...) log_detail::write_error(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_CLEAR() log_detail::clear()
