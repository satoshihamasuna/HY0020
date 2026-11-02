#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <bluetooth/services/nus.h>

static atomic_t connected;
static atomic_t notifications_enabled;

static const struct bt_data advertisement[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
};

static const struct bt_data scan_response[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void advertise(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(advertise_work, advertise);

static void advertise(struct k_work *work)
{
    ARG_UNUSED(work);
    if (atomic_get(&connected)) {
        return;
    }
    int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2,
                             advertisement, ARRAY_SIZE(advertisement),
                             scan_response, ARRAY_SIZE(scan_response));
    if (err == 0) {
        printk("Advertising as %s\n", CONFIG_BT_DEVICE_NAME);
    } else if (err != -EALREADY) {
        /* The old connection object may not yet have been recycled. */
        printk("Advertising retry: %d\n", err);
        k_work_reschedule(&advertise_work, K_SECONDS(1));
    }
}

static void on_connected(struct bt_conn *conn, uint8_t err)
{
    ARG_UNUSED(conn);
    if (err) {
        printk("Connection failed: %u\n", err);
        k_work_reschedule(&advertise_work, K_MSEC(250));
        return;
    }
    atomic_set(&connected, 1);
    printk("BLE connected; enable NUS TX notifications\n");
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
    ARG_UNUSED(conn);
    atomic_clear(&notifications_enabled);
    atomic_clear(&connected);
    printk("BLE disconnected: 0x%02x\n", reason);
    k_work_reschedule(&advertise_work, K_MSEC(250));
}

BT_CONN_CB_DEFINE(connection_callbacks) = {
    .connected = on_connected,
    .disconnected = on_disconnected,
};

static void on_send_enabled(enum bt_nus_send_status status)
{
    bool enabled = status == BT_NUS_SEND_STATUS_ENABLED;
    atomic_set(&notifications_enabled, enabled);
    printk("NUS notifications %s\n", enabled ? "enabled" : "disabled");
}

static void on_received(struct bt_conn *conn, const uint8_t *data, uint16_t len)
{
    ARG_UNUSED(conn);
    printk("BLE RX (%u bytes): %.*s\n", len, (int)len, (const char *)data);
}

static struct bt_nus_cb nus_callbacks = {
    .send_enabled = on_send_enabled,
    .received = on_received,
};

int main(void)
{
    printk("\nHY0020 BLE text test (NUS)\n");
    int err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed: %d\n", err);
        return 0;
    }
    err = bt_nus_init(&nus_callbacks);
    if (err) {
        printk("NUS init failed: %d\n", err);
        return 0;
    }
    k_work_reschedule(&advertise_work, K_NO_WAIT);

    unsigned counter = 0;
    for (;;) {
        if (atomic_get(&connected) && atomic_get(&notifications_enabled)) {
            /* 19 bytes including CRLF, fits the default ATT payload of 20. */
            char message[21];
            int len = snprintk(message, sizeof(message),
                               "Hello HY0020 %04u\r\n", counter);
            if (len > 0 && len <= 20) {
                err = bt_nus_send(NULL, (const uint8_t *)message, (uint16_t)len);
                if (err == 0) {
                    printk("BLE TX: %s", message);
                    counter = (counter + 1u) % 10000u;
                } else {
                    printk("BLE TX failed: %d\n", err);
                }
            }
        }
        k_sleep(K_SECONDS(1));
    }
}
