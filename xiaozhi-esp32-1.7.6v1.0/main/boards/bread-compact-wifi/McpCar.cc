#include "board.h"
#include "mcp_server.h"
#include "driver/uart.h"
#include <driver/gpio.h>
#include <esp_log.h>
#include <cstdio>
#include <cstring>
#include "mcpcar.h"

// 全局UART配置（所有设备共用）
#define UART_NUM UART_NUM_1
#define UART_TX 20
#define UART_RX 21
#define BUF_SIZE 1024

// ------------------------------
// 设备控制函数（空调、风扇、灯光、锁具）
// ------------------------------

// 空调控制
static void air_send_cmd(const char *state, int temp)
{
    char cmd[32];
    int clamped_temp = (temp < 16) ? 16 : (temp > 32) ? 32
                                                      : temp;
    sprintf(cmd, "BT,Air,%s,%d\r\n", state, clamped_temp);
    uart_write_bytes(UART_NUM, (const uint8_t *)cmd, strlen(cmd));
}

// 风扇控制
static void fan_send_cmd(const char *state, int gear)
{
    char cmd[32];
    sprintf(cmd, "BT,Fan,%s,%d\r\n", state, gear);
    uart_write_bytes(UART_NUM, (const uint8_t *)cmd, strlen(cmd));
}

// 灯光控制
static void light_send_cmd(const char *state, int gear)
{
    char cmd[32];
    int clamped_gear = (gear < 1) ? 1 : (gear > 5) ? 5
                                                   : gear;
    sprintf(cmd, "BT,Light,%s,%d\r\n", state, clamped_gear);
    uart_write_bytes(UART_NUM, (const uint8_t *)cmd, strlen(cmd));
}

// 锁具控制
static void lock_send_cmd(const char *state, int gear)
{
    char cmd[32];
    sprintf(cmd, "BT,Lock,%s,%d\r\n", state, gear); 
    uart_write_bytes(UART_NUM, (const uint8_t *)cmd, strlen(cmd));
}

// ------------------------------
// 注册所有设备工具
// ------------------------------
static void register_device_tools()
{
    auto &server = McpServer::GetInstance();

    // 空调工具
    server.AddTool("air.off", "空调关闭", PropertyList(),
                   [](const PropertyList &) -> ReturnValue
                   {
                       air_send_cmd("OFF", 0);
                       return true;
                   });
    server.AddTool("air.set_temp", "设置空调温度（16-32度）",
                   PropertyList({Property("temperature", kPropertyTypeInteger, 16, 32)}),
                   [](const PropertyList &props) -> ReturnValue
                   {
                       air_send_cmd("ON", props["temperature"].value<int>());
                       return true;
                   });

    // 风扇工具
    server.AddTool("fan.off", "风扇关闭", PropertyList(),
                   [](const PropertyList &) -> ReturnValue
                   {
                       fan_send_cmd("OFF", 0);
                       return true;
                   });
    server.AddTool("fan.set_gear", "设置风扇档位（1-3档）",
                   PropertyList({Property("gear", kPropertyTypeInteger, 1, 3)}),
                   [](const PropertyList &props) -> ReturnValue
                   {
                       fan_send_cmd("ON", props["gear"].value<int>());
                       return true;
                   });

    // 灯光工具
    server.AddTool("light.off", "灯光关闭", PropertyList(),
                   [](const PropertyList &) -> ReturnValue
                   {
                       light_send_cmd("OFF", 0);
                       return true;
                   });
    server.AddTool("light.set_gear", "设置灯光档位（1-5档，对应20%-99%亮度）",
                   PropertyList({Property("gear", kPropertyTypeInteger, 1, 5)}),
                   [](const PropertyList &props) -> ReturnValue
                   {
                       light_send_cmd("ON", props["gear"].value<int>());
                       return true;
                   });

    // 锁具工具
    server.AddTool("lock.unlock", "锁具解锁", PropertyList(),
                   [](const PropertyList &) -> ReturnValue
                   {
                       lock_send_cmd("ON", 1);
                       return true;
                   });
    server.AddTool("lock.lock", "锁具锁定", PropertyList(),
                   [](const PropertyList &) -> ReturnValue
                   {
                       lock_send_cmd("OFF", 0);
                       return true;
                   });
}

// ------------------------------
// McpCar类实现（含原有动作控制）
// ------------------------------
#define TAG "McpCar"

// UART初始化（供所有设备共用）
void McpCar::uart1_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_set_pin(UART_NUM, UART_TX, UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_param_config(UART_NUM, &uart_config);
}

// 发送单字节指令（McpCar动作控制）
void McpCar::send_uart_command(uint8_t command)
{
    uart_write_bytes(UART_NUM, &command, 1);
}

