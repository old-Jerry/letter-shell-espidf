/**
 * @file shell_port.c
 * @author Letter (NevermindZZT@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2019-02-22
 * 
 * @copyright (c) 2019 Letter
 * 
 */
#include "shell_port.h"
#include "shell.h"
#include "log.h"
#include "freertos/FreeRTOS.h"
#include "driver/uart.h"
#include <Arduino.h>




static char usbShellBuffer[SHELL_BUFFER_SIZE];
static char uartShellBuffer[SHELL_BUFFER_SIZE];

Shell usbShell;
Shell uartShell;
static uart_port_t uartPort = UART_NUM_0;

static Log usbLog = {
    .write = NULL,
    .active = 0,
    .level = LOG_DEBUG,
#if LOG_USING_LOCK == 1
    .lock = NULL,
    .unlock = NULL,
#endif
    .shell = NULL};
static Log uartLog = {
    .write = NULL,
    .active = 0,
    .level = LOG_DEBUG,
#if LOG_USING_LOCK == 1
    .lock = NULL,
    .unlock = NULL,
#endif
    .shell = NULL};
static void usbLogWrite(char *buffer, short len)
{
    shellWriteEndLine(&usbShell, buffer, len);
}

static void uartLogWrite(char *buffer, short len)
{
    shellWriteEndLine(&uartShell, buffer, len);
}
/**
 * @brief 用户shell写
 * 
 * @param data 数据
 * @param len 数据长度
 * 
 * @return short 写入实际长度
 */
short uartShellWrite(char *data, unsigned short len)
{
    if (len == 0)
    {
        return 0;
    }
    int written = uart_write_bytes(uartPort, data, len);
    return written < 0 ? 0 : static_cast<short>(written);
}
short usbShellWrite(char *data, unsigned short len)
{
    if (len == 0) return 0;
    
    // HWCDC的write()内部已经调用txfifo_flush()和ena_intr_mask触发ISR
    size_t written = SHELL_USB_STREAM.write(reinterpret_cast<uint8_t *>(data), len);
    
    // 添加微秒级延迟，给ISR时间从ring buffer取数据发送到USB
    // 200us足够ISR处理，且不会造成明显卡顿
    delayMicroseconds(200);
    
    return static_cast<short>(written);
}

/**
 * @brief 用户shell读
 * 
 * @param data 数据
 * @param len 数据长度
 * 
 * @return short 读取实际长度
 */
short uartShellRead(char *data, unsigned short len)
{
    if (len == 0)
    {
        return 0;
    }

    int read = uart_read_bytes(uartPort,
                               reinterpret_cast<uint8_t *>(data),
                               len,
                               pdMS_TO_TICKS(20));
    if (read < 0)
    {
        return 0;
    }
    return static_cast<short>(read);
}
short usbShellRead(char *data, unsigned short len)
{
    if (len == 0)
    {
        return 0;
    }
    const uint32_t timeoutMs = 20;
    TickType_t endTick = xTaskGetTickCount() + pdMS_TO_TICKS(timeoutMs);
    short copyLen = 0;
    while (copyLen < len)
    {
        int r = SHELL_USB_STREAM.read(reinterpret_cast<uint8_t *>(data) + copyLen,
                                      static_cast<size_t>(len - copyLen));
        if (r > 0)
        {
            copyLen += static_cast<short>(r);
            if (copyLen >= len)
            {break;}
            taskYIELD();
            continue;
        }
        if (xTaskGetTickCount() >= endTick)
        {break;}
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    return copyLen;
}

/**
 * @brief 用户shell初始化
 * 
 */
void uartShellInit(void)
{
    uart_config_t uartConfig = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(uartPort, &uartConfig);
    uart_driver_install(uartPort, 256 * 2, 0, 0, NULL, 0);
    
    // 清零Shell结构体，避免未初始化的函数指针
    memset(&uartShell, 0, sizeof(Shell));
    
    uartShell.write = uartShellWrite;
    uartShell.read = uartShellRead;
    shellInit(&uartShell, uartShellBuffer, SHELL_BUFFER_SIZE);
    xTaskCreate(shellTask, "uart_shell", SHELL_TASK_STACK_SIZE, &uartShell, SHELL_TASK_PRIORITY, NULL);
}
void usbShellInit(void)
{
    // 增大USB CDC的TX缓冲区，减少数据丢失
    SHELL_USB_STREAM.setTxBufferSize(1024);
    SHELL_USB_STREAM.setRxBufferSize(512);
    
    // 清零Shell结构体，避免未初始化的函数指针
    memset(&usbShell, 0, sizeof(Shell));
    
    usbShell.write = usbShellWrite;
    usbShell.read = usbShellRead;
    shellInit(&usbShell, usbShellBuffer, SHELL_BUFFER_SIZE);
    usbLog.write = usbLogWrite;
    usbLog.active = 1;
    usbLog.level = LOG_DEBUG;
    usbLog.shell = &usbShell;
    logRegister(&usbLog, &usbShell);
    xTaskCreate(shellTask, "usb_shell", SHELL_TASK_STACK_SIZE, &usbShell, SHELL_TASK_PRIORITY, NULL);
}

// ESP32的shell基础命令，重启，显示有wifi的IP，显示编译ID
void shellReboot()
{
    Shell *activeShell = shellGetCurrent();
    if (activeShell)
    {
        shellPrint(activeShell, "Rebooting...\r\n");
    }
    esp_restart();
}
void shellShowIP()
{
    Shell *activeShell = shellPortResolveShell();
    if (WiFi.status() == WL_CONNECTED)
    {
        IPAddress ip = WiFi.localIP();
        shellPrint(activeShell, "IP Address: %d.%d.%d.%d\r\n", ip[0], ip[1], ip[2], ip[3]);
    }
    else
    {
        shellPrint(activeShell, "WiFi not connected.\r\n");
    }
}
int showID(int argc, char **argv)
{
    Shell *activeShell = shellPortResolveShell();
    if (argc != 1)
    {
        shellPrint(activeShell, "Usage: id?\r\n");
        return -1;
    }
    shellPrint(activeShell, "Running %s, Built %s\r\n", __FILE__, __DATE__);
    return EXIT_SUCCESS;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC) | SHELL_CMD_DISABLE_RETURN, reboot, shellReboot, Reboot the system);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, id, showID, Show device information);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC) | SHELL_CMD_DISABLE_RETURN, ip, shellShowIP, Show current IP address);
