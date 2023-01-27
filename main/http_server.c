/* HTTP Server Example

	 This example code is in the Public Domain (or CC0 licensed, at your option.)

	 Unless required by applicable law or agreed to in writing, this
	 software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	 CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <mbedtls/base64.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_chip_info.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include <driver/gpio.h>

#include "gpio.h"

static const char *TAG = "HTTP";

extern QueueHandle_t xQueueHttp;
extern GPIO_t *gpios;
extern int16_t ngpios;

#define SCRATCH_BUFSIZE (1024)

typedef struct rest_server_context {
	char base_path[ESP_VFS_PATH_MAX + 1]; // Not used in this project
	char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

esp_err_t Text2Button(httpd_req_t *req, char * textFileName, char * type, char * action)
{
	esp_err_t ret = ESP_FAIL;
	struct stat st;
	if (stat(textFileName, &st) != 0) {
		ESP_LOGE(TAG, "[%s] not found", textFileName);
		return ret;
	}

	FILE * fp;
	char buffer[64];
	if((fp=fopen(textFileName, "r"))==NULL){
		ESP_LOGE(TAG, "fopen fail. [%s]", textFileName);
		return ESP_FAIL;
	}

	if (strlen(action) > 0) {
		httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"");
		httpd_resp_sendstr_chunk(req, action);
		httpd_resp_sendstr_chunk(req, "\">");
		httpd_resp_sendstr_chunk(req, "<button type=\"submit\">");
	}


	if (strcmp(type, "jpeg") == 0) {
		httpd_resp_sendstr_chunk(req, "<img src=\"data:image/jpeg;base64,");
	} else if (strcmp(type, "jpg") == 0) {
		httpd_resp_sendstr_chunk(req, "<img src=\"data:image/jpeg;base64,");
	} else if (strcmp(type, "png") == 0) {
		httpd_resp_sendstr_chunk(req, "<img src=\"data:image/png;base64,");
	} else {
		ESP_LOGW(TAG, "file type fail. [%s]", type);
		httpd_resp_sendstr_chunk(req, "<img src=\"data:image/png;base64,");
	}

	while(1) {
		size_t bufferSize = fread(buffer, 1, sizeof(buffer), fp);
		//ESP_LOGD(TAG, "bufferSize=%d", bufferSize);
		if (bufferSize > 0) {
			httpd_resp_send_chunk(req, buffer, bufferSize);
		} else {
			break;
		}
	}
	fclose(fp);
	httpd_resp_sendstr_chunk(req, "\">");

	if (strlen(action) > 0) {
		httpd_resp_sendstr_chunk(req, "</button></form>");
	}
	return ESP_OK;
}

esp_err_t Image2Html(httpd_req_t *req, char * filename, char * type)
{
	FILE * fhtml = fopen(filename, "r");
	if (fhtml == NULL) {
		ESP_LOGE(TAG, "fopen fail. [%s]", filename);
		return ESP_FAIL;
	}else{
		char buffer[64];

		if (strcmp(type, "jpeg") == 0) {
			httpd_resp_sendstr_chunk(req, "<img src=\"data:image/jpeg;base64,");
		} else if (strcmp(type, "jpg") == 0) {
			httpd_resp_sendstr_chunk(req, "<img src=\"data:image/jpeg;base64,");
		} else if (strcmp(type, "png") == 0) {
			httpd_resp_sendstr_chunk(req, "<img src=\"data:image/png;base64,");
		} else {
			ESP_LOGW(TAG, "file type fail. [%s]", type);
			httpd_resp_sendstr_chunk(req, "<img src=\"data:image/png;base64,");
		}
		while(1) {
			size_t bufferSize = fread(buffer, 1, sizeof(buffer), fhtml);
			ESP_LOGD(TAG, "bufferSize=%d", bufferSize);
			if (bufferSize > 0) {
				httpd_resp_send_chunk(req, buffer, bufferSize);
			} else {
				break;
			}
		}
		fclose(fhtml);
		httpd_resp_sendstr_chunk(req, "\">");
	}
	return ESP_OK;
}

/* Handler for root get */
static esp_err_t root_get_handler(httpd_req_t *req)
{
	ESP_LOGD(TAG, "root_get_handler req->uri=[%s]", req->uri);

	for(int index=0;index<ngpios;index++) {
		ESP_LOGD(__FUNCTION__, "gpios[%d] pin=%d mode=%d value=%d",
		index, gpios[index].pin, gpios[index].mode, gpios[index].value);
	}

	/* Send HTML file header */
	httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><body>");

#if 0
	/* Send Image to HTML file */
	//Image2Html(req, "/icons/ESP-IDF.txt", "png");
	Image2Html(req, "/icons/ESP-LOGO.txt", "png");
#endif

	// Start the table
	httpd_resp_sendstr_chunk(req, "<table border=\"1\">");

	// Start th thead
	httpd_resp_sendstr_chunk(req, "<thead><tr><th>GPIO</th><th>CURRENT MODE</th><th>SET MODE</th><th>CURRENT VALUE</th><th>SET VALUE</th></tr></thead>");

	// Start the tbody
	httpd_resp_sendstr_chunk(req, "<tbody>");

	char chunk[32];
	char textFileName[64];
	char action[64];
	for(int index=0;index<ngpios;index++) {
		ESP_LOGD(__FUNCTION__, "gpios[%d] pin=%d mode=%d value=%d",
		index, gpios[index].pin, gpios[index].mode, gpios[index].value);
		httpd_resp_sendstr_chunk(req, "<tr>");
		sprintf(chunk, "<td align=\"center\">%02d</td>", gpios[index].pin);
		httpd_resp_sendstr_chunk(req, chunk);
		if (gpios[index].mode == 1) { // INPUT pin
			strcpy(textFileName, "/icons/box-in-icon.txt");
			// Get current value
			gpios[index].value = gpio_get_level(gpios[index].pin);
			ESP_LOGI(__FUNCTION__, "gpios.pin=%d value=%d", gpios[index].pin, gpios[index].value);
		} else { // OUTPUT pin
			strcpy(textFileName, "/icons/box-out-icon.txt");
		}
		httpd_resp_sendstr_chunk(req, "<td align=\"center\">");
		Text2Button(req, textFileName, "png", "");
		httpd_resp_sendstr_chunk(req, "</td>");

		// change mode
		if (gpios[index].mode == 1) {
			strcpy(textFileName, "/icons/turn-out-icon.txt");
			// POST to [/changeMode/OUTPUT/index]
			sprintf(action, "/changeMode/OUTPUT/%d", index);
		} else {
			strcpy(textFileName, "/icons/turn-in-icon.txt");
			// POST to [/changeMode/INPUT/index]
			sprintf(action, "/changeMode/INPUT/%d", index);
		}
		httpd_resp_sendstr_chunk(req, "<td align=\"center\">");
		Text2Button(req, textFileName, "png", action);
		httpd_resp_sendstr_chunk(req, "</td>");

		// current value
		if (gpios[index].value == 0) {
			strcpy(textFileName, "/icons/current-off-icon.txt");
		} else {
			strcpy(textFileName, "/icons/current-on-icon.txt");
		}
		httpd_resp_sendstr_chunk(req, "<td align=\"center\">");
		Text2Button(req, textFileName, "png", "");
		httpd_resp_sendstr_chunk(req, "</td>");

		// change value
		if (gpios[index].mode == 1) {
			httpd_resp_sendstr_chunk(req, "<td>");
			httpd_resp_sendstr_chunk(req, "<be>");
			httpd_resp_sendstr_chunk(req, "</td>");
		} else {
			if (gpios[index].value == 0) {
				strcpy(textFileName, "/icons/turn-on-icon.txt");
				// POST to [/changeValue/ON/index]
				sprintf(action, "/changeValue/ON/%d", index);
			} else {
				strcpy(textFileName, "/icons/turn-off-icon.txt");
				// POST to [/changeValue/OFF/index]
				sprintf(action, "/changeValue/OFF/%d", index);
			}
			httpd_resp_sendstr_chunk(req, "<td align=\"center\">");
			Text2Button(req, textFileName, "png", action);
			httpd_resp_sendstr_chunk(req, "</td>");
		}
		httpd_resp_sendstr_chunk(req, "</tr>");
	}

	/* Finish the tbody */
	httpd_resp_sendstr_chunk(req, "</tbody>");

	/* Finish the table */
	httpd_resp_sendstr_chunk(req, "</table>");

#if 1
	/* Send Image to HTML file */
	Image2Html(req, "/icons/ESP-LOGO.txt", "png");
#endif

	/* Send remaining chunk of HTML file to complete it */
	httpd_resp_sendstr_chunk(req, "</body></html>");

	/* Send empty chunk to signal HTTP response completion */
	httpd_resp_sendstr_chunk(req, NULL);

	return ESP_OK;
}

