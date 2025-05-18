#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define MAX_SAMPLES 100000

int main(int argc, char** argv){
	if (argc != 3){
     	   printf("Usage: %s U8_input.pcm S16_output.pcm\n", argv[0]);
           exit(1);
    	}
	
	FILE* input = fopen(argv[1], "rb");
	int input_length = 0, _r = 0;
	uint8_t input_data[MAX_SAMPLES];
	while(_r = fread(&(input_data[input_length]), sizeof(uint8_t), 512, input)) input_length += _r; //input_length equivale a samples, perche' un sample e' 1 byte in questo caso specifico
	fclose(input);
	
	short output_data[input_length];
	for (int i=0; i < input_length; i++){
		output_data[i] = ((int16_t)(input_data[i]) << 8) - 32768;
	}


	FILE* output = fopen(argv[2], "wb");
	fwrite(output_data, sizeof(short), input_length, output);
	fclose(output);

	return 0;
}
