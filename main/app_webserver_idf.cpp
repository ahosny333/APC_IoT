#include <string.h>

#include <freertos/FreeRTOS.h>
#include <esp_http_server.h>
#include <freertos/task.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
//#include <esp_wifi.h>
#include <esp_log.h>
#include <WebServer.h>
#include "mbedtls/base64.h"
#include "main.h"
#include "embedded_wifimanager.h"

const char auth_username[15] = "admin";
const char master_key[15] = "12345678";
char auth_password[15] = "";
bool flashUpdateRequest = false;

bool wifi_scan_start;
extern enum wifi_scan_states wifi_scan_status;
extern int scan_task_return;

/* const httpd related values stored in ROM */
const static char http_200_hdr[] = "200 OK";
// const static char http_302_hdr[] = "302 Found";
const static char http_400_hdr[] = "400 Bad Request";
const static char http_404_hdr[] = "404 Not Found";
const static char http_401_hdr[] = "401 UNAUTHORIZED";
// const static char http_503_hdr[] = "503 Service Unavailable";
// const static char http_location_hdr[] = "Location";
const static char http_content_type_html[] = "text/html";
const static char http_content_type_txt[] = "text/plain";
const static char http_content_type_js[] = "text/javascript";
const static char http_content_type_css[] = "text/css";
// const static char http_content_type_json[] = "application/json";
const static char http_content_type_font_woff[] ="application/font-woff";
const static char http_content_type_gz[] ="application/x-gzip";
// const static char http_cache_control_hdr[] = "Cache-Control";
// const static char http_cache_control_no_cache[] = "no-store, no-cache, must-revalidate, max-age=0";
// const static char http_cache_control_cache[] = "public, max-age=31536000";
// const static char http_pragma_hdr[] = "Pragma";
// const static char http_pragma_no_cache[] = "no-cache";

// ota.html  
extern const uint8_t ota_html_start[] asm("_binary_ota_html_start");
extern const uint8_t ota_html_end[] asm("_binary_ota_html_end");

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

/**
 * Decode URL-encoded string in place.
 */
void urldecode(char *str)
{
	unsigned int x;

	for (int i = 0; str[i] != 0; i++) {
		char c = str[i];
		if (c == '+') {
			str[i] = ' ';
		} else if (c == '%') {
			// decode the byte
			sscanf(&str[i + 1], "%02x", &x);
			str[i] = x;

			// shift following chars
			for (int a = i + 3, b = i + 1;; a++, b++) {
				str[b] = str[a]; // move
				if (str[a] == 0) break;
			}
		}
	}
}

static const char *TAG = "auth";
typedef struct {
    char    *username;
    char    *password;
} basic_auth_info_t;

static char *http_auth_basic(const char *username, const char *password)
{
    int out;
    char *user_info = NULL;
    char *digest = NULL;
    size_t n = 0;
    asprintf(&user_info, "%s:%s", username, password);
    if (!user_info) {
        ESP_LOGE(TAG, "No enough memory for user information");
        return NULL;
    }
	mbedtls_base64_encode(NULL, 0, &n, (const unsigned char *)user_info, strlen(user_info));
    //esp_crypto_base64_encode(NULL, 0, &n, (const unsigned char *)user_info, strlen(user_info));

    /* 6: The length of the "Basic " string
     * n: Number of bytes for a base64 encode format
     * 1: Number of bytes for a reserved which be used to fill zero
    */
    digest = (char*)calloc(1, 6 + n + 1);
    if (digest) {
        strcpy(digest, "Basic ");
		mbedtls_base64_encode((unsigned char *)digest + 6, n, (size_t *)&out, (const unsigned char *)user_info, strlen(user_info));
        //esp_crypto_base64_encode((unsigned char *)digest + 6, n, (size_t *)&out, (const unsigned char *)user_info, strlen(user_info));
    }
    free(user_info);
    return digest;
}

