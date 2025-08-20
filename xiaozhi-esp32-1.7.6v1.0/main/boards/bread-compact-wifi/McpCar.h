#ifndef MCPCAR_H
#define MCPCAR_H

#include "mcp_server.h"
#include <driver/uart.h>

class McpCar
{
public:
    McpCar();                                // 构造函数：初始化UART并注册所有工具
    void uart1_init(void);                   // UART初始化
    void send_uart_command(uint8_t command); // 发送单字节指令
    int GetBatteryLevel() const;             // 获取电量

private:
    void uart1_receive_task(void *pvParameters); // 串口接收任务（处理电量）
};

#endif // MCPCAR_H