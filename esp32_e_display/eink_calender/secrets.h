#ifndef SECRETS_H
#define SECRETS_H

const char* ssid = "iptime Mesh";
const char* password = "0553213247";

const char* ha_base_url = "https://iot.eileens-garden.co.kr";
const char* ha_token = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiI4ZDU2MDdhODM5Y2M0MDIwOTU3NmQ4N2E4ZTdjODk0MyIsImlhdCI6MTc2NzEzNzU2MSwiZXhwIjoyMDgyNDk3NTYxfQ.bcfT9bRGqbGdlMJE_WvZpnYp-gTdIRa2jVHSMe9n5eQ";

// Replace with your actual calendar entity ID
// Available options based on your curl output:
// calendar.jun3021303_gmail_com
// calendar.gaeiniljeong (Personal)
// calendar.gajog (Family)
// calendar.daehanmingugyi_hyuil (Holidays)
const char* calendar_entity_id = "calendar.jun3021303_gmail_com"; 

#endif
