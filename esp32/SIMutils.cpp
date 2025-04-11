#include "SIMutils.hpp"

int call_in_progress = 0;
// extern SoftwareSerial GPRS(3, 2);


int check_caller_in_pb(char* str){
  int i=0, check=0, comma_counter=0;
  while (str[i] != '\0'){
    if (str[i] == ','){
      comma_counter += 1;
      Serial.print("Found a comma, now counter is "); Serial.println(comma_counter);
      i+=1;
      continue;
    }

    if (comma_counter == 4 && str[i] == str[i+1] && str[i] == '"')
      return 0;
    else if (comma_counter == 4)
      return 1;
    
    i=i+1;
  
  }
  // Serial.println("found anything :-(");
  return 0;
}

void parse_serial(char* str){
  
  if (strcmp("RING", str) > 0){
    if ((ACCEPT_ONLY_PB && check_caller_in_pb(str)) || !ACCEPT_ONLY_PB){
      Serial2.write("ATA\n");
      call_in_progress = 1;
    }
  }

  if (strcmp("NO CARRIER", str) >= 0){ 
    call_in_progress = 0;
  }

}