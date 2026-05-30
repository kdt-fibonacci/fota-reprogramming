#ifndef SERVER_H
#define SERVER_H

void start_server(void);

void forward_report_to_dashboard(const char* json_body);

#endif