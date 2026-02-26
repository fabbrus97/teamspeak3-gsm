#include "SIMutils.hpp"

int ACCEPT_ONLY_PB = 0;

int call_in_progress = 0; 
int recording_noise = 0; 


int check_caller_in_pb(const char* str){
  int i=0, check=0, comma_counter=0;
  int ret_value=0;
  while (str[i] != '\0'){
    if (str[i] == ','){
      comma_counter += 1;
      i+=1;
      continue;
    }

    if (comma_counter == 4 && str[i] == str[i+1] && str[i] == '"'){
      ret_value = 0;
      break;
    }
    else if (comma_counter == 4){
      ret_value = 1;
      break;
    }
    
    i=i+1;
  
  }
  return ret_value;
}

void parse_serial(const char* str){

  printf("[DEBUG] checking string %s\n", str);
  
  if (strncmp("RING", str, 4) == 0){
    if ((ACCEPT_ONLY_PB && check_caller_in_pb(str)) || !ACCEPT_ONLY_PB){
      if (call_in_progress){
        SerialAT.write("AT+CHLD=2\n");
      } else {
        SerialAT.write("ATA\n");
        call_in_progress = 1;
      }
    }
  }

  if (strncmp("NO CARRIER", str, 10) == 0){ 
    call_in_progress = 0;
  }

}