/* An HTTP GET handler */
static esp_err_t basic_auth_get_handler(httpd_req_t *req, char *auth_pass)
{
    char *buf = NULL;
    size_t buf_len = 0;
    basic_auth_info_t *basic_auth_info = (basic_auth_info_t *)req->user_ctx;

    buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
    if (buf_len > 1) {
        buf = (char*)calloc(1, buf_len);
        if (!buf) {
            ESP_LOGE(TAG, "No enough memory for basic authorization");
            return ESP_ERR_NO_MEM;
        }

        if (httpd_req_get_hdr_value_str(req, "Authorization", buf, buf_len) == ESP_OK) {
            //ESP_LOGI(TAG, "Found header => Authorization: %s", buf);
        } else {
            ESP_LOGE(TAG, "No auth value received");
        }
        // authenticate using master key
        char *auth_credentials = http_auth_basic(basic_auth_info->username, basic_auth_info->password);
        if (!auth_credentials) {
            ESP_LOGE(TAG, "No enough memory for basic authorization credentials");
            free(buf);
            return ESP_ERR_NO_MEM;
        }

        if (strncmp(auth_credentials, buf, buf_len)==0) {
            //ESP_LOGI(TAG, "Authenticated!");
            free(auth_credentials);
            free(buf);
            return ESP_OK;
        } else {
            ESP_LOGE(TAG, "Not authenticated check with auth pass");
            free(auth_credentials);
        }
        // authenticate using auth pass
        char *auth_credentials_2 = http_auth_basic(basic_auth_info->username, auth_pass);
        if (!auth_credentials_2) {
            ESP_LOGE(TAG, "No enough memory for basic authorization credentials");
            free(buf);
            return ESP_ERR_NO_MEM;
        }

        if (strncmp(auth_credentials_2, buf, buf_len)) {
            ESP_LOGE(TAG, "Not authenticated");      
            httpd_resp_set_status(req, http_401_hdr);
            httpd_resp_set_type(req,"text/plain"); 
            // httpd_resp_set_hdr(req, "Connection", "keep-alive");
            // httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Hello\"");
            httpd_resp_send(req, "Authentication Error.", HTTPD_RESP_USE_STRLEN);
            free(auth_credentials_2);
            free(buf);
            return -1;
        } else {
            //ESP_LOGI(TAG, "Authenticated!");
            free(auth_credentials_2);
            free(buf);
            return ESP_OK;
        }

    } else {
        ESP_LOGE(TAG, "No auth header received");
        httpd_resp_set_status(req, http_401_hdr);
        httpd_resp_set_type(req,"text/plain"); 
        // httpd_resp_set_hdr(req, "Connection", "keep-alive");
        // httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Hello\"");
        httpd_resp_send(req, "Authentication Error.", HTTPD_RESP_USE_STRLEN);
        return -1;
    }
}


