#include <psyz.h>
#include <libcd.h>
#include <psyz/log.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include "../audio.h"
#include "../internal.h"

#define SECTOR_SIZE 2352
#define MAX_TRACKS 99
#define MAX_PATH_LEN 512

// Track types
typedef enum {
    TRACK_AUDIO,
    TRACK_MODE1_2352,
    TRACK_MODE2_2352,
    TRACK_UNKNOWN
} TrackType;

// Track entry from CUE file
typedef struct {
    char file_path[MAX_PATH_LEN];
    int start_sector; // MSF sector offset within the file
    int abs_sector;   // Absolute sector position on the "virtual CD"
    int track_num;
    TrackType type;
    int is_valid;
} TrackEntry;

// Global CUE data storage
static TrackEntry g_tracks[MAX_TRACKS];
static int g_track_count = 0;
static char g_cue_base_path[MAX_PATH_LEN];

char* CD_comstr[] = {
    "CdlSync",    "CdlNop",
    "CdlSetloc",  "CdlPlay",
    "CdlForward", "CdlBackward",
    "CdlReadN",   "CdlStandby",
    "CdlStop",    "CdlPause",
    "CdlReset",   "CdlMute",
    "CdlDemute",  "CdlSetfilter",
    "CdlSetmode", "CdlGetparam",
    "CdlGetlocL", "CdlGetlocP",
    "?",          "CdlGetTN",
    "CdlGetTD",   "CdlSeekL",
    "CdlSeekP",   "?",
    "?",          "?",
    "?",          "CdlReadS",
    "?",          "?",
    "?",          "?",
};
char* CD_intstr[] = {
    "NoIntr",  "DataReady", "Complete", "Acknowledge",
    "DataEnd", "DiskError", "?",        "?",
};

CdlCB CD_cbsync = NULL;
CdlCB CD_cbready = NULL;
int CD_cbread = 0;
int CD_debug = 0;
int CD_status = 0;
int CD_status1 = 0;
int CD_nopen = 0;
CdlLOC CD_pos = {2, 0, 0, 0};
u_char CD_mode = 0;
u_char CD_com = 0;
int DS_active = 0;

// Trim whitespace from both ends of a string
static char* str_trim(char* str) {
    char* end;
    while (isspace((unsigned char)*str))
        str++;
    if (*str == 0)
        return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;
    end[1] = '\0';
    return str;
}

// Extract quoted string or unquoted token
static char* extract_token(char* str, char* dest, int max_len) {
    char* p = str;
    int len = 0;

    // Skip leading whitespace
    while (*p && isspace((unsigned char)*p))
        p++;
    if (*p == '"') {
        // Quoted string
        p++;
        while (*p && *p != '"' && len < max_len - 1) {
            dest[len++] = *p++;
        }
        if (*p == '"')
            p++;
    } else {
        // Unquoted token (up to space)
        while (*p && !isspace((unsigned char)*p) && len < max_len - 1) {
            dest[len++] = *p++;
        }
    }
    dest[len] = '\0';
    return p;
}

