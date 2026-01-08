/**
 * @file shell_port.h
 * @author Letter (NevermindZZT@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2019-02-22
 * 
 * @copyright (c) 2019 Letter
 * 
 */

#ifndef __SHELL_PORT_H__
#define __SHELL_PORT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "shell.h"

#ifdef __cplusplus
}
#endif

#define SHELL_BUFFER_SIZE 512
#define SHELL_TASK_STACK_SIZE 4096
#define SHELL_TASK_PRIORITY 10

#ifndef SHELL_USB_STREAM
#define SHELL_USB_STREAM Serial
#endif

extern Shell uartShell;
extern Shell usbShell;

short usbShellWrite(char *data, unsigned short len);
short usbShellRead(char *data, unsigned short len);

void uartShellInit(void);
void usbShellInit(void);

#endif /* __SHELL_PORT_H__ */
