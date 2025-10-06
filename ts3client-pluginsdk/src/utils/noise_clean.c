#include "noise_clean.h"

static float mean = 0;
static int noise_length = 0;
static int noise_status = UNLOADED;

int compute_file_path(char* path, char* output_path){
    char* full_path = malloc(sizeof(char)*1024);

    if (path[0] == '~') {
        const char *home = getenv("HOME"); // get home directory
        if (home) {
            snprintf(full_path, sizeof(full_path), "%s%s", home, path + 1);
        } else {
            return 1;
        }
    } else {
        strncpy(full_path, path, sizeof(full_path));
        full_path[sizeof(full_path)-1] = '\0';
    }

    output_path = full_path;

    return 0;
}

//cancella il vecchio file dove hai salvato il profilo del rumore
//il path è definito nei valori di default
int delete_noiseprof_file(){
    char* fullpath = malloc(sizeof(char)*1024);
    compute_file_path(noise_noiseprofilefile, fullpath);
    return remove(fullpath);
}


//salva il rumore in un file definito nei valori di default
//salva sempre in append, ricordati di cancellare i vecchi file!    
int save_noise(uint8_t* raw_audio, size_t n_sample){
    char* fullpath = malloc(sizeof(char)*1024);
    compute_file_path(noise_noiserecordingfile, fullpath);
    FILE* fp = fopen(fullpath, "a");
    fwrite(raw_audio, sizeof(uint8_t), n_sample, fp);
    fclose(fp);
    noise_status = RELOAD;
    return 0;
}


// a partire dal file con il rumore salvato, applica il primo algoritmo per calcolare il profilo del rumore
//il dato viene salvato in un file
//noise_supprlevel is DFT_BINS
int compute_noise_profile(){
    char* fullpath = malloc(sizeof(char)*1024);
    compute_file_path(noise_noiserecordingfile, fullpath);

    FILE* noise_file = fopen(fullpath, "rb");
    if (noise_file == NULL){
        free(fullpath);
        noise_status = NOISE_UNDEF;
        return 1; 
    }

    sdft_fdx_t* noise = malloc(100000 * noise_suppr_level * sizeof(sdft_fdx_t));
    noise_length = pcm_uint8_t_read_and_dft(noise_file, noise);


    float mean_i[noise_length*noise_suppr_level]; 

    for (int i=0; i<noise_length*noise_suppr_level; i++){
        mean_i[i] = sqrt(noise[i].r*noise[i].r
                        + noise[i].i*noise[i].i);
    }
    
    for (int i = 0; i < noise_length*noise_suppr_level; i ++) mean += mean_i[i];
    mean /= noise_length*noise_suppr_level;
    printf("noise magnitude is %f\n", mean);
    
    fwrite(&mean, sizeof(float), 1, noise_file);
    fwrite(&noise_length, sizeof(int), 1, noise_file);
    fclose(noise_file);

    noise_status = LOADED;

    return 0;
}

//se esiste il file con il profilo del rumore, caricalo in memoria
int load_noise_profile(){
    
    char* fullpath = malloc(sizeof(char)*1024);
    compute_file_path(noise_noiseprofilefile, fullpath);

    FILE* noise_file = fopen(fullpath, "rb");
    if (noise_file == NULL){
        free(fullpath);
        noise_status = NOISE_UNDEF;
        return 1; 
    }

    fread(&mean, sizeof(float), 1, noise_file);
    fread(&noise_length, sizeof(int), 1, noise_file);
    fclose(noise_file);

    noise_status = LOADED;
    return 0;
}


size_t pcm_uint8_t_read_and_dft(FILE* file, sdft_fdx_t* output){
    uint8_t input_data[MAX_INPUT];
    size_t input_length = 0;
    int _r = 0;

    printf("Reading file\n");

    while(_r = fread(&(input_data[input_length]), sizeof(uint8_t), 512, file)) input_length += _r;

    float input_data_f[input_length];

    for (int i=0; i < input_length; i++){
        input_data_f[i] = input_data[i];
        input_data_f[i] = input_data_f[i] * 0.00784313725490196078f;    /* 0..255 to 0..2 */
        input_data_f[i] = input_data_f[i] - 1;
    }

    // float* x = input_data_f; // analysis samples of shape (n)
    float* y = malloc(input_length*sizeof(float)); // synthesis samples of shape (n)

    sdft_t* sdft = sdft_alloc(noise_suppr_level); // create sdft plan

    // sdft_float_complex_t* buffer = malloc(HOP_SIZE * m * sizeof(sdft_float_complex_t));

    // output = malloc(input_length * DFT_BINS * sizeof(sdft_fdx_t));

    sdft_sdft_n(sdft, input_length, input_data_f, output); // extract dft matrix from input samples
    printf("rightest (%f, %f)\n", output[180].r, output[180].i);

    // sdft_free(sdft);

    return input_length;
}

void idft_and_float_conversion(sdft_fdx_t* data, size_t length, short* output){
    sdft_t* sdft = sdft_alloc(noise_suppr_level); // create sdft plan

    float buffer[length];
    
    data[180].r = 0; data[180].i = 0;

    sdft_isdft_n(sdft, length, data, buffer); // synthesize output samples from dft matrix
    
    int r;
    size_t i;
    for (i = 0; i < length; ++i) {
        float x = buffer[i];
        float c;
        c = ((x < -1) ? -1 : ((x > 1) ? 1 : x));
        c = c + 1;
        r = (int)(c * 32767.5f);
        r = r - 32768;
        output[i] = (short)r;  
    }
}

//rimuovi il rumore
int remove_noise(uint8_t* input, size_t input_length, uint8_t* output){

    switch(noise_status) {
        case UNLOADED:
            if (!load_noise_profile()){
                return 1;
            }
            break;
        case RELOAD:
            compute_noise_profile();
            if (!load_noise_profile()){
                return 1;
            }
            break;
        case NOISE_UNDEF:
            return 1;
            break;
        case LOADED:
            break;
        default:
            break;
    }

    sdft_fdx_t* data = malloc(MAX_INPUT * noise_suppr_level *sizeof(sdft_fdx_t));
    
    for (int b=0; b < noise_suppr_level; b++){

        for (int i=0; i < input_length; i++){
            float phase = atan2(data[b*input_length + i].i, data[b*input_length + i].r);
            float magn = sqrt(data[b*input_length + i].r*data[b*input_length + i].r
                         + data[b*input_length + i].i*data[b*noise_length + i].i);
            magn = magn - mean > 0 ? magn - mean : 0;

            data[b*input_length + i].r = magn * cosf(phase);
            data[b*input_length + i].i = magn * sinf(phase);
        }
    }
    
    short short_output[input_length];
    idft_and_float_conversion(data, input_length, output);

    return 0;

}

/*
 TODO 
 - aggiungere logica nel main
    - chiamare funzione di cancellazione del rumore (se l'impostazione è attiva) ✅
    - aggiungere funzione per registrare audio del rumore per calibrazione per 30 secondi 🚧
    - aggiungere funzione nel firmware per registrare rumores
    - aggiungere record_noise a help()
    - server.c quando riceve audio deve controllare se la variabile di rec e' 1, in tal caso deve salvare nel file il rumore
    - plugin.c non deve applicare la soppressione del rumore se la variabile di rec e' 1 ✅

    NOTE test audio con sdft: 
    - ./sdft audio2clean_S16.pcm noise.pcm output
    - aplay -f S16 -r 8000 -c 1 output
*/