esp_err_t http_server_get_handler(httpd_req_t *req)
{
    wm_activity_callback();
    char text_string[1822];
    // insert static routing code
    if (strcmp(req->uri, "/") == 0)
    {
        httpd_resp_set_status(req, http_200_hdr);
        httpd_resp_set_type(req, http_content_type_html);
        httpd_resp_set_hdr(req, "Content-Encoding", "html");
        httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);
    }
    // else if (strcmp(req->uri, "/favicon.1e51c1d1.ico") == 0)
    // {
    //     httpd_resp_set_status(req, http_200_hdr);
    //     httpd_resp_set_type(req, "image/x-icon");
    //     httpd_resp_set_hdr(req, "Cache-Control", "max-age=31536000, immutable");
    //     httpd_resp_send(req, (const char *)favicon_1e51c1d1_ico_start, favicon_1e51c1d1_ico_end - favicon_1e51c1d1_ico_start);
    // }
    // else if (strcmp(req->uri, "/main.5d52f4d7.css") == 0)
    // {
    //     httpd_resp_set_status(req, http_200_hdr);
    //     httpd_resp_set_type(req, http_content_type_css);
    //     httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    //     httpd_resp_set_hdr(req, "Cache-Control", "max-age=31536000, immutable");
    //     httpd_resp_send(req, (const char *)main_5d52f4d7_css_gz_start, main_5d52f4d7_css_gz_end - main_5d52f4d7_css_gz_start);
    // }
    // else if (strcmp(req->uri, "/main.e7514866.js") == 0)
    // {
    //     httpd_resp_set_status(req, http_200_hdr);
    //     httpd_resp_set_type(req, http_content_type_js);
    //     httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    //     httpd_resp_set_hdr(req, "Cache-Control", "max-age=31536000, immutable");
    //     httpd_resp_send(req, (const char *)main_e7514866_js_gz_start, main_e7514866_js_gz_end - main_e7514866_js_gz_start);
    // }
    // else if (strcmp(req->uri, "/Mont_med.26426d0d.woff2") == 0)
    // {
    //     httpd_resp_set_status(req, http_200_hdr);
    //     httpd_resp_set_type(req, http_content_type_font_woff);
    //     httpd_resp_set_hdr(req, "Cache-Control", "max-age=31536000, immutable");
    //     httpd_resp_send(req, (const char *)Mont_med_26426d0d_woff2_start, Mont_med_26426d0d_woff2_end - Mont_med_26426d0d_woff2_start);
    // }
    // else if (strcmp(req->uri, "/style.9128e8e2.css") == 0)
    // {
    //     httpd_resp_set_status(req, http_200_hdr);
    //     httpd_resp_set_type(req, http_content_type_css);
    //     httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    //     httpd_resp_set_hdr(req, "Cache-Control", "max-age=31536000, immutable");
    //     httpd_resp_send(req, (const char *)style_9128e8e2_css_gz_start, style_9128e8e2_css_gz_end - style_9128e8e2_css_gz_start);
    // }

    // else if(strstr(req->uri, "/settxt.cgi"))
    // {
    //     if(strlen(auth_password)) 
    //     {
    //         if(basic_auth_get_handler(req,auth_password)!= ESP_OK)
    //         {
    //             return ESP_FAIL;
    //         }
    //     }
    //     else
    //     {
    //         if(basic_auth_get_handler(req,default_auth_password)!= ESP_OK)
    //         {
    //             return ESP_FAIL;
    //         }
    //     }
    //     bool error = false;
    //     bool mqttFlag = false;
    //     bool bECloudFlag = false;
    //     char*  buf;
    //     size_t buf_len;
    //     bool mb_gw_flag=false;
    //     buf_len = httpd_req_get_url_query_len(req) + 1;
    //     if (buf_len > 1) {
    //         char ipAddressTemp[16];
    //         char temp_param[100];
    //         char byte_temp[2],mqtt_byte_temp[3],rs_temp1[4],rs_temp2[4],rs_temp3[4],rs_temp4[4];
    //         char calib_pass[6],calib_ch[4],calib_val[10],lc_val[7];
    //         buf = (char*)malloc(buf_len);
    //         if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
    //             urldecode(buf);
    //             //ESP_LOGI(TAG, "Found URL query => %s", buf);
    //             /* Get value of expected key from query string */
    //             if (httpd_query_key_value(buf, "ipAddressUser", ipAddressTemp, sizeof(ipAddressTemp)) == ESP_OK) {
    //                 //ESP_LOGI(TAG, "Found URL query parameter => ipAddressUser=%s", ipAddressUser);
    //                 if (checkIPaddressValidity(ipAddressTemp)) {
    //                     memset(ipAddress, 0, sizeof(ipAddress));
    //                     strcpy(ipAddress, ipAddressTemp);
    //                     memset(ipAddressTemp, 0, sizeof(ipAddressTemp));
    //                     flashUpdateRequest = true;
    //                     } else
    //                     {
    //                         error = true;
    //                     }
    //             }
    //             if (httpd_query_key_value(buf, "nmAddressUser", ipAddressTemp, sizeof(ipAddressTemp)) == ESP_OK) {
    //                 if (checkIPaddressValidity(ipAddressTemp)) {
    //                     memset(netMaskAddress, 0, sizeof(netMaskAddress));
    //                     strcpy(netMaskAddress, ipAddressTemp);
    //                     memset(ipAddressTemp, 0, sizeof(ipAddressTemp));
    //                     flashUpdateRequest = true;
    //                     } else
    //                     error = true;
    //             }
                
    //         }
    //         free(buf);

    //     }
    // }
    if (strcmp(req->uri, "/test") == 0)
    {
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");  
        httpd_resp_set_hdr(req, "Access-Control-Allow-Credentials", "true");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Authorization");
        httpd_resp_set_status(req, http_200_hdr);
        httpd_resp_set_type(req,http_content_type_txt);
        httpd_resp_send(req, "ok", HTTPD_RESP_USE_STRLEN);
    }
    // else if(strstr(req->uri, "/settings"))
    // {

    //     // if(basic_auth_get_handler(req,auth_password)!= ESP_OK)
    //     // {
    //     //     return ESP_FAIL;
    //     // }
       
    //     bool error = false;
    //     char*  buf;
    //     size_t buf_len;
    //     buf_len = httpd_req_get_url_query_len(req) + 1;
    //     if (buf_len > 1) {
    //         char word_temp[6];
    //         buf = (char*)malloc(buf_len);
    //         if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
    //             urldecode(buf);
    //             //ESP_LOGI(TAG, "Found URL query => %s", buf);
    //             /* Get value of expected key from query string */
    //             if (httpd_query_key_value(buf, "TIME_TO_SLEEP", word_temp, sizeof(word_temp)) == ESP_OK) {
    //                 if( atoi(word_temp) <= 120 && atoi(word_temp) > 0 ){
    //                 TIME_TO_SLEEP = atoi(word_temp) * 60;
    //                 error = false;
    //                 flashUpdateRequest = true;
    //                 }
    //                 else
    //                     error = true;
    //                  memset(word_temp, 0, sizeof(word_temp));
    //             }
                
    //         }
    //         free(buf);

    //     }
    //     httpd_resp_set_hdr(req, "Access-Control-Allow-Credentials", "true");
    //     httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Authorization");
    //     httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    //     if (!error)
    //     {
    //         httpd_resp_set_status(req, http_200_hdr);
    //         httpd_resp_set_type(req,http_content_type_txt);
    //         httpd_resp_send(req, "Log in OK!", HTTPD_RESP_USE_STRLEN);
    //     }
    //     else{
    //         httpd_resp_set_status(req, http_400_hdr);
    //         httpd_resp_set_type(req,http_content_type_txt);
    //         httpd_resp_send(req, "error", HTTPD_RESP_USE_STRLEN);
    //     }
    // }

    // else if (strcmp(req->uri, "/get_settings") == 0)
    // {
    //     //char text_string[200];
    //     memset(text_string, 0, sizeof(text_string));
    //     snprintf(text_string, sizeof(text_string),
    //     "{"
    //     "\"NODE_ID\":\"%d\","
    //     "\"firmware_version\":\"%s\","
    //     "\"time_to_sleep\":%d,\"sd_card\":%d}",
    //     NODE_ID, FIRMWARE_VERSION,TIME_TO_SLEEP,sd_exist);
    //     httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");  
    //     httpd_resp_set_status(req, http_200_hdr);
    //     httpd_resp_set_type(req,http_content_type_txt);
    //     httpd_resp_send(req, text_string, HTTPD_RESP_USE_STRLEN);
    // }
    // else if (strcmp(req->uri, "/get_readings") == 0)
    // {
    //     //char text_string[200];
    //     memset(text_string, 0, sizeof(text_string));
    //     jsonStr.toCharArray(text_string, 1822);
    //     httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");  
    //     httpd_resp_set_status(req, http_200_hdr);
    //     httpd_resp_set_type(req,http_content_type_txt);
    //     httpd_resp_send(req, text_string, HTTPD_RESP_USE_STRLEN);
    // }
    // else if (strstr(req->uri, "/sync"))
    // {
    //     memset(text_string, 0, sizeof(text_string));          
    //     char*  buf;
    //     size_t buf_len;
    //     char temp_v[16];
    //     buf_len = httpd_req_get_url_query_len(req) + 1;
    //     if (buf_len > 1) {
    //         buf = (char*)malloc(buf_len);
    //         if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
    //             //urldecode(buf);
    //             //ESP_LOGI(TAG, "Found URL query => %s", buf);
    //             /* Get value of expected key from query string */
    //             if (httpd_query_key_value(buf, "ts", temp_v, sizeof(temp_v)) == ESP_OK) {
    //                 //ESP_LOGI(TAG, "Found URL query => %s", buf);
    //                 rtc.setTime(atoi(temp_v));
    //                 rtc_done = true;
    //                 flashUpdateRequest = true;
    //                 memset(text_string, 0, sizeof(text_string));
    //                 sprintf(text_string,"ok");
    //             }
    //         }
    //         else
    //         {
    //             sprintf(text_string,"error");
    //             rtc_done = false;
    //         }
    //         free(buf);
    //     }
    //     else
    //     {
    //         sprintf(text_string,"error");
    //         rtc_done = false;
    //     }

    //     httpd_resp_set_status(req, http_200_hdr);
    //     httpd_resp_set_type(req,http_content_type_txt);
    //     httpd_resp_send(req,text_string, HTTPD_RESP_USE_STRLEN);
    // }

    else if (strcmp(req->uri, "/scanNetwork") == 0)
    {
        memset(text_string, 0, sizeof(text_string));
        switch (wifi_scan_status) 
        {
            case INIT:
                wifi_scan_status = STARTED;
                wifi_scan_start = true;
                sprintf(text_string, "{\"networks\":[],\"status\":0}");
                break;
            case STARTED:
                sprintf(text_string, "{\"networks\":[],\"status\":0}");
                break;
            case FINISHED:
                if(scan_task_return < 0)
                {
                    sprintf(text_string, "{\"networks\":[],\"status\":-1}");
                }
                else if(scan_task_return == 0)
                {
                    sprintf(text_string, "{\"networks\":[],\"status\":1}");
                }
                else
                {
                    if(scan_task_return > 1)
                    {
                        char *ptr_res = text_string;
                        sprintf(ptr_res, "{\"networks\":[");
                        ptr_res += strlen(ptr_res);

                        for (int i = 0; i < scan_task_return-1; ++i) {
                            sprintf(ptr_res, "[\"%s\",%d,%d],",WiFi.SSID(i).c_str(),WiFi.RSSI(i),(WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?0:1); 
                            ptr_res += strlen(ptr_res);              
                        }

                        sprintf(ptr_res,"[\"%s\",%d,%d]],\"status\":1}",WiFi.SSID(scan_task_return-1).c_str(),WiFi.RSSI(scan_task_return-1),(WiFi.encryptionType(scan_task_return-1) == WIFI_AUTH_OPEN)?0:1);
                    }
                    else
                    {
                        sprintf(text_string, "{\"networks\":[[\"%s\",%d,%d]],\"status\":1}",WiFi.SSID(0).c_str(),WiFi.RSSI(0),(WiFi.encryptionType(0) == WIFI_AUTH_OPEN)?0:1);
                    }
                    
                }
                WiFi.scanDelete();
                wifi_scan_status = INIT;
                break;

        }
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");  
        httpd_resp_set_status(req, http_200_hdr);
        httpd_resp_set_type(req,http_content_type_txt);
        httpd_resp_send(req, text_string, HTTPD_RESP_USE_STRLEN);
    }




    else if (strcmp(req->uri, "/ota.html") == 0)
    {      
        httpd_resp_set_status(req, http_200_hdr);
        httpd_resp_set_type(req,http_content_type_html);
        httpd_resp_set_hdr(req, "Access-Control-Allow-Credentials", "true");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Authorization");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, (const char *)ota_html_start, ota_html_end - ota_html_start);
    }
    return ESP_OK;
}



