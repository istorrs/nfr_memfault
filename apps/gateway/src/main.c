/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/gatt.h>

#include <bluetooth/services/mds.h>

#include <app_version.h>

LOG_MODULE_REGISTER(nfr_gateway, CONFIG_LOG_DEFAULT_LEVEL);

/*
 * TODO: Implement MDS GATT client.
 * After bonding with the sensor, discover the Memfault Diagnostic Service,
 * subscribe to BT_UUID_MDS_DATA_EXPORT notifications, collect chunks, and
 * forward them to Memfault Cloud (e.g. via UART to a host-side script or a
 * future Wi-Fi/LTE transport).
 */

static struct bt_conn *sensor_conn;

/* Match advertising payloads that include the MDS service UUID. */
static const uint8_t mds_uuid_bytes[] = { BT_UUID_MDS_VAL };

static bool find_mds_uuid(struct bt_data *data, void *user_data)
{
	bool *found = user_data;

	if (data->type != BT_DATA_UUID128_ALL && data->type != BT_DATA_UUID128_SOME) {
		return true;
	}

	for (uint8_t i = 0; i + 16 <= data->data_len; i += 16) {
		if (memcmp(&data->data[i], mds_uuid_bytes, 16) == 0) {
			*found = true;
			return false;
		}
	}

	return true;
}

static void scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf)
{
	bool found = false;

	if (sensor_conn != NULL) {
		return;
	}

	bt_data_parse(buf, find_mds_uuid, &found);

	if (!found) {
		return;
	}

	LOG_INF("Found sensor, connecting...");
	bt_le_scan_stop();

	int err = bt_conn_le_create(info->addr, BT_CONN_LE_CREATE_CONN,
				    BT_LE_CONN_PARAM_DEFAULT, &sensor_conn);
	if (err) {
		LOG_ERR("Connect failed: %d — restarting scan", err);
		bt_le_scan_start(BT_LE_SCAN_PASSIVE, NULL);
	}
}

static struct bt_le_scan_cb scan_callbacks = {
	.recv = scan_recv,
};

static void start_scan(void)
{
	int err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, NULL);

	if (err) {
		LOG_ERR("Scan start failed: %d", err);
		return;
	}

	LOG_INF("Scanning for NFR sensor (MDS UUID)...");
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("Connection failed: %u", err);
		bt_conn_unref(sensor_conn);
		sensor_conn = NULL;
		start_scan();
		return;
	}

	LOG_INF("Connected to sensor, requesting security...");
	bt_conn_set_security(conn, BT_SECURITY_L2);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected: 0x%02x", reason);
	bt_conn_unref(sensor_conn);
	sensor_conn = NULL;
	start_scan();
}

static void security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
	ARG_UNUSED(conn);

	if (err) {
		LOG_ERR("Security failed: level %u err %d", level, err);
		return;
	}

	LOG_INF("Bonded with sensor (security level %u) — MDS client TODO", level);
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

	LOG_INF("Starting NFR gateway %s", APP_VERSION_STRING);

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

	ret = bt_enable(NULL);
	if (ret) {
		LOG_ERR("Bluetooth init failed: %d", ret);
		return ret;
	}

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	bt_le_scan_cb_register(&scan_callbacks);
	start_scan();

	return 0;
}
