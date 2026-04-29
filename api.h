#ifndef API_H
#define API_H

// API for the elevator simulation
void api_set_base_url(const char *port);

// API functions
int api_start_simulation();
int api_check_simulation(char *response, int response_size);
int api_get_next_input(char *response, int response_size);
int api_get_elevator_status(const char *elevatorID,char *response, int response_size);
int api_add_person_to_elevator(const char *personID, const char *elevatorID, char *response, int response_size);

#endif // API_H