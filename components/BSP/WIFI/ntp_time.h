#ifndef NTP_TIME_H
#define NTP_TIME_H

// 引入tm结构体的头文件
#include <time.h>

/**
 * @brief 初始化NTP时间同步
 */
void ntp_init(void);

/**
 * @brief 获取当前北京时间
 * @param tm 存储时间的结构体指针（需提前分配内存）
 */
void ntp_get_time(struct tm *tm);

#endif // NTP_TIME_H