/* Handler for change gpio mode */
static esp_err_t change_mode_handler(httpd_req_t *req)
{
	ESP_LOGI(__FUNCTION__, "change_mode_handler req->uri=[%s]", req->uri);
	ESP_LOGI(__FUNCTION__, "change_mode_handler req->uri=[%s]", req->uri+strlen("/changeMode/"));

	GPIO_t gpioBuf;
	long wk;
	gpioBuf.command = CMD_SETMODE;
	if (strncmp(req->uri+strlen("/changeMode/"), "INPUT/", 6) == 0) {
		wk = strtol(req->uri+strlen("/changeMode/INPUT/"), NULL, 10);
		ESP_LOGI(__FUNCTION__, "[to INPUT] index=%ld gpio=%d", wk, gpios[wk].pin);
		gpioBuf.pin = wk;
		gpioBuf.mode = MODE_INPUT;
		if (xQueueSend(xQueueHttp, &gpioBuf, portMAX_DELAY) != pdPASS) {
			ESP_LOGE(__FUNCTION__, "xQueueSend Fail");
		}
	} else if (strncmp(req->uri+strlen("/changeMode/"), "OUTPUT/", 7) == 0) {
		wk = strtol(req->uri+strlen("/changeMode/OUTPUT/"), NULL, 10);
		ESP_LOGI(__FUNCTION__, "[to OUTPUT] index=%ld gpio=%d", wk, gpios[wk].pin);
		gpioBuf.pin = wk;
		gpioBuf.mode = MODE_OUTPUT;
		if (xQueueSend(xQueueHttp, &gpioBuf, portMAX_DELAY) != pdPASS) {
			ESP_LOGE(__FUNCTION__, "xQueueSend Fail");
		}
	}

	/* Redirect onto root to see the updated file list */
	httpd_resp_set_status(req, "303 See Other");
	httpd_resp_set_hdr(req, "Location", "/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
	httpd_resp_set_hdr(req, "Connection", "close");
#endif
	httpd_resp_sendstr(req, "File change mode successfully\n");
	return ESP_OK;
}

/* Handler for change gpio value */
static esp_err_t change_value_handler(httpd_req_t *req)
{
	ESP_LOGI(__FUNCTION__, "change_value_handler req->uri=[%s]", req->uri);
	ESP_LOGI(__FUNCTION__, "change_value_handler req->uri=[%s]", req->uri+strlen("/changeValue/"));

	GPIO_t gpioBuf;
	long wk;
	gpioBuf.command = CMD_SETVALUE;
	if (strncmp(req->uri+strlen("/changeValue/"), "ON/", 3) == 0) {
		wk = strtol(req->uri+strlen("/changeValue/ON/"), NULL, 10);
		ESP_LOGI(__FUNCTION__, "[to ON] index=%ld gpio=%d", wk, gpios[wk].pin);
		gpioBuf.pin = wk;
		gpioBuf.value = 1;
		if (xQueueSend(xQueueHttp, &gpioBuf, portMAX_DELAY) != pdPASS) {
			ESP_LOGE(__FUNCTION__, "xQueueSend Fail");
		}
	} else if (strncmp(req->uri+strlen("/changeValue/"), "OFF/", 4) == 0) {
		wk = strtol(req->uri+strlen("/changeValue/OFF/"), NULL, 10);
		ESP_LOGI(__FUNCTION__, "[to OFF] index=%ld gpio=%d", wk, gpios[wk].pin);
		gpioBuf.pin = wk;
		gpioBuf.value = 0;
		if (xQueueSend(xQueueHttp, &gpioBuf, portMAX_DELAY) != pdPASS) {
			ESP_LOGE(__FUNCTION__, "xQueueSend Fail");
		}
	}

	/* Redirect onto root to see the updated file list */
	httpd_resp_set_status(req, "303 See Other");
	httpd_resp_set_hdr(req, "Location", "/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
	httpd_resp_set_hdr(req, "Connection", "close");
#endif
	httpd_resp_sendstr(req, "File change value successfully\n");
	return ESP_OK;
}

/* Handler for getting system information */
static esp_err_t system_info_get_handler(httpd_req_t *req)
{
	ESP_LOGI(__FUNCTION__, "system_info_get_handler req->uri=[%s]", req->uri);
	httpd_resp_set_type(req, "application/json");
	cJSON *root = cJSON_CreateObject();
	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);
	cJSON_AddStringToObject(root, "version", IDF_VER);
	cJSON_AddNumberToObject(root, "cores", chip_info.cores);
	//const char *sys_info = cJSON_Print(root);
	char *sys_info = cJSON_Print(root);
	httpd_resp_sendstr(req, sys_info);
	// Buffers returned by cJSON_Print must be freed by the caller.
	// Please use the proper API (cJSON_free) rather than directly calling stdlib free.
	cJSON_free(sys_info);
	cJSON_Delete(root);
	return ESP_OK;
}

// Create array
cJSON *Create_array_of_anything(cJSON **objects,int array_num)
{
	cJSON *prev = 0;
	cJSON *root;
	root = cJSON_CreateArray();
	for (int i=0;i<array_num;i++) {
		if (!i)	{
			root->child=objects[i];
		} else {
			prev->next=objects[i];
			objects[i]->prev=prev;
		}
		prev=objects[i];
	}
	return root;
}


/* Handler for getting gpio infomation */
static esp_err_t gpio_info_get_handler(httpd_req_t *req)
{
	ESP_LOGI(__FUNCTION__, "req->uri=[%s]", req->uri);
	for(int index=0;index<ngpios;index++) {
		ESP_LOGI(__FUNCTION__, "gpios[%d] pin=%d mode=%d value=%d",
		index, gpios[index].pin, gpios[index].mode, gpios[index].value);
		if (gpios[index].mode == 1) { // INPUT pin
			// Get current value
			gpios[index].value = gpio_get_level(gpios[index].pin);
			ESP_LOGI(__FUNCTION__, "gpios.pin=%d value=%d", gpios[index].pin, gpios[index].value);
		}
	}

	httpd_resp_set_type(req, "application/json");

	int array_num = ngpios;
	//cJSON *objects[4];
	cJSON **objects = NULL;
	objects = (cJSON **)calloc(array_num, sizeof(cJSON *));
	if (objects == NULL) {
		ESP_LOGE(__FUNCTION__, "calloc fail");
	}

	for(int i=0;i<array_num;i++) {
		objects[i] = cJSON_CreateObject();
	}

	cJSON *root;
	root = Create_array_of_anything(objects, array_num);

	for(int index=0;index<ngpios;index++) {
		cJSON_AddNumberToObject(objects[index], "id", index);
		cJSON_AddNumberToObject(objects[index], "gpio", gpios[index].pin);
		if (gpios[index].mode == MODE_INPUT) {
			cJSON_AddStringToObject(objects[index], "mode", "INPUT");
		} else {
			cJSON_AddStringToObject(objects[index], "mode", "OUTPUT");
		}
		cJSON_AddNumberToObject(objects[index], "value", gpios[index].value);
	}
	//const char *gpio_info = cJSON_Print(root);
	char *gpio_info = cJSON_Print(root);
	ESP_LOGD(__FUNCTION__, "gpio_info\n%s",gpio_info);
	httpd_resp_sendstr(req, gpio_info);
	// Buffers returned by cJSON_Print must be freed by the caller.
	// Please use the proper API (cJSON_free) rather than directly calling stdlib free.
	cJSON_free(gpio_info);
	cJSON_Delete(root);
	free(objects);
	return ESP_OK;
}

/* Handler for setting gpio mode */
static esp_err_t gpio_mode_set_handler(httpd_req_t *req)
{
	ESP_LOGI(__FUNCTION__, "req->uri=[%s]", req->uri);
	int total_len = req->content_len;
	int cur_len = 0;
	char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
	int received = 0;
	if (total_len >= SCRATCH_BUFSIZE) {
		/* Respond with 500 Internal Server Error */
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
		return ESP_FAIL;
	}
	while (cur_len < total_len) {
		received = httpd_req_recv(req, buf + cur_len, total_len);
		if (received <= 0) {
			/* Respond with 500 Internal Server Error */
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
			return ESP_FAIL;
		}
		cur_len += received;
	}
	buf[total_len] = '\0';
	ESP_LOGI(__FUNCTION__, "buf=[%s]", buf);

	bool parse = true;
	cJSON *root = cJSON_Parse(buf);

	// Search gpio item
	int gpio = 0;
	cJSON* state = cJSON_GetObjectItem(root, "gpio");
	if (state) {
		gpio = cJSON_GetObjectItem(root, "gpio")->valueint;
	} else {
		ESP_LOGE(__FUNCTION__, "gpio item not found");
		parse = false;
	}

	// Search mode item
	char mode[12];
	state = cJSON_GetObjectItem(root, "mode");
	if (state) {
		//mode = cJSON_GetObjectItem(root,"mode")->valuestring;
		strcpy(mode, cJSON_GetObjectItem(root,"mode")->valuestring);
		ESP_LOGI(__FUNCTION__, "mode=[%s]", mode);
		//if (value != 0 && value != 1) {
		if (strcmp(mode, "INPUT") != 0 && strcmp(mode, "OUTPUT") != 0 ) {
			ESP_LOGE(__FUNCTION__, "mode item not correct");
			parse = false;
		}
	} else {
		ESP_LOGE(__FUNCTION__, "mode item not found");
		parse = false;
	}

	cJSON_Delete(root);
	if (parse) {
		ESP_LOGI(__FUNCTION__, "gpio_mode_set_handler gpio = %d, mode = %s", gpio, mode);
		bool isMatch = false;
		for(int index=0;index<ngpios;index++) {
			ESP_LOGI(__FUNCTION__, "gpios[%d] pin=%d mode=%d value=%d",
			index, gpios[index].pin, gpios[index].mode, gpios[index].value);
			if (gpios[index].pin == gpio) {
				isMatch = true;
				GPIO_t gpioBuf;
				gpioBuf.command = CMD_SETMODE;
				gpioBuf.pin = index;
				if (strcmp(mode, "INPUT") == 0) {
					ESP_LOGI(__FUNCTION__, "[to INPUT] index=%d gpio=%d", index, gpios[index].pin);
					gpioBuf.mode = MODE_INPUT;
				} else {
					ESP_LOGI(__FUNCTION__, "[to OUTPUT] index=%d gpio=%d", index, gpios[index].pin);
					gpioBuf.mode = MODE_OUTPUT;
				}
				if (xQueueSend(xQueueHttp, &gpioBuf, portMAX_DELAY) != pdPASS) {
					ESP_LOGE(__FUNCTION__, "xQueueSend Fail");
				}
				httpd_resp_sendstr(req, "GPIO mode set successfully\n");
				break;
			} // end if
		} // end for
		if (isMatch == false) {
			ESP_LOGE(__FUNCTION__, "Not found gpio in csv");
			httpd_resp_sendstr(req, "Not found gpio in csv\n");
		}
		
	} else {
		ESP_LOGE(__FUNCTION__, "Request parameter not correct [%s]", buf);
		httpd_resp_sendstr(req, "Request parameter not correct\n");
	}
	return ESP_OK;
}

/* Handler for setting gpio value */
static esp_err_t gpio_value_set_handler(httpd_req_t *req)
{
	ESP_LOGI(__FUNCTION__, "req->uri=[%s]", req->uri);
	int total_len = req->content_len;
	int cur_len = 0;
	char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
	int received = 0;
	if (total_len >= SCRATCH_BUFSIZE) {
		/* Respond with 500 Internal Server Error */
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
		return ESP_FAIL;
	}
	while (cur_len < total_len) {
		received = httpd_req_recv(req, buf + cur_len, total_len);
		if (received <= 0) {
			/* Respond with 500 Internal Server Error */
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
			return ESP_FAIL;
		}
		cur_len += received;
	}
	buf[total_len] = '\0';
	ESP_LOGI(__FUNCTION__, "buf=[%s]", buf);

	bool parse = true;
	cJSON *root = cJSON_Parse(buf);

	// Search gpio item
	int gpio = 0;
	cJSON* state = cJSON_GetObjectItem(root, "gpio");
	if (state) {
		gpio = cJSON_GetObjectItem(root, "gpio")->valueint;
	} else {
		ESP_LOGE(__FUNCTION__, "gpio item not found");
		parse = false;
	}

	// Search value item
	int value = 0;
	state = cJSON_GetObjectItem(root, "value");
	if (state) {
		value = cJSON_GetObjectItem(root, "value")->valueint;
		if (value != 0 && value != 1) {
			ESP_LOGE(__FUNCTION__, "value item not correct");
			parse = false;
		}
	} else {
		ESP_LOGE(__FUNCTION__, "value item not found");
		parse = false;
	}

	cJSON_Delete(root);
	if (parse) {
		ESP_LOGI(__FUNCTION__, "gpio_value_set_handler gpio = %d, value = %d", gpio, value);
		bool isMatch = false;
		for(int index=0;index<ngpios;index++) {
			ESP_LOGI(__FUNCTION__, "gpios[%d] pin=%d mode=%d value=%d",
			index, gpios[index].pin, gpios[index].mode, gpios[index].value);
			if (gpios[index].pin == gpio) {
				isMatch = true;
				if (gpios[index].mode == MODE_INPUT) {
					ESP_LOGE(__FUNCTION__, "GPIO%d is for INPUT", gpio);
					httpd_resp_sendstr(req, "GPIO is for INPUT\n");
				} else {
					GPIO_t gpioBuf;
					gpioBuf.command = CMD_SETVALUE;
					gpioBuf.pin = index;
					gpioBuf.value = value;
					if (value == 0) {
						ESP_LOGI(__FUNCTION__, "[to OFF] index=%d gpio=%d", index, gpios[index].pin);
					} else {
						ESP_LOGI(__FUNCTION__, "[to ON] index=%d gpio=%d", index, gpios[index].pin);
					}
					if (xQueueSend(xQueueHttp, &gpioBuf, portMAX_DELAY) != pdPASS) {
						ESP_LOGE(__FUNCTION__, "xQueueSend Fail");
					}
					httpd_resp_sendstr(req, "GPIO value set successfully\n");
				}
				break;
			} // end if
		} // end for
		if (isMatch == false) {
			ESP_LOGE(__FUNCTION__, "Not found gpio in csv");
			httpd_resp_sendstr(req, "Not found gpio in csv\n");
		}
		
	} else {
		ESP_LOGE(__FUNCTION__, "Request parameter not correct [%s]", buf);
		httpd_resp_sendstr(req, "Request parameter not correct\n");
	}
	return ESP_OK;
}

/* favicon get handler */
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
	ESP_LOGI(__FUNCTION__, "favicon_get_handler req->uri=[%s]", req->uri);
	return ESP_OK;
}

