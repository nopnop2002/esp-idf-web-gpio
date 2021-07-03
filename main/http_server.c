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
#include "esp_http_server.h"
#include <driver/gpio.h>

#include "gpio.h"

static const char *TAG = "HTTP";
//static char *HTML_BODY = "/spiffs/body";
//static SemaphoreHandle_t ctrl_task_sem;

extern QueueHandle_t xQueueHttp;
extern GPIO_t *gpios;
extern int16_t ngpios;

static void SPIFFS_Directory(char * path) {
	DIR* dir = opendir(path);
	assert(dir != NULL);
	while (true) {
		struct dirent*pe = readdir(dir);
		if (!pe) break;
		ESP_LOGI(__FUNCTION__,"d_name=%s d_ino=%d d_type=%x", pe->d_name,pe->d_ino, pe->d_type);
	}
	closedir(dir);
}

// Calculate the size after conversion to base64
// http://akabanessa.blog73.fc2.com/blog-entry-83.html
int32_t calcBase64EncodedSize(int origDataSize)
{
	// 6bit単位のブロック数（6bit単位で切り上げ）
	// Number of blocks in 6-bit units (rounded up in 6-bit units)
	int32_t numBlocks6 = ((origDataSize * 8) + 5) / 6;
	// 4文字単位のブロック数（4文字単位で切り上げ）
	// Number of blocks in units of 4 characters (rounded up in units of 4 characters)
	int32_t numBlocks4 = (numBlocks6 + 3) / 4;
	// 改行を含まない文字数
	// Number of characters without line breaks
	int32_t numNetChars = numBlocks4 * 4;
	// 76文字ごとの改行（改行は "\r\n" とする）を考慮したサイズ
	// Size considering line breaks every 76 characters (line breaks are "\ r \ n")
	//return numNetChars + ((numNetChars / 76) * 2);
	return numNetChars;
}

esp_err_t Image2Base64(char * filename, size_t fsize, unsigned char * base64_buffer, size_t base64_buffer_len)
{
	unsigned char* image_buffer = NULL;
	image_buffer = malloc(fsize);
	if (image_buffer == NULL) {
		ESP_LOGE(TAG, "malloc fail. image_buffer %d", fsize);
		return ESP_FAIL;
	}

	FILE * fp;
	if((fp=fopen(filename,"rb"))==NULL){
		ESP_LOGE(TAG, "fopen fail. [%s]", filename);
		free(image_buffer);
		return ESP_FAIL;
	} else {
		for (int i=0;i<fsize;i++) {
			fread(&image_buffer[i],sizeof(char),1,fp);
		}
		fclose(fp);
	}

	size_t encord_len;
	esp_err_t ret = mbedtls_base64_encode(base64_buffer, base64_buffer_len, &encord_len, image_buffer, fsize);
	ESP_LOGI(TAG, "mbedtls_base64_encode=%d encord_len=%d", ret, encord_len);
	free(image_buffer);
	return ret;
}

