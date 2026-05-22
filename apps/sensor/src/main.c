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

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

/* Number of active connections — sensor supports up to CONFIG_BT_MAX_CONN (2).
 * One slot for the gateway (primary app), one for a phone running nRF Connect
 * (Memfault diagnostics upload). MDS access is gated on BT_SECURITY_L2.
 */
static struct bt_conn *mds_conn;
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

static bool mds_access_enable(struct bt_conn *conn)
{
	return mds_conn == conn;
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
	LOG_INF("BLE disconnected: 0x%02x", reason);

	if (conn_count > 0) {
		conn_count--;
	}

	if (mds_conn == conn) {
		bt_conn_unref(mds_conn);
		mds_conn = NULL;
	}

	start_advertising();
}

static void security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
	if (err) {
		LOG_ERR("BLE security failed: level %u err %d", level, err);
		return;
	}

	if (level < BT_SECURITY_L2) {
		return;
	}

	if (mds_conn == NULL) {
		mds_conn = bt_conn_ref(conn);
		LOG_INF("MDS enabled for bonded gateway");
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.security_changed = security_changed,
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
	.pairing_failed = pairing_failed,
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