/* Function to start the file server */
esp_err_t start_server(const char *base_path, int port)
{
	rest_server_context_t *rest_context = calloc(1, sizeof(rest_server_context_t));
	if (rest_context == NULL) {
		ESP_LOGE(__FUNCTION__, "No memory for rest context");
		while(1) { vTaskDelay(1); }
	}

	httpd_handle_t server = NULL;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.server_port = port;

	/* Use the URI wildcard matching function in order to
	 * allow the same handler to respond to multiple different
	 * target URIs which match the wildcard scheme */
	config.uri_match_fn = httpd_uri_match_wildcard;

	ESP_LOGD(__FUNCTION__, "Starting HTTP Server on port: '%d'", config.server_port);
	if (httpd_start(&server, &config) != ESP_OK) {
		ESP_LOGE(__FUNCTION__, "Failed to start file server!");
		return ESP_FAIL;
	}

	/* URI handler for root */
	httpd_uri_t root = {
		.uri		 = "/",	// Match all URIs of type /path/to/file
		.method		 = HTTP_GET,
		.handler	 = root_get_handler,
		//.user_ctx  = server_data	// Pass server data as context
	};
	httpd_register_uri_handler(server, &root);

	/* URI handler for change mode */
	httpd_uri_t change_mode = {
		.uri		 = "/changeMode/*",	// Match all URIs of type /changeMode/*
		.method		 = HTTP_POST,
		.handler	 = change_mode_handler,
		//.user_ctx  = server_data	// Pass server data as context
	};
	httpd_register_uri_handler(server, &change_mode);

	/* URI handler for change value */
	httpd_uri_t change_value = {
		.uri		 = "/changeValue/*",	// Match all URIs of type /changeValue/*
		.method		 = HTTP_POST,
		.handler	 = change_value_handler,
		//.user_ctx  = server_data	// Pass server data as context
	};
	httpd_register_uri_handler(server, &change_value);

	/* URI handler for getting system info */
	httpd_uri_t system_info_get_uri = {
		.uri		 = "/api/system/info",
		.method		 = HTTP_GET,
		.handler	 = system_info_get_handler,
		.user_ctx = rest_context
	};
	httpd_register_uri_handler(server, &system_info_get_uri);

	/* URI handler for getting gpio info */
	httpd_uri_t gpio_info_get_uri = {
		.uri		 = "/api/gpio/info",
		.method		 = HTTP_GET,
		.handler	 = gpio_info_get_handler,
		.user_ctx = rest_context
	};
	httpd_register_uri_handler(server, &gpio_info_get_uri);

	/* URI handler for setting gpio mode */
	httpd_uri_t gpio_mode_set_uri = {
		.uri		 = "/api/gpio/mode",
		.method		 = HTTP_POST,
		.handler	 = gpio_mode_set_handler,
		.user_ctx = rest_context
	};
	httpd_register_uri_handler(server, &gpio_mode_set_uri);

	/* URI handler for setting gpio value */
	httpd_uri_t gpio_value_set_uri = {
		.uri		 = "/api/gpio/value",
		.method		 = HTTP_POST,
		.handler	 = gpio_value_set_handler,
		.user_ctx = rest_context
	};
	httpd_register_uri_handler(server, &gpio_value_set_uri);

	/* URI handler for favicon.ico */
	httpd_uri_t _favicon_get_handler = {
		.uri		 = "/favicon.ico",
		.method		 = HTTP_GET,
		.handler	 = favicon_get_handler,
		//.user_ctx  = server_data	// Pass server data as context
	};
	httpd_register_uri_handler(server, &_favicon_get_handler);

	return ESP_OK;
}

