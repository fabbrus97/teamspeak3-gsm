FILENAME = spectral_sub

all:
	gcc -o $(FILENAME) stft-main.c -lm -lkissfft-float

demo:
	./${FILENAME} audio2clean_S16.pcm noise.pcm output.pcm

clean:
	rm $(FILENAME)
	# rm *.o