esp_err_t Image2Button(httpd_req_t *req, char * imageFileName, char * action)
{
	esp_err_t ret = ESP_FAIL;
	struct stat st;
	if (stat(imageFileName, &st) != 0) {
		ESP_LOGE(TAG, "[%s] not found", imageFileName);
		return ret;
	}

	char extent[8] = {0};
	for (int i=0;i<strlen(imageFileName);i++) {
		if (imageFileName[i] == '.') strcpy(extent, &imageFileName[i+1]);
	}
	ESP_LOGI(TAG, "extent=%s", extent);

	ESP_LOGI(TAG, "%s exist st.st_size=%ld", imageFileName, st.st_size);
	int32_t base64Size = calcBase64EncodedSize(st.st_size);
	ESP_LOGI(TAG, "base64Size=%d", base64Size);

	// Convert from JPEG to BASE64
	unsigned char*	img_src_buffer = NULL;
	size_t img_src_buffer_len = base64Size + 1;
	img_src_buffer = malloc(img_src_buffer_len);
	if (img_src_buffer == NULL) {
		ESP_LOGE(TAG, "malloc fail. img_src_buffer_len %d", img_src_buffer_len);
	} else {
		ret = Image2Base64(imageFileName, st.st_size, img_src_buffer, img_src_buffer_len);
		ESP_LOGI(TAG, "Image2Base64=%d", ret);
		if (ret != 0) {
			ESP_LOGE(TAG, "Error in mbedtls encode! ret = -0x%x", -ret);
		} else {
			httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"");
			httpd_resp_sendstr_chunk(req, action);
			httpd_resp_sendstr_chunk(req, "\">");
			httpd_resp_sendstr_chunk(req, "<button type=\"submit\">");

			// <img src="data:image/jpeg;base64,ENCORDED_DATA" />
			if (strcmp(extent, "png") == 0) {
				httpd_resp_sendstr_chunk(req, "<img src=\"data:image/png;base64,");
			} else {
				httpd_resp_sendstr_chunk(req, "<img src=\"data:image/jpeg;base64,");
			}
			httpd_resp_sendstr_chunk(req, (char *)img_src_buffer);
			httpd_resp_sendstr_chunk(req, "\" />");

			httpd_resp_sendstr_chunk(req, "</button></form>");
			if (img_src_buffer != NULL) free(img_src_buffer);
			ret = ESP_OK;
		}
	}
	return ret;
}

esp_err_t Text2Button(httpd_req_t *req, char * textFileName, char * action)
{
	esp_err_t ret = ESP_FAIL;
	struct stat st;
	if (stat(textFileName, &st) != 0) {
		ESP_LOGE(TAG, "[%s] not found", textFileName);
		return ret;
	}

	FILE * fp;
	char  buffer[64];
	if((fp=fopen(textFileName, "r"))==NULL){
		ESP_LOGE(TAG, "fopen fail. [%s]", textFileName);
		return ESP_FAIL;
	}else{

		if (strlen(action) > 0) {
			httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"");
			httpd_resp_sendstr_chunk(req, action);
			httpd_resp_sendstr_chunk(req, "\">");
			httpd_resp_sendstr_chunk(req, "<button type=\"submit\">");
		}


		//httpd_resp_sendstr_chunk(req, "<img src=\"data:image/jpeg;base64,");
		httpd_resp_sendstr_chunk(req, "<img src=\"data:image/png;base64,");
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
		httpd_resp_sendstr_chunk(req, "\" />");
	}
	if (strlen(action) > 0) {
		httpd_resp_sendstr_chunk(req, "</button></form>");
	}
	return ESP_OK;
}

