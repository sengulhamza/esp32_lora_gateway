#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "sx127x.h"

#define SX127X_UNSED_PIN_NUM -1

static const char *TAG = "sx127x_driver";

static TaskHandle_t task_handle;

typedef struct {
    void (*task)(void *pvParameter);
} sx127x_frtos_params;

typedef struct {
    spi_host_device_t spi_host;
    gpio_num_t pin_miso;
    gpio_num_t pin_mosi;
    gpio_num_t pin_sclk;
    gpio_num_t pin_nss;
    gpio_num_t pin_rx_tx;
    gpio_num_t pin_rst;
    gpio_num_t pin_dio0;
    sx127x_frtos_params frtos_p;
} sx127x_pin_conf_t;
static sx127x_pin_conf_t sx127x_conf;
static spi_device_handle_t spi_handle;
static spi_transaction_t spi_transaction;
static long __frequency = 0;
static int __implicit  = 0;
static void assert_nss(spi_transaction_t *trans);
static void deassert_nss(spi_transaction_t *trans);

#define NOTIFY_BIT_DIO 1
void IRAM_ATTR qio_irq_handler(void *arg)
{
    BaseType_t higher_prio_task_woken = pdFALSE;
    xTaskNotifyFromISR(task_handle, NOTIFY_BIT_DIO, eSetBits, &higher_prio_task_woken);
    if (higher_prio_task_woken) {
        portYIELD_FROM_ISR();
    }
}

void sx127x_set_task_params(void *task)
{
    sx127x_conf.frtos_p.task = task;
    if (sx127x_conf.frtos_p.task == NULL) {
        ESP_LOGE(TAG, "sx127x_conf.frtos_p.task is null");
    }
}

void sx127x_configure_pins(spi_host_device_t host, uint8_t miso, uint8_t mosi, uint8_t sclk, uint8_t nss, uint8_t rst, uint8_t dio0)
{
    sx127x_conf.spi_host = host;
    sx127x_conf.pin_miso = (gpio_num_t)miso,
    sx127x_conf.pin_mosi = (gpio_num_t)mosi,
    sx127x_conf.pin_sclk = (gpio_num_t)sclk,
    sx127x_conf.pin_nss = (gpio_num_t)nss;
    sx127x_conf.pin_rst = (gpio_num_t)rst;
    sx127x_conf.pin_dio0 = (gpio_num_t)dio0;
}

static void sx127x_init_io(void)
{
    // ASSERT(sx127x_conf.pin_nss != SX127X_UNSED_PIN_NUM);
    // ASSERT(sx127x_conf.pin_dio0 != SX127X_UNSED_PIN_NUM);
    // ASSERT(sx127x_conf.pin_rst != SX127X_UNSED_PIN_NUM);

    // Initialize the GPIO ISR handler service
    esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    ESP_ERROR_CHECK(err);

    gpio_config_t output_pin_config = {
        .pin_bit_mask = BIT64(sx127x_conf.pin_nss),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = false,
        .pull_down_en = false,
        .intr_type = GPIO_INTR_DISABLE
    };
    output_pin_config.pin_bit_mask |= BIT64(sx127x_conf.pin_rst);
    gpio_config(&output_pin_config);
    gpio_set_level(sx127x_conf.pin_nss, 1);
    gpio_set_level(sx127x_conf.pin_rst, 0);

    // DIO pins with interrupt handlers
    gpio_config_t input_pin_config = {
        .pin_bit_mask = BIT64(sx127x_conf.pin_dio0),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = false,
        .pull_down_en = true,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    gpio_config(&input_pin_config);
    gpio_isr_handler_add(sx127x_conf.pin_dio0, qio_irq_handler, (void *)0);
    ESP_LOGI(TAG, "IO initialized");
}

static void sx127x_init_spi(void)
{
    // Initialize SPI bus
    spi_bus_config_t spi_bus_config = {0};
    spi_bus_config.miso_io_num =  sx127x_conf.pin_miso;
    spi_bus_config.mosi_io_num =  sx127x_conf.pin_mosi;
    spi_bus_config.sclk_io_num =  sx127x_conf.pin_sclk;
    spi_bus_config.quadwp_io_num = -1;
    spi_bus_config.quadhd_io_num = -1;
    spi_bus_config.max_transfer_sz = 0;
    esp_err_t err = spi_bus_initialize(sx127x_conf.spi_host, &spi_bus_config, SPI_DMA_DISABLED);
    ESP_ERROR_CHECK(err);

    spi_device_interface_config_t spi_config = {
        .mode = 0,
        .clock_speed_hz = SPI_MASTER_FREQ_10M,
        .command_bits = 0,
        .address_bits = 8,
        .spics_io_num = -1,
        .queue_size = 1,
        .pre_cb = assert_nss,
        .post_cb = deassert_nss,
    };
    esp_err_t ret = spi_bus_add_device(sx127x_conf.spi_host, &spi_config, &spi_handle);
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "SPI initialized");
}