// 构造函数：初始化UART+注册所有工具（McpCar动作+设备控制）
McpCar::McpCar()
{
    uart1_init(); // 初始化UART（所有设备共用）
    auto &server = McpServer::GetInstance();

    // 1. 注册设备控制工具（空调、风扇等）
    register_device_tools();

    // 2. 注册McpCar动作控制工具（原有动作）
    server.AddTool("self.mcpcar.relax", "放松趴下", PropertyList(),
                   [this](const PropertyList &) -> ReturnValue
                   {
                       ESP_LOGI(TAG, "执行放松趴下命令");
                       send_uart_command(0x29);
                       return true;
                   });

    server.AddTool("self.mcpcar.squat", "蹲下", PropertyList(),
                   [this](const PropertyList &) -> ReturnValue
                   {
                       ESP_LOGI(TAG, "执行蹲下命令");
                       send_uart_command(0x30);
                       return true;
                   });

    server.AddTool("self.mcpcar.stand", "直立", PropertyList(),
                   [this](const PropertyList &) -> ReturnValue
                   {
                       ESP_LOGI(TAG, "执行直立命令");
                       send_uart_command(0x31);
                       return true;
                   });

    server.AddTool("self.mcpcar.lie_down", "趴下", PropertyList(),
                   [this](const PropertyList &) -> ReturnValue
                   {
                       ESP_LOGI(TAG, "执行趴下命令");
                       send_uart_command(0x32);
                       return true;
                   });

    server.AddTool("self.mcpcar.forward", "前进", PropertyList(),
                   [this](const PropertyList &) -> ReturnValue
                   {
                       ESP_LOGI(TAG, "执行前进命令");
                       send_uart_command(0x33);
                       return true;
                   });

    server.AddTool("self.mcpcar.backward", "后退", PropertyList(),
                   [this](const PropertyList &) -> ReturnValue
                   {
                       ESP_LOGI(TAG, "执行后退命令");
                       send_uart_command(0x34);
                       return true;
                   });

    server.AddTool("self.mcpcar.turn_left", "左转", PropertyList(),
                   [this](const PropertyList &) -> ReturnValue
                   {
                       ESP_LOGI(TAG, "执行左转命令");
                       send_uart_command(0x35);
                       return true;
                   });

    server.AddTool("self.mcpcar.turn_right", "右转", PropertyList(),
                   [this](const PropertyList &) -> ReturnValue
                   {
                       ESP_LOGI(TAG, "执行右转命令");
                       send_uart_command(0x36);
                       return true;
                   });

    server.AddTool("self.mcpcar.swing", "摇摆", PropertyList(),
                   [this](const PropertyList &) -> ReturnValue
                   {
                       ESP_LOGI(TAG, "执行摇摆命令");
                       send_uart_command(0x37);
                       return true;
                   });

    server.AddTool("self.mcpcar.speed_up", "增加移动速度", PropertyList(),
                   [this](const PropertyList &) -> ReturnValue
                   {
                       ESP_LOGI(TAG, "执行增加移动速度命令");
                       send_uart_command(0x38);
                       return true;
                   });

    server.AddTool("self.mcpcar.swing_speed_up", "增加摇摆速度", PropertyList(),
                   [this](const PropertyList &) -> ReturnValue
                   {
                       ESP_LOGI(TAG, "执行增加摇摆速度命令");
                       send_uart_command(0x39);
                       return true;
                   });

    server.AddTool("self.mcpcar.wag_tail", "摇尾巴", PropertyList(),
                   [this](const PropertyList &) -> ReturnValue
                   {
                       ESP_LOGI(TAG, "执行摇尾巴命令");
                       send_uart_command(0x40);
                       return true;
                   });

    server.AddTool("self.mcpcar.jump_forward", "向前跳", PropertyList(),
                   [this](const PropertyList &) -> ReturnValue
                   {
                       ESP_LOGI(TAG, "执行向前跳命令");
                       send_uart_command(0x41);
                       return true;
                   });

    server.AddTool("self.mcpcar.jump_backward", "向后跳", PropertyList(),
                   [this](const PropertyList &) -> ReturnValue
                   {
                       ESP_LOGI(TAG, "执行向后跳命令");
                       send_uart_command(0x42);
                       return true;
                   });

    server.AddTool("self.mcpcar.greet", "打招呼", PropertyList(),
                   [this](const PropertyList &) -> ReturnValue
                   {
                       ESP_LOGI(TAG, "执行打招呼命令");
                       send_uart_command(0x43);
                       return true;
                   });

    server.AddTool("self.mcpcar.Identify who I am", "识别我是谁", PropertyList(),
                   [this](const PropertyList &) -> ReturnValue
                   {
                       ESP_LOGI(TAG, "执行识别我是谁命令");
                       send_uart_command(0x44);
                       return true;
                   });

    server.AddTool("self.mcpcar.Enter the master's command", "录入主人的指令", PropertyList(),
                   [this](const PropertyList &) -> ReturnValue
                   {
                       ESP_LOGI(TAG, "执行录入主人指令命令");
                       send_uart_command(0x45);
                       return true;
                   });

    server.AddTool("self.mcpcar.Enter a command", "录入一个指令", PropertyList(),
                   [this](const PropertyList &) -> ReturnValue
                   {
                       ESP_LOGI(TAG, "执行录入指令命令");
                       send_uart_command(0x46);
                       return true;
                   });

    // 电量获取工具
    server.AddTool("self.mcpcar.get_battery_level", "获取电量百分比", PropertyList(),
                   [this](const PropertyList &) -> ReturnValue
                   {
                       send_uart_command(0x47); // 请求电量
                       vTaskDelay(100 / portTICK_PERIOD_MS);
                       return GetBatteryLevel();
                   });

    // 启动串口接收任务（处理电量反馈）
    xTaskCreate([](void *pv)
                { static_cast<McpCar *>(pv)->uart1_receive_task(nullptr); }, "uart_rx_task", 2048, this, 10, nullptr);
}

// 电量缓存与接收解析
static int g_battery_level = 100;
void McpCar::uart1_receive_task(void *pv)
{
    uint8_t data[4];
    while (true)
    {
        int len = uart_read_bytes(UART_NUM, data, sizeof(data), 100 / portTICK_PERIOD_MS);
        if (len == 4 && data[0] == 0xAA && data[1] == 0xBB && data[3] == 0xCC)
        {
            g_battery_level = std::clamp(static_cast<int>(data[2]), 0, 100); // 限制在0-100
            ESP_LOGI(TAG, "电量更新: %d%%", g_battery_level);
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

// 获取电量接口
int McpCar::GetBatteryLevel() const
{
    return g_battery_level;
}

// 全局实例（触发初始化）
McpCar carInstance;