void http_server_task(void *pvParameters)
{
	char *task_parameter = (char *)pvParameters;
	ESP_LOGI(TAG, "Start task_parameter=%s", task_parameter);
	char url[64];
	sprintf(url, "http://%s:%d", task_parameter, CONFIG_WEB_PORT);
	ESP_LOGI(TAG, "Starting server on %s", url);

	for(int index=0;index<ngpios;index++) {
		ESP_LOGI(TAG, "gpios[%d] pin=%d mode=%d value=%d",
		index, gpios[index].pin, gpios[index].mode, gpios[index].value);
		//gpio_pad_select_gpio(gpios[index].pin);
		gpio_reset_pin(gpios[index].pin);
		if (gpios[index].mode == MODE_INPUT) {
			gpio_set_direction(gpios[index].pin, GPIO_MODE_INPUT );
		} else {
			gpio_set_direction(gpios[index].pin, GPIO_MODE_OUTPUT );
			gpio_set_level(gpios[index].pin, gpios[index].value);
		}
	}

	ESP_ERROR_CHECK(start_server("/spiffs", CONFIG_WEB_PORT));
	
	GPIO_t gpioBuf;
	while(1) {
		//Waiting for HTTP event.
		if (xQueueReceive(xQueueHttp, &gpioBuf, portMAX_DELAY) == pdTRUE) {
			ESP_LOGI(TAG, "gpioBuf.command=%d", gpioBuf.command);
			if (gpioBuf.command == CMD_SETMODE) {
				ESP_LOGI(TAG, "SETMODE gpioBuf.pin=%d gpioBuf.mode=%d", gpioBuf.pin, gpioBuf.mode);
				int index = gpioBuf.pin;
				if (gpioBuf.mode == MODE_INPUT) {
					gpio_set_direction(gpios[index].pin, GPIO_MODE_INPUT );
				} else {
					gpio_set_direction(gpios[index].pin, GPIO_MODE_OUTPUT );
					gpios[index].value = 0;
					gpio_set_level(gpios[index].pin, gpios[index].value);
				}
				gpios[index].mode = gpioBuf.mode;
			} else if (gpioBuf.command == CMD_SETVALUE) {
				ESP_LOGI(TAG, "SETVALUE gpioBuf.pin=%d gpioBuf.value=%d", gpioBuf.pin, gpioBuf.value);
				int index = gpioBuf.pin;
				gpio_set_level(gpios[index].pin, gpioBuf.value);
				gpios[index].value = gpioBuf.value;
			}
		}
	}

	// Never reach here
	ESP_LOGI(TAG, "finish");
	vTaskDelete(NULL);
}