/* HTTP GET handler */
static esp_err_t root_get_handler(httpd_req_t *req)
{
	ESP_LOGD(TAG, "root_get_handler req->uri=[%s]", req->uri);

#if 0
	SPIFFS_Directory("/icons/");
#endif
	for(int index=0;index<ngpios;index++) {
		ESP_LOGD(TAG, "gpios[%d] pin=%d mode=%d value=%d",
		index, gpios[index].pin, gpios[index].mode, gpios[index].value);
	}

	/* Send HTML file header */
	httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><body>");

	// Start the table
	httpd_resp_sendstr_chunk(req, "<table border=\"1\">");
	httpd_resp_sendstr_chunk(req, "<thead><tr><th>GPIO</th><th>CURRENT MODE</th><th>SET MODE</th><th>CURRENT VALUE</th><th>SET VALUE</th></tr></thead>");

	char chunk[32];
	char textFileName[64];
	char action[64];
	for(int index=0;index<ngpios;index++) {
		ESP_LOGD(TAG, "gpios[%d] pin=%d mode=%d value=%d",
		index, gpios[index].pin, gpios[index].mode, gpios[index].value);
		httpd_resp_sendstr_chunk(req, "<tr>");
		sprintf(chunk, "<td align=\"center\">%02d</td>", gpios[index].pin);
		httpd_resp_sendstr_chunk(req, chunk);
		if (gpios[index].mode == 1) { // INPUT pin
			strcpy(textFileName, "/icons/box-in-icon.txt");
			// Get current value
			gpios[index].value = gpio_get_level(gpios[index].pin);
			ESP_LOGI(TAG, "gpios.pin=%d value=%d", gpios[index].pin, gpios[index].value);
		} else { // OUTPUT pin
			strcpy(textFileName, "/icons/box-out-icon.txt");
		}
		httpd_resp_sendstr_chunk(req, "<td align=\"center\">");
		Text2Button(req, textFileName, "");
		httpd_resp_sendstr_chunk(req, "</td>");

		// change mode
		if (gpios[index].mode == 1) {
			strcpy(textFileName, "/icons/turn-out-icon.txt");
			// POST to [/changeMode/OUTPUT/index]
			sprintf(action, "/changeMode%.10sOUTPUT%.10s%d", req->uri, req->uri, index);
		} else {
			strcpy(textFileName, "/icons/turn-in-icon.txt");
			// POST to [/changeMode/INPUT/index]
			sprintf(action, "/changeMode%.10sINPUT%.10s%d", req->uri, req->uri, index);
		}
		httpd_resp_sendstr_chunk(req, "<td align=\"center\">");
		Text2Button(req, textFileName, action);
		httpd_resp_sendstr_chunk(req, "</td>");

		// current value
		if (gpios[index].value == 0) {
			strcpy(textFileName, "/icons/current-off-icon.txt");
		} else {
			strcpy(textFileName, "/icons/current-on-icon.txt");
		}
		httpd_resp_sendstr_chunk(req, "<td align=\"center\">");
		Text2Button(req, textFileName, "");
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
				sprintf(action, "/changeValue%.10sON%.10s%d", req->uri, req->uri, index);
			} else {
				strcpy(textFileName, "/icons/turn-off-icon.txt");
				// POST to [/changeValue/OFF/index]
				sprintf(action, "/changeValue%.10sOFF%.10s%d", req->uri, req->uri, index);
			}
			httpd_resp_sendstr_chunk(req, "<td align=\"center\">");
			Text2Button(req, textFileName, action);
			httpd_resp_sendstr_chunk(req, "</td>");
		}
		httpd_resp_sendstr_chunk(req, "</tr>");
	}

	/* Finish the table */
	httpd_resp_sendstr_chunk(req, "</tbody></table>");

	/* Send remaining chunk of HTML file to complete it */
	httpd_resp_sendstr_chunk(req, "</body></html>");

	/* Send empty chunk to signal HTTP response completion */
	httpd_resp_sendstr_chunk(req, NULL);

	return ESP_OK;
}

/* HTTP change mode handler */
static esp_err_t change_mode_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "change_mode_handler req->uri=[%s]", req->uri);
	ESP_LOGI(TAG, "change_mode_handler req->uri=[%s]", req->uri+strlen("/changeMode/"));

	GPIO_t gpioBuf;
	long wk;
	gpioBuf.command = CMD_SETMODE;
	if (strncmp(req->uri+strlen("/changeMode/"), "INPUT/", 6) == 0) {
		wk = strtol(req->uri+strlen("/changeMode/INPUT/"), NULL, 10);
		ESP_LOGI(TAG, "[to INPUT] index=%ld gpio=%d", wk, gpios[wk].pin);
		gpioBuf.pin = wk;
		gpioBuf.mode = MODE_INPUT;
		if (xQueueSend(xQueueHttp, &gpioBuf, portMAX_DELAY) != pdPASS) {
			ESP_LOGE(TAG, "xQueueSend Fail");
		}
	} else if (strncmp(req->uri+strlen("/changeMode/"), "OUTPUT/", 7) == 0) {
		wk = strtol(req->uri+strlen("/changeMode/OUTPUT/"), NULL, 10);
		ESP_LOGI(TAG, "[to OUTPUT] index=%ld gpio=%d", wk, gpios[wk].pin);
		gpioBuf.pin = wk;
		gpioBuf.mode = MODE_OUTPUT;
		if (xQueueSend(xQueueHttp, &gpioBuf, portMAX_DELAY) != pdPASS) {
			ESP_LOGE(TAG, "xQueueSend Fail");
		}
	}

	/* Redirect onto root to see the updated file list */
	httpd_resp_set_status(req, "303 See Other");
	httpd_resp_set_hdr(req, "Location", "/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
	httpd_resp_set_hdr(req, "Connection", "close");
#endif
	httpd_resp_sendstr(req, "File change mode successfully");
	return ESP_OK;
}

