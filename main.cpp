#include <NetworkManager.h>
#include <glib.h>
#include <iostream>
#include <unordered_set>
#include <string>

void activatedCallback(GObject *client, GAsyncResult *result, gpointer user_data)
{
    GError *error = nullptr;
    NMActiveConnection *active = nm_client_activate_connection_finish(NM_CLIENT(client), result, &error);

    if (error) {
        g_print("Error activating connection: %s\n", error->message);
        g_error_free(error);
    } else {
        g_print("Successfully connected!\n");
        g_object_unref(active);
    }

    g_main_loop_quit((GMainLoop *)user_data);
}

void addedCallback(GObject *client, GAsyncResult *result, gpointer user_data)
{
    GError *error = nullptr;
    NMRemoteConnection *remote = nm_client_add_connection_finish(NM_CLIENT(client), result, &error);

    if (error) {
        g_print("Error adding connection: %s", error->message);
        g_error_free(error);
    } else {
        g_print("Added: %s\n", nm_connection_get_path(NM_CONNECTION(remote)));
        nm_client_activate_connection_async(NM_CLIENT(client), NM_CONNECTION(remote), nullptr, nullptr, nullptr, activatedCallback, user_data );
        g_object_unref(remote);
    }

}

void scanCallback(GObject *source, GAsyncResult *result, gpointer user_data) {
    auto *device = (NMDeviceWifi *)NM_DEVICE(source);
    GError *error = nullptr;

    if (nm_device_wifi_request_scan_finish(device, result, &error)) {
        const GPtrArray *networks = nm_device_wifi_get_access_points(device);

        std::unordered_set<std::string> seen;

        for (guint i = 0; i < networks->len; i++) {
            auto *ap = (NMAccessPoint *)g_ptr_array_index(networks, i);
            GBytes *ssid = nm_access_point_get_ssid(ap);

            if (ssid) {
                gsize len;

                const auto *data = (const guint8 *)g_bytes_get_data(ssid, &len);
                g_autofree char *name = nm_utils_ssid_to_utf8(data, len);

                if (!name || !seen.insert(name).second)
                    continue;

                g_print("SSID: %s\n", name);
            }
        }
    }
    else {
        g_print("Could not access NetworkManager: %s", error->message);
        g_error_free(error);
    }

    g_main_loop_quit((GMainLoop *)user_data);

}

void addWifiConnection(NMClient *client, GMainLoop *loop, const char *connID, const char *password) {

    NMConnection *connection = nm_simple_connection_new();
    auto *settings = (NMSettingWireless *)nm_setting_wireless_new();
    auto *security = (NMSettingWirelessSecurity *)nm_setting_wireless_security_new();
    auto *settingConnection = (NMSettingConnection *)nm_setting_connection_new();
    auto *IP4Setting = (NMSettingIP4Config *)nm_setting_ip4_config_new();

    const char *uuid = nm_utils_uuid_generate();
    const GBytes *ssid = g_bytes_new(connID, strlen(connID));

    g_object_set(G_OBJECT(settingConnection), NM_SETTING_CONNECTION_UUID, uuid, NM_SETTING_CONNECTION_ID, connID, NM_SETTING_CONNECTION_TYPE, "802-11-wireless", NULL);
    nm_connection_add_setting(connection, NM_SETTING(settingConnection));

    g_object_set(G_OBJECT(settings), NM_SETTING_WIRELESS_SSID, ssid, NULL);
    nm_connection_add_setting(connection, NM_SETTING(settings));

    g_object_set(G_OBJECT(security), NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "wpa-psk", NM_SETTING_WIRELESS_SECURITY_PSK, password, NULL);
    nm_connection_add_setting(connection, NM_SETTING(security));

    g_object_set(G_OBJECT(IP4Setting), NM_SETTING_IP_CONFIG_METHOD, NM_SETTING_IP4_CONFIG_METHOD_AUTO, NULL);
    nm_connection_add_setting(connection, NM_SETTING(IP4Setting));

    nm_client_add_connection_async(client, connection, TRUE, nullptr, addedCallback, loop);
    g_object_unref(connection);

}

void scanNetworks(NMClient *client, GMainLoop *loop) {

    auto *device = (NMDeviceWifi *)nm_client_get_device_by_iface(client, "wlo1");
    if (!device) {
        g_printerr("No device found for wlo1\n");
        g_main_loop_quit(loop);
        return;
    }

    nm_device_wifi_request_scan_async(device, nullptr, scanCallback, loop);
}

int main() {

    GError *error = nullptr;
    GMainLoop *loop = g_main_loop_new(nullptr, FALSE);

    NMClient *client = nm_client_new(nullptr, &error);

    if (!client) {
        g_message("Could not access NetworkManager: %s", error->message);
        g_error_free(error);
        return 1;
    }

    scanNetworks(client, loop);
    // addWifiConnection(client, loop, "Demo", "Password");

    g_main_loop_run(loop);

    g_main_loop_unref(loop);
    g_object_unref(client);

    return 0;
}