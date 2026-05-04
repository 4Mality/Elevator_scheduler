#include "api.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>

#define MAX_URL 512

static char base_url[MAX_URL] = "http://127.0.0.1:8080";

typedef struct {
    char  *data;
    size_t size;
    size_t capacity;
} ResponseBuffer;

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total = size * nmemb;
    ResponseBuffer *buf = (ResponseBuffer *)userp;

    if (buf->size + total + 1 > buf->capacity) {
        buf->capacity *= 2;
        buf->data = realloc(buf->data, buf->capacity);
    }

    memcpy(buf->data + buf->size, contents, total);
    buf->size += total;
    buf->data[buf->size] = '\0';
    return total;
}

void api_init() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void api_cleanup() {
    curl_global_cleanup();
}

void api_set_base_url(const char *port) {
    snprintf(base_url, MAX_URL, "http://127.0.0.1:%s", port);
}

static int api_request(const char *endpoint, const char *method, char *response, int response_size) {
    CURL *curl;
    CURLcode res;

    ResponseBuffer buf;
    buf.capacity = 1024;
    buf.size     = 0;
    buf.data     = malloc(buf.capacity);
    if (!buf.data) return -1;
    buf.data[0] = '\0';

    curl = curl_easy_init();
    if (!curl) { free(buf.data); return -1; }

    char url[MAX_URL];
    snprintf(url, MAX_URL, "%s%s", base_url, endpoint);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);

    if (strcmp(method, "PUT") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
    }

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl error: %s\n", curl_easy_strerror(res));
        free(buf.data);
        curl_easy_cleanup(curl);
        return -1;
    }

    strncpy(response, buf.data, response_size - 1);
    response[response_size - 1] = '\0';

    free(buf.data);
    curl_easy_cleanup(curl);
    return 0;
}

int api_start_simulation() {
    char response[256];
    return api_request("/Simulation/start", "PUT", response, sizeof(response));
}

int api_check_simulation(char *response, int response_size) {
    return api_request("/Simulation/check", "GET", response, response_size);
}

int api_get_next_input(char *response, int response_size) {
    return api_request("/NextInput", "GET", response, response_size);
}

int api_get_elevator_status(const char *elevatorID, char *response, int response_size) {
    char endpoint[MAX_URL];
    snprintf(endpoint, MAX_URL, "/ElevatorStatus/%s", elevatorID);
    return api_request(endpoint, "GET", response, response_size);
}

int api_add_person_to_elevator(const char *personID, const char *elevatorID, char *response, int response_size) {
    char endpoint[MAX_URL];
    snprintf(endpoint, MAX_URL, "/AddPersonToElevator/%s/%s", personID, elevatorID);
    return api_request(endpoint, "PUT", response, response_size);
}
