all:avi face main v4l2

avi:avi.cpp
	g++ `pkg-config opencv --cflags` avi.cpp -o avi `pkg-config opencv --libs` -ljpeg
face:face.cpp
	g++ `pkg-config opencv --cflags` face.cpp -o face `pkg-config opencv --libs` -ljpeg
main:main.cpp
	g++ `pkg-config opencv --cflags` main.cpp -o main `pkg-config opencv --libs` -ljpeg -g
v4l2:v4l2.c
	gcc -o v4l2 -g v4l2.c -ljpeg