void sx127x_spi_write(uint8_t cmd, const uint8_t *buf, size_t len)
{
    memset(&spi_transaction, 0, sizeof(spi_transaction));
    spi_transaction.addr = cmd;
    spi_transaction.length = 8 * len;
    spi_transaction.tx_buffer = buf;
    esp_err_t err = spi_device_transmit(spi_handle, &spi_transaction);
    ESP_ERROR_CHECK(err);
}

void sx127x_spi_read(uint8_t cmd, uint8_t *buf, size_t len)
{
    memset(buf, 0, len);
    memset(&spi_transaction, 0, sizeof(spi_transaction));
    spi_transaction.addr = cmd;
    spi_transaction.length = 8 * len;
    spi_transaction.rxlength = 8 * len;
    spi_transaction.tx_buffer = buf;
    spi_transaction.rx_buffer = buf;
    esp_err_t err = spi_device_transmit(spi_handle, &spi_transaction);
    ESP_ERROR_CHECK(err);
}

static void IRAM_ATTR assert_nss(spi_transaction_t *trans)
{
    gpio_set_level(sx127x_conf.pin_nss, 0);
}

static void IRAM_ATTR deassert_nss(spi_transaction_t *trans)
{
    gpio_set_level(sx127x_conf.pin_nss, 1);
}

void sx127x_write_reg(uint8_t addr, uint8_t data)
{
    sx127x_spi_write(addr | 0x80, &data, 1);
}

void sx127x_write_buf(uint8_t addr, uint8_t *buf, uint8_t len)
{
    sx127x_spi_write(addr | 0x80, buf, len);
}

uint8_t sx127x_read_reg(uint8_t addr)
{
    uint8_t reg_value_buf;
    sx127x_spi_read(addr & 0x7f, &reg_value_buf, 1);
    return reg_value_buf;
}

void sx127x_read_buf(uint8_t addr, uint8_t *buf, uint8_t len)
{
    sx127x_spi_read(addr & 0x7f, buf, len);
}

void sx127x_set_frequency(long frequency)
{
    __frequency = frequency;

    uint64_t frf = ((uint64_t)frequency << 19) / 32000000;

    sx127x_write_reg(REG_FRF_MSB, (uint8_t)(frf >> 16));
    sx127x_write_reg(REG_FRF_MID, (uint8_t)(frf >> 8));
    sx127x_write_reg(REG_FRF_LSB, (uint8_t)(frf >> 0));
}

void sx127x_enable_crc(void)
{
    sx127x_write_reg(REG_MODEM_CONFIG_2, sx127x_read_reg(REG_MODEM_CONFIG_2) | 0x04);
}

void sx127x_idle(void)
{
    sx127x_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
}

void sx127x_sleep(void)
{
    sx127x_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
}

