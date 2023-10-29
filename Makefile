CC = gcc
source = waitTemp.c
target = waitTemp

$(target): $(source)
	$(CC) -o $(target) $(source)

clean:
	rm -rf $(target)