// Get file size in sectors
static int get_file_size_in_sectors(const char* file_path) {
    FILE* fp = fopen(file_path, "rb");
    if (!fp) {
        WARNF("Failed to open file for size calculation: %s", file_path);
        return 0;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fclose(fp);

    if (file_size < 0) {
        WARNF("Failed to get file size: %s", file_path);
        return 0;
    }

    // Convert bytes to sectors (rounded up)
    return (file_size + SECTOR_SIZE - 1) / SECTOR_SIZE;
}

// Parse a CUE file and populate the global track table
static int parse_cue_file(const char* cue_path) {
    FILE* fp = fopen(cue_path, "r");
    if (!fp) {
        ERRORF("error opening CUE file %s", cue_path);
        return -1;
    }

    // Extract base directory from CUE path for resolving relative file paths
    const char* last_slash = strrchr(cue_path, '/');
    if (!last_slash) {
        last_slash = strrchr(cue_path, '\\');
    }
    if (last_slash) {
        int len = last_slash - cue_path + 1;
        if (len >= MAX_PATH_LEN)
            len = MAX_PATH_LEN - 1;
        strncpy(g_cue_base_path, cue_path, len);
        g_cue_base_path[len] = '\0';
    } else {
        g_cue_base_path[0] = '\0';
    }

    char full_line[1024];
    char current_file[MAX_PATH_LEN] = {0};
    char current_file_full_path[MAX_PATH_LEN] = {0};
    int current_track = -1;
    int current_file_abs_sector =
        0; // Absolute sector where current file starts

    g_track_count = 0;
    while (fgets(full_line, sizeof(full_line), fp)) {
        char* line = str_trim(full_line);
        if (strncmp(line, "FILE", 4) == 0) {
            // Before switching to new file, calculate size of previous file
            if (current_file[0] != '\0') {
                int prev_file_size =
                    get_file_size_in_sectors(current_file_full_path);
                current_file_abs_sector += prev_file_size;
            }

            char* p = line + 4;
            extract_token(p, current_file, MAX_PATH_LEN);

            // Build full path for the new file
            if (current_file[0] == '/' || current_file[0] == '\\' ||
                (current_file[0] != '\0' && current_file[1] == ':')) {
                // Absolute path
                strncpy(current_file_full_path, current_file, MAX_PATH_LEN - 1);
            } else {
                // Relative path - prepend base directory
                snprintf(current_file_full_path, MAX_PATH_LEN, "%s%s",
                         g_cue_base_path, current_file);
            }
        } else if (strncmp(line, "TRACK", 5) == 0) {
            int track_num = 1;
            char type_str[64];
            if (sscanf(line, "TRACK %d %63s", &track_num, type_str) == 2) {
                if (track_num < 1 || track_num > MAX_TRACKS) {
                    ERRORF("Invalid track number: %d", track_num);
                    continue;
                }

                current_track = track_num - 1;
                TrackEntry* track = &g_tracks[current_track];
                track->track_num = track_num;
                track->is_valid = 1;
                track->start_sector = 0; // Will be set by INDEX 01
                track->abs_sector =
                    0; // Will be calculated when INDEX 01 is parsed

                // Store file path
                strncpy(
                    track->file_path, current_file_full_path, MAX_PATH_LEN - 1);

                if (strcmp(type_str, "AUDIO") == 0) {
                    track->type = TRACK_AUDIO;
                } else if (strcmp(type_str, "MODE1/2352") == 0) {
                    track->type = TRACK_MODE1_2352;
                } else if (strcmp(type_str, "MODE2/2352") == 0) {
                    track->type = TRACK_MODE2_2352;
                } else {
                    track->type = TRACK_UNKNOWN;
                    DEBUGF("Unknown track type: %s", type_str);
                }

                if (track_num > g_track_count) {
                    g_track_count = track_num;
                }
            }
        } else if (strncmp(line, "INDEX", 5) == 0) {
            if (current_track < 0) {
                ERRORF("INDEX found before TRACK");
                continue;
            }
            int index_num;
            char msf_str[32];
            if (sscanf(line, "INDEX %d %31s", &index_num, msf_str) == 2) {
                int minute = 0, second = 0, frame = 0;
                if (sscanf(msf_str, "%d:%d:%d", &minute, &second, &frame) ==
                    3) {
                    int sector = minute * 60 * 75 + second * 75 + frame;
                    if (index_num == 1) { // INDEX 01 is the track data
                        TrackEntry* track = &g_tracks[current_track];
                        track->start_sector = sector;
                        track->abs_sector = current_file_abs_sector + sector;
                        DEBUGF("Track %02d: %s at sector %d", track->track_num,
                               track->file_path, track->abs_sector);
                    }
                }
            }
        } else {
            WARNF("CUE line not recognized: %s", line);
        }
    }
    fclose(fp);

    if (g_track_count == 0) {
        ERRORF("No valid tracks found in CUE file");
        return -1;
    }
    DEBUGF("Parsed CUE file: %d tracks", g_track_count);
    return 0;
}

int Psyz_SetDiskPath(const char* diskPath) {
    if (!diskPath) {
        ERRORF("diskPath is NULL");
        return -1;
    }
    g_track_count = 0;
    memset(g_tracks, 0, sizeof(g_tracks));
    if (parse_cue_file(diskPath) < 0) {
        return -1;
    }
    DEBUGF("Disk path set: %s", diskPath);
    return 0;
}

static DiskReadCB disk_read_cb = NULL;
void Psyz_SetDiskCallback(DiskReadCB cb) { disk_read_cb = cb; }

static void psyz_play() {
    if (!(CD_mode & CdlModeDA)) {
        WARNF("CdlModeDA not active, audio will not be played");
        return;
    }
    int sector = CdPosToInt(&CD_pos);
    TrackEntry* track = NULL;
    for (int i = 0; i < g_track_count; i++) {
        if (!g_tracks[i].is_valid)
            continue;

        // Check if sector falls within this track
        // For the last track, assume it extends to end of file
        int next_abs_sector = INT_MAX;
        if (i + 1 < g_track_count && g_tracks[i + 1].is_valid) {
            next_abs_sector = g_tracks[i + 1].abs_sector;
        }
        if (sector >= g_tracks[i].abs_sector && sector < next_abs_sector) {
            track = &g_tracks[i];
            break;
        }
    }
    if (!track) {
        WARNF("no track found for sector %d", sector);
        return;
    }
    FILE* file = fopen(track->file_path, "rb");
    if (!file) {
        ERRORF("failed to open audio file: %s", track->file_path);
        return;
    }

    // sector is the absolute sector, track->abs_sector is where this track
    // starts track->start_sector is the MSF offset within the file
    int sector_offset = (sector - track->abs_sector) + track->start_sector;
    long byte_offset = (long)sector_offset * SECTOR_SIZE;
    if (fseek(file, byte_offset, SEEK_SET) != 0) {
        ERRORF("failed to seek to sector %d in %s", sector, track->file_path);
        fclose(file);
        return;
    }
    DEBUGF("playing audio from %s at sector %d (offset %d)", track->file_path,
           sector, sector_offset);
    Audio_PlayCdAudio(file);
}

static void psyz_stop() { Audio_Stop(); }

static void psyz_pause() { Audio_Pause(); }

static void psyz_mute() { Audio_Mute(); }

static void psyz_demute() { Audio_Demute(); }

static void cdaudio_end_cb(void) {
    if (CD_cbready) {
        CD_cbready(CdlDataEnd, NULL);
    }
}
int CD_init(void) {
    Audio_SetCdAudioEndCB(cdaudio_end_cb);
    return 1;
}
void CD_initintr(void) { NOT_IMPLEMENTED; }
void CD_flush(void) { NOT_IMPLEMENTED; }
int CD_initvol(void) {
    NOT_IMPLEMENTED;
    return 0;
}
int CD_sync(int mode, u_char* result) {
    NOT_IMPLEMENTED;
    return CdlComplete;
}
int CD_ready(int mode, u_char* result) {
    NOT_IMPLEMENTED;
    return CdlComplete;
}
int CD_cw(u8 com, u8* param, u_char* result, s32 arg3) {
    CD_sync(0, 0);
    switch (com) {
    case CdlSetloc:
        if (!param) {
            ERRORF("%s got NULL param", CD_comstr[com]);
            return -2;
        }
        CD_pos = *(CdlLOC*)param;
        break;
    case CdlPlay:
        psyz_play();
        break;
    case CdlStop:
        psyz_stop();
        break;
    case CdlPause:
        psyz_pause();
        break;
    case CdlMute:
        psyz_mute();
        break;
    case CdlDemute:
        psyz_demute();
        break;
    case CdlSetmode:
        if (!param) {
            ERRORF("%s got NULL param", CD_comstr[com]);
            return -2;
        }
        if (*param & CdlModeDA) {
            Audio_Init();
        }
        if (*param & CdlModeAP) {
            DEBUGF("%s does not support CdlModeAP", CD_comstr[com]);
        }
        if (*param & CdlModeRept) {
            DEBUGF("%s does not support CdlModeRept", CD_comstr[com]);
        }
        if (*param & CdlModeSF) {
            DEBUGF("%s does not support CdlModeSF", CD_comstr[com]);
        }
        if (*param & CdlModeSize0) {
            DEBUGF("%s does not support CdlModeSize0", CD_comstr[com]);
        }
        if (*param & CdlModeSize1) {
            DEBUGF("%s does not support CdlModeSize1", CD_comstr[com]);
        }
        if (*param & CdlModeRT) {
            DEBUGF("%s does not support CdlModeRT", CD_comstr[com]);
        }
        if (*param & CdlModeSpeed) {
            DEBUGF("%s does not support CdlModeSpeed", CD_comstr[com]);
        }
        if (*param & CdlModeStream2) {
            DEBUGF("%s does not support CdlModeStream2", CD_comstr[com]);
        }
        if (*param & CdlModeStream) {
            DEBUGF("%s does not support CdlModeStream", CD_comstr[com]);
        }
        CD_mode = *param;
        break;
    default:
        if (com >= LEN(CD_comstr)) {
            ERRORF("com %X invalid", com);
            return -1;
        }
        DEBUGF("com %s not implemented", CD_comstr[com]);
    }
    return 0;
}
int CD_vol(CdlATV* vol) { NOT_IMPLEMENTED; }
int CD_getsector(void* madr, int size) { NOT_IMPLEMENTED; }
int CD_getsector2(void) { NOT_IMPLEMENTED; }
void CD_datasync(int mode) { NOT_IMPLEMENTED; }

int CdInit(void) {
    NOT_IMPLEMENTED;
    return CD_init();
}

int CdReading() {
    NOT_IMPLEMENTED;
    return 0;
}

void ExecCd() { NOT_IMPLEMENTED; }

CdlFILE* CdSearchFile(CdlFILE* fp, char* name) {
    NOT_IMPLEMENTED;
    return NULL;
}

int CdRead(int sectors, u_long* buf, int mode) {
    NOT_IMPLEMENTED;
    return 0;
}

int CdRead2(long mode) {
    NOT_IMPLEMENTED;
    return 0;
}

int CdReadSync(int mode, u_char* result) {
    NOT_IMPLEMENTED;
    return 0;
}

u_long StGetNext(u_long** addr, u_long** header) {
    NOT_IMPLEMENTED;
    return 0;
}

void StSetRing(u_long* ring_addr, u_long ring_size) { NOT_IMPLEMENTED; }

void StSetStream(u_long mode, u_long start_frame, u_long end_frame,
                 void (*func1)(), void (*func2)()) {
    NOT_IMPLEMENTED;
}

u_long StFreeRing(u_long* base) {
    NOT_IMPLEMENTED;
    return 0;
}