void sx127x_send_packet(uint8_t *buf, int size)
{
    sx127x_idle();
    sx127x_write_reg(REG_FIFO_ADDR_PTR, 0);

    sx127x_write_buf(REG_FIFO, buf, size);

    // for (int i = 0; i < size; i++) {
    //     lora_write_reg(REG_FIFO, *buf++);
    // }

    sx127x_write_reg(REG_PAYLOAD_LENGTH, size);

    /*
     * Start transmission and wait for conclusion.
     */
    sx127x_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);
    while ((sx127x_read_reg(REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK) == 0) {
        vTaskDelay(2);
    }

    sx127x_write_reg(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);
}

void sx127x_receive(void)
{
    sx127x_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_RX_CONTINUOUS);
}

uint8_t sx127x_received(void)
{
    return (sx127x_read_reg(REG_IRQ_FLAGS) & IRQ_RX_DONE_MASK) ? 1 : 0;
}

void sx127x_set_tx_power(int level)
{
    // RF9x module uses PA_BOOST pin
    if (level < 2) {
        level = 2;
    } else if (level > 17) {
        level = 17;
    }
    sx127x_write_reg(REG_PA_CONFIG, PA_BOOST | (level - 2));
}

void sx127x_explicit_header_mode(void)
{
    __implicit = 0;
    sx127x_write_reg(REG_MODEM_CONFIG_1, sx127x_read_reg(REG_MODEM_CONFIG_1) & 0xfe);
}

int sx127x_receive_packet(uint8_t *buf, int size)
{
    int len = 0;

    /*
     * Check interrupts.
     */
    int irq = sx127x_read_reg(REG_IRQ_FLAGS);
    sx127x_write_reg(REG_IRQ_FLAGS, irq);
    if ((irq & IRQ_RX_DONE_MASK) == 0) {
        return 0;
    }
    if (irq & IRQ_PAYLOAD_CRC_ERROR_MASK) {
        return 0;
    }

    /*
     * Find packet size.
     */
    if (__implicit) {
        len = sx127x_read_reg(REG_PAYLOAD_LENGTH);
    } else {
        len = sx127x_read_reg(REG_RX_NB_BYTES);
    }

    /*
     * Transfer data from radio.
     */
    sx127x_idle();
    sx127x_write_reg(REG_FIFO_ADDR_PTR, sx127x_read_reg(REG_FIFO_RX_CURRENT_ADDR));
    if (len > size) {
        len = size;
    }
    sx127x_read_buf(REG_FIFO, buf, len);
    // for (int i = 0; i < len; i++) {
    //     *buf++ = sx127x_read_reg(REG_FIFO);
    // }
    return len;
}

int sx127x_packet_rssi(void)
{
    return (sx127x_read_reg(REG_PKT_RSSI_VALUE) - (__frequency < 868E6 ? 164 : 157));
}

void sx127x_reset(void)
{
    gpio_set_level(sx127x_conf.pin_rst, 0);
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(sx127x_conf.pin_rst, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}

void sx127x_init(void)
{
    sx127x_init_io();
    sx127x_init_spi();
    xTaskCreate(sx127x_conf.frtos_p.task, "ttn_lmic", 1024 * 4, NULL, 10, &task_handle);
    sx127x_reset();
    sx127x_set_frequency(LoRa_EUROPE_FREQUENCY);
    sx127x_enable_crc();
    sx127x_explicit_header_mode();
    sx127x_sleep();
    sx127x_write_reg(REG_FIFO_RX_BASE_ADDR, LORA_WRITE_REG_VALUE_1);
    sx127x_write_reg(REG_FIFO_TX_BASE_ADDR, LORA_WRITE_REG_VALUE_1);
    sx127x_write_reg(REG_LNA, sx127x_read_reg(REG_LNA) | LORA_WRITE_REG_VALUE_2);
    sx127x_write_reg(REG_MODEM_CONFIG_3, LORA_WRITE_REG_VALUE_3);
    sx127x_set_tx_power(LORA_TX_POWER);
    sx127x_idle();
}
