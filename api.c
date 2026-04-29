#include "api.h"
#include <string.h>
#include <stdio.h>
#include <curl/curl.h>

#define MAX_URL 512

static char base_url[MAX_URL] = "http://localhost:8080";

typedef struct {
    char *data;
    size_t size;
    int capacity;
} ResponseBuffer;

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total_size = size * nmemb;
    ResponseBuffer *buffer = (ResponseBuffer *)userp;

    if (buffer->size + total_size + 1 > buffer->capacity) {
        buffer->capacity *= 2;
        buffer->data = realloc(buffer->data, buffer->capacity);
    }

    memcpy(buffer->data + buffer->size, contents, total_size);
    buffer->size += total_size;
    buffer->data[buffer->size] = '\0';

    return total_size;
}

void api_set_base_url(const char *port) {
    snprintf(base_url, MAX_URL, "http://localhost:%s", port);
}

static int api_request(const char *endpoint, const char *method, const char *payload, char *response, int response_size) {
    CURL *curl;
    CURLcode res;
    ResponseBuffer buffer = {0};

    buffer.capacity = 1024;
    buffer.data = malloc(buffer.capacity);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl) {
        char url[MAX_URL];
        snprintf(url, MAX_URL, "%s%s", base_url, endpoint);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

        if (strcmp(method, "POST") == 0) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
        }

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            free(buffer.data);
            curl_easy_cleanup(curl);
            return -1;
        }

        strncpy(response, buffer.data, response_size - 1);
        response[response_size - 1] = '\0';

        free(buffer.data);
        curl_easy_cleanup(curl);
    } else {
        free(buffer.data);
        return -1;
    }

    return 0;
}

int api_start_simulation() {
    char response[256];
    return api_request("/start", "POST", "", response, sizeof(response));
}

int api_check_simulation(char *response, int response_size) {
    return api_request("/check", "GET", "", response, response_size);
}

int api_get_next_input(char *response, int response_size) {
    return api_request("/next", "GET", "", response, response_size);
}

int api_get_elevator_status(const char *elevatorID, char *response, int response_size) {
    char endpoint[MAX_URL];
    snprintf(endpoint, MAX_URL, "/elevator/%s/status", elevatorID);
    return api_request(endpoint, "GET", "", response, response_size);
}

int api_add_person_to_elevator(const char *personID, const char *elevatorID, char *response, int response_size) {
    char endpoint[MAX_URL];
    char payload[256];
    snprintf(endpoint, MAX_URL, "/elevator/%s/add", elevatorID);
    snprintf(payload, sizeof(payload), "{\"personID\": \"%s\"}", personID);
    return api_request(endpoint, "POST", payload, response, response_size);
}