/*
 * Handle OTA file upload
 */
esp_err_t update_post_handler(httpd_req_t *req)
{
    wm_activity_callback();
	char buf[1000];
	esp_ota_handle_t ota_handle;
	int remaining = req->content_len;

	const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
	ESP_ERROR_CHECK(esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle));

	while (remaining > 0) {
		int recv_len = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));

		// Timeout Error: Just retry
		if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
			continue;

		// Serious Error: Abort OTA
		} else if (recv_len <= 0) {
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
			return ESP_FAIL;
		}

		// Successful Upload: Flash firmware chunk
		if (esp_ota_write(ota_handle, (const void *)buf, recv_len) != ESP_OK) {
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash Error");
			return ESP_FAIL;
		}

		remaining -= recv_len;
	}

	// Validate and switch to new OTA image and reboot
	if (esp_ota_end(ota_handle) != ESP_OK || esp_ota_set_boot_partition(ota_partition) != ESP_OK) {
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Validation / Activation Error");
			return ESP_FAIL;
	}

	httpd_resp_sendstr(req, "Firmware update complete, rebooting now!");
	vTaskDelay(500 / portTICK_PERIOD_MS);
	esp_restart();

	return ESP_OK;
}

esp_err_t http_server_options_handler(httpd_req_t *req) {
    wm_activity_callback();
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");  // Adjust origin as needed
    httpd_resp_set_hdr(req, "Access-Control-Allow-Credentials", "true");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Authorization");  // Customize allowed headers
    httpd_resp_set_status(req, http_200_hdr);
    httpd_resp_set_type(req,http_content_type_txt);
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

httpd_uri_t http_server_get_request = {
	.uri	  = "/*",
	.method   = HTTP_GET,
	.handler  = http_server_get_handler,
	.user_ctx = NULL
};

// httpd_uri_t pw_change_post_request = {
// 	.uri	  = "/change_password",
// 	.method   = HTTP_POST,
// 	.handler  = pw_change_post_handler,
// 	.user_ctx = NULL
// };

httpd_uri_t http_server_options_request = {
    .uri       = "/*",  // Match any path
    .method    = HTTP_OPTIONS,
    .handler   = http_server_options_handler,  // Your OPTIONS handler
    .user_ctx  = NULL
};

httpd_uri_t update_post = {
	.uri	  = "/update",
	.method   = HTTP_POST,
	.handler  = update_post_handler,
	.user_ctx = NULL
};

static esp_err_t http_server_init(void)
{
	static httpd_handle_t http_server = NULL;

	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	/* this is an important option that isn't set up by default.
         * we could register all URLs one by one, but this would not work while the fake DNS is active */
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.stack_size=10000;
    config.lru_purge_enable=true;
    config.max_open_sockets=12;

	if (httpd_start(&http_server, &config) == ESP_OK) {

        basic_auth_info_t *basic_auth_info = (basic_auth_info_t *)calloc(1, sizeof(basic_auth_info_t));
        if (basic_auth_info) {
            basic_auth_info->username = (char*)auth_username;//"admin";
            basic_auth_info->password = (char*)master_key;//"12345678";

            http_server_get_request.user_ctx = basic_auth_info;
            httpd_register_uri_handler(http_server, &http_server_get_request);
            // pw_change_post_request.user_ctx = basic_auth_info;
            // httpd_register_uri_handler(http_server, &pw_change_post_request);
            http_server_options_request.user_ctx = basic_auth_info;
            httpd_register_uri_handler(http_server, &http_server_options_request);
        }
            httpd_register_uri_handler(http_server, &update_post);
	}

	return http_server == NULL ? ESP_FAIL : ESP_OK;
}


void webserver_task(void) {

	ESP_ERROR_CHECK(http_server_init());
}