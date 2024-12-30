#ifndef __HTTP_REQUEST_H__
#define __HTTP_REQUEST_H__



void http_get_task(void *pvParameters);
int get_sign();
void set_id_finger(int id);
void set_sign(int sign);
int get_id_finger();
void http_post(char json_data[], const char api_post[]);

#endif // __HTTP_REQUEST_H__