/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>

#include <bluetooth/services/mds.h>

#include <app_version.h>

LOG_MODULE_REGISTER(nfr_sensor, CONFIG_LOG_DEFAULT_LEVEL);

#define DEVICE_NAME     CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

static uint8_t conn_count;

static const struct bt_data ad[] = {
BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_MDS_VAL),
};

static const struct bt_data sd[] = {
BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static void start_advertising(void)
{
int ret;

ret = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
if (ret == -EALREADY) {
return;
}

if (ret) {
LOG_ERR("BLE advertising failed: %d", ret);
return;
}

LOG_INF("Advertising as %s", DEVICE_NAME);
}

/* Allow any connected device to access the Memfault Diagnostic Service.
 * Data is forwarded to the cloud over HTTPS by the mobile app, so there
 * is no need to gate on BLE security level here.
 */
static bool mds_access_enable(struct bt_conn *conn)
{
ARG_UNUSED(conn);
return true;
}

static const struct bt_mds_cb mds_cb = {
.access_enable = mds_access_enable,
};

static void connected(struct bt_conn *conn, uint8_t err)
{
ARG_UNUSED(conn);

if (err) {
LOG_ERR("BLE connection failed: %u", err);
return;
}

conn_count++;
LOG_INF("BLE connected (active connections: %u)", conn_count);

/* Keep advertising so a second device can also connect. */
if (conn_count < CONFIG_BT_MAX_CONN) {
start_advertising();
}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
ARG_UNUSED(conn);

LOG_INF("BLE disconnected: 0x%02x", reason);

if (conn_count > 0) {
conn_count--;
}

start_advertising();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
.connected    = connected,
.disconnected = disconnected,
};

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
ARG_UNUSED(conn);
LOG_INF("Pairing complete, bonded=%d", bonded);
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
ARG_UNUSED(conn);
LOG_ERR("Pairing failed: %d", reason);
}

static struct bt_conn_auth_cb conn_auth_callbacks;

static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
.pairing_complete = pairing_complete,
.pairing_failed   = pairing_failed,
};

int main(void)
{
int ret;

LOG_INF("Starting NFR sensor %s", APP_VERSION_STRING);

ret = bt_conn_auth_cb_register(&conn_auth_callbacks);
if (ret) {
LOG_ERR("Auth callback registration failed: %d", ret);
return ret;
}

ret = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
if (ret) {
LOG_ERR("Auth info callback registration failed: %d", ret);
return ret;
}

bt_mds_cb_register(&mds_cb);

ret = bt_enable(NULL);
if (ret) {
LOG_ERR("Bluetooth init failed: %d", ret);
return ret;
}

if (IS_ENABLED(CONFIG_SETTINGS)) {
settings_load();
}

start_advertising();

return 0;
}