/* HTTP change value handler */
static esp_err_t change_value_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "change_value_handler req->uri=[%s]", req->uri);
	ESP_LOGI(TAG, "change_value_handler req->uri=[%s]", req->uri+strlen("/changeValue/"));

	GPIO_t gpioBuf;
	long wk;
	gpioBuf.command = CMD_SETVALUE;
	if (strncmp(req->uri+strlen("/changeValue/"), "ON/", 3) == 0) {
		wk = strtol(req->uri+strlen("/changeValue/ON/"), NULL, 10);
		ESP_LOGI(TAG, "[to ON] index=%ld gpio=%d", wk, gpios[wk].pin);
		gpioBuf.pin = wk;
		gpioBuf.value = 1;
		if (xQueueSend(xQueueHttp, &gpioBuf, portMAX_DELAY) != pdPASS) {
			ESP_LOGE(TAG, "xQueueSend Fail");
		}
	} else if (strncmp(req->uri+strlen("/changeValue/"), "OFF/", 4) == 0) {
		wk = strtol(req->uri+strlen("/changeValue/OFF/"), NULL, 10);
		ESP_LOGI(TAG, "[to OFF] index=%ld gpio=%d", wk, gpios[wk].pin);
		gpioBuf.pin = wk;
		gpioBuf.value = 0;
		if (xQueueSend(xQueueHttp, &gpioBuf, portMAX_DELAY) != pdPASS) {
			ESP_LOGE(TAG, "xQueueSend Fail");
		}
	}

	/* Redirect onto root to see the updated file list */
	httpd_resp_set_status(req, "303 See Other");
	httpd_resp_set_hdr(req, "Location", "/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
	httpd_resp_set_hdr(req, "Connection", "close");
#endif
	httpd_resp_sendstr(req, "File change value successfully");
	return ESP_OK;
}

/* Function to start the file server */
esp_err_t start_server(const char *base_path, int port)
{
	httpd_handle_t server = NULL;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.server_port = port;

	/* Use the URI wildcard matching function in order to
	 * allow the same handler to respond to multiple different
	 * target URIs which match the wildcard scheme */
	config.uri_match_fn = httpd_uri_match_wildcard;

	ESP_LOGD(TAG, "Starting HTTP Server on port: '%d'", config.server_port);
	if (httpd_start(&server, &config) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to start file server!");
		return ESP_FAIL;
	}

	/* URI handler for root */
	httpd_uri_t root = {
		.uri	   = "/",	// Match all URIs of type /path/to/file
		.method    = HTTP_GET,
		.handler   = root_get_handler,
		//.user_ctx  = server_data	// Pass server data as context
	};
	httpd_register_uri_handler(server, &root);

	/* URI handler for change mode */
	httpd_uri_t change_mode = {
		.uri	   = "/changeMode/*",	// Match all URIs of type /changeMode/*
		.method    = HTTP_POST,
		.handler   = change_mode_handler,
		//.user_ctx  = server_data	// Pass server data as context
	};
	httpd_register_uri_handler(server, &change_mode);

	/* URI handler for change value */
	httpd_uri_t change_value = {
		.uri	   = "/changeValue/*",	// Match all URIs of type /changeValue/*
		.method    = HTTP_POST,
		.handler   = change_value_handler,
		//.user_ctx  = server_data	// Pass server data as context
	};
	httpd_register_uri_handler(server, &change_value);

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
		gpio_pad_select_gpio(gpios[index].pin);
		if (gpios[index].mode == MODE_INPUT) {
			gpio_set_direction(gpios[index].pin, GPIO_MODE_INPUT );
		} else {
			gpio_set_direction(gpios[index].pin, GPIO_MODE_OUTPUT );
			gpio_set_level(gpios[index].pin, gpios[index].value);
		}
	}

#if 0
	// Create Semaphore
	// This Semaphore is used for locking
	ctrl_task_sem = xSemaphoreCreateBinary();
	configASSERT( ctrl_task_sem );
	xSemaphoreGive(ctrl_task_sem);
#endif

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
