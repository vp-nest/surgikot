#pragma once
//
// littlefs_manager.hpp
// Thin wrapper around lfs_t bound to the on-chip / QSPI-NOR flash driver.
// Only LoggerTask ever touches this object, so no internal locking is
// required -- the single-writer invariant is enforced by the fact that
// every other task can only reach the log by sending a LogEventMsg
// through g_loggerQueue, never by calling into this class directly.
//
#include "lfs.h"
#include <cstdint>
#include <cstddef>

class LittleFsManager {
public:
    static LittleFsManager& instance() {
        static LittleFsManager inst;
        return inst;
    }

    // Mounts the filesystem; formats automatically on first boot / if the
    // superblock is invalid (LFS_ERR_CORRUPT).
    bool mount();

    // Appends `len` bytes to `path`, creating it if needed. Used for the
    // running event log so we never have to load the whole file into RAM.
    bool appendLine(const char* path, const char* data, size_t len);

    lfs_t* raw() { return &m_lfs; }

private:
    LittleFsManager() = default;

    bool bindBlockDevice();  // wires m_cfg.read/prog/erase/sync to the flash HAL

    static int bdRead (const lfs_config* c, lfs_block_t block, lfs_off_t off, void* buf, lfs_size_t size);
    static int bdProg (const lfs_config* c, lfs_block_t block, lfs_off_t off, const void* buf, lfs_size_t size);
    static int bdErase(const lfs_config* c, lfs_block_t block);
    static int bdSync (const lfs_config* c);

    lfs_t      m_lfs{};
    lfs_config m_cfg{};
    bool       m_mounted = false;
};
