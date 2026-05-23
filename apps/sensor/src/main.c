/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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

/* Holds the connection that has been granted MDS access (requires BT_SECURITY_L2). */
static struct bt_conn *mds_conn;
static uint8_t conn_count;

static struct k_work adv_work;

static const struct bt_data ad[] = {
BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_MDS_VAL),
};

static const struct bt_data sd[] = {
BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static void adv_work_handler(struct k_work *work)
{
int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));

if (err && err != -EALREADY) {
LOG_ERR("BLE advertising failed: %d", err);
return;
}

LOG_INF("Advertising as %s", DEVICE_NAME);
}

static void advertising_start(void)
{
k_work_submit(&adv_work);
}

/* MDS access requires a bonded, encrypted connection (BT_SECURITY_L2). */
static bool mds_access_enable(struct bt_conn *conn)
{
return mds_conn && (conn == mds_conn);
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
	LOG_INF("BLE connected (active: %u)", conn_count);

	err = bt_conn_set_security(conn, BT_SECURITY_L2);
	if (err) {
		LOG_WRN("BLE security request failed: %d", err);
	}

	/* Keep advertising so a second device can connect (gateway + phone). */
	if (conn_count < CONFIG_BT_MAX_CONN) {
advertising_start();
}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
LOG_INF("BLE disconnected: 0x%02x", reason);

if (conn_count > 0) {
conn_count--;
}

if (mds_conn == conn) {
bt_conn_unref(mds_conn);
mds_conn = NULL;
}
/* Advertising restarts in recycled_cb once connection object is freed. */
}

/* Called when the connection object is truly freed — safe to re-advertise. */
static void recycled_cb(void)
{
advertising_start();
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
     enum bt_security_err err)
{
if (err == BT_SECURITY_ERR_PIN_OR_KEY_MISSING) {
/* Bond info mismatch (device reflashed, phone has stale LTK).
 * Remove the local stale bond — the phone must also forget the
 * device in Bluetooth settings and re-pair.
 */
bt_unpair(BT_ID_DEFAULT, bt_conn_get_dst(conn));
LOG_WRN("Stale bond removed — forget device on phone and re-pair");
return;
}

if (err) {
LOG_ERR("BLE security failed: level %u err %d", level, err);
return;
}

LOG_INF("Security level %u", level);

if (level >= BT_SECURITY_L2 && !mds_conn) {
mds_conn = bt_conn_ref(conn);
LOG_INF("MDS enabled for bonded connection");
}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
.connected        = connected,
.disconnected     = disconnected,
.security_changed = security_changed,
.recycled         = recycled_cb,
};

static void auth_cancel(struct bt_conn *conn)
{
ARG_UNUSED(conn);
LOG_WRN("Pairing cancelled");
}

static struct bt_conn_auth_cb conn_auth_callbacks = {
.cancel = auth_cancel,
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

static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
.pairing_complete = pairing_complete,
.pairing_failed   = pairing_failed,
};

int main(void)
{
int err;

LOG_INF("Starting NFR sensor %s", APP_VERSION_STRING);

err = bt_mds_cb_register(&mds_cb);
if (err) {
LOG_ERR("MDS callback registration failed: %d", err);
return err;
}

err = bt_enable(NULL);
if (err) {
LOG_ERR("Bluetooth init failed: %d", err);
return err;
}

err = bt_conn_auth_cb_register(&conn_auth_callbacks);
if (err) {
LOG_ERR("Auth callback registration failed: %d", err);
return err;
}

err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
if (err) {
LOG_ERR("Auth info callback registration failed: %d", err);
return err;
}

if (IS_ENABLED(CONFIG_SETTINGS)) {
settings_load();
}

k_work_init(&adv_work, adv_work_handler);
advertising_start();

return 0;
}
