#include "littlefs_manager.hpp"
// #include "fsl_flexspi.h" / on-chip flash HAL, matching whichever NOR
// device is wired to the RT1176's FlexSPI for LittleFS storage.

bool LittleFsManager::bindBlockDevice() {
    m_cfg.read  = &LittleFsManager::bdRead;
    m_cfg.prog  = &LittleFsManager::bdProg;
    m_cfg.erase = &LittleFsManager::bdErase;
    m_cfg.sync  = &LittleFsManager::bdSync;

    // These must match the physical flash geometry exactly.
    m_cfg.read_size      = 256;
    m_cfg.prog_size      = 256;
    m_cfg.block_size     = 4096;
    m_cfg.block_count    = 512;      // e.g. 2 MB region reserved for logs
    m_cfg.cache_size     = 256;
    m_cfg.lookahead_size = 32;
    m_cfg.block_cycles   = 500;

    return true;
}

bool LittleFsManager::mount() {
    if (!bindBlockDevice()) return false;

    int err = lfs_mount(&m_lfs, &m_cfg);
    if (err) {
        // No valid filesystem yet (first boot) -- format then retry once.
        lfs_format(&m_lfs, &m_cfg);
        err = lfs_mount(&m_lfs, &m_cfg);
    }
    m_mounted = (err == 0);
    return m_mounted;
}

bool LittleFsManager::appendLine(const char* path, const char* data, size_t len) {
    if (!m_mounted) return false;

    lfs_file_t file;
    if (lfs_file_open(&m_lfs, &file, path, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND) != 0) {
        return false;
    }
    lfs_ssize_t written = lfs_file_write(&m_lfs, &file, data, len);
    lfs_file_close(&m_lfs, &file);

    return written == static_cast<lfs_ssize_t>(len);
}

int LittleFsManager::bdRead(const lfs_config* c, lfs_block_t block, lfs_off_t off, void* buf, lfs_size_t size) {
    (void)c; (void)block; (void)off; (void)buf; (void)size;
    return 0; // e.g. FLEXSPI_ReadFlash(...)
}
int LittleFsManager::bdProg(const lfs_config* c, lfs_block_t block, lfs_off_t off, const void* buf, lfs_size_t size) {
    (void)c; (void)block; (void)off; (void)buf; (void)size;
    return 0; // e.g. FLEXSPI_ProgramFlash(...)
}
int LittleFsManager::bdErase(const lfs_config* c, lfs_block_t block) {
    (void)c; (void)block;
    return 0; // e.g. FLEXSPI_EraseSector(...)
}
int LittleFsManager::bdSync(const lfs_config* c) {
    (void)c;
    return 0;
}
