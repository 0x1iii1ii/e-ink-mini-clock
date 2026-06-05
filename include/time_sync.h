//
// Created by Li on 5/1/2026.
//

#ifndef TIME_MGR_H
#define TIME_MGR_H

#include "wifi_mgr.h"

#define NTP_INTERVAL_SEC ((uint32_t)cfg.clockCfg.ntpSyncDays * 24 * 3600)
#define NTP_RETRY_SEC ((uint32_t)cfg.clockCfg.ntpReSyncDays * 24 * 3600)

extern uint32_t rtcNvBootCount;
extern uint32_t rtcNvLastNtpEpoch;
extern bool     rtcNvNtpPending;
extern uint8_t  rtcNvRetryDays;

bool getRtcTime(struct tm* timeinfo);
void rtc_init();
void restore_rtc();
void sync_time();
bool shouldSyncNtp();
void doNtpSync();

#endif //TIME_